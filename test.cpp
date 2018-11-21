#include <stdio.h>
#include <type_traits>
#include <dlfcn.h>
#include <iostream>
#include <limits>
#include "libtest.h"
#include "RLBox_MyApp.h"
#include "RLBox_DynLib.h"
#ifndef NO_NACL
	#include "RLBox_NaCl.h"
#endif
#ifndef NO_WASM
	#include "RLBox_Wasm.h"
#endif
#include "testlib_structs_for_cpp_api.h"
#include "rlbox.h"

using namespace rlbox;

#define ENSURE(a) if(!(a)) { printf("%s check failed\n", #a); abort(); }
#define UNUSED(a) (void)(a)

//////////////////////////////////////////////////////////////////

template<typename... T>
void printTypes()
{
	#ifdef _MSC_VER
 		printf("%s\n", __FUNCSIG__);
 	#else
 		printf("%s\n", __PRETTY_FUNCTION__);
 	#endif
}

//////////////////////////////////////////////////////////////////

rlbox_load_library_api(testlib, RLBox_MyApp)
rlbox_load_library_api(testlib, RLBox_DynLib)
#ifndef NO_NACL
	rlbox_load_library_api(testlib, RLBox_NaCl)
#endif
#ifndef NO_WASM
	rlbox_load_library_api(testlib, RLBox_Wasm)
#endif

//////////////////////////////////////////////////////////////////

template<typename TSandbox>
class SandboxTests
{
public:
	RLBoxSandbox<TSandbox>* sandbox;

	void testSizes()
	{
		ENSURE(sizeof(tainted<long, TSandbox>) == sizeof(long));
		ENSURE(sizeof(tainted<int*, TSandbox>) == sizeof(int*));
		ENSURE(sizeof(tainted<testStruct, TSandbox>) == sizeof(testStruct));
		ENSURE(sizeof(tainted<testStruct*, TSandbox>) == sizeof(testStruct*));

		ENSURE(sizeof(tainted_volatile<long, TSandbox>) == sizeof(long));
		ENSURE(sizeof(tainted_volatile<int*, TSandbox>) == sizeof(int*));
		ENSURE(sizeof(tainted_volatile<testStruct, TSandbox>) == sizeof(testStruct));
		ENSURE(sizeof(tainted_volatile<testStruct*, TSandbox>) == sizeof(testStruct*));
	}

	void testAssignment()
	{
		tainted<int, TSandbox> a;
		a = 4;
		tainted<int, TSandbox> b = 5;
		tainted<int, TSandbox> c = b;
		tainted<int, TSandbox> d;
		d = b;
		ENSURE(a.UNSAFE_Unverified() == 4);
		ENSURE(b.UNSAFE_Unverified() == 5);
		ENSURE(c.UNSAFE_Unverified() == 5);
		ENSURE(d.UNSAFE_Unverified() == 5);
	}

	void testBinaryOperators()
	{
		tainted<int, TSandbox> a = 3;
		tainted<int, TSandbox> b = 3 + 4;
		tainted<int, TSandbox> c = a + 3;
		tainted<int, TSandbox> d = a + b;
		ENSURE(a.UNSAFE_Unverified() == 3);
		ENSURE(b.UNSAFE_Unverified() == 7);
		ENSURE(c.UNSAFE_Unverified() == 6);
		ENSURE(d.UNSAFE_Unverified() == 10);
	}

	void testDerefOperators()
	{
		tainted<int*, TSandbox> pa = sandbox->template mallocInSandbox<int>();
		tainted_volatile<int, TSandbox>& deref = *pa;
		tainted<int, TSandbox> deref2 = *pa;
		deref2 = *pa;
		UNUSED(deref);
		UNUSED(deref2);
	}

