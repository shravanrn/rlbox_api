#include <stdio.h>
#include <type_traits>
#include <dlfcn.h>
#include <iostream>
#include "libtest.h"

template<typename... T>
void printTypes()
{
	#ifdef _MSC_VER
 		printf("%s\n", __FUNCSIG__);
 	#else
 		printf("%s\n", __PRETTY_FUNCTION__);
 	#endif
}

#include "rlbox.h"

using namespace rlbox;

#define ENSURE(a) if(!(a)) { printf("%s check failed\n", #a); exit(1); }
#define UNUSED(a) (void)(a)

class DynLibNoSandbox;
thread_local DynLibNoSandbox* dynLibNoSandbox_SavedState = nullptr;

class DynLibNoSandbox
{
private:
	void* allowedFunctions[32];
	void* functionState[32];
	void* libHandle = nullptr;
	int pushPopCount = 0;

public:
	void impl_CreateSandbox(const char* sandboxRuntimePath, const char* libraryPath)
	{
		libHandle = dlopen(libraryPath, RTLD_LAZY);
		if(!libHandle)
		{
			printf("Library Load Failed: %s\n", libraryPath);
			exit(1);
		}
	}
	void* impl_mallocInSandbox(size_t size)
	{
		return malloc(size);
	}
	void impl_freeInSandbox(void* val)
	{
		free(val);
	}
	void* impl_pushStackArr(size_t size)
	{
		pushPopCount++;
		return malloc(size);
	}
	void impl_popStackArr(void* ptr, size_t size)
	{
		pushPopCount--;
		ENSURE(pushPopCount >= 0);
		return free(ptr);
	}

	static inline void* impl_GetUnsandboxedPointer(void* p)
	{
		return p;
	}
	
	static inline void* impl_GetSandboxedPointer(void* p)
	{
		return p;
	}

	void* impl_KeepAddressInSandboxedRange(void* p)
	{
		return p;
	}

	bool impl_isPointerInSandboxMemoryOrNull(const void* p)
	{
		return true;
	}

	bool impl_isPointerInAppMemoryOrNull(const void* p)
	{
		return true;
	}

	template<typename TRet, typename... TArgs> 
	static TRet impl_CallbackReceiver0(TArgs... params)
	{
		using fnType = TRet(*)(TArgs..., void*);
		fnType fnPtr = (fnType)(uintptr_t) dynLibNoSandbox_SavedState->allowedFunctions[0];
		return fnPtr(params..., dynLibNoSandbox_SavedState->functionState[0]);
	}

	template<typename TRet, typename... TArgs>
	void* impl_RegisterCallback(void* callback, void* state)
	{
		for(unsigned int i = 0; i < sizeof(allowedFunctions)/sizeof(void*); i++)
		{
			if(allowedFunctions[i] == nullptr)
			{
				allowedFunctions[i] = (void*)(uintptr_t) callback;
				functionState[i] = state;
				return (void*)(uintptr_t) impl_CallbackReceiver0<TRet, TArgs...>;
			}
		}

		return nullptr;
	}

	template<typename TRet, typename... TArgs>
	void impl_UnregisterCallback(TRet(*callback)(TArgs...))
	{
		for(unsigned int i = 0; i < sizeof(allowedFunctions)/sizeof(void*); i++)
		{
			if(allowedFunctions == (void*)(uintptr_t)callback)
			{
				allowedFunctions[i] = nullptr;
				functionState[i] = nullptr;
				break;
			}
		}
	}

	void* impl_LookupSymbol(const char* name)
	{
		auto ret = dlsym(libHandle, name);
		if(!ret)
		{
			printf("Symbol not found: %s. Error %s.", name, dlerror());
			exit(1);
		}
		return ret;
	}

	template <typename T, typename ... TArgs>
	return_argument<T> impl_InvokeFunction(T* fnPtr, TArgs... params)
	{
		dynLibNoSandbox_SavedState = this;
		return (*fnPtr)(params...);
	}
};

RLBoxSandbox<DynLibNoSandbox>* sandbox;

void testSizes()
{
	ENSURE(sizeof(tainted<int, DynLibNoSandbox>) == sizeof(int));
	ENSURE(sizeof(tainted<int*, DynLibNoSandbox>) == sizeof(int*));
}
void testAssignment()
{
	tainted<int, DynLibNoSandbox> a;
	a = 4;
	tainted<int, DynLibNoSandbox> b = 5;
	tainted<int, DynLibNoSandbox> c = b;
	tainted<int, DynLibNoSandbox> d;
	d = b;
	ENSURE(a.UNSAFE_Unverified() == 4);
	ENSURE(b.UNSAFE_Unverified() == 5);
	ENSURE(c.UNSAFE_Unverified() == 5);
	ENSURE(d.UNSAFE_Unverified() == 5);
}

