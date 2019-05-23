#include <stdlib.h>
#include <stdio.h>

// in this test, I tested a variety of getelementptr users for a
// struct alloca instruction

// the test corresponds to the P1 condition:
// 1. It is of the form: getelementptr ptr, 0, constant[, ... constant].
// 2. The result of the getelementptr is only used in instructions of type U1 or U2,
// or as the pointer argument of a load or store instruction
// a helper function used in canBeEliminatedStructAlloca()

// the test can also checks for the correctness of handling P1 type insts when
// eliminating a struct allocas

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

	// the getelementptr result can be used in a load inst as the pointer arg
	int a = t1.arr[1];

	printf("Test t1: [%d] [%d] [%d] [%d]\n", t1.one, t1.two, t1.arr[0], a);

	return 0;
}