	void testPointerAssignments()
	{
		tainted<int**, TSandbox> pa = nullptr;
		pa = nullptr;
		tainted<int**, TSandbox> pb = nullptr;
		pb = nullptr;

		tainted<int***, TSandbox> pc = sandbox->template mallocInSandbox<int**>();
		*pc = nullptr;
		pb = *pc;

		tainted<void**, TSandbox> pv = sandbox->template mallocInSandbox<void*>();
		*pv = nullptr;

		tainted<testStruct*, TSandbox> ps = sandbox->template mallocInSandbox<testStruct>();
		ps->voidPtr = nullptr;
	}

	void testVolatilePtrToTaintedPtrConversions()
	{
		tainted<testStruct*, TSandbox> ps = sandbox->template mallocInSandbox<testStruct>();

		tainted_volatile<void*, TSandbox>& psref = ps->voidPtr;
		tainted<void**, TSandbox> psCopy = &psref;
		ps->voidPtr = psCopy;
		ps->voidPtr = &psref;
	}

	void testPointerNullChecks()
	{
		auto tempValPtr = sandbox->template mallocInSandbox<int>();
		*tempValPtr = 3;
		auto resultT = sandbox_invoke(sandbox, echoPointer, tempValPtr);
		//make sure null checks go through

		ENSURE(resultT != nullptr);
		auto result = resultT.copyAndVerify([](int* val) -> int { return *val; });
		ENSURE(result == 3);
	}

	void testVolatileDerefOperator()
	{
		tainted<int**, TSandbox> ppa = sandbox->template mallocInSandbox<int*>();
		*ppa = sandbox->template mallocInSandbox<int>();
		tainted<int, TSandbox> a = **ppa;
		UNUSED(a);
	}

	void testAddressOfOperators()
	{
		tainted<int*, TSandbox> pa = sandbox->template mallocInSandbox<int>();
		tainted<int*, TSandbox> pa2 = &(*pa);
		UNUSED(pa2);
	}

	void testAppPointer()
	{
		tainted<int**, TSandbox> ppa = sandbox->template mallocInSandbox<int*>();
		int* pa = new int;
		*ppa = sandbox->app_ptr(pa);
	}

	void testFunctionInvocation()
	{
		tainted<int, TSandbox> a = 20;
		auto ret2 = sandbox_invoke(sandbox, simpleAddTest, a, 22);
		ENSURE(ret2.UNSAFE_Unverified() == 42);
	}

	void test64BitReturns()
	{
		auto ret2 = sandbox_invoke(sandbox, simpleLongAddTest, std::numeric_limits<std::uint32_t>::max(), 20);
		unsigned long result = ((unsigned long)std::numeric_limits<std::uint32_t>::max()) + 20ul;
		ENSURE(ret2.UNSAFE_Unverified() == result);		
	}

	void testTwoVerificationFunctionFormats()
	{
		auto result1 = sandbox_invoke(sandbox, simpleAddTest, 2, 3)
			.copyAndVerify([](int val){ return val > 0 && val < 10? RLBox_Verify_Status::SAFE : RLBox_Verify_Status::UNSAFE;}, -1);
		ENSURE(result1 == 5);

		auto result2 = sandbox_invoke(sandbox, simpleAddTest, 2, 3)
			.copyAndVerify([](int val){ return val > 0 && val < 10? val : -1;});
		ENSURE(result2 == 5);
	}

	void testEnumVerificationFunction()
	{
		typedef enum {
			ENUM_UNKNOWN,
			ENUM_FIRST,
			ENUM_SECOND,
			ENUM_THIRD
		} Example_Enum;

		tainted<Example_Enum, TSandbox> ref;
		auto enumVal = ref.copyAndVerify([](Example_Enum val){
			return val <= ENUM_THIRD? val : ENUM_UNKNOWN;
		});
		UNUSED(enumVal);
	}

	void testPointerVerificationFunctionFormats()
	{
		tainted<int*, TSandbox> pa = sandbox->template mallocInSandbox<int>();
		*pa = 4;

		auto result1 = sandbox_invoke(sandbox, echoPointer, pa)
			.copyAndVerify([](int* val){ return *val > 0 && *val < 10? *val : -1;});
		ENSURE(result1 == 4);
	}

