#pragma once

#include <stdio.h>

extern "C" {
    typedef int (*CallbackType)(unsigned, const char*, unsigned[1]);

    struct testStruct
    {
        unsigned long fieldLong;
        const char* fieldString;
        unsigned int fieldBool; 
        char fieldFixedArr[8];
        int (*fieldFnPtr)(unsigned, const char*, unsigned[1]);
        struct unknownClass* fieldUnknownPtr;
        void* voidPtr;
        CallbackType fnArray[4];
    };

    struct frozenStruct
    {
        int normalField;
        int fieldForFreeze;
        struct frozenStructTest* next;
    };

    struct pointersStruct
    {
        char* firstPointer;
        char* pointerArray[4];
        char* lastPointer;
    };

    unsigned long simpleAddNoPrintTest(unsigned long a, unsigned long b);
    int simpleAddTest(int a, int b);
    size_t simpleStrLenTest(const char* str);
    int simpleCallbackTest(unsigned a, const char* b, CallbackType callback);
    int simpleWriteToFileTest(FILE* file, const char* str);
    char* simpleEchoTest(char * str);
    float simpleFloatAddTest(const float a, const float b);
    double simpleDoubleAddTest(const double a, const double b);
    unsigned long simpleLongAddTest(unsigned long a, unsigned long b);
    struct testStruct simpleTestStructVal();
    struct testStruct* simpleTestStructPtr();
    struct testStruct simpleTestStructValBadPtr();
    struct testStruct* simpleTestStructPtrBadPtr();
    int* echoPointer(int* pointer);
    double simplePointerValAddTest(double* ptr, double val);
    struct pointersStruct initializePointerStruct(char* initVal);
    struct pointersStruct* initializePointerStructPtr(char* initVal);
    int internalCallback(unsigned, const char*, unsigned[1]);
    void simplePointerWrite(int* ptr, int val);
}