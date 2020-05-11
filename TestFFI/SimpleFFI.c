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

__attribute__ ((naked)) void sffi_memcpy(void* des, void* src, unsigned long len){
    //des(x0) src(x1)  len(x2)
    __asm__ (
             "1:"
             "cmp x2, 0\n"              //把 x2(len) 与 0 比较
             "b.eq 20\n"                //相等，这条转到 ret
             "ldrb w10, [x1], #0x1\n"   //从 x1(src) 读 1byte 到 w10，x1++
             "strb w10, [x0], #0x1\n"   //把 w10 写 1byte 到 x0(des)，x0++
             "sub x2, x2, #1\n"         //x2(len)--
             "b 1b\n"                   //跳开头
             "ret"
             );
}

/*
 regX  整数寄存器buf 8byte * 8
 regV  浮点寄存器buf 8byte * 8，可为 null
 stack 栈空间buf，变长，可为 null
 sl    栈空间buf长度，>=0 必须16byte对齐
 addr  要调用函数的地址
 */
__attribute__ ((naked)) void sffi_raw_call(unsigned long* regX, unsigned long* regV,
                                           unsigned char* stack, long sl, void* addr){
    //regX(x0), regV(x1), stack(x2), sl(stack len)(x3), addr(x4)
    __asm__ __volatile__(
                         "sub sp, sp, #0x10\n"              //要调用目标函数 addr，需要保存 x29(fp) x30(lr），堆上分配空间 0x10(16)字节
                         "stp x29, x30, [sp]\n"             //保存 x29,x30
                         "mov x29, sp\n"                    //设置 x29 为当前的栈顶
                         
                         "sub sp, sp, #0x10\n"              //函数最后要用到 x0(regX)，把它保存到栈上，sp要16byte对齐
                         "str x0, [sp]\n"                   //保存x0到栈顶，此时堆栈是这样的 (x0, 不用, x29, x30)
                         
                         "sub sp, sp, x3\n"                 //给要通过堆栈传递的参数分配空间，x3(sl) 必须是16byte 对齐
                         
                         //设置通过栈传递的参数
                         "cmp x2, 0\n"                      //if stack==0 then jump 2:
                         "b.eq 2f\n"

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
                         
                         "ldr d0, [x1]\n"                   //x1(regV)，copy to d0-d7
                         "ldr d1, [x1, #0x8]\n"
                         "ldr d2, [x1, #0x10]\n"
                         "ldr d3, [x1, #0x18]\n"
                         "ldr d4, [x1, #0x20]\n"
                         "ldr d5, [x1, #0x28]\n"
                         "ldr d6, [x1, #0x30]\n"
                         "ldr d7, [x1, #0x38]\n"
                         
                         //设置通用寄存器
                         "3:\n"
                         "mov x9, x0\n"                     //x9=x0(regX), 因为传参要用 x0-x7, 后面要用到，先备份一下
                         "mov x10, x4\n"                    //x10=x4(addr)
                         
                         "ldr x0, [x9]\n"                   //x9(regX)，把它设置到 x0-x7
                         "ldr x1, [x9, #0x8]\n"
                         "ldr x2, [x9, #0x10]\n"
                         "ldr x3, [x9, #0x18]\n"
                         "ldr x4, [x9, #0x20]\n"
                         "ldr x5, [x9, #0x28]\n"
                         "ldr x6, [x9, #0x30]\n"
                         "ldr x7, [x9, #0x38]\n"
                         
                         "blr x10\n"                         //跳 x10(addr)
                         
                         //设置返回值
                         "ldr x9, [x29, -0x10]\n"            //x9 设置为之前保存在栈上的 x0值, x9=regX
                         "str x0, [x9]\n"                    //x9=regX，保存整数返回值到 0-3  浮点返回到 4-7
                         "str x1, [x9, #0x8]\n"
                         "str x2, [x9, #0x10]\n"
                         "str x3, [x9, #0x18]\n"
                         "str d0, [x9, #0x20]\n"
                         "str d1, [x9, #0x28]\n"
                         "str d2, [x9, #0x30]\n"
                         "str d3, [x9, #0x38]\n"
                         
                         //恢复
                         "mov sp, x29\n"
                         "ldp x29, x30, [sp] \n"            //恢复 x29(fp) x30(lr)
                         "add sp, sp, #0x10 \n"             //恢复 sp
                         "ret \n"                           //跳 lr
                         );
}

#define SFFI_CALL_RETURN(ERR_CODE) err_code=ERR_CODE;goto end;

struct sffi_st {
    short size;
    short number;
    unsigned char* elements;
    unsigned char isHFAorHVA;
};

//                          a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z
static char gBaseTypeSize['z'+1] = {0,0,1,8,0,4,0,0,4,0,0,8,1,8,0,8,0,0,2,0,4,0,2,0,0,0}; // 根据signCode 得到对应类型的大小
static char gBaseTypeFFlag['z'+1] = {0,0,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; // 根据signCode 得到是否是浮点数


int sffi_pre_st(struct sffi_st* st, const char* sign){
    if(!st || !sign){
        return SFFI_ERR_INVALID_PARAMS;
    }
    
    
    int err_code = SFFI_ERR_OK;
    
    
    return err_code;
}

int sffi_call(void* addr, const char* sign, unsigned long* args, unsigned long* rt){
    return sffi_call_var(addr, sign, args, rt, -1);
}

int sffi_call_var(void* addr, const char* sign, unsigned long* args, unsigned long* rt, int fix_arg_count){
    static char defSign[] = "v"; //默认的函数签名 void();
    
    int err_code = SFFI_ERR_OK;
    
    if(!sign){
        sign = defSign;
    }

    if(!addr || (sign[1] != 0 && args == 0)){
        SFFI_CALL_RETURN(SFFI_ERR_INVALID_PARAMS);
    }
    
    int NGRN = 0;  //使用通用寄存器的数量 x0-x7
    int NSRN = 0;  //使用浮点寄存器的数量 d0-d7
    int NSAA = 0;  //使用堆栈的字节数，必须 16byte 对齐
    
    const int stackSize = 128; //必须 stackSize % 16 == 0
    
    unsigned long XR[8] = {0}; //x0-x7
    unsigned long* VR = 0;     //d0-d7
    unsigned char* Stack = 0;  //stack
      
    for(int i = 1; sign[i] != 0; i++){
        //IOS Arm64 变参直接扔堆栈
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
            if(!VR){
                VR = alloca(8*8);
            }
            
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
