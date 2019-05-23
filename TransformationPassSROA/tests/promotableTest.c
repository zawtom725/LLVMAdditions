#include <stdlib.h>
#include <stdio.h>

// the following code test the P1 and P2 condition of the
// isAllocaPromotable() function
// P1: The alloca is a “first-class” type, which you can approximate conservatively with
// isFPOrFPVectorTy() || isIntOrIntVectorTy() || isPtrOrPtrVectorTy.

// all the following variables are promotable

// before my pass: -sccp

struct Sub {
	int a;
};

int main(int argc, char *argv[]){
	int a = 1;

	double c = 3.0;

	int temp = 5;
	int *e = &temp;

	printf("Test: [%d] [%f] [%d]\n", a, c, *e);

	// s is not replaceable in the first iteration
	// after the mem2reg of the first iteration
	// s is then promotable
	struct Sub s;
	s.a = 7;

	struct Sub *sPtr = &s;
	printf("Sub: [%d]\n", sPtr->a);
	
	return 0;
}