#include "rlbox.h"
#include <stdio.h>

using namespace rlbox;

#define ensure(a) if(!(a)) { printf("%s check failed\n", #a); exit(1); }

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
	tainted<int> a;
	a = 4;
	tainted<int> b = 5;
	tainted<int> c = b;
	tainted<int> d;
	d = b;
	ensure(a.UNSAFE_Unverified() == 4);
	ensure(b.UNSAFE_Unverified() == 5);
	ensure(c.UNSAFE_Unverified() == 5);
	ensure(d.UNSAFE_Unverified() == 5);
}

void testBinaryOperators()
{
	tainted<int> a = 3;
	tainted<int> b = 3 + 4;
	tainted<int> c = a + 3;
	tainted<int> d = a + b;
	ensure(a.UNSAFE_Unverified() == 3);
	ensure(b.UNSAFE_Unverified() == 7);
	ensure(c.UNSAFE_Unverified() == 6);
	ensure(d.UNSAFE_Unverified() == 10);
}

void testDerefOperators()
{
	int a = 3;
	tainted<int*> pa;
	tainted_volatile<int>& deref = *pa;
	tainted<int> deref2 = *pa;
}

int main(int argc, char const *argv[])
{
	testAssignment();
	testBinaryOperators();
	return 0;
}