#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

// in this test, I tested a variety of getelementptr users for a
// struct alloca instruction

// the test corresponds to the P1 condition:
// 1. It is of the form: getelementptr ptr, 0, constant[, ... constant].
// 2. The result of the getelementptr is only used in instructions of type U1 or U2,
// or as the pointer argument of a load or store instruction
// a helper function used in canBeEliminatedStructAlloca()

// the following struct becomes not eliminateable

// before my pass: -sccp -mem2reg

struct Test{
	int one;
	int two;
	int arr[2];
};

int main(int argc, char *argv[]){
	// t1 should be elminatable
	struct Test t1;
	t1.one = 1;
	t1.two = 2;
	t1.arr[0] = 3;
	t1.arr[1] = 4;

	int *b = &t1.one;
	int *c = &t1.arr[1];

	bool whether = b > c;

	printf("Test t1: [%d] [%d]\n", *b, *c);

	return 0;
}