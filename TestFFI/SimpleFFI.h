//
//  SimpleFFI.h
//  TestFFI
//
//  Created by yanjun on 2020/5/5.
//  Copyright © 2020 yanjun. All rights reserved.
//

#ifndef SimpleFFI_h
#define SimpleFFI_h

#ifdef __cplusplus
extern "C" {
#endif

/*
 IOS Arm64 下动态调用所有参数都是基本类型的函数
 支持的类型     类型大小  对齐     签名字符
 int8_t         1       1        c
 int16_t        2       2        s
 int32_t        4       4        i
 int64_t        8       8        l
 uint8_t        1       1        m
 uint16_t       2       2        w
 uint32_t       4       4        u
 uint64_t       8       8        n
 void*          8       8        p
 float          4       4        f
 double         8       8        d
 void           0       0        v
 
 example:
 //通过 sffi_call 动态调用 add 方法
 int add(int a, int b) {
    return a+b;
 }
 
 int test_sffi(){
    unsigned long args[2] = {0};
    *(int*)&args[0] = 94;
    *(int*)&args[1] = 9981;
     
    unsigned long rt = 0;
    sffi_call((void*)&add, "iii", args, &rt, 0);
    
    return *(int*)&rt;
 }
 */

//错误码
#define SFFI_ERR_OK              0             //成功
#define SFFI_ERR_INVALID_PARAMS  1             //入参错误
#define SFFI_ERR_UNSUPPORT_TYPE  2             //签名中的类型不支持
#define SFFI_ERR_OVERFLOW        3             //参数太多，超过了内部限制，可能是参数个数、可能是栈空间大小
#define SFFI_ERR_UNSUPORT_ARCH   4             //不支持的架构平台
#define SFFI_ERR_UNSUPPORT_ST_PASS_TYPE  5     //不支持的结构体，传递类型

//参数传递规则
#define SFFI_PASS_BY_VOID     0      //不需要
#define SFFI_PASS_BY_GR_SHARE 1      //用通用寄存器传，所有成员，共享 1-2个寄存器 (size <= 16byte 的pod)
#define SFFI_PASS_BY_VR_MONO  2      //用浮点寄存器传，每个成员，一个寄存器（float、double 或 HFA）
#define SFFI_PASS_BY_POINT    3      //用指针传(size > 16byte 的pod，或者 非pod数据结构)
#define SFFI_PASS_BY_GR_MONO  4      //用通用寄存器传，每个成员，一个寄存器 (HVA)

#define SFFI_MAX_ARG_COUNT 24        //函数参数数量最大值，结构体成员数和函数参数数都收这个限制
#define SFFI_MAX_ARG_STACK_SIZE 128  //存放参数栈的最大值，必须 16 对齐

//结构体成员的信息
struct sffi_st_member{
    unsigned char sign;     //签名
    unsigned char offset;   //偏移
    unsigned char size;     //大小
};

//结构体的信息
struct sffi_st {
    unsigned char align;         //结构体的对齐，是其成员中对齐的最大值，
                                 //如果有增加编译选项，限制对齐数字，编译选项优先
    unsigned char size;          //结构体大小
    unsigned char count;         //成员数量
    unsigned char passType;      //传输类型，SFFI_PASS_BY_*
    struct sffi_st_member members[SFFI_MAX_ARG_COUNT]; //所有成员信息
};

/*
 通过签名，解析出 sffi_st 信息
 */
int sffi_get_st(const char* sign, struct sffi_st* st);

/*
 通过签名，调用指定函数
 sign       函数签名，比如
            struct FOO { float a; float b; }
            void foo(int a, FOO b, char c) =>  "vi[ff]c"
            int add(int a, int b, long c) =>   "iiil"
            FOO foo(FOO a, double b, FOO c) => "[ff][ff]d[ff]"
 
 args       入参数列表，如果入参是基础类型，直接存参数值，
                      如果入参是复杂类型，存参数的地址， 比如
            struct FOO { float a; float b; };
            FOO st;
            void foo(int a, FOO b, float c);
     
            unsigned long args[3] = {0};
            *(int*)&args[0] = 123;
            args[1] = (unsigned long)&st;
            *(float*)&args[2] = 3.1415;
 
 addr       目标函数地址， 比如
            (void*)&TestFun
 
 rt         存返回值的内存空间，必须与 sign 中设置的返回值类型匹配，大小一致。
            可以为 0，表示不用处理返回值
 
 fixedArg   默认 0， 调用变参函数时，指定固定参数的数量，比如
            printf(const char*, ...) fixedArg=1
 */
int sffi_call(const char* sign, unsigned long* args, void* addr, void* rt);
int sffi_call_var(const char* sign, unsigned long* args, void* addr, void* rt, int fixedArg);

//最开始实现的，只支持基本类型，不用调用模版的实现版本，
int sffi_call_var_demo(const char* sign, unsigned long* args, void* addr, void* rt, int fixedArg);

/*
 ===========================================
 为了不用每次调用都去解析 sign，提高调用效率，增加调用模版，可以解析一次，多次使用
 因为结构体是堆空间分配，模版使用完，调用 sffi_free_ctpl 释放
 ===========================================
 */

//函数调用模版
struct sffi_ctpl;

/*
 通过签名，生成对应的调用模版
 sign      签名
 fixedArg  固定参数的数量，默认 0，变参时是真实的固定参数的数量
 rt        返回的调用模版，函数内部堆空间分配，要用 sffi_free_ctpl 释放
 */
int sffi_mk_ctpl(const char* sign, int fixedArg, struct sffi_ctpl** rt);

//通过 sffi_mk_ctpl 产生的调用模版，需要内部 free
void sffi_free_ctpl(struct sffi_ctpl* ctpl);

//使用调用模版支持调用，参数要和模版匹配，否则后果不可预期
int sffi_call_with_ctpl(struct sffi_ctpl* ctpl, unsigned long* args, void* addr, void* rt);

#ifdef __cplusplus
}
#endif

#endif /* SimpleFFI_h */
