//
//  TestCase.m
//  TestFFI
//
//  Created by yanjun on 2020/4/27.
//  Copyright © 2020 yanjun. All rights reserved.
//

#import "TestCase.h"
#include "SimpleFFI.h"
#include <iostream>

struct TypeAttr{
    bool isSigned;
    bool isFloating;
    char sign[2];
    
    int size;
    int align;
    
    unsigned long min;
    unsigned long max;
};

struct TypeAttr GTypeAttrs[] = {
    {true, false, "c", 1, 1, 0x0000000000000080, 0x000000000000007f}, //int8_t       0
    {true, false, "s", 2, 2, 0x0000000000008000, 0x0000000000007fff}, //int16_t      1
    {true, false, "i", 4, 4, 0x0000000080000000, 0x000000007fffffff}, //int32_t      2
    {true, false, "l", 8, 8, 0x8000000000000000, 0x7fffffffffffffff}, //int64_t      3
    
    {false, false, "m", 1, 1, 0UL, 0x00000000000000ff}, //uint8_t     4
    {false, false, "w", 2, 2, 0UL, 0x000000000000ffff}, //uint16_t    5
    {false, false, "u", 4, 4, 0UL, 0x00000000ffffffff}, //uint32_t    6
    {false, false, "n", 8, 8, 0UL, 0xffffffffffffffff}, //uint64_t    7

    {true, true, "f", 4, 4, 0x0000000000800000, 0x000000007f7fffff},   //float         8
    {true, true, "d", 8, 8, 0x0010000000000000, 0x7fefffffffffffff},   //double        9
    
    {false, false, "p", 8, 8, 0x0000000000000000, 0xffffffffffffffff}, //void*         10
};


template<class T>
int TypeAttrIndex(){ return -1; }
template<> int TypeAttrIndex<char>(){ return 0; }
template<> int TypeAttrIndex<int8_t>(){ return 0; }
template<> int TypeAttrIndex<int16_t>(){ return 1; }
template<> int TypeAttrIndex<int32_t>(){ return 2; }
template<> int TypeAttrIndex<long>(){ return 3; }
template<> int TypeAttrIndex<int64_t>(){ return 3; }
template<> int TypeAttrIndex<uint8_t>(){ return 4; }
template<> int TypeAttrIndex<uint16_t>(){ return 5; }
template<> int TypeAttrIndex<uint32_t>(){ return 6; }
template<> int TypeAttrIndex<uint64_t>(){ return 7; }
template<> int TypeAttrIndex<float>(){ return 8; }
template<> int TypeAttrIndex<double>(){ return 9; }
template<> int TypeAttrIndex<void*>(){ return 10; }

template<class T>
TypeAttr* GetTypeAttr(){
    if(TypeAttrIndex<T>() >=0)
        return &GTypeAttrs[TypeAttrIndex<T>()];
    
    return nullptr;
}

template<class T>
class VarProc {
public:
    static void randFill(unsigned long* r){
        int rnd = rand() % 10;
        if(rnd < 1)
            fillWithMin(r);
        else if(rnd < 2)
            fillWithMax(r);
        else
            fillWithRand(r);
    }
    
    static void fillWithRand(unsigned long* r){
        *r = 0UL;
        
        if(VarProc<T>::isFloating())
            *(T*)r = (T)rand() / (T)rand();
        else
            *(T*)r = (T)rand();
        
        if(VarProc<T>::isSigned() && rand()%2 == 0){
            *(T*)r =  *(T*)r * (T)-1;
        }
    }
    
    static void fillWithMax(unsigned long* r){
        *r = 0UL;
        *(T*)r = VarProc<T>::max();
    }
    
    static void fillWithMin(unsigned long* r){
        *r = 0UL;
        *(T*)r = VarProc<T>::min();
    }
    
    static bool isSigned(){ return GetTypeAttr<T>()->isSigned; }
    static bool isFloating(){ return GetTypeAttr<T>()->isFloating; }
    static T max() { return *((T*)(&(GetTypeAttr<T>()->max))); }
    static T min() { return *((T*)(&(GetTypeAttr<T>()->min))); }

    static bool equal(T a, T b){
        if(VarProc<T>::isFloating()){
            return abs((double)a-(double)b) < 0.000001;
        }
        
        return a==b;
    }
};

template<>
class VarProc<void*> {
public:
    static void randFill(unsigned long* r){
        int rnd = rand() % 10;
        if(rnd < 1)
            fillWithMin(r);
        else if(rnd < 2)
            fillWithMax(r);
        else
            fillWithRand(r);
    }
    
    static void fillWithRand(unsigned long* r){
        *r = 0UL;
        *r = 0x1234567812345678;
    }
    
    static void fillWithMax(unsigned long* r){
        *r = 0UL;
        *r = (unsigned long)VarProc<void*>::max();
    }
    
    static void fillWithMin(unsigned long* r){
        *r = 0UL;
        *r = (unsigned long)VarProc<void*>::min();
    }
    
    static bool isSigned(){ return GetTypeAttr<void*>()->isSigned; }
    static bool isFloating(){ return GetTypeAttr<void*>()->isFloating; }
    static void* max() { return (void*)(GetTypeAttr<void*>()->max); }
    static void* min() { return (void*)(GetTypeAttr<void*>()->min); }

    static bool equal(void* a, void* b){
        return a==b;
    }
};


int GTestDataIndex = 0;
bool GTCErrFlag = false;
bool GCurTCItemErrFlag = false;
char GTestCaseName[512] = {0};
char GCurFuncSign[64] = {0};
unsigned long GArgs[64] = {0};

void InitTestData(){ GTestDataIndex = 0; }
template <class T ,class...  Args>
void InitTestData(T var, Args...args){
    strcat(GCurFuncSign, GetTypeAttr<T>()->sign);
    GCurTCItemErrFlag = false;
    
    VarProc<T>::randFill(&GArgs[GTestDataIndex++]);
    InitTestData(args...);
}

