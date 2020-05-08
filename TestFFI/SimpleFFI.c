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
    __asm__ __volatile__(
                         "sffi_memcpy:"
                         "cmp x2, 0\n"              //把 x2(len) 与 0 比较
                         "b.eq 20\n"                //相等，这条转到 ret
                         "ldrb w10, [x1], #0x1\n"   //从 x1(src) 读 1byte 到 w10，x1++
                         "strb w10, [x0], #0x1\n"   //把 w10 写 1byte 到 x0(des)，x0++
                         "sub x2, x2, #1\n"         //x2(len)--
                         "b sffi_memcpy\n"          //跳开头
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
                         //备份信息
                         "sub sp, sp, #0x30\n"         //分配栈空间 48byte，要 16byte 对齐
                                                       //备份数据是 (x19, x20, x21, 0, x29, x30)
                         "stp x19, x20, [sp]\n"        //备份 x19, x20
                         "str x21, [sp, #0x10]\n"      //备份 x21
                         "stp x29, x30, [sp, #0x20]\n" //备份 x29, x30
                         "add x29, sp, #0x20\n"        //更新 x29(fp)
                         
                         "mov x19, x0\n"               //x19 = regX
                         "mov x20, x1\n"               //x20 = regV
                         "mov x21, x4\n"               //x21 = addr
                         
                         //把 stack 复制到 到 sp 上
                         "cmp x2, 0\n"                 //检查 stack 是否为 null
                         "b.eq 32\n"                   //为 null则跳出
                         "cmp x3, 0\n"                 //把 x3(sl) 与 0 比较
                         "b.eq 24\n"                   //相等，这条转到 ret
                         "sub sp, sp, x3\n"            //给通过堆栈传递的参数，保留栈空间 x3=sl
                                                       //x3 必须 16byte 对齐
                         "mov x0, sp\n"                //dest
                         "mov x1, x2\n"                //src
                         "mov x2, x3\n"                //len
                         "bl sffi_memcpy\n"            //call sffi_memcpy
                         
                         //复制X0-X7
                         "ldr x0, [x19]\n"              //x19=regX, 把它设置到 x0-x7
                         "ldr x1, [x19, #0x8]\n"
                         "ldr x2, [x19, #0x10]\n"
                         "ldr x3, [x19, #0x18]\n"
                         "ldr x4, [x19, #0x20]\n"
                         "ldr x5, [x19, #0x28]\n"
                         "ldr x6, [x19, #0x30]\n"
                         "ldr x7, [x19, #0x38]\n"
                         
                         //复制V0-V7
                         "cmp x20, 0\n"                 //如果regV==0，则不复制
                         "b.eq 36\n"
                         "ldr d0, [x20]\n"              //x20=regV，把它设置到 d0-d7
                         "ldr d1, [x20, #0x8]\n"
                         "ldr d2, [x20, #0x10]\n"
                         "ldr d3, [x20, #0x18]\n"
                         "ldr d4, [x20, #0x20]\n"
                         "ldr d5, [x20, #0x28]\n"
                         "ldr d6, [x20, #0x30]\n"
                         "ldr d7, [x20, #0x38]\n"

                         //call addr
                         "blr x21\n"                   //x21=addr
                         
                         //保存返回值
                         "str x0, [x19]\n"             //x19=regX，保存整数返回值到 0-3  浮点返回到 4-7
                         "str x1, [x19, #0x8]\n"
                         "str x2, [x19, #0x10]\n"
                         "str x3, [x19, #0x18]\n"
                         "str d0, [x19, #0x20]\n"
                         "str d1, [x19, #0x28]\n"
                         "str d2, [x19, #0x30]\n"
                         "str d3, [x19, #0x38]\n"
                         
                         //恢复堆栈
                         "mov sp, x29\n"
                         "ldp x19, x20, [sp, #-0x20]\n"
                         "ldr x21, [sp, #-0x10]\n"
                         "ldp x29, x30, [sp] \n"
                         "add sp, sp, #0x10 \n"
                         "ret \n");
}

#define SFFI_CALL_RETURN(ERR_CODE) err_code=ERR_CODE;goto end;

int sffi_call(void* addr, const char* sign, unsigned long* args, unsigned long* rt, int fix_arg_count){
    //                          a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t,u,v,w,x,y,z
    static char tSizes['z'+1] = {0,0,1,8,0,4,0,0,4,0,0,8,1,8,0,8,0,0,2,0,4,0,2,0,0,0}; // 根据signCode 得到对应类型的大小
    static char fFlags['z'+1] = {0,0,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; // 根据signCode 得到是否是浮点数
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
        int tSize = tSizes[signCode-'a'];
        
        //大小是否合法？
        if(tSize == 0){
            SFFI_CALL_RETURN(SFFI_ERR_UNSUPPORT_TYPE);
        }
        
        //默认操作整形相关的变量 XR NGRN
        unsigned long* pR = XR;
        int* pNR = &NGRN;
        
        //如果是浮点数，改为操作浮点数相关的变量 VR NSRN
        if(fFlags[signCode-'a']){
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
        int tSize = tSizes[sign[0]-'a'];
        if(tSize > 0){
            if(fFlags[sign[0]-'a'])
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
