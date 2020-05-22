//
//  SimpleFFI.c
//  TestFFI
//
//  Created by yanjun on 2020/5/5.
//  Copyright © 2020 yanjun. All rights reserved.
//

#include "SimpleFFI.h"
#include <stdlib.h>
#include <stdio.h>

#if defined (__arm64__)

/*
des(x0) 目标地址
src(x1) 源地址
len(x2) 长度
*/
__attribute__ ((naked)) void sffi_memcpy(void* des, void* src, unsigned long len){
    __asm__ (
             "1:\n"
             "cmp x2, 0\n"              //把 x2(len) 与 0 比较
             "b.eq 2f\n"                //相等，这条转到 ret
             "ldrb w9, [x1], #0x1\n"   //从 x1(src) 读 1byte 到 w10，x1++
             "strb w9, [x0], #0x1\n"   //把 w10 写 1byte 到 x0(des)，x0++
             "sub x2, x2, #1\n"         //x2(len)--
             "b 1b\n"                   //跳
             "2:\n"
             "ret"
             );
}

/*
 regX(x0)  整数寄存器buf 8byte * 8
 regV(x1)  浮点寄存器buf 8byte * 8，可为 null
 stack(x2) 栈空间buf，变长，sl>0时，不能为null
 sl(x3)    栈空间buf长度，>=0 必须16byte对齐
 addr(x4)  要调用函数的地址
 rt(x5)    返回值地址，会直接设置给 x8，再没有其他操作
 */
__attribute__ ((naked)) void sffi_raw_call(unsigned long* regX, unsigned long* regV,
                                           unsigned char* stack, long sl, void* addr, void* rt){
    __asm__ __volatile__(
                         "sub sp, sp, #0x10\n"              //要调用目标函数 addr，需要保存 x29(fp) x30(lr），堆上分配空间 0x10(16)字节
                         "stp x29, x30, [sp]\n"             //保存 x29,x30
                         "mov x29, sp\n"                    //设置 x29 为当前的栈顶
    
                         "sub sp, sp, #0x10\n"              //函数最后要用到 x0(regX)，把它保存到栈上，sp要16byte对齐
                         "str x0, [sp]\n"                   //保存x0到栈顶，此时堆栈是这样的 (x0, 不用, x29, x30)
                                                            //现在栈的样子是 (x0, 不用, x29, x30)
                         "mov x8, x5\n"                     //先把 rt 设置给 x8，用不用都可以
                          
                         //设置通过栈传递的参数
                         "sub sp, sp, x3\n"                 //给要通过堆栈传递的参数分配空间，x3(sl) 必须是16byte 对齐
                         "1:\n"
                         "cmp x3, 0\n"                      //if sl==0 then jump 2:
                         "b.eq 2f\n"
                         "sub x3, x3, #1\n"                 //sl--
                         "ldrb w10, [x2, x3]\n"             //stack[sl] -> w10
                         "strb w10, [sp, x3]\n"             //w10 -> sp[sl]
                         "b 1b\n"                           //jump to 1:
                         
                         //设置浮点寄存去
                         "2:\n"
                         "cmp x1, 0\n"                      //if regV==0 then jump to 3:
                         "b.eq 3f\n"
                         "ldp d0, d1, [x1]\n"               //x1(regV)，copy to d0-d7
                         "ldp d2, d3, [x1, 0x10]\n"
                         "ldp d4, d5, [x1, 0x20]\n"
                         "ldp d6, d7, [x1, 0x30]\n"
                         
                         //设置通用寄存器
                         "3:\n"
                         "mov x9, x0\n"                     //x9=x0(regX), 因为传参要用 x0-x7, 后面要用到，先备份一下
                         "mov x10, x4\n"                    //x10=x4(addr)
                         "ldp x0, x1, [x9]\n"               //x9(regX)，把它设置到 x0-x7
                         "ldp x2, x3, [x9, 0x10]\n"
                         "ldp x4, x5, [x9, 0x20]\n"
                         "ldp x6, x7, [x9, 0x30]\n"
                         
                         "blr x10\n"                         //跳 x10(addr)
                         
                         /*
                          返回值可能通过 x0-x4 或 d0-d4 或 入参的x8 返回
                          这里统一处理，把 x0-x4 d0-d4 cp 到 regX[8]中，备用
                          */
                         "ldr x9, [x29, -0x10]\n"            //x9 设置为之前保存在栈上的 x0值, x9=regX
                         "stp x0, x1, [x9]\n"                //x9=regX，保存整数返回值到 0-3  浮点返回到 4-7
                         "stp x2, x3, [x9, 0x10]\n"
                         "stp d0, d1, [x9, 0x20]\n"
                         "stp d2, d3, [x9, 0x30]\n"
                         
                         //恢复
                         "mov sp, x29\n"
                         "ldp x29, x30, [sp] \n"            //恢复 x29(fp) x30(lr)
                         "add sp, sp, #0x10 \n"             //恢复 sp
                         "ret \n"                           //跳 lr
                         );
}

