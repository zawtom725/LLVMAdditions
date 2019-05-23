#include <stdlib.h>
#include <stdio.h>

// one layer struct. optimizable within one iteration
// there is only one struct alloca
// every use of such alloca is getelementptr with 3 args
// the resulting new allocas are all promotable

// before my pass:

struct Test{
	int one;
	int two;
};

int main(int argc, char *argv[]){
	struct Test t1;
	t1.one = 1;
	t1.two = 2;

	printf("Test: [%d] [%d]\n", t1.one, t1.two);
	return 0;
}