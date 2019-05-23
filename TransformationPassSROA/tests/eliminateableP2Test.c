#include <stdlib.h>
#include <stdio.h>

// in this test, I test the CmpInst users of the struct allcoas
// essentially P2 condition:
// In a ’eq’ or ’ne’ comparison instruction,
// where the other operand is the NULL pointer value

// the test also checks for the correct handling of P2 inst when
// eliminating the old struct alloca

// before my pass: -sccp

struct Test {
	int one;
	int two;
};

struct Test2 {
	struct Test *t1;
};

int main(int argc, char *argv[]){
	// t1 is eliminatable
	struct Test t1;
	t1.one = 1;
	t1.two = 2;

	struct Test *t1nil = NULL;
	if (&t1 != t1nil) {
		printf("Test t1: [%d] [%d]\n", t1.one, t1.two);
	} else if (&t1 == t1nil) {
		printf("Not reachable\n");
	}

	// t2 is not eliminatable in the first iteration
	// once t3 is replaced, t2 is eliminatable
	struct Test t2;
	t2.one = 3;
	t2.two = 4;

	// t3 is eliminateable
	struct Test2 t3;
	t3.t1 = &t2;
	if (t3.t1 != t1nil) {
		printf("Test t3: [%d] [%d]\n", t3.t1->one, t3.t1->two);
	} else {
		printf("Not reachable\n");
	}

	
	return 0;
}