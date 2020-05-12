//
//  SimpleFFI.c
//  TestFFI
//
//  Created by yanjun on 2020/5/5.
//  Copyright © 2020 yanjun. All rights reserved.
//

#include "SimpleFFI.h"
#include <stdlib.h>

#if defined (__arm64__)

#define SFFI_ALIGN(v,a)  (((((unsigned long) (v))-1) | ((a)-1))+1)

/*
des(x0) 目标地址
src(x1) 源地址
len(x2) 长度
*/
__attribute__ ((naked)) void sffi_memcpy(void* des, void* src, unsigned long len){
    __asm__ (
             "1:"
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
 */
__attribute__ ((naked)) void sffi_raw_call(unsigned long* regX, unsigned long* regV,
                                           unsigned char* stack, long sl, void* addr){
    __asm__ __volatile__(
                         "sub sp, sp, #0x10\n"              //要调用目标函数 addr，需要保存 x29(fp) x30(lr），堆上分配空间 0x10(16)字节
                         "stp x29, x30, [sp]\n"             //保存 x29,x30
                         "mov x29, sp\n"                    //设置 x29 为当前的栈顶
    
                         "sub sp, sp, #0x10\n"              //函数最后要用到 x0(regX)，把它保存到栈上，sp要16byte对齐
                         "str x0, [sp]\n"                   //保存x0到栈顶，此时堆栈是这样的 (x0, 不用, x29, x30)
                                                            //现在栈的样子是 (x0, 不用, x29, x30)
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

#define SFFI_CALL_RETURN(ERR_CODE) err_code=ERR_CODE;goto end;

//                                   a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z
static char gBaseTypeSize['z'+1] =  {0,0,1,8,0,4,0,0,4,0,0,8,1,8,0,8,0,0,2,0,4,0,2,0,0,0}; // 根据signCode 得到对应类型的大小
static char gBaseTypeFFlag['z'+1] = {0,0,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; // 根据signCode 得到是否是浮点数

/*
 struct BaseTypeInfo{
    unsigned char size;
    unsigned char isFloating;
};

static struct BaseTypeInfo GBaseTypeInfo['z'+1] = {{0,0}, {0,0}, {1,0}, {8,1}};
*/

int sffi_call(void* addr, const char* sign, unsigned long* args, unsigned long* rt){
    return sffi_call_var(addr, sign, args, rt, -1);
}

int sffi_call_var(void* addr, const char* sign, unsigned long* args, unsigned long* rt, int fix_arg_count){
    static char defSign[] = "v"; //默认的函数签名 void();
    if(!sign)
        sign = defSign;
    
    int err_code = SFFI_ERR_OK;

    if(!addr || (sign[1] != 0 && args == 0)){
        SFFI_CALL_RETURN(SFFI_ERR_INVALID_PARAMS);
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
        if(fix_arg_count > 0 && fix_arg_count < i){
            NGRN = 8;
            NSRN = 8;
        }
        
        char signCode = sign[i];
        
        //signCode 是否有效？
        if(signCode < 'a' || signCode > 'z'){
            SFFI_CALL_RETURN(SFFI_ERR_UNSUPPORT_TYPE);
        }
        
        //得到类型的大小
        int tSize = gBaseTypeSize[signCode-'a'];
        
        //大小是否合法？
        if(tSize == 0){
            SFFI_CALL_RETURN(SFFI_ERR_UNSUPPORT_TYPE);
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
                SFFI_CALL_RETURN(SFFI_ERR_OVERFLOW);
            }
        }
    }
    
    //栈空间，必须 16byte 对齐
    NSAA = (int)SFFI_ALIGN(NSAA, 16);
    
    //返回值会放在 XR 中，前4个放x0-x3 后4个放d0-d3
    sffi_raw_call(XR, VR, Stack, NSAA, addr);

    //如果 rt 有效，根据 rt 的类型，设置数值
    if(rt){
        *rt = 0L;
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

int sffi_call(void* addr, const char* sign, unsigned long* args, unsigned long* rt, int fix_arg_count){
    return SFFI_ERR_UNSUPPORT_ARCH;
}

int sffi_call_var(void* addr, const char* sign, unsigned long* args, unsigned long* rt, int fix_arg_count){
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
