#include <stdio.h>
#include <type_traits>
#include <dlfcn.h>
#include <iostream>
#include <limits>
#include "libtest.h"
#include "RLBox_DynLib.h"
#include "RLBox_NaCl.h"
#include "testlib_structs_for_cpp_api.h"
#include "rlbox.h"

using namespace rlbox;

#define ENSURE(a) if(!(a)) { printf("%s check failed\n", #a); exit(1); }
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

rlbox_load_library_api(testlib, RLBox_DynLib)
rlbox_load_library_api(testlib, RLBox_NaCl)

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
		tainted<int**, TSandbox> pb = 0;
		pb = 0;

		tainted<int***, TSandbox> pc = sandbox->template mallocInSandbox<int**>();
		*pc = 0;
		pb = *pc;
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
		auto ret = sandbox_invoke_static(sandbox, simpleAddTest, a, 22);
		ENSURE(ret.UNSAFE_Unverified() == 42);

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
	}

	void testStructWithBadPtr()
	{
		auto resultT = sandbox_invoke(sandbox, simpleTestStructValBadPtr);
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
	}

	void testStructPtrWithBadPtr()
	{
		auto resultT = sandbox_invoke(sandbox, simpleTestStructPtrBadPtr);
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
		testPointerNullChecks();
		testVolatileDerefOperator();
		testAddressOfOperators();
		testAppPointer();
		testFunctionInvocation();
		test64BitReturns();
		testTwoVerificationFunctionFormats();
		testPointerVerificationFunctionFormats();
		testStackAndHeapArrAndStringParams();
		testCallback();
		testCallbackOnStruct();
		testEchoAndPointerLocations();
		testFloatingPoint();
		testStructures();
		testStructurePointers();
		testStatefulLambdas();
		testAppPtrFunctionReturn();
	}

	void runBadPointersTest()
	{
		testStructWithBadPtr();
		testStructPtrWithBadPtr();
	}
};

int main(int argc, char const *argv[])
{
	printf("Testing dyn lib\n");
	SandboxTests<RLBox_DynLib> testerDynLib;
	testerDynLib.init("", "./libtest.so");
	testerDynLib.runTests();
	//the RLBox_DynLib doesn't mask bad pointers, so can't test with 'runBadPointersTest'

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
	return 0;
}