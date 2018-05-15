#include <stdio.h>
#include <type_traits>
#include <dlfcn.h>

#include "libtest.h"
#include "rlbox.h"

using namespace rlbox;

#define ensure(a) if(!(a)) { printf("%s check failed\n", #a); exit(1); }

class DynLibNoSandbox
{
private:
	void* allowedFunctions[32];
	void* libHandle = nullptr;

public:
	void initMemoryIsolation(const char* sandboxRuntimePath, const char* libraryPath)
	{
		libHandle = dlopen(libraryPath, RTLD_LAZY);
		if(!libHandle)
		{
			printf("Library Load Failed: %s\n", libraryPath);
			exit(1);
		}
	}
	void* mallocInSandbox(size_t size)
	{
		return malloc(size);
	}
	void freeInSandbox(void* val)
	{
		free(val);
	}

	static inline void* getUnsandboxedPointer(void* p)
	{
		return p;
	}
	
	static inline void* getSandboxedPointer(void* p)
	{
		return p;
	}

	void* keepAddressInSandboxedRange(void* p)
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
	ensure(sizeof(tainted<int, DynLibNoSandbox>) == sizeof(int));
	ensure(sizeof(tainted<int*, DynLibNoSandbox>) == sizeof(int*));
}
void testAssignment()
{
	tainted<int, DynLibNoSandbox> a;
	a = 4;
	tainted<int, DynLibNoSandbox> b = 5;
	tainted<int, DynLibNoSandbox> c = b;
	tainted<int, DynLibNoSandbox> d;
	d = b;
	ensure(a.UNSAFE_Unverified() == 4);
	ensure(b.UNSAFE_Unverified() == 5);
	ensure(c.UNSAFE_Unverified() == 5);
	ensure(d.UNSAFE_Unverified() == 5);
}

void testBinaryOperators()
{
	tainted<int, DynLibNoSandbox> a = 3;
	tainted<int, DynLibNoSandbox> b = 3 + 4;
	tainted<int, DynLibNoSandbox> c = a + 3;
	tainted<int, DynLibNoSandbox> d = a + b;
	ensure(a.UNSAFE_Unverified() == 3);
	ensure(b.UNSAFE_Unverified() == 7);
	ensure(c.UNSAFE_Unverified() == 6);
	ensure(d.UNSAFE_Unverified() == 10);
}

void testDerefOperators()
{
	tainted<int*, DynLibNoSandbox> pa = sandbox->newInSandbox<int>();
	tainted_volatile<int, DynLibNoSandbox>& deref = *pa;
	tainted<int, DynLibNoSandbox> deref2 = *pa;
	deref2 = *pa;
	(void)(deref);
	(void)(deref2);
}

void testVolatileDerefOperator()
{
	tainted<int**, DynLibNoSandbox> ppa = sandbox->newInSandbox<int*>();
	*ppa = sandbox->newInSandbox<int>();
	tainted<int, DynLibNoSandbox> a = **ppa;
	(void)(a);
}

void testAddressOfOperators()
{
	tainted<int*, DynLibNoSandbox> pa = sandbox->newInSandbox<int>();
	tainted<int*, DynLibNoSandbox> pa2 = &(*pa);
	(void)(pa2);
}

void testAppPointer()
{
	tainted<int**, DynLibNoSandbox> ppa = sandbox->newInSandbox<int*>();
	int* pa = new int;
	*ppa = app_ptr(pa);
}

void testFunctionInvocation()
{
	auto ret = sandbox_invoke_static(sandbox, simpleFunction);
	ensure(ret.UNSAFE_Unverified() == 42);

	auto ret2 = sandbox_invoke(sandbox, simpleFunction);
	ensure(ret2.UNSAFE_Unverified() == 42);
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
	return 0;
}