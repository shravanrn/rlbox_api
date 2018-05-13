#include "rlbox.h"
#include <stdio.h>

using namespace rlbox;

#define ensure(a) if(!(a)) { printf("%s check failed\n", #a); exit(1); }

template<typename T>
void printFunc()
{
 	printf("%s\n", __PRETTY_FUNCTION__);
}

void testAssignment()
{
	tainted<int> a;	
	a = 4;
	tainted<int> b = 4;	
	tainted<int> c = b;
	ensure(a.unsafeUnverified() == 4);
	ensure(b.unsafeUnverified() == 4);
	ensure(c.unsafeUnverified() == 4);
}

void testOperators()
{
	tainted<int> a = 3;
	tainted<int> b = 3 + 4;
	tainted<int> c = a + 3;
	tainted<int> d = a + b;
	ensure(a.unsafeUnverified() == 3);
	ensure(b.unsafeUnverified() == 7);
	ensure(c.unsafeUnverified() == 6);
	ensure(d.unsafeUnverified() == 10);
}


int main(int argc, char const *argv[])
{
	testAssignment();
	testOperators();

	(void)(my_is_same_v<int, int>);
	return 0;
}