	void testStackAndHeapArrAndStringParams()
	{
		auto result1 = sandbox_invoke(sandbox, simpleStrLenTest, sandbox->stackarr("Hello"))
			.copyAndVerify([](size_t val) -> size_t { return (val <= 0 || val >= 10)? -1 : val; });
		ENSURE(result1 == 5);

		auto result2 = sandbox_invoke(sandbox, simpleStrLenTest, sandbox->heaparr("Hello"))
			.copyAndVerify([](size_t val) -> size_t { return (val <= 0 || val >= 10)? -1 : val; });
		ENSURE(result2 == 5);
	}

	static int exampleCallback(RLBoxSandbox<TSandbox>* sandbox, tainted<unsigned, TSandbox> a, tainted<const char*, TSandbox> b, tainted<unsigned[1], TSandbox> c)
	{
		auto aCopy = a.copyAndVerify([](unsigned val){ return val > 0 && val < 100? val : -1; });
		auto bCopy = b.copyAndVerifyString(sandbox, [](const char* val) { return strlen(val) < 100? RLBox_Verify_Status::SAFE : RLBox_Verify_Status::UNSAFE; }, nullptr);
		unsigned cCopy[1];
		c.copyAndVerify(cCopy, sizeof(cCopy), [](unsigned arr[1], size_t arrSize){ 
			UNUSED(arrSize); 
			unsigned val = *arr; 
			return val > 0 && val < 100? RLBox_Verify_Status::SAFE : RLBox_Verify_Status::UNSAFE;
		});
		if(cCopy[0] + 1 != aCopy)
		{
			printf("Unexpected callback value: %d, %d\n", cCopy[0] + 1, aCopy);
			exit(1);
		}
		return aCopy + strlen(bCopy);
	}

	void testCallback()
	{
		auto registeredCallback = sandbox->createCallback(exampleCallback);
		auto resultT = sandbox_invoke(sandbox, simpleCallbackTest, (unsigned) 4, sandbox->stackarr("Hello"), registeredCallback);

		auto result = resultT
			.copyAndVerify([](int val){ return val > 0 && val < 100? val : -1; });
		ENSURE(result == 10);
	}

	void testInternalCallback()
	{
		auto fnPtr = sandbox_function(sandbox, internalCallback);

		tainted<testStruct*, TSandbox> pFoo = sandbox->template mallocInSandbox<testStruct>();
		pFoo->fieldFnPtr = fnPtr;

		auto resultT = sandbox_invoke(sandbox, simpleCallbackTest, (unsigned) 4, sandbox->stackarr("Hello"), fnPtr);
		auto result = resultT
			.copyAndVerify([](int val){ return val > 0 && val < 100? val : -1; });
		ENSURE(result == 10);
	}

	void testCallbackOnStruct()
	{
		tainted<testStruct, TSandbox> foo;
		auto registeredCallback = sandbox->createCallback(exampleCallback);
		foo.fieldFnPtr = registeredCallback;

		tainted<testStruct*, TSandbox> pFoo = sandbox->template mallocInSandbox<testStruct>();
		pFoo->fieldFnPtr = registeredCallback;
	}

	void testEchoAndPointerLocations()
	{
		const char* str = "Hello";

		//str is allocated in our heap, not the sandbox's heap
		ENSURE(sandbox->isPointerInAppMemoryOrNull(str));

		tainted<char*, TSandbox> temp = sandbox->template mallocInSandbox<char>(strlen(str) + 1);
		char* strInSandbox = temp.UNSAFE_Unverified();
		ENSURE(sandbox->isPointerInSandboxMemoryOrNull(strInSandbox));

		strcpy(strInSandbox, str);

		auto retStrRaw = sandbox_invoke(sandbox, simpleEchoTest, temp);
		*retStrRaw = 'g';
		*retStrRaw = 'H';
		char* retStr = retStrRaw.copyAndVerifyString(sandbox, [](char* val) { return strlen(val) < 100? RLBox_Verify_Status::SAFE : RLBox_Verify_Status::UNSAFE; }, nullptr);

		ENSURE(retStr != nullptr && sandbox->isPointerInAppMemoryOrNull(retStr));

		auto isStringSame = strcmp(str, retStr) == 0;
		ENSURE(isStringSame);

		sandbox->freeInSandbox(temp);
	}