#define SFFI_ALIGN(v,a)  (((((unsigned long) (v))-1) | ((a)-1))+1)
#define SFFI_FUNC_RETURN(ERR_CODE) {err_code=ERR_CODE;goto end;}
#define SFFI_CHECK_RESULT(){if(err_code!=SFFI_ERR_OK){ goto end; }}

//                                a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z
static char gBaseTypeSize[26]  = {0,0,1,8,0,4,0,0,4,0,0,8,1,8,0,8,0,0,2,0,4,0,2,0,0,0}; // 根据signCode 得到对应类型的大小
static char gBaseTypeFFlag[26] = {0,0,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; // 根据signCode 得到是否是浮点数

int sffi_call(const char* sign, unsigned long* args, void* addr, void* rt){
    return sffi_call_var(sign, args, addr, rt, 0);
}

int sffi_call_var(const char* sign, unsigned long* args, void* addr, void* rt, int fixedArg){
    struct sffi_ctpl* ctpl = 0;
    int r = sffi_mk_ctpl(sign, fixedArg, &ctpl);
    if(r == SFFI_ERR_OK){
        r = sffi_call_with_ctpl(ctpl, args, addr, rt);
        sffi_free_ctpl(ctpl);
    }
    return r;
}

//call template arg
struct sffi_arg {
    unsigned char index;       //参数在参数列表中的索引
    unsigned char size;        //参数的大小
    unsigned char isST;        //参数是否是复合结构
    unsigned char offset;      //参数在复合结构中的偏移地址
    unsigned char destType;    //1 复制到通用寄存器上. 2 复制到浮点寄存器上。3 复制到堆栈上
    unsigned char destIndex;   //对应的索引
};

#define SFFI_CTPL_MAGIC 0xc7
struct sffi_ctpl{
    unsigned char magic;       //标志， SFFI_CTPL_MAGIC, release的时候，简单校验一下
    unsigned char sizeGR;      //通用寄存器用的数量
    unsigned char sizeVR;      //浮点寄存器用的数量
    unsigned char sizeStack;   //堆栈用的空间
    unsigned char countArg;    //参数的数量
    unsigned char countFixArg; //固定参数的数量(0表示不是变参函数)
    unsigned char rtPassType;  //SFFI_PASS_BY_VOID
                               //SFFI_PASS_BY_GR_SHARE
                               //SFFI_PASS_BY_VR_MONO
                               //SFFI_PASS_BY_POINT
                               //SFFI_PASS_BY_GR_MONO
    unsigned char rtCount;     //返回值元素的数量，结构体可能是多个
    unsigned char rtSize;      //返回值的大小
    struct sffi_arg args;      //最终生成参数的时候，会生成多个参数，通过 (&args)[0-n] 访问
};

/*
 通过签名，生成到结构体的信息，包括结构体有几个成员，结构体的便宜，每个成员的大小和偏移等
 sign      签名
 st        结构体信息
 endIndex  描述结构体签名结束的地方, 比如 “[cdfi]”, ‘]’ 后就是index的位置
 */
int sffi_get_st_inner(const char* sign, struct sffi_st* st, int* endIndex){
    if(!sign || sign[0] == 0 || !st){
        return SFFI_ERR_INVALID_PARAMS;
    }
    
    int err_code = SFFI_ERR_OK;
    
    char signCode = 0;
    
    unsigned char count = 0;   //结构体成员数，如果有子结构体，会展开
    unsigned char align = 0;   //结构体对齐
    unsigned char offset = 0;  //成员的偏移
    unsigned char fixAlign = 0;//结构体强制的对齐大小
    
    int index = 0;

    for(; sign[index] != 0; index++){
        signCode = sign[index];
        
        if(signCode == ']'){
            index++;
            break;
        }
        
        if(signCode >= 'a' && signCode <= 'z'){ //基本类型
            //如果成员数超过最大限制
            if(count >= SFFI_MAX_ARG_COUNT){
                SFFI_FUNC_RETURN(SFFI_ERR_OVERFLOW);
            }

            //根据类型大小判断合法性
            unsigned char numSize = gBaseTypeSize[signCode-'a'];
            if(numSize == 0){ //大小不合法
                SFFI_FUNC_RETURN(SFFI_ERR_UNSUPPORT_TYPE);
            }
            
            //对齐
            if(fixAlign > 0 && fixAlign < numSize){ //如果强制对齐小于成员对齐，使用强制对齐
                offset = SFFI_ALIGN(offset, fixAlign);
            }
            else {
                offset = SFFI_ALIGN(offset, numSize);
            }
            
            if(align < numSize){ //如果成员的对齐，大于结构体的对齐，更新结构体对齐
                align = numSize;
            }
            
            struct sffi_st_member* num = &st->members[count++];
            num->sign = signCode;
            num->offset = offset;
            num->size = numSize;
            offset += numSize;
            
            if(!endIndex){
                //printf("num(%d) offset:%d  size:%d\n", count, num->offset, num->size);
            }
        }
        else {
            switch (signCode) {
                case '1':
                case '2':
                case '4':
                case '8':
                {
                    if(index != 0 && sign[index-1] != '['){ // '[' 后跟的数表示结构体强制对齐的大小，可选
                        SFFI_FUNC_RETURN(SFFI_ERR_UNSUPPORT_TYPE);
                    }
                    
                    //align = 16
                    if(signCode == '1' && sign[index+1] == '6'){
                        index++;
                        fixAlign = 16;
                    }
                    else {
                        fixAlign = signCode-'0';
                    }
                    
                }
                    break;
                    
                case '[':
                {
                    index++;
                    
                    int rIndex = 0;
                    struct sffi_st sub = {0};
                    err_code = sffi_get_st_inner(&sign[index], &sub, &rIndex);
                    SFFI_CHECK_RESULT();

                    index += rIndex - 1;
                    
                    //对齐
                    if(fixAlign > 0 && fixAlign < sub.align){
                        offset = SFFI_ALIGN(offset, fixAlign);
                    }
                    else {
                        offset = SFFI_ALIGN(offset, sub.align);
                    }
                    
                    if(align < sub.align){
                        align = sub.align;
                    }

                    //把子结构的成员添加到现在的队列中
                    for(int i = 0; i < sub.count; i++){
                        struct sffi_st_member* subNum = &sub.members[i];
                        struct sffi_st_member* num = &st->members[count++];
                        num->sign = subNum->sign;
                        num->offset = offset + subNum->offset;
                        num->size = subNum->size;
                        
                        if(!endIndex){
                            //printf("num(%d) offset:%d  size:%d\n", count, num->offset, num->size);
                        }
                    }
                    
                    offset += sub.size;
                }
                    break;
                default:
                {
                    SFFI_FUNC_RETURN(SFFI_ERR_UNSUPPORT_TYPE);
                }
                    break;
            }
        }
    }
    
    st->align = align;
    if(fixAlign > 0 && fixAlign < st->align){
        st->align = fixAlign;
    }
    
    st->count = count;
    st->size = SFFI_ALIGN(offset, st->align);
    
    if(endIndex){
        *endIndex = index;
    }
    
    //判断是否是 HFA
    char isHFA = 0;
    do{
        if(st->count > 4 || st->count == 0)
            break;  //HFA 必须是 1-4个
        
        if(st->members[0].sign != 'f' &&
           st->members[0].sign != 'd')
            break;  //HFA 必须是 float 或 double
        
        int i = 1;
        for(; i < st->count; i++){
            if(st->members[i-1].sign != st->members[i].sign)
                break; //HFA 必须相同
        }
        
        if(i == st->count){
            isHFA = 1;
        }
    }while(0);
    
    if(isHFA){
        st->passType = SFFI_PASS_BY_VR_MONO;
    }
    else {
        if( st->size <= 16)
            st->passType = SFFI_PASS_BY_GR_SHARE;
        else
            st->passType = SFFI_PASS_BY_POINT;
    }

end:
    return err_code;
}

int sffi_get_st(const char* sign, struct sffi_st* st){
    return sffi_get_st_inner(sign, st, 0);
}

int sffi_mk_ctpl(const char* sign, int countFixArg, struct sffi_ctpl** rt){
    static char defSign[] = "v"; //默认的函数签名 void();
    if(!sign)
        sign = defSign;
    
    if(countFixArg < 0 || countFixArg > 128 || !rt){
        return SFFI_ERR_INVALID_PARAMS;
    }
    
    int err_code = SFFI_ERR_OK;
    
    char signCode = 0; //临时
    int index = 0;     //签名位置的索引
    
    //先处理返回值=========================================
    unsigned char rtSize = 0;       //返回值的大小
    unsigned char rtCount = 0;      //返回值包含元素的个数，如果是结构体，指的就是结构体成员的个数
    unsigned char rtPassType = 0;   //返回值的传递类型
    
    {
        signCode = sign[index];
        
        if(signCode == '['){             //返回值是结构体
            struct sffi_st st;
            int endIndex = 0;
            err_code = sffi_get_st_inner(&sign[1], &st, &endIndex);
            SFFI_CHECK_RESULT();
            
            rtSize = st.size;           //大小
            rtCount = st.count;         //元素数量
            rtPassType = st.passType;   //传递类型
            
            index += endIndex + 1;      //跳过结构体的签名段
        }
        else {
            //是普通类型
            if(signCode < 'a' || signCode > 'z'){
                SFFI_FUNC_RETURN(SFFI_ERR_UNSUPPORT_TYPE);
            }

            rtSize = gBaseTypeSize[signCode-'a'];
            if(rtSize != 0) {
                if(rtSize > 32){ //如果是通过寄存器返回，大小不能超过 8*4
                    SFFI_FUNC_RETURN(SFFI_ERR_INVALID_PARAMS);
                }
                
                rtCount = 1;
                
                if(gBaseTypeFFlag[signCode-'a'])
                    rtPassType = SFFI_PASS_BY_VR_MONO;
                else
                    rtPassType = SFFI_PASS_BY_GR_SHARE;
            }
            
            index++;
        }
    }
    
    //处理所有入参数=================================
    int NSAA = 0;         //使用堆栈的字节数，必须 16byte 对齐
    int nArgCount = 0;    //参数的数量。结构体这里统计个数时，会被拆开，
    int nUseXR = 0;       //真正使用 XR 的数量。变参时，可能出现没有使用8个通用寄存器但NGRN=8的情况
    int nUseVR = 0;       //真正使用 VR 的数量。同上
    struct sffi_arg args[SFFI_MAX_ARG_COUNT] = {0};
    
    {
        int NGRN = 0;         //使用通用寄存器的数量 x0-x7
        int NSRN = 0;         //使用浮点寄存器的数量 d0-d7
        int nOrgArgCount = 0; //原始参数的数量，结构体算 1 个
 
        for(; sign[index] != 0; index++){
            signCode = sign[index];
            
            //IOS Arm64 变参部分直接进堆栈
            if(countFixArg > 0 && countFixArg < nOrgArgCount+1){
                NGRN = 8;
                NSRN = 8;
            }
            
            if(signCode == '['){
                //结构体的开始，分析结构体信息
                struct sffi_st st;
                int endIndex = 0;
                err_code = sffi_get_st_inner(&sign[index+1], &st, &endIndex);
                SFFI_CHECK_RESULT();
                
                index += endIndex;

                if(st.passType == SFFI_PASS_BY_POINT){ //按地址传递，把参数替换成 p, 走后面的逻辑
                    signCode = 'p';
                }
                else {
                    nOrgArgCount++; //结构体算 1 个原始参数
                    
                    if(st.passType == SFFI_PASS_BY_VR_MONO){ //通过浮点寄存器传，每个成员，一个寄存器
                        //浮点寄存器够不够？
                        if(st.count <= (8-NSRN)){
                            //够, 每个成员，单独保存在一个浮点寄存器中
                            for(int count = 0; count < st.count; count++){
                                struct sffi_arg* arg = &args[nArgCount++];
                                arg->destIndex = NSRN;
                                arg->destType = 2;
                                arg->index = nOrgArgCount-1;
                                arg->isST = 1;
                                arg->offset = st.members[count].offset;
                                arg->size = st.members[count].size;
                                
                                nUseVR++;
                                NSRN++;
                            }
                            continue; //处理结束
                        }
                        else {
                            NSRN = 8; //不够，放栈上
                        }
                    }
                    else if(st.passType == SFFI_PASS_BY_GR_SHARE){ //通过寄存器传，共享 1-2个寄存器
                        //通用寄存器够不够
                        if(st.size <= (8 - NGRN) * 8){
                            //够，放通用寄存器上
                            struct sffi_arg* arg = &args[nArgCount++];
                            arg->destIndex = NGRN;
                            arg->destType = 1;
                            arg->index = nOrgArgCount-1;
                            arg->isST = 1;
                            arg->offset = 0;
                            arg->size = st.size;
                            
                            NGRN++;
                            nUseXR++;
                            
                            if(st.size > 8){
                                NGRN++;
                                nUseXR++;
                            }
                            
                            continue; //处理结束
                        }
                        else {
                            NGRN = 8; //不够，放栈上
                        }
                    }
                    else {
                        SFFI_FUNC_RETURN(SFFI_ERR_UNSUPPORT_ST_PASS_TYPE);
                    }
                    
                    //栈空间，先对齐结构体
                    NSAA = (int)SFFI_ALIGN(NSAA, st.align);
                    
                    //设置参数信息
                    struct sffi_arg* arg = &args[nArgCount++];
                    arg->destIndex = NSAA;
                    arg->destType = 3;
                    arg->index = nOrgArgCount-1;
                    arg->isST = 1;
                    arg->offset = 0;
                    arg->size = st.size;
                    
                    NSAA += st.size;
                    continue; //处理结束
                }
            }

            //基本类型
            if(signCode < 'a' || signCode > 'z'){
                SFFI_FUNC_RETURN(SFFI_ERR_UNSUPPORT_TYPE);
            }
            
            int tSize = gBaseTypeSize[signCode-'a'];
            if(tSize == 0){
                 SFFI_FUNC_RETURN(SFFI_ERR_UNSUPPORT_TYPE);
            }
            
            struct sffi_arg* arg = &args[nArgCount++];
            arg->index = nOrgArgCount++;  //参数的索引，从入参中取参数值的时候用
            arg->size = tSize;            //参数的大小

            if(nArgCount > SFFI_MAX_ARG_COUNT){
                SFFI_FUNC_RETURN(SFFI_ERR_OVERFLOW);
            }

            char hasProc = 0;
            if(gBaseTypeFFlag[signCode-'a']){
                //浮点类型 且 有
                if(NSRN < 8){
                    arg->destType = 2;
                    arg->destIndex = nUseVR++;
                    hasProc = 1;
                    NSRN++;
                }
            }
            else {
                if(NGRN < 8){
                    arg->destType = 1;
                    arg->destIndex = nUseXR++;
                    hasProc = 1;
                    NGRN++;
                }
            }
            
            if(!hasProc){
                NSAA = (int)SFFI_ALIGN(NSAA, tSize);
                arg->destType = 3;
                arg->destIndex = NSAA;
                NSAA += tSize;
                
                if(NSAA > SFFI_MAX_ARG_STACK_SIZE){
                    SFFI_FUNC_RETURN(SFFI_ERR_OVERFLOW);
                }
            }
        }
        
        NSAA = (int)SFFI_ALIGN(NSAA, 16); //栈空间，必须 16byte 对齐
    }
    
    //生成 sffi_ctpl
    *rt = (struct sffi_ctpl*)malloc(sizeof(struct sffi_ctpl) + sizeof(struct sffi_arg) * (nArgCount-1));
    (*rt)->magic = SFFI_CTPL_MAGIC;
    (*rt)->sizeGR = nUseXR;
    (*rt)->sizeVR = nUseVR;
    (*rt)->sizeStack = NSAA;
    (*rt)->rtPassType = rtPassType;
    (*rt)->rtSize = rtSize;
    (*rt)->rtCount = rtCount;
    (*rt)->countArg = nArgCount;
    (*rt)->countFixArg = (unsigned char)countFixArg;
    sffi_memcpy(&(*rt)->args, args, sizeof(struct sffi_arg) * nArgCount);
    
end:
    return err_code;
}

void sffi_free_ctpl(struct sffi_ctpl* ctpl){
    if(ctpl && ctpl->magic == SFFI_CTPL_MAGIC){
        free(ctpl);
    }
}

int sffi_call_with_ctpl(struct sffi_ctpl* ctpl, unsigned long* args, void* addr, void* rt){
    if(!ctpl || !addr)
        return SFFI_ERR_INVALID_PARAMS;
    
    int err_code = SFFI_ERR_OK;
    
    //准备存放参数的空间
    unsigned long XR[8] = {0};  //x0-x7
    unsigned long* VR = 0;      //d0-d7
    unsigned char* Stack = 0;   //栈空间
    
    if(ctpl->sizeVR > 0)
        VR = alloca(sizeof(long)*ctpl->sizeVR);

    if(ctpl->sizeStack > 0)
        Stack = alloca(ctpl->sizeStack);
    
    struct sffi_arg* arg = (struct sffi_arg*)&ctpl->args;
    void* src = 0;
    void* dest = 0;

    //根据参数投递方式，投递到不同的buf中
    for(int i = 0; i < ctpl->countArg; i++, arg++){
        if(arg->destType == 1)
            dest = &XR[arg->destIndex];
        else if(arg->destType == 2)
            dest = &VR[arg->destIndex];
        else if(arg->destType == 3)
            dest = &Stack[arg->destIndex];
        else{
            SFFI_FUNC_RETURN(SFFI_ERR_INVALID_PARAMS);
        }

        if(arg->isST) //如果是结构体，arg是一指向结构体的指针，通过指针+便宜，得到成员变量地址
            src = ((unsigned char*)args[arg->index]) + arg->offset;
        else
            src = &args[arg->index];
            
        sffi_memcpy(dest, src, arg->size);
    }
    
    //设置好buf，调用目标函数
    sffi_raw_call(XR, VR, Stack, ctpl->sizeStack, addr, rt);
    
    //如果需要保存返回值，根据类型，保存返回结果
    if(rt){
        if(ctpl->rtPassType == SFFI_PASS_BY_GR_SHARE){  //共享通用寄存器
            sffi_memcpy(rt, &XR[0], ctpl->rtSize);
        }
        else if(ctpl->rtPassType == SFFI_PASS_BY_VR_MONO){ //独占浮点寄存器
            int memberSize = ctpl->rtSize / ctpl->rtCount; //只有HFA会走这里，一定是1-4个相同的float或者double
            for(int i = 0; i < ctpl->rtCount; i++){
                sffi_memcpy(((unsigned char*)rt + i * memberSize), &XR[4+i], memberSize);
            }
        }
        //ctpl->rtPassType == SFFI_PASS_BY_POINT 不用处理，已经通过x8设置好了
        //ctpl->rtPassType == SFFI_PASS_BY_GR_MONO 还不支持
    }
    
end:
    return err_code;
}

int sffi_call_var_demo(const char* sign, unsigned long* args, void* addr, void* rt, int fixedArg){
    static char defSign[] = "v"; //默认的函数签名 void();
    if(!sign)
        sign = defSign;
    
    int err_code = SFFI_ERR_OK;

    if(!addr || (sign[1] != 0 && args == 0)){
        SFFI_FUNC_RETURN(SFFI_ERR_INVALID_PARAMS);
    }
    
    const int stackSize = 128; //必须 stackSize % 16 == 0
    
    int NGRN = 0;  //使用通用寄存器的数量 x0-x7
    int NSRN = 0;  //使用浮点寄存器的数量 d0-d7
    int NSAA = 0;  //使用堆栈的字节数，必须 16byte 对齐

    unsigned long XR[8] = {0}; //x0-x7
    unsigned long* VR = 0;     //d0-d7  用到才分配栈空间
    unsigned char* Stack = 0;  //stack  用到才分配栈空间
      
    for(int i = 1; sign[i] != 0; i++){
        //IOS Arm64 变参部分直接进堆栈
        if(fixedArg > 0 && fixedArg < i){
            NGRN = 8;
            NSRN = 8;
        }
        
        char signCode = sign[i];
        
        //signCode 是否有效？
        if(signCode < 'a' || signCode > 'z'){
            SFFI_FUNC_RETURN(SFFI_ERR_UNSUPPORT_TYPE);
        }
        
        //得到类型的大小
        int tSize = gBaseTypeSize[signCode-'a'];
        
        //大小是否合法？
        if(tSize == 0){
            SFFI_FUNC_RETURN(SFFI_ERR_UNSUPPORT_TYPE);
        }
        
        //默认操作整形相关的变量 XR NGRN
        unsigned long* pR = XR;
        int* pNR = &NGRN;
        
        //如果是浮点数，改为操作浮点数相关的变量 VR NSRN
        if(gBaseTypeFFlag[signCode-'a']){
            if(!VR)
                VR = alloca(8*8);
            
            pR = VR;
            pNR = &NSRN;
        }
        
        if(*pNR < 8){
            //如果 8 个寄存器还有空余
            sffi_memcpy(&pR[*pNR], &args[i-1], tSize);
            (*pNR)++;
        }
        else {
            //寄存器满了，需要用栈空间
            if(!Stack){
                Stack = alloca(stackSize);
            }
            
            //先对齐类型的大小
            NSAA = (int)SFFI_ALIGN(NSAA, tSize);
            
            if(NSAA < (stackSize - tSize)){
                //栈空间足够，保存参数
                sffi_memcpy(&Stack[NSAA], &args[i-1], tSize);
                NSAA += tSize;
            }
            else {
                //栈空间不足，溢出错误
                SFFI_FUNC_RETURN(SFFI_ERR_OVERFLOW);
            }
        }
    }
    
    //栈空间，必须 16byte 对齐
    NSAA = (int)SFFI_ALIGN(NSAA, 16);
    
    //返回值会放在 XR 中，前4个放x0-x3 后4个放d0-d3
    sffi_raw_call(XR, VR, Stack, NSAA, addr, rt);

    //如果 rt 有效，根据 rt 的类型，设置数值
    if(rt){
        int tSize = gBaseTypeSize[sign[0]-'a'];
        if(tSize > 0){
            if(gBaseTypeFFlag[sign[0]-'a'])
                sffi_memcpy(rt, &XR[4], tSize);
            else
                sffi_memcpy(rt, &XR[0], tSize);
        }
    }
    
end:
    return err_code;
}

#else

int sffi_call(void* addr, const char* sign, unsigned long* args, void* rt, int fixedArg){
    return SFFI_ERR_UNSUPPORT_ARCH;
}

int sffi_call_var(void* addr, const char* sign, unsigned long* args, void* rt, int fixedArg){
    return SFFI_ERR_UNSUPPORT_ARCH;
}

#endif //__arm64__








//save x0 d0
/*__asm__ __volatile__(
                     "str x0, %0\n"
                     "str d0, %1\n"
                     :"=m" (ulR), "=m" (dbR)
                     :
                     :"%x0", "%d0"
                     );
*/
