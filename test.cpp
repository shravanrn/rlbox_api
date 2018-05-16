#include <stdio.h>
#include <type_traits>
#include <dlfcn.h>

#include "libtest.h"
#include "rlbox.h"

using namespace rlbox;

#define ENSURE(a) if(!(a)) { printf("%s check failed\n", #a); exit(1); }
#define UNUSED(a) (void)(a)

class DynLibNoSandbox
{
private:
	void* allowedFunctions[32];
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

	template<typename T, typename std::enable_if<std::is_function<T>::value || std::is_member_function_pointer<T*>::value>::type* = nullptr>
	void* registerCallback(T* callback)
	{
		for(unsigned int i = 0; i < sizeof(allowedFunctions)/sizeof(void*); i++)
		{
			if(allowedFunctions == nullptr)
			{
				allowedFunctions[i] = (void*)(uintptr_t) callback;
				return callback;
			}
		}

		return nullptr;
	}

	void unregisterCallback(void* callback)
	{
		for(unsigned int i = 0; i < sizeof(allowedFunctions)/sizeof(void*); i++)
		{
			if(allowedFunctions == callback)
			{
				allowedFunctions[i] = nullptr;
				break;
			}
		}
	}

	void* lookupSymbol(const char* name)
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
	return_argument<T> invokeFunction(T* fnPtr, TArgs... params)
	{
		return (*fnPtr)(params...);
	}
};

RLBoxSandbox<DynLibNoSandbox>* sandbox;

template<typename T>
void printType()
{
	#ifdef _MSC_VER
 		printf("%s\n", __FUNCSIG__);
 	#else
 		printf("%s\n", __PRETTY_FUNCTION__);
 	#endif
}

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

void testStackArrAndStringParams()
{
	auto result = sandbox_invoke(sandbox, simpleStrLenTest, sandbox->stackarr("Hello"))
		.copyAndVerify([](size_t val) -> size_t { return (val <= 0 || val >= 10)? -1 : val; });
	ENSURE(result == 5);
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
	testStackArrAndStringParams();
	return 0;
}