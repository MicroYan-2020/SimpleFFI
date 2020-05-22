//
//  TestFFI.m
//  TestFFI
//
//  Created by yanjun on 2020/5/6.
//  Copyright © 2020 yanjun. All rights reserved.
//

#import "TestFFI.h"
#include "include/ffi/ffi.h"
#import <JavaScriptCore/JavaScriptCore.h>
#include <arm_neon.h>
#include "SimpleFFI.h"

struct st_tt {
    char i1;
    char i2;
    char i3;
    char i4;
    char i5;
};

void puts_binding(ffi_cif *cif, unsigned int *ret, void* args[],
                  FILE *stream)
{
    *ret = fputs(*(char **)args[0], stream);
}

@interface TestFFI()

@end

@implementation TestFFI
{
    int m_var;
}

//==========================================================================
+(void)RunWithClosure {
    ffi_cif cif;
    ffi_type *args[1];
    ffi_closure *closure;

    int (*bound_puts)(const char *);
    int rc;

    closure = (ffi_closure *)ffi_closure_alloc(sizeof(ffi_closure), (void**)&bound_puts);
    if (closure)
    {
        args[0] = &ffi_type_pointer;
        if (ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 1,
                         &ffi_type_uint, args) == FFI_OK)
        {
            if (ffi_prep_closure_loc(closure,
                                     &cif,
                                     (void (*)(ffi_cif*,void*,void**,void*))puts_binding,
                                     stdout,
                                     (void*)bound_puts) == FFI_OK)
            {
                rc = bound_puts("Hello World!");
            }
        }
    }
    ffi_closure_free(closure);
}


int add_i(int a, int b){
    return a+b;
}

double add(int a, long b, float c, double d){
    return a+b+c+d;
}

+(void)RunWithFFI {
    ffi_type* types[4] = {&ffi_type_sint, &ffi_type_slong, &ffi_type_float, &ffi_type_double}; //入参类型
    ffi_type* rtType = &ffi_type_double; //返回值类型

    //初始化函数模版，主要设置 abi，填充入参和返回值类型，以及根据类型，设置flags等
    ffi_cif cif;
    ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 4, rtType, types);
    
    //准备参数
    double ret = 0.f;
    int a = 19;
    long b = 33;
    float c = 3.14f;
    double d = 2.718;
    void* args[4] = { &a, &b, &c, &d};

    ffi_call(&cif, (void (*)(void))add, (void*)&ret, (void**)args); //调用

    NSLog(@"Result: %lf", ret); //打印结果
}

struct ST_FOO{
    char a;
    long b;
    float c;
    double d;
};

ST_FOO add_st(ST_FOO a, ST_FOO b){
    a.a += b.a; a.b += b.b; a.c += b.c; a.d += b.d;
    return a;
}

+(void)TestStructWithFFI {
    ffi_type stType;
    stType.type = FFI_TYPE_STRUCT;
    stType.size = sizeof(ST_FOO);
    stType.alignment = sizeof(ST_FOO);
    ffi_type* stElements[5] = {&ffi_type_schar, &ffi_type_slong, &ffi_type_float, &ffi_type_double, NULL};
    stType.elements = stElements;
    
    ffi_type* types[2] = {&stType, &stType}; //入参类型
    ffi_type* rtType = &stType; //返回值类型
    ffi_cif cif;
    ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 2, rtType, types);
    
    ST_FOO a = {1,2,3.3,4.4};
    ST_FOO b = {2,3,4.4,5.5};
    void* args[2] = { &a, &b};
    
    ST_FOO ret = {0};

    ffi_call(&cif, (void (*)(void))add_st, (void*)&ret, (void**)args); //调用
    
    NSLog(@"Result: %d %ld %f %lf", (int)ret.a, ret.b, ret.c, ret.d); //打印结果
}

+(void)TestVarArgsWithFFI {
    //用 libffi 调用  int printf(const char * __restrict, ...)
    //并打印出 hello world! 123
    //3个入参 char*("hello %s! %d"), char*("world"), int(123)
    ffi_type* types[3] = {&ffi_type_pointer, &ffi_type_pointer, &ffi_type_sint}; //入参类型
    ffi_type* rtType = &ffi_type_sint; //返回值类型
    
    ffi_cif cif;
    //变参要用 ffi_prep_cif_var, 1是固定参数的个数， 3是总参数的个数
    ffi_prep_cif_var(&cif, FFI_DEFAULT_ABI, 1, 3, rtType, types);
    
    int ret = 0;
    const char* a = "hello %s! %d";
    const char* b = "world";
    int c = 123;
    void* args[3] = { &a, &b, &c};

    ffi_call(&cif, (void (*)(void))printf, (void*)&ret, (void**)args); //调用
}

-(float)Add :(int)a  B:(float) b{
    return a+b+ self->m_var;
}

+(void)TestOCFuncWithFFI{
    TestFFI* tf = [[TestFFI alloc] init];
    tf->m_var = 4;

    void* func = (void*)[tf methodForSelector:@selector (Add:B:)];
    
    ffi_type* types[4] = {&ffi_type_pointer, &ffi_type_pointer, &ffi_type_sint, &ffi_type_float}; //入参类型
    ffi_type* rtType = &ffi_type_float; //返回值类型

    //初始化函数模版，主要设置 abi，填充入参和返回值类型，以及根据类型，设置flags等
    ffi_cif cif;
    ffi_prep_cif(&cif, FFI_DEFAULT_ABI, 4, rtType, types);
    
    //准备参数
    float ret = 0.f;
    void* a = (__bridge void*)tf;
    void* b = @selector (Add:B:);
    int c = 3;
    float d = 3.14;
    void* args[4] = { &a, &b, &c, &d};

    ffi_call(&cif, (void (*)(void))func, (void*)&ret, (void**)args); //调用

    NSLog(@"r = %f", ret);
    
    {
        unsigned long args[4];
        args[0] = (unsigned long)(__bridge void*)tf;
        args[1] = (unsigned long)@selector (Add:B:);
        args[2] =3;
        *(float*)&args[3] = 3.14;
        
        unsigned long r = 0;
        sffi_call("fppif", (unsigned long*)args, (void*)func, &r);
        
        NSLog(@"r = %f", *(float*)&r);
    }
    
}


#define OFFSET(structure, member) ((long)&((structure*)0)->member)


struct Foo {
    struct Foo1{
        long a1;
    }b;
    
    struct Foo2{
        int a1;
        char b1;
    }c;
};

__attribute__ ((noinline)) struct Foo show(struct Foo a, struct Foo b){
    struct Foo r;
    //r.a = a.a + b.a;
    r.b.a1 = a.b.a1 + b.b.a1;
    r.c.a1 = a.c.a1 + b.c.a1;
    r.c.b1 = a.c.b1 + b.c.b1;
    return r;
}

struct Foo1{
    char a;
    struct Foo2{
        char b;
        int c;
    }b;
};

/*
 
 */

__attribute__ ((noinline)) struct Foo1 show_char_ary(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, char a9, char a10, struct Foo1 a){
    for(int i = 0; i < sizeof(a)/sizeof(int); i++){
        ((double*)&a)[i] += ((double*)&a)[i];
    }
    
    a.b.b += a1+a2+a3+a4+a5+a6+a7+a8+a9+a10;
    return a;
}

+(void)Run {
    [TestFFI TestOCFuncWithFFI];
    [TestFFI TestVarArgsWithFFI];
    [TestFFI TestStructWithFFI];
    [TestFFI RunWithFFI];
    [TestFFI RunWithClosure];
}

@end