	void testFloatingPoint()
	{
		auto resultF = sandbox_invoke(sandbox, simpleFloatAddTest, 1.0f, 2.0f)
			.copyAndVerify([](float val){ return val > 0 && val < 100? val : -1.0; });
		ENSURE(resultF == 3.0);

		auto resultD = sandbox_invoke(sandbox, simpleDoubleAddTest, 1.0, 2.0)
			.copyAndVerify([](double val){ return val > 0 && val < 100? val : -1.0; });
		ENSURE(resultD == 3.0);

		//test float to double conversions

		auto resultFD = sandbox_invoke(sandbox, simpleFloatAddTest, 1.0, 2.0)
			.copyAndVerify([](double val){ return val > 0 && val < 100? val : -1.0; });
		ENSURE(resultFD == 3.0);
	}

	void testPointerValAdd()
	{
		tainted<double*, TSandbox> pd = sandbox->template mallocInSandbox<double>();
		*pd = 1.0;

		double d = 2.0d;

		auto resultD = sandbox_invoke(sandbox, simplePointerValAddTest, pd, d)
			.copyAndVerify([](double val){ return val > 0 && val < 100? val : -1.0; });
		ENSURE(resultD == 3.0);
	}

	void testStructures()
	{
		auto resultT = sandbox_invoke(sandbox, simpleTestStructVal);
		auto result = resultT
			.copyAndVerify([this](tainted<testStruct, TSandbox>& val){ 
				testStruct ret;
				ret.fieldLong = val.fieldLong.copyAndVerify([](unsigned long val) { return val; });
				ret.fieldString = val.fieldString.copyAndVerifyString(sandbox, [](const char* val) { return strlen(val) < 100? RLBox_Verify_Status::SAFE : RLBox_Verify_Status::UNSAFE; }, nullptr);
				ret.fieldBool = val.fieldBool.copyAndVerify([](unsigned int val) { return val; });
				val.fieldFixedArr.copyAndVerify(ret.fieldFixedArr, sizeof(ret.fieldFixedArr), [](char* arr, size_t size){ UNUSED(arr); UNUSED(size); return RLBox_Verify_Status::SAFE; });
				return ret; 
			});
		ENSURE(result.fieldLong == 7 && 
			strcmp(result.fieldString, "Hello") == 0 &&
			result.fieldBool == 1 &&
			strcmp(result.fieldFixedArr, "Bye") == 0);

		//writes should still go through
		resultT.fieldLong = 17;
		long val = resultT.fieldLong.copyAndVerify([](unsigned long val) { return val; });
		ENSURE(val == 17);
	}

	void testStructurePointers()
	{
		auto resultT = sandbox_invoke(sandbox, simpleTestStructPtr);
		auto result = resultT
			.copyAndVerify([this](tainted<testStruct, TSandbox>* val) { 
				testStruct ret;
				ret.fieldLong = val->fieldLong.copyAndVerify([](unsigned long val) { return val; });
				ret.fieldString = val->fieldString.copyAndVerifyString(sandbox, [](const char* val) { return strlen(val) < 100? RLBox_Verify_Status::SAFE : RLBox_Verify_Status::UNSAFE; }, nullptr);
				ret.fieldBool = val->fieldBool.copyAndVerify([](unsigned int val) { return val; });
				val->fieldFixedArr.copyAndVerify(ret.fieldFixedArr, sizeof(ret.fieldFixedArr), [](char* arr, size_t size){ UNUSED(arr); UNUSED(size); return RLBox_Verify_Status::SAFE; });
				return ret; 
			});
		ENSURE(result.fieldLong == 7 && 
			strcmp(result.fieldString, "Hello") == 0 &&
			result.fieldBool == 1 &&
			strcmp(result.fieldFixedArr, "Bye") == 0);

		//writes should still go through
		resultT->fieldLong = 17;
		long val2 = resultT->fieldLong.copyAndVerify([](unsigned long val) { return val; });
		ENSURE(val2 == 17);

		//test & and * operators
		unsigned long val3 = (*&resultT->fieldLong).copyAndVerify([](unsigned long val) { return val; });
		ENSURE(val3 == 17);
	}