void RunTestFunc(){ }
template <class T ,class...  Args>
void RunTestFunc(T var, Args...args){
    if(!VarProc<T>::equal(var, *(T*)&GArgs[GTestDataIndex])){
        std::cout << "Test Fail: " << GTestCaseName << "_" << GTestDataIndex << " v1=" << var <<
        " v2=" << *(T*)&GArgs[GTestDataIndex] << std::endl;
        GCurTCItemErrFlag = true;
        GTCErrFlag = true;
    }
    GTestDataIndex++;
    
    RunTestFunc(args...);
}

#define RUN_TC_GPARAMS_INIT(NAME) \
strcpy(GTestCaseName, NAME);GTestDataIndex = 0;GCurFuncSign[0] = 0;strcat(GCurFuncSign, "v");unsigned long rt=0;

#define RUN_TC1(NAME,T1) \
{RUN_TC_GPARAMS_INIT(NAME); \
InitTestData((T1)0); \
sffi_call(GCurFuncSign, GArgs, (void*)&RunTestFunc<T1>, &rt);}

#define RUN_TC2(NAME,T1,T2) \
{RUN_TC_GPARAMS_INIT(NAME); \
InitTestData((T1)0,(T2)0); \
sffi_call(GCurFuncSign, GArgs, (void*)&RunTestFunc<T1,T2>, &rt);}

#define RUN_TC3(NAME,T1,T2,T3) \
{RUN_TC_GPARAMS_INIT(NAME); \
InitTestData((T1)0,(T2)0,(T3)0); \
sffi_call(GCurFuncSign, GArgs, (void*)&RunTestFunc<T1,T2,T3>, &rt);}

#define RUN_TC4(NAME,T1,T2,T3,T4) \
{RUN_TC_GPARAMS_INIT(NAME); \
InitTestData((T1)0,(T2)0,(T3)0,(T4)0); \
sffi_call(GCurFuncSign, GArgs, (void*)&RunTestFunc<T1,T2,T3,T4>, &rt);}

#define RUN_TC5(NAME,T1,T2,T3,T4,T5) \
{RUN_TC_GPARAMS_INIT(NAME); \
InitTestData((T1)0,(T2)0,(T3)0,(T4)0,(T5)0); \
sffi_call(GCurFuncSign, GArgs, (void*)&RunTestFunc<T1,T2,T3,T4,T5>, &rt);}

#define RUN_TC6(NAME,T1,T2,T3,T4,T5,T6) \
{RUN_TC_GPARAMS_INIT(NAME); \
InitTestData((T1)0,(T2)0,(T3)0,(T4)0,(T5)0, (T6)0); \
sffi_call(GCurFuncSign, GArgs, (void*)&RunTestFunc<T1,T2,T3,T4,T5,T6>, &rt);}

#define RUN_TC7(NAME,T1,T2,T3,T4,T5,T6,T7) \
{RUN_TC_GPARAMS_INIT(NAME); \
InitTestData((T1)0,(T2)0,(T3)0,(T4)0,(T5)0, (T6)0, (T7)0); \
sffi_call(GCurFuncSign, GArgs, (void*)&RunTestFunc<T1,T2,T3,T4,T5,T6,T7>, &rt);}

#define RUN_TC8(NAME,T1,T2,T3,T4,T5,T6,T7,T8) \
{RUN_TC_GPARAMS_INIT(NAME); \
InitTestData((T1)0,(T2)0,(T3)0,(T4)0,(T5)0, (T6)0, (T7)0,(T8)0); \
sffi_call(GCurFuncSign, GArgs, (void*)&RunTestFunc<T1,T2,T3,T4,T5,T6,T7,T8>, &rt);}

#define RUN_TC9(NAME,T1,T2,T3,T4,T5,T6,T7,T8,T9) \
{RUN_TC_GPARAMS_INIT(NAME); \
InitTestData((T1)0,(T2)0,(T3)0,(T4)0,(T5)0, (T6)0, (T7)0,(T8)0,(T9)0); \
sffi_call(GCurFuncSign, GArgs, (void*)&RunTestFunc<T1,T2,T3,T4,T5,T6,T7,T8,T9>, &rt);}

#define RUN_TC10(NAME,T1,T2,T3,T4,T5,T6,T7,T8,T9,T10) \
{RUN_TC_GPARAMS_INIT(NAME); \
InitTestData((T1)0,(T2)0,(T3)0,(T4)0,(T5)0, (T6)0, (T7)0,(T8)0,(T9)0,(T10)0); \
sffi_call(GCurFuncSign, GArgs, (void*)&RunTestFunc<T1,T2,T3,T4,T5,T6,T7,T8,T9,T10>, &rt);}

#define RUN_TC11(NAME,T1,T2,T3,T4,T5,T6,T7,T8,T9,T10,T11) \
{RUN_TC_GPARAMS_INIT(NAME); \
InitTestData((T1)0,(T2)0,(T3)0,(T4)0,(T5)0, (T6)0, (T7)0,(T8)0,(T9)0,(T10)0,(T11)0); \
sffi_call(GCurFuncSign, GArgs, (void*)&RunTestFunc<T1,T2,T3,T4,T5,T6,T7,T8,T9,T10,T11>, &rt);}

#define RUN_TC12(NAME,T1,T2,T3,T4,T5,T6,T7,T8,T9,T10,T11,T12) \
{RUN_TC_GPARAMS_INIT(NAME); \
InitTestData((T1)0,(T2)0,(T3)0,(T4)0,(T5)0,(T6)0,(T7)0,(T8)0,(T9)0,(T10)0,(T11)0,(T12)0); \
sffi_call(GCurFuncSign, GArgs, (void*)&RunTestFunc<T1,T2,T3,T4,T5,T6,T7,T8,T9,T10,T11,T12>, &rt);}

