#include "rlbox.h"
#include <stdio.h>

int main(int argc, char const *argv[])
{
	tainted<int> a;
	tainted<int*> pa;
	printf("Hello world: %d\n", a.unsafeUnverified());
	return 0;
}