	void testStatefulLambdas()
	{
		int val2 = 42;
		auto tempValPtr = sandbox->template mallocInSandbox<int>();
		*tempValPtr = 3;

		//capture something to test stateful lambdas
		int result = tempValPtr->copyAndVerify([&val2](int val) { return val + val2; });
		ENSURE(result == 45);
	}

	void testAppPtrFunctionReturn()
	{
		#if defined(_M_X64) || defined(__x86_64__)
			int* val = (int*)0x1234567812345678;
		#else
			int* val = (int*)0x12345678;
		#endif

		auto appPtr = sandbox->app_ptr(val);
		auto resultT = sandbox_invoke_return_app_ptr(sandbox, echoPointer, appPtr);
		ENSURE(resultT == val);

		//Test app pointers in tainted structs
		auto testPtr = "Testing";
		auto tempValPtr = sandbox->template mallocInSandbox<testStruct>();
		tempValPtr->fieldString = sandbox->app_ptr(testPtr);
		auto testPtr2 = tempValPtr->fieldString.copyAndVerifyAppPtr(sandbox, [](const char* val){ return val; });
		ENSURE(testPtr == testPtr2);

		sandbox->template freeInSandbox(tempValPtr);
	}

	void testPointersInStruct()
	{
		auto initVal = sandbox->template mallocInSandbox<char>();
		auto resultT = sandbox_invoke(sandbox, initializePointerStruct, initVal);
		auto result = resultT
			.copyAndVerify([this](tainted<pointersStruct, TSandbox>& val){ 
				pointersStruct ret;
				ret.firstPointer = val.firstPointer.UNSAFE_Unverified();
				ret.pointerArray[0] = val.pointerArray[0].UNSAFE_Unverified();
				ret.pointerArray[1] = val.pointerArray[1].UNSAFE_Unverified();
				ret.pointerArray[2] = val.pointerArray[2].UNSAFE_Unverified();
				ret.pointerArray[3] = val.pointerArray[3].UNSAFE_Unverified();
				ret.lastPointer = val.lastPointer.UNSAFE_Unverified();
				return ret; 
			});
		char* initValRaw = initVal.UNSAFE_Unverified();
		sandbox->freeInSandbox(initVal);

		ENSURE(
			result.firstPointer == initValRaw &&
			result.pointerArray[0] == (char*) (((uintptr_t) initValRaw) + 1) &&
			result.pointerArray[1] == (char*) (((uintptr_t) initValRaw) + 2) &&
			result.pointerArray[2] == (char*) (((uintptr_t) initValRaw) + 3) &&
			result.pointerArray[3] == (char*) (((uintptr_t) initValRaw) + 4) &&
			result.lastPointer ==     (char*) (((uintptr_t) initValRaw) + 5)
		);
	}

