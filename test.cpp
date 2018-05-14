#include "rlbox.h"
#include <stdio.h>

using namespace rlbox;

#define ensure(a) if(!(a)) { printf("%s check failed\n", #a); exit(1); }

class NoOpSandbox
{
public:
	void createSandbox(char* sandboxRuntimePath, char* libraryPath)
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

	// template<typename T>
	// void* registerCallback(std::function<T> callback);

	static void* getUnsandboxedPointer(void* p)
	{
		return p;
	}

	void unregisterCallback(void* callback)
	{

	}
};
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
	tainted<int*, NoOpSandbox> pa;
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

int main(int argc, char const *argv[])
{
	testAssignment();
	testBinaryOperators();
	return 0;
}