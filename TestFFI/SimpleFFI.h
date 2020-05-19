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
 
 addr:
    调用函数的地址
 
 sign:
    函数签名
    例如：
        "vii" 表示 void foo(int32_t,int32_t);
        "fid" 表示 float foo(int32_t,double);
 args:
    入参数组，参数数目必须与签名一致，所有参数需要复制到 unsigned long 的空间，
    可以为 nullptr
    例如：
        "vcfd" 表示 void foo(int8_t, float, double), 准备参数时
        unsigned long args[3] = {0};
        *(int8_t*)&args[0] = arg1;
        *(float*)&args[1] = arg2;
        *(double*)&args[2] = arg3;
 
 rt:
    存返回值的变量
    可以为 nullptr
 
 fixArgCount:
    定参函数，可以设置为 0，或者参数的真实数量
    调用变参函数的时候，设置为固定参数的数量，比如
       printf(const char*, ...) fixArgCount=1

 err_code:
    SFFI_ERR_OK(0)              成功
    SFFI_ERR_INVALID_PARAMS(1)  入参不合法
    SFFI_ERR_UNSUPPORT_TYPE(2)  函数签名中的数据类型不支持
    SFFI_ERR_OVERFLOW(3)        参数太多，超过了内部栈设置的大小
    SFFI_ERR_UNSUPORT_ARCH(4)   不支持的架构平台
 
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

#define SFFI_ERR_OK              0
#define SFFI_ERR_INVALID_PARAMS  1
#define SFFI_ERR_UNSUPPORT_TYPE  2
#define SFFI_ERR_OVERFLOW        3
#define SFFI_ERR_UNSUPORT_ARCH   4
#define SFFI_ERR_TOO_MANY_ARGS   5

#define SFFI_MAX_ARG_COUNT 24        //函数参数数量最大值

/*
 结构体成员的信息，偏移 + 大小
 */
struct sffi_st_number{
    unsigned char sign;
    unsigned char offset;
    unsigned char size;
};

/*
 结构体的信息
 align:   对齐
 size:    大小
 count:   成员数量
 numbers: 成员信息
 */
struct sffi_st {
    unsigned char align;         //结构体的对齐，是其成员中对齐的最大值，
                                 //如果有增加编译选项，限制对齐数字，编译选项优先
    unsigned char size;          //结构体大小
    unsigned char count;         //成员数量
    unsigned char isHFAorHVA;    //目前只支持HFA
    unsigned char isPassByPoint; //是否是以指针的形式传递参数的
    struct sffi_st_number numbers[SFFI_MAX_ARG_COUNT]; //成员
};

int sffi_get_st(const char* sign, struct sffi_st* st);

int sffi_call(const char* sign, unsigned long* args, void* addr, unsigned long* rt);
int sffi_call_var(const char* sign, unsigned long* args, void* addr, unsigned long* rt, int fixArgCount);








//simple ffi call template
struct sffi_ctpl;

//通过签名，生成对应的调用模版，提高调用速度
int sffi_mk_ctpl(const char* sign, int countFixArg, struct sffi_ctpl** rt);

//通过 sffi_mk_ctpl 产生的调用模版，需要内部 free
void sffi_free_ctpl(struct sffi_ctpl* ctpl);

//使用调用模版支持调用，参数要和模版匹配，否则后果不可预期
int sffi_call_with_ctpl(struct sffi_ctpl* ctpl, unsigned long* args, void* addr, unsigned long* rt);

#ifdef __cplusplus
}
#endif

#endif /* SimpleFFI_h */