	void test32BitPointerEdgeCases()
	{
		auto initVal = sandbox->template mallocInSandbox<char>(8);
		*(initVal.getPointerIncrement(sandbox, 3)) = 'v';
		char* initValRaw = initVal.UNSAFE_Unverified();

		auto resultT = sandbox_invoke(sandbox, initializePointerStructPtr, initVal);

		//check that reading a pointer in an array doesn't read neighboring elements
		ENSURE(
			resultT->pointerArray[0].UNSAFE_Unverified() == (char*) (((uintptr_t) initValRaw) + 1)
		);

		//check that a write doesn't overwrite neighboring elements
		resultT->pointerArray[0] = nullptr;
		ENSURE(
			resultT->pointerArray[1].UNSAFE_Unverified() == (char*) (((uintptr_t) initValRaw) + 2)
		);

		//check that array reference decay followed by a read doesn't read neighboring elements
		tainted<char**, TSandbox> elRef = &(resultT->pointerArray[2]);
		ENSURE((**elRef).UNSAFE_Unverified() == 'v');

		//check that array reference decay followed by a write doesn't overwrite neighboring elements
		*elRef = nullptr;
		ENSURE(
			resultT->pointerArray[3].UNSAFE_Unverified() == (char*) (((uintptr_t) initValRaw) + 4)
		);
	}

	void testFrozenValues()
	{
		tainted_freezable<int*, TSandbox> pfa = sandbox->template mallocFrozenInSandbox<int>();
		*pfa = 42;

		tainted<int*, TSandbox> pa = sandbox->template mallocInSandbox<int>();
		*pa = *pfa;

		pfa->freeze();

		auto ret = pfa->copyAndVerify([](int val) { return val; });
		ENSURE(ret == 42);

		pfa->unfreeze();
		pfa->freeze();

		tainted<int, TSandbox> taintedCopy;
		taintedCopy = *pfa;
		auto ret2 = taintedCopy.copyAndVerify([](int val) { return val; });
		ENSURE(ret2 == 42);

		tainted<int, TSandbox> a = *pfa;
		ENSURE(a.UNSAFE_Unverified() == 42);

		pfa->unfreeze();

		sandbox->template freeInSandbox(pa);
		sandbox->template freeInSandbox(pfa);
	}

	void testFrozenStructs() {
		tainted<struct frozenStruct*, TSandbox> pa = sandbox->template mallocInSandbox<struct frozenStruct>();
		pa->normalField = 1;
		pa->fieldForFreeze = 2;

		//The following would be an error as we are performing computation on a frozen type without freezing
		//tainted<int, TSandbox> temp = pa->fieldForFreeze + 1;
		pa->fieldForFreeze.freeze();

		sandbox_invoke(sandbox, simplePointerWrite, &(pa->fieldForFreeze), pa->fieldForFreeze + 1);
		//read here would be an error as there is a new value at the location
		pa->fieldForFreeze.freeze();

		auto normalFieldVal = pa->normalField.UNSAFE_Unverified();
		auto frozenFieldVal = pa->fieldForFreeze.UNSAFE_Unverified();

		ENSURE(normalFieldVal == 1);
		UNUSED(frozenFieldVal == 2);
	}

	void testStructWithBadPtr()
	{
		auto resultT = sandbox_invoke(sandbox, simpleTestStructValBadPtr);
		auto result = resultT
			.copyAndVerify([this](tainted<testStruct, TSandbox>& val){ 
				testStruct ret;
				ret.fieldLong = val.fieldLong.copyAndVerify([](unsigned long val) { return val; });
				ret.fieldString = val.fieldString.UNSAFE_Unverified();
				ret.fieldBool = val.fieldBool.copyAndVerify([](unsigned int val) { return val; });
				val.fieldFixedArr.copyAndVerify(ret.fieldFixedArr, sizeof(ret.fieldFixedArr), [](char* arr, size_t size){ UNUSED(arr); UNUSED(size); return RLBox_Verify_Status::SAFE; });
				return ret; 
			});
		ENSURE(result.fieldLong == 7 && 
			result.fieldString == 0 &&
			result.fieldBool == 1 &&
			strcmp(result.fieldFixedArr, "Bye") == 0);
	}