void testBinaryOperators()
{
	tainted<int, DynLibNoSandbox> a = 3;
	tainted<int, DynLibNoSandbox> b = 3 + 4;
	tainted<int, DynLibNoSandbox> c = a + 3;
	tainted<int, DynLibNoSandbox> d = a + b;
	ENSURE(a.UNSAFE_Unverified() == 3);
	ENSURE(b.UNSAFE_Unverified() == 7);
	ENSURE(c.UNSAFE_Unverified() == 6);
	ENSURE(d.UNSAFE_Unverified() == 10);
}

void testDerefOperators()
{
	tainted<int*, DynLibNoSandbox> pa = sandbox->mallocInSandbox<int>();
	tainted_volatile<int, DynLibNoSandbox>& deref = *pa;
	tainted<int, DynLibNoSandbox> deref2 = *pa;
	deref2 = *pa;
	UNUSED(deref);
	UNUSED(deref2);
}

void testVolatileDerefOperator()
{
	tainted<int**, DynLibNoSandbox> ppa = sandbox->mallocInSandbox<int*>();
	*ppa = sandbox->mallocInSandbox<int>();
	tainted<int, DynLibNoSandbox> a = **ppa;
	UNUSED(a);
}

void testAddressOfOperators()
{
	tainted<int*, DynLibNoSandbox> pa = sandbox->mallocInSandbox<int>();
	tainted<int*, DynLibNoSandbox> pa2 = &(*pa);
	UNUSED(pa2);
}

void testAppPointer()
{
	tainted<int**, DynLibNoSandbox> ppa = sandbox->mallocInSandbox<int*>();
	int* pa = new int;
	*ppa = sandbox->app_ptr(pa);
}

void testFunctionInvocation()
{
	tainted<int, DynLibNoSandbox> a = 20;
	auto ret = sandbox_invoke_static(sandbox, simpleAddTest, a, 22);
	ENSURE(ret.UNSAFE_Unverified() == 42);

	auto ret2 = sandbox_invoke(sandbox, simpleAddTest, a, 22);
	ENSURE(ret2.UNSAFE_Unverified() == 42);
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

void testStackAndHeapArrAndStringParams()
{
	auto result1 = sandbox_invoke(sandbox, simpleStrLenTest, sandbox->stackarr("Hello"))
		.copyAndVerify([](size_t val) -> size_t { return (val <= 0 || val >= 10)? -1 : val; });
	ENSURE(result1 == 5);

	auto result2 = sandbox_invoke(sandbox, simpleStrLenTest, sandbox->heaparr("Hello"))
		.copyAndVerify([](size_t val) -> size_t { return (val <= 0 || val >= 10)? -1 : val; });
	ENSURE(result2 == 5);
}

int exampleCallback(tainted<unsigned, DynLibNoSandbox> a, tainted<const char*, DynLibNoSandbox> b, tainted<unsigned[1], DynLibNoSandbox> c)
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

void testEchoAndPointerLocations()
{
	const char* str = "Hello";

	//str is allocated in our heap, not the sandbox's heap
	ENSURE(sandbox->isPointerInAppMemoryOrNull(str));

	tainted<char*, DynLibNoSandbox> temp = sandbox->mallocInSandbox<char>(strlen(str) + 1);
	char* strInSandbox = temp.UNSAFE_Unverified();
	ENSURE(sandbox->isPointerInSandboxMemoryOrNull(strInSandbox));

	strcpy(strInSandbox, str);

	auto retStrRaw = sandbox_invoke(sandbox, simpleEchoTest, strInSandbox);
	*retStrRaw = 'g';
	*retStrRaw = 'H';
	char* retStr = retStrRaw.copyAndVerifyString(sandbox, [](char* val) { return strlen(val) < 100? RLBox_Verify_Status::SAFE : RLBox_Verify_Status::UNSAFE; }, nullptr);

	ENSURE(retStr != nullptr && sandbox->isPointerInAppMemoryOrNull(retStr));

	auto isStringSame = strcmp(str, retStr) == 0;
	ENSURE(isStringSame);

	sandbox->freeInSandbox(strInSandbox);
}

void testFloatingPoint()
{
	auto resultF = sandbox_invoke(sandbox, simpleFloatAddTest, 1.0, 2.0)
		.copyAndVerify([](double val){ return val > 0 && val < 100? val : -1.0; });
	ENSURE(resultF == 3.0);

	auto resultD = sandbox_invoke(sandbox, simpleDoubleAddTest, 1.0, 2.0)
		.copyAndVerify([](double val){ return val > 0 && val < 100? val : -1.0; });
	ENSURE(resultD == 3.0);
}

int main(int argc, char const *argv[])
{
	sandbox = RLBoxSandbox<DynLibNoSandbox>::createSandbox("", "./libtest.so");
	testSizes();
	testAssignment();
	testBinaryOperators();
	testDerefOperators();
	testVolatileDerefOperator();
	testAddressOfOperators();
	testAppPointer();
	testFunctionInvocation();
	testTwoVerificationFunctionFormats();
	testStackAndHeapArrAndStringParams();
	testCallback();
	testEchoAndPointerLocations();
	testFloatingPoint();
	return 0;
}