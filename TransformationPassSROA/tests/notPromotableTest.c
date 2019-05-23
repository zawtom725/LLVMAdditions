#include <stdlib.h>
#include <stdio.h>

// the following code test the P1 and P2 condition of the
// isAllocaPromotable() function
// P1: The alloca is a “first-class” type, which you can approximate conservatively with
// isFPOrFPVectorTy() || isIntOrIntVectorTy() || isPtrOrPtrVectorTy.

// all the following variables are not promotable

// before my pass: -sccp

struct Sub {
	int a;
};

int main(int argc, char *argv[]){
	int b[2];
	b[1] = 2;

	double d[5];
	d[4] = 4.0;

	int temp2 = 6;
	int *f[3];
	f[2] = &temp2;

	printf("Test: [%d] [%f] [%d]\n", b[1], d[4], *f[2]);

	// s is not replaceable in the first iteration
	// after the mem2reg of the first iteration
	// s is then promotable
	struct Sub s;
	s.a = 7;

	struct Sub *sPtr = &s;
	printf("Sub: [%d]\n", sPtr->a);
	
	return 0;
}