#define RUN_TC2_ONE_TYPE(NAME, T) RUN_TC2(NAME,T,T)
#define RUN_TC3_ONE_TYPE(NAME, T) RUN_TC3(NAME,T,T,T)
#define RUN_TC4_ONE_TYPE(NAME, T) RUN_TC4(NAME,T,T,T,T)
#define RUN_TC5_ONE_TYPE(NAME, T) RUN_TC5(NAME,T,T,T,T,T)
#define RUN_TC6_ONE_TYPE(NAME, T) RUN_TC6(NAME,T,T,T,T,T,T)
#define RUN_TC7_ONE_TYPE(NAME, T) RUN_TC7(NAME,T,T,T,T,T,T,T)
#define RUN_TC8_ONE_TYPE(NAME, T) RUN_TC8(NAME,T,T,T,T,T,T,T,T)
#define RUN_TC9_ONE_TYPE(NAME, T) RUN_TC9(NAME,T,T,T,T,T,T,T,T,T)
#define RUN_TC10_ONE_TYPE(NAME, T) RUN_TC10(NAME,T,T,T,T,T,T,T,T,T,T)
#define RUN_TC11_ONE_TYPE(NAME, T) RUN_TC11(NAME,T,T,T,T,T,T,T,T,T,T,T)
#define RUN_TC12_ONE_TYPE(NAME, T) RUN_TC12(NAME,T,T,T,T,T,T,T,T,T,T,T,T)


template<class T>
T GetTypeValue(){
    VarProc<T>::randFill(&GArgs[0]);
    return *(T*)&GArgs[0];
}

template<class T>
void TestSimpleFFIReturnValue(){
    unsigned long rt = 0;
    sffi_call(GetTypeAttr<T>()->sign, NULL, (void*)&GetTypeValue<T>, &rt);
    
    if(!VarProc<T>::equal(*(T*)&rt, *(T*)&GArgs[0])){
        std::cout << "Test Fail: " << GTestCaseName << " v1=" << *(T*)&rt <<
        " v2=" << *(T*)&GArgs[0] << std::endl;
        GTCErrFlag = true;
    }
    else {
        //std::cout << "Test Pass: " << GTestCaseName << std::endl;
    }
}