	void testStructPtrWithBadPtr()
	{
		auto resultT = sandbox_invoke(sandbox, simpleTestStructPtrBadPtr);
		resultT->fieldString = nullptr;
		auto result = resultT
			.copyAndVerify([this](tainted<testStruct, TSandbox>* val) { 
				testStruct ret;
				ret.fieldLong = val->fieldLong.copyAndVerify([](unsigned long val) { return val; });
				ret.fieldString = nullptr;
				ret.fieldBool = val->fieldBool.copyAndVerify([](unsigned int val) { return val; });
				val->fieldFixedArr.copyAndVerify(ret.fieldFixedArr, sizeof(ret.fieldFixedArr), [](char* arr, size_t size){ UNUSED(arr); UNUSED(size); return RLBox_Verify_Status::SAFE; });
				return ret; 
			});
		ENSURE(result.fieldLong == 7 && 
			result.fieldString == 0 &&
			result.fieldBool == 1 &&
			strcmp(result.fieldFixedArr, "Bye") == 0);
	}

	void testBadRangePointer()
	{
		tainted<char*, TSandbox> maxPtr = sandbox->getMaxPointer();

		auto result = maxPtr.UNSAFE_Unverified_Check_Range(sandbox, 32);
		ENSURE(result == nullptr);
	}

	void init(const char* runtimePath, const char* libraryPath)
	{
		sandbox = RLBoxSandbox<TSandbox>::createSandbox(runtimePath, libraryPath);
	}

	void runTests()
	{
		testSizes();
		testAssignment();
		testBinaryOperators();
		testDerefOperators();
		testPointerAssignments();
		testVolatilePtrToTaintedPtrConversions();
		testVolatileDerefOperator();
		testAddressOfOperators();
		testAppPointer();
		testFunctionInvocation();
		testPointerNullChecks();
		test64BitReturns();
		testTwoVerificationFunctionFormats();
		testEnumVerificationFunction();
		testPointerVerificationFunctionFormats();
		testStackAndHeapArrAndStringParams();
		testCallback();
		testInternalCallback();
		testCallbackOnStruct();
		testEchoAndPointerLocations();
		testFloatingPoint();
		testPointerValAdd();
		testStructures();
		testStructurePointers();
		testStatefulLambdas();
		testAppPtrFunctionReturn();
		testPointersInStruct();
		test32BitPointerEdgeCases();
		testFrozenValues();
		testFrozenStructs();
	}

	void runBadPointersTest()
	{
		testStructWithBadPtr();
		testStructPtrWithBadPtr();
		testBadRangePointer();
	}
};

int main(int argc, char const *argv[])
{
	printf("Testing calls within my app - i.e. no sandbox\n");
	SandboxTests<RLBox_MyApp> testerMyApp;
	testerMyApp.init("", "");
	testerMyApp.runTests();
	//the RLBox_MyApp doesn't mask bad pointers, so can't test with 'runBadPointersTest'

	printf("Testing dyn lib\n");
	SandboxTests<RLBox_DynLib> testerDynLib;
	testerDynLib.init("", "./libtest.so");
	testerDynLib.runTests();
	//the RLBox_DynLib doesn't mask bad pointers, so can't test with 'runBadPointersTest'

	#ifndef NO_NACL
		printf("Testing NaCl\n");
		SandboxTests<RLBox_NaCl> testerNaCl;
		testerNaCl.init(
		#if defined(_M_IX86) || defined(__i386__)
		"../../../Sandboxing_NaCl/native_client/scons-out-firefox/nacl_irt-x86-32/staging/irt_core.nexe"
		#else
		"../../../Sandboxing_NaCl/native_client/scons-out-firefox/nacl_irt-x86-64/staging/irt_core.nexe"
		#endif
		, "./libtest.nexe");
		testerNaCl.runTests();
		testerNaCl.runBadPointersTest();
	#endif

	#ifndef NO_WASM
		#if !(defined(_M_IX86) || defined(__i386__))
		printf("Testing WASM\n");
		SandboxTests<RLBox_Wasm> testerWasm;
		testerWasm.init(
			""
			, "./libwasm_test.so");
		testerWasm.runTests();
		testerWasm.runBadPointersTest();
		#endif
	#endif

	return 0;
}