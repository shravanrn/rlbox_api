#include "rlbox.h"
#include <stdio.h>
#include <type_traits>
using namespace rlbox;

#define ensure(a) if(!(a)) { printf("%s check failed\n", #a); exit(1); }

class NoOpSandbox
{
private:
	void* allowedFunctions[32];

public:
	void initMemoryIsolation(const char* sandboxRuntimePath, const char* libraryPath)
	{

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
};

RLBoxSandbox<NoOpSandbox>* sandbox;

template<typename T>
void printType()
{
	#ifdef _MSC_VER
 		printf("%s\n", __FUNCSIG__);
 	#else
 		printf("%s\n", __PRETTY_FUNCTION__);
 	#endif
}

void testAssignment()
{
	tainted<int, NoOpSandbox> a;
	a = 4;
	tainted<int, NoOpSandbox> b = 5;
	tainted<int, NoOpSandbox> c = b;
	tainted<int, NoOpSandbox> d;
	d = b;
	ensure(a.UNSAFE_Unverified() == 4);
	ensure(b.UNSAFE_Unverified() == 5);
	ensure(c.UNSAFE_Unverified() == 5);
	ensure(d.UNSAFE_Unverified() == 5);
}

void testBinaryOperators()
{
	tainted<int, NoOpSandbox> a = 3;
	tainted<int, NoOpSandbox> b = 3 + 4;
	tainted<int, NoOpSandbox> c = a + 3;
	tainted<int, NoOpSandbox> d = a + b;
	ensure(a.UNSAFE_Unverified() == 3);
	ensure(b.UNSAFE_Unverified() == 7);
	ensure(c.UNSAFE_Unverified() == 6);
	ensure(d.UNSAFE_Unverified() == 10);
}

void testDerefOperators()
{
	tainted<int*, NoOpSandbox> pa = sandbox->newInSandbox<int>();
	tainted_volatile<int, NoOpSandbox>& deref = *pa;
	tainted<int, NoOpSandbox> deref2 = *pa;
	(void)(deref);
	(void)(deref2);
}

void testVolatileDerefOperator()
{
	tainted<int**, NoOpSandbox> ppa;
	tainted<int, NoOpSandbox> a = **ppa;
	(void)(a);
}

void testAddressOfOperators()
{
	tainted<int*, NoOpSandbox> pa;
	tainted<int*, NoOpSandbox> pa2 = &(*pa);
	(void)(pa2);
}

void testAppPointer()
{
	tainted<int**, NoOpSandbox> ppa = sandbox->newInSandbox<int*>();
	int* pa = new int;
	*ppa = app_ptr(pa);
}

int main(int argc, char const *argv[])
{
	sandbox = RLBoxSandbox<NoOpSandbox>::createSandbox("", "");
	testAssignment();
	testBinaryOperators();
	return 0;
}