#define RUN_TC_CHECK_RET(T) \
strcpy(GTestCaseName, "rt_"#T);\
TestSimpleFFIReturnValue<T>();


@implementation TestCase

+(void) Run {
    GTCErrFlag = false;
    
    for(int i = 0; i < 100; i++){
        [TestCase RunCCallTestCase];
        if(GTCErrFlag){
            break;
        }
    }
    
    if(!GTCErrFlag){
        std::cout << "All Test Pass!" << std::endl;
    }
    else {
        std::cout << "Test Fail!" << std::endl;
    }
}

double add_var(int a, double b, ...){
    va_list args;
    va_start(args, b);
    
    double r = a + b;
    long arg = 0;
    while ((arg = va_arg(args, long))) {
        r += arg;
    }
    
    va_end(args);
    return r;
}

void TestST(const char* name, const char* sign, int align, int size, int num_count, int* offsets){
    struct sffi_st st;
    sffi_get_st(sign, &st);
    
    if(st.align != align){
        GTCErrFlag = true;
        std::cout << name << " sturct align fail" << std::endl;
    }
    else if(st.size != size){
        GTCErrFlag = true;
        std::cout << name << " sturct size fail! sffi:" << (int)st.size << " org:" << size << std::endl;
    }
    else if(st.count != num_count){
        GTCErrFlag = true;
        std::cout << name << " sturct number count fail" << std::endl;
    }
    else {
        for(int i = 0; i < num_count; i++){
            if(st.numbers[i].offset != offsets[i]){
                GTCErrFlag = true;
                std::cout << name << " sturct number(" << i << ") offset fail! sffi:" << (int)st.numbers[i].offset << " org:" << offsets[i] << std::endl;
                break;
            }
        }
    }
}


+(void)RunCCallTestCase {
    RUN_TC1("1", uint8_t);
    RUN_TC1("2", uint16_t);
    RUN_TC1("3", uint32_t);
    RUN_TC1("4", uint64_t);
    RUN_TC1("5", int8_t);
    RUN_TC1("6", int16_t);
    RUN_TC1("7", int32_t);
    RUN_TC1("8", int64_t);
    RUN_TC1("9", float);
    RUN_TC1("10", double);
    
    RUN_TC2_ONE_TYPE("11", uint8_t);
    RUN_TC2_ONE_TYPE("12", uint16_t);
    RUN_TC2_ONE_TYPE("13", uint32_t);
    RUN_TC2_ONE_TYPE("14", uint64_t);
    RUN_TC2_ONE_TYPE("15", int8_t);
    RUN_TC2_ONE_TYPE("16", int16_t);
    RUN_TC2_ONE_TYPE("17", int32_t);
    RUN_TC2_ONE_TYPE("18", int64_t);
    RUN_TC2_ONE_TYPE("19", float);
    RUN_TC2_ONE_TYPE("20", double);
    
    RUN_TC3_ONE_TYPE("21", uint8_t);
    RUN_TC3_ONE_TYPE("22", uint16_t);
    RUN_TC3_ONE_TYPE("23", uint32_t);
    RUN_TC3_ONE_TYPE("24", uint64_t);
    RUN_TC3_ONE_TYPE("25", int8_t);
    RUN_TC3_ONE_TYPE("26", int16_t);
    RUN_TC3_ONE_TYPE("27", int32_t);
    RUN_TC3_ONE_TYPE("28", int64_t);
    RUN_TC3_ONE_TYPE("29", float);
    RUN_TC3_ONE_TYPE("30", double);
    
    RUN_TC4_ONE_TYPE("31", uint8_t);
    RUN_TC4_ONE_TYPE("32", uint16_t);
    RUN_TC4_ONE_TYPE("33", uint32_t);
    RUN_TC4_ONE_TYPE("34", uint64_t);
    RUN_TC4_ONE_TYPE("35", int8_t);
    RUN_TC4_ONE_TYPE("36", int16_t);
    RUN_TC4_ONE_TYPE("37", int32_t);
    RUN_TC4_ONE_TYPE("38", int64_t);
    RUN_TC4_ONE_TYPE("39", float);
    RUN_TC4_ONE_TYPE("40", double);
    
    RUN_TC5_ONE_TYPE("41", uint8_t);
    RUN_TC5_ONE_TYPE("42", uint16_t);
    RUN_TC5_ONE_TYPE("43", uint32_t);
    RUN_TC5_ONE_TYPE("44", uint64_t);
    RUN_TC5_ONE_TYPE("45", int8_t);
    RUN_TC5_ONE_TYPE("46", int16_t);
    RUN_TC5_ONE_TYPE("47", int32_t);
    RUN_TC5_ONE_TYPE("48", int64_t);
    RUN_TC5_ONE_TYPE("49", float);
    RUN_TC5_ONE_TYPE("50", double);
    
    RUN_TC6_ONE_TYPE("51", uint8_t);
    RUN_TC6_ONE_TYPE("52", uint16_t);
    RUN_TC6_ONE_TYPE("53", uint32_t);
    RUN_TC6_ONE_TYPE("54", uint64_t);
    RUN_TC6_ONE_TYPE("55", int8_t);
    RUN_TC6_ONE_TYPE("56", int16_t);
    RUN_TC6_ONE_TYPE("57", int32_t);
    RUN_TC6_ONE_TYPE("58", int64_t);
    RUN_TC6_ONE_TYPE("59", float);
    RUN_TC6_ONE_TYPE("60", double);
    
    RUN_TC7_ONE_TYPE("61", uint8_t);
    RUN_TC7_ONE_TYPE("62", uint16_t);
    RUN_TC7_ONE_TYPE("63", uint32_t);
    RUN_TC7_ONE_TYPE("64", uint64_t);
    RUN_TC7_ONE_TYPE("65", int8_t);
    RUN_TC7_ONE_TYPE("66", int16_t);
    RUN_TC7_ONE_TYPE("67", int32_t);
    RUN_TC7_ONE_TYPE("68", int64_t);
    RUN_TC7_ONE_TYPE("69", float);
    RUN_TC7_ONE_TYPE("70", double);
    
    RUN_TC8_ONE_TYPE("71", uint8_t);
    RUN_TC8_ONE_TYPE("72", uint16_t);
    RUN_TC8_ONE_TYPE("73", uint32_t);
    RUN_TC8_ONE_TYPE("74", uint64_t);
    RUN_TC8_ONE_TYPE("75", int8_t);
    RUN_TC8_ONE_TYPE("76", int16_t);
    RUN_TC8_ONE_TYPE("77", int32_t);
    RUN_TC8_ONE_TYPE("78", int64_t);
    RUN_TC8_ONE_TYPE("79", float);
    RUN_TC8_ONE_TYPE("80", double);
    
    RUN_TC9_ONE_TYPE("81", uint8_t);
    RUN_TC9_ONE_TYPE("82", uint16_t);
    RUN_TC9_ONE_TYPE("83", uint32_t);
    RUN_TC9_ONE_TYPE("84", uint64_t);
    RUN_TC9_ONE_TYPE("85", int8_t);
    RUN_TC9_ONE_TYPE("86", int16_t);
    RUN_TC9_ONE_TYPE("87", int32_t);
    RUN_TC9_ONE_TYPE("88", int64_t);
    RUN_TC9_ONE_TYPE("89", float);
    RUN_TC9_ONE_TYPE("90", double);
    
    RUN_TC10_ONE_TYPE("91", uint8_t);
    RUN_TC10_ONE_TYPE("92", uint16_t);
    RUN_TC10_ONE_TYPE("93", uint32_t);
    RUN_TC10_ONE_TYPE("94", uint64_t);
    RUN_TC10_ONE_TYPE("95", int8_t);
    RUN_TC10_ONE_TYPE("96", int16_t);
    RUN_TC10_ONE_TYPE("97", int32_t);
    RUN_TC10_ONE_TYPE("98", int64_t);
    RUN_TC10_ONE_TYPE("99", float);
    RUN_TC10_ONE_TYPE("100", double);
    
    RUN_TC11_ONE_TYPE("101", uint8_t);
    RUN_TC11_ONE_TYPE("102", uint16_t);
    RUN_TC11_ONE_TYPE("103", uint32_t);
    RUN_TC11_ONE_TYPE("104", uint64_t);
    RUN_TC11_ONE_TYPE("105", int8_t);
    RUN_TC11_ONE_TYPE("106", int16_t);
    RUN_TC11_ONE_TYPE("107", int32_t);
    RUN_TC11_ONE_TYPE("108", int64_t);
    RUN_TC11_ONE_TYPE("109", float);
    RUN_TC11_ONE_TYPE("110", double);
    
    RUN_TC12_ONE_TYPE("111", uint8_t);
    RUN_TC12_ONE_TYPE("112", uint16_t);
    RUN_TC12_ONE_TYPE("113", uint32_t);
    RUN_TC12_ONE_TYPE("114", uint64_t);
    RUN_TC12_ONE_TYPE("115", int8_t);
    RUN_TC12_ONE_TYPE("116", int16_t);
    RUN_TC12_ONE_TYPE("117", int32_t);
    RUN_TC12_ONE_TYPE("118", int64_t);
    RUN_TC12_ONE_TYPE("119", float);
    RUN_TC12_ONE_TYPE("120", double);
    
    RUN_TC8("121",int8_t,int16_t,int32_t,int64_t,int8_t,int16_t,int32_t,int64_t);
    RUN_TC9("122",int8_t,int16_t,int32_t,int64_t,int8_t,int16_t,int32_t,int64_t,int8_t);
    RUN_TC10("123",int8_t,int16_t,int32_t,int64_t,int8_t,int16_t,int32_t,int64_t,int8_t,int16_t);
    RUN_TC11("124",int8_t,int16_t,int32_t,int64_t,int8_t,int16_t,int32_t,int64_t,int8_t,int16_t,int32_t);
    RUN_TC12("125",int8_t,int16_t,int32_t,int64_t,int8_t,int16_t,int32_t,int64_t,int8_t,int16_t,int32_t,int64_t);
    
    RUN_TC8("126",float,double,float,double,float,double,float,double);
    RUN_TC9("127",float,double,float,double,float,double,float,double,float);
    RUN_TC10("128",float,double,float,double,float,double,float,double,float,double);
    RUN_TC11("129",float,double,float,double,float,double,float,double,float,double,float);
    RUN_TC12("130",float,double,float,double,float,double,float,double,float,double,float,double);
    
    RUN_TC8("131",int8_t,float,int16_t,double,int32_t,float,int64_t,double);
    RUN_TC9("132",int8_t,float,int16_t,double,int32_t,float,int64_t,double,int8_t);
    RUN_TC10("133",int8_t,float,int16_t,double,int32_t,float,int64_t,double,int8_t,float);
    RUN_TC11("134",int8_t,float,int16_t,double,int32_t,float,int64_t,double,int8_t,float,int16_t);
    RUN_TC12("135",int8_t,float,int16_t,double,int32_t,float,int64_t,double,int8_t,float,int16_t,double);
    
    RUN_TC1("136", void*);
    RUN_TC2_ONE_TYPE("137", void*);
    RUN_TC3_ONE_TYPE("138", void*);
    RUN_TC4_ONE_TYPE("139", void*);
    RUN_TC8_ONE_TYPE("140", void*);
    RUN_TC9_ONE_TYPE("141", void*);
    RUN_TC10_ONE_TYPE("142", void*);
    RUN_TC11_ONE_TYPE("143", void*);
    RUN_TC12_ONE_TYPE("144", void*);
    RUN_TC10("145", void*,int8_t, float, int64_t, double, void*, int8_t, void*, int16_t, void*);
    
    //测试返回值
    RUN_TC_CHECK_RET(uint8_t);
    RUN_TC_CHECK_RET(uint16_t);
    RUN_TC_CHECK_RET(uint32_t);
    RUN_TC_CHECK_RET(uint64_t);
    RUN_TC_CHECK_RET(int8_t);
    RUN_TC_CHECK_RET(int16_t);
    RUN_TC_CHECK_RET(int32_t);
    RUN_TC_CHECK_RET(int64_t);
    RUN_TC_CHECK_RET(float);
    RUN_TC_CHECK_RET(double);
    RUN_TC_CHECK_RET(void*);
    
    //测试变参函数
    {
        unsigned long args[5] = {0};
        *(int*)&args[0] = 6;
        *(double*)&args[1] = 50.5;
        *(long*)&args[2] = 5;
        *(long*)&args[3] = 70;
        *(long*)&args[4] = 0;
        
        unsigned long rt = 0;
        sffi_call_var("didlll", args, (void*)&add_var, &rt, 2);
        
        
        if(*(double*)&rt != (6+50.5+5+70)){
            GTCErrFlag = true;
            std::cout << "Variable parameter function test Fail!" << std::endl;
        }
    }
    
    //测试计算结构体布局
    //普通的类型结构
    TestSTructLayout<int8_t,int16_t,int32_t,int64_t,float,double,int8_t,int16_t,int32_t,int64_t>("TestST1");
    TestSTructLayout<int16_t,int16_t,int32_t,int32_t,int64_t,int64_t,int8_t,int8_t,int16_t,int16_t>("TestST2");
    TestSTructLayout<int8_t,int8_t,int8_t,int8_t,float,double,int8_t,int8_t,int8_t,int64_t>("TestST3");
    TestSTructLayout<float,int16_t,float,int64_t,float,double,int8_t,float,int32_t,float>("TestST4");
    TestSTructLayout<int8_t,int16_t,double,int64_t,float,double,int8_t,int16_t,float,int64_t>("TestST5");
    TestSTructLayout<int8_t,float,int32_t,int64_t,float,double,int8_t,float,int32_t,double>("TestST6");
    TestSTructLayout<int8_t,int8_t,int8_t,int8_t,int8_t,int8_t,int8_t,int8_t,int8_t,int8_t>("TestST7");
    TestSTructLayout<int16_t,float,int8_t,int64_t,int8_t,double,int8_t,int16_t,int8_t,int64_t>("TestST8");
    TestSTructLayout<int8_t,int64_t,int8_t,int64_t,double,int64_t,int8_t,int64_t,float,int8_t>("TestST9");
    
    //带1层嵌套的类型结构
    TestFixAlign0StructLayout<int8_t,int16_t,int32_t,int64_t,float,double,int8_t,int16_t,int32_t,int64_t>("TestST10");
    TestFixAlign0StructLayout<int8_t,int16_t,int32_t,int64_t,float,double,int8_t,int16_t,int32_t,int64_t>("TestST20");
    TestFixAlign0StructLayout<int16_t,int16_t,int32_t,int32_t,int64_t,int64_t,int8_t,int8_t,int16_t,int16_t>("TestST11");
    TestFixAlign0StructLayout<int8_t,int8_t,int8_t,int8_t,float,double,int8_t,int8_t,int8_t,int64_t>("TestST12");
    TestFixAlign0StructLayout<float,int16_t,float,int64_t,float,double,int8_t,float,int32_t,float>("TestST13");
    TestFixAlign0StructLayout<int8_t,int16_t,double,int64_t,float,double,int8_t,int16_t,float,int64_t>("TestST14");
    TestFixAlign0StructLayout<int8_t,float,int32_t,int64_t,float,double,int8_t,float,int32_t,double>("TestST15");
    
    //强制对齐 1 的结构体
    TestFixAlign1StructLayout<int8_t,int16_t,int32_t,int64_t,float,double,int8_t,int16_t,int32_t,int64_t>("TestST20");
    TestFixAlign1StructLayout<int16_t,int16_t,int32_t,int32_t,int64_t,int64_t,int8_t,int8_t,int16_t,int16_t>("TestST21");
    TestFixAlign1StructLayout<int8_t,int8_t,int8_t,int8_t,float,double,int8_t,int8_t,int8_t,int64_t>("TestST22");
    TestFixAlign1StructLayout<float,int16_t,float,int64_t,float,double,int8_t,float,int32_t,float>("TestST23");
    TestFixAlign1StructLayout<int8_t,int16_t,double,int64_t,float,double,int8_t,int16_t,float,int64_t>("TestST24");
    TestFixAlign1StructLayout<int8_t,float,int32_t,int64_t,float,double,int8_t,float,int32_t,double>("TestST25");
    
    //强制对齐 2 的结构体
    TestFixAlign2StructLayout<int8_t,int16_t,int32_t,int64_t,float,double,int8_t,int16_t,int32_t,int64_t>("TestST30");
    TestFixAlign2StructLayout<int16_t,int16_t,int32_t,int32_t,int64_t,int64_t,int8_t,int8_t,int16_t,int16_t>("TestST31");
    TestFixAlign2StructLayout<int8_t,int8_t,int8_t,int8_t,float,double,int8_t,int8_t,int8_t,int64_t>("TestST32");
    TestFixAlign2StructLayout<float,int16_t,float,int64_t,float,double,int8_t,float,int32_t,float>("TestST33");
    TestFixAlign2StructLayout<int8_t,int16_t,double,int64_t,float,double,int8_t,int16_t,float,int64_t>("TestST34");
    TestFixAlign2StructLayout<int8_t,float,int32_t,int64_t,float,double,int8_t,float,int32_t,double>("TestST35");
    
    //强制对齐 4 的结构体
    TestFixAlign4StructLayout<int8_t,int16_t,int32_t,int64_t,float,double,int8_t,int16_t,int32_t,int64_t>("TestST30");
    TestFixAlign4StructLayout<int16_t,int16_t,int32_t,int32_t,int64_t,int64_t,int8_t,int8_t,int16_t,int16_t>("TestST31");
    TestFixAlign4StructLayout<int8_t,int8_t,int8_t,int8_t,float,double,int8_t,int8_t,int8_t,int64_t>("TestST32");
    TestFixAlign4StructLayout<float,int16_t,float,int64_t,float,double,int8_t,float,int32_t,float>("TestST33");
    TestFixAlign4StructLayout<int8_t,int16_t,double,int64_t,float,double,int8_t,int16_t,float,int64_t>("TestST34");
    TestFixAlign4StructLayout<int8_t,float,int32_t,int64_t,float,double,int8_t,float,int32_t,double>("TestST35");
    
    //强制对齐 8 的结构体
    TestFixAlign8StructLayout<int8_t,int16_t,int32_t,int64_t,float,double,int8_t,int16_t,int32_t,int64_t>("TestST40");
    TestFixAlign8StructLayout<int16_t,int16_t,int32_t,int32_t,int64_t,int64_t,int8_t,int8_t,int16_t,int16_t>("TestST41");
    TestFixAlign8StructLayout<int8_t,int8_t,int16_t,int8_t,float,double,int8_t,int8_t,int8_t,int64_t>("TestST42");
    TestFixAlign8StructLayout<float,int16_t,float,int64_t,float,double,int8_t,float,int8_t,float>("TestST43");
    TestFixAlign8StructLayout<int8_t,int16_t,double,int64_t,int8_t,double,int8_t,int16_t,float,int64_t>("TestST44");
    TestFixAlign8StructLayout<int8_t,float,int32_t,float,float,double,int8_t,float,int32_t,double>("TestST45");
    
    //强制对齐 16 的结构体
    TestFixAlign16StructLayout<int8_t,int16_t,int32_t,int64_t,float,double,int8_t,int16_t,int32_t,int64_t>("TestST50");
    TestFixAlign16StructLayout<int16_t,int16_t,int32_t,int32_t,int64_t,int64_t,int8_t,int8_t,int16_t,int16_t>("TestST51");
    TestFixAlign16StructLayout<int8_t,int8_t,int16_t,int8_t,float,double,int8_t,int8_t,int8_t,int64_t>("TestST52");
    TestFixAlign16StructLayout<float,int16_t,float,int64_t,float,double,int8_t,float,int8_t,float>("TestST53");
    TestFixAlign16StructLayout<int8_t,int16_t,double,int64_t,int8_t,double,int8_t,int16_t,float,int64_t>("TestST54");
    TestFixAlign16StructLayout<int8_t,float,int32_t,float,float,double,int8_t,float,int32_t,double>("TestST55");
}

template<class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10>
void TestSTructLayout(const char* name){
    struct Foo1{
        T1 a; T2 b; T3 c; T4 d; T5 e; T6 f; T7 g; T8 h; T9 i; T10 j;
    };
    
    struct Foo2{
        char a;
        Foo1 b;
    };
    
    int offsets[10] = {
    offsetof(Foo1, a), offsetof(Foo1, b), offsetof(Foo1, c), offsetof(Foo1, d),
    offsetof(Foo1, e), offsetof(Foo1, f), offsetof(Foo1, g), offsetof(Foo1, h),
    offsetof(Foo1, i), offsetof(Foo1, j)};
    
    std::string strSign;
    strSign += GetTypeAttr<T1>()->sign[0];
    strSign += GetTypeAttr<T2>()->sign[0];
    strSign += GetTypeAttr<T3>()->sign[0];
    strSign += GetTypeAttr<T4>()->sign[0];
    strSign += GetTypeAttr<T5>()->sign[0];
    strSign += GetTypeAttr<T6>()->sign[0];
    strSign += GetTypeAttr<T7>()->sign[0];
    strSign += GetTypeAttr<T8>()->sign[0];
    strSign += GetTypeAttr<T9>()->sign[0];
    strSign += GetTypeAttr<T10>()->sign[0];
    
    TestST(name, strSign.c_str(), offsetof(Foo2, b), sizeof(Foo1), 10, offsets);
}

void SpliceSignStr(std::string& str, int fixAlign){}

template <class T ,class...  Args>
void SpliceSignStr(std::string& str, int fixAlign, T var, Args...args){
    if(str.empty()){
        if(fixAlign == 1 ||
           fixAlign == 2 ||
           fixAlign == 4 ||
           fixAlign == 8){
            str += ('0'+fixAlign);
        }
        else if(fixAlign == 16){
            str += "16";
        }
    }
    
    if(GetTypeAttr<T>()->sign[0] == 'c' &&
       (var == '[' || var == ']')){
        str += var;
        if(var == '['){
            if(fixAlign == 1 ||
               fixAlign == 2 ||
               fixAlign == 4 ||
               fixAlign == 8){
                str += ('0'+fixAlign);
            }
            else if(fixAlign == 16){
                str += "16";
            }
        }
    }
    else {
        str += GetTypeAttr<T>()->sign[0];
    }
    
    SpliceSignStr(str, fixAlign, args...);
}

template<class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10>
void SpliceSignStrWith10(std::string& str, int fixAlign){
    SpliceSignStr(str, fixAlign, VarProc<T1>::min(), '[', VarProc<T2>::min(), VarProc<T3>::min(), VarProc<T4>::min(),
                  VarProc<T5>::min(), ']', '[',  VarProc<T6>::min(), VarProc<T7>::min(), VarProc<T8>::min(), ']',
                  VarProc<T9>::min(), VarProc<T10>::min());
}

#undef TC_FIX_ALIGN
#define TC_FIX_ALIGN 0
template<class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10>
void TestFixAlign0StructLayout(const char* name){
#pragma pack(TC_FIX_ALIGN)
    struct Foo2{ T2 b; T3 c; T4 d; T5 e; };
    struct Foo3{ T6 f; T7 g; T8 h; };
    struct Foo1{ T1 a; Foo2 b; Foo3 c; T9 d; T10 e; };
#pragma pack()
    struct Foo4{ char a; Foo1 b; };

    int offsets[10] = {
    offsetof(Foo1, a), offsetof(Foo1, b), offsetof(Foo1, b) + offsetof(Foo2, c), offsetof(Foo1, b) + offsetof(Foo2, d),
    offsetof(Foo1, b) + offsetof(Foo2, e), offsetof(Foo1, c), offsetof(Foo1, c) + offsetof(Foo3, g), offsetof(Foo1, c) + offsetof(Foo3, h),
    offsetof(Foo1, d), offsetof(Foo1, e)};
    
    std::string strSign;
    SpliceSignStrWith10<T1,T2,T3,T4,T5,T6,T7,T8,T9,T10>(strSign, TC_FIX_ALIGN);
    
    TestST(name, strSign.c_str(), offsetof(Foo4, b), sizeof(Foo1), 10, offsets);
}

#undef TC_FIX_ALIGN
#define TC_FIX_ALIGN 1
template<class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10>
void TestFixAlign1StructLayout(const char* name){
#pragma pack(TC_FIX_ALIGN)
    struct Foo2{ T2 b; T3 c; T4 d; T5 e; };
    struct Foo3{ T6 f; T7 g; T8 h; };
    struct Foo1{ T1 a; Foo2 b; Foo3 c; T9 d; T10 e; };
#pragma pack()
    struct Foo4{ char a; Foo1 b; };

    int offsets[10] = {
    offsetof(Foo1, a), offsetof(Foo1, b), offsetof(Foo1, b) + offsetof(Foo2, c), offsetof(Foo1, b) + offsetof(Foo2, d),
    offsetof(Foo1, b) + offsetof(Foo2, e), offsetof(Foo1, c), offsetof(Foo1, c) + offsetof(Foo3, g), offsetof(Foo1, c) + offsetof(Foo3, h),
    offsetof(Foo1, d), offsetof(Foo1, e)};
    
    std::string strSign;
    SpliceSignStrWith10<T1,T2,T3,T4,T5,T6,T7,T8,T9,T10>(strSign, TC_FIX_ALIGN);
    
    TestST(name, strSign.c_str(), offsetof(Foo4, b), sizeof(Foo1), 10, offsets);
}



#undef TC_FIX_ALIGN
#define TC_FIX_ALIGN 2
template<class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10>
void TestFixAlign2StructLayout(const char* name){
#pragma pack(TC_FIX_ALIGN)
    struct Foo2{ T2 b; T3 c; T4 d; T5 e; };
    struct Foo3{ T6 f; T7 g; T8 h; };
    struct Foo1{ T1 a; Foo2 b; Foo3 c; T9 d; T10 e; };
#pragma pack()
    struct Foo4{ char a; Foo1 b; };

    int offsets[10] = {
    offsetof(Foo1, a), offsetof(Foo1, b), offsetof(Foo1, b) + offsetof(Foo2, c), offsetof(Foo1, b) + offsetof(Foo2, d),
    offsetof(Foo1, b) + offsetof(Foo2, e), offsetof(Foo1, c), offsetof(Foo1, c) + offsetof(Foo3, g), offsetof(Foo1, c) + offsetof(Foo3, h),
    offsetof(Foo1, d), offsetof(Foo1, e)};
    
    std::string strSign;
    SpliceSignStrWith10<T1,T2,T3,T4,T5,T6,T7,T8,T9,T10>(strSign, TC_FIX_ALIGN);
    
    TestST(name, strSign.c_str(), offsetof(Foo4, b), sizeof(Foo1), 10, offsets);
}

#undef TC_FIX_ALIGN
#define TC_FIX_ALIGN 4
template<class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10>
void TestFixAlign4StructLayout(const char* name){
#pragma pack(TC_FIX_ALIGN)
    struct Foo2{ T2 b; T3 c; T4 d; T5 e; };
    struct Foo3{ T6 f; T7 g; T8 h; };
    struct Foo1{ T1 a; Foo2 b; Foo3 c; T9 d; T10 e; };
#pragma pack()
    struct Foo4{ char a; Foo1 b; };

    int offsets[10] = {
    offsetof(Foo1, a), offsetof(Foo1, b), offsetof(Foo1, b) + offsetof(Foo2, c), offsetof(Foo1, b) + offsetof(Foo2, d),
    offsetof(Foo1, b) + offsetof(Foo2, e), offsetof(Foo1, c), offsetof(Foo1, c) + offsetof(Foo3, g), offsetof(Foo1, c) + offsetof(Foo3, h),
    offsetof(Foo1, d), offsetof(Foo1, e)};
    
    std::string strSign;
    SpliceSignStrWith10<T1,T2,T3,T4,T5,T6,T7,T8,T9,T10>(strSign, TC_FIX_ALIGN);
    
    TestST(name, strSign.c_str(), offsetof(Foo4, b), sizeof(Foo1), 10, offsets);
}

#undef TC_FIX_ALIGN
#define TC_FIX_ALIGN 8
template<class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10>
void TestFixAlign8StructLayout(const char* name){
#pragma pack(TC_FIX_ALIGN)
    struct Foo2{ T2 b; T3 c; T4 d; T5 e; };
    struct Foo3{ T6 f; T7 g; T8 h; };
    struct Foo1{ T1 a; Foo2 b; Foo3 c; T9 d; T10 e; };
#pragma pack()
    struct Foo4{ char a; Foo1 b; };

    int offsets[10] = {
    offsetof(Foo1, a), offsetof(Foo1, b), offsetof(Foo1, b) + offsetof(Foo2, c), offsetof(Foo1, b) + offsetof(Foo2, d),
    offsetof(Foo1, b) + offsetof(Foo2, e), offsetof(Foo1, c), offsetof(Foo1, c) + offsetof(Foo3, g), offsetof(Foo1, c) + offsetof(Foo3, h),
    offsetof(Foo1, d), offsetof(Foo1, e)};
    
    std::string strSign;
    SpliceSignStrWith10<T1,T2,T3,T4,T5,T6,T7,T8,T9,T10>(strSign, TC_FIX_ALIGN);
    
    TestST(name, strSign.c_str(), offsetof(Foo4, b), sizeof(Foo1), 10, offsets);
}

#undef TC_FIX_ALIGN
#define TC_FIX_ALIGN 16
template<class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8, class T9, class T10>
void TestFixAlign16StructLayout(const char* name){
#pragma pack(TC_FIX_ALIGN)
    struct Foo2{ T2 b; T3 c; T4 d; T5 e; };
    struct Foo3{ T6 f; T7 g; T8 h; };
    struct Foo1{ T1 a; Foo2 b; Foo3 c; T9 d; T10 e; };
#pragma pack()
    struct Foo4{ char a; Foo1 b; };

    int offsets[10] = {
    offsetof(Foo1, a), offsetof(Foo1, b), offsetof(Foo1, b) + offsetof(Foo2, c), offsetof(Foo1, b) + offsetof(Foo2, d),
    offsetof(Foo1, b) + offsetof(Foo2, e), offsetof(Foo1, c), offsetof(Foo1, c) + offsetof(Foo3, g), offsetof(Foo1, c) + offsetof(Foo3, h),
    offsetof(Foo1, d), offsetof(Foo1, e)};
    
    std::string strSign;
    SpliceSignStrWith10<T1,T2,T3,T4,T5,T6,T7,T8,T9,T10>(strSign, TC_FIX_ALIGN);
    
    TestST(name, strSign.c_str(), offsetof(Foo4, b), sizeof(Foo1), 10, offsets);
}





__attribute__ ((noinline)) double foo1(int a, long b, float c, double d){
     return a + b + c + d;
}

+(void) RunExample{
    unsigned long args[4] = {0};
    *(int*)&args[0] = -15;
    *(long*)&args[1] = 93;
    *(float*)&args[2] = -83.134;
    *(double*)&args[3] = 55.4239;
    
    unsigned long rt = 0;
    sffi_call("dilfd", args, (void*)&foo1, &rt);
    
    double r1 = foo1(-15, 93, -83.134, 55.4239);
    double r2 = *(double*)&rt;

    printf("foo1: -15 + 93 - 83.134 + 55.4239 = %lf\n", r1);
    printf("sffi: -15 + 93 - 83.134 + 55.4239 = %lf\n", r2);
    if(r1 == r2){
        printf("pass!\n");
    }
    else {
        printf("fail!\n");
    }
    
    //变参函数调用演示
    {
        unsigned long args[3] = {0};
        *(char**)&args[0] = (char*)"hello %s %d";
        *(char**)&args[1] = (char*)"world";
        *(int*)&args[2] = 2020;
        
        unsigned long rt = 0;
        sffi_call_var("vppi", args, (void*)&printf, &rt, 1);
    }
}

@end

