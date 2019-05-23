#include <stdlib.h>
#include <stdio.h>

// in this test, I test the CmpInst users of the struct allcoas
// essentially P2 condition:
// In a ’eq’ or ’ne’ comparison instruction,
// where the other operand is the NULL pointer value

// the following is not eliminatable

// before my pass: -sccp

struct Test {
	int one;
	int two;
};

int main(int argc, char *argv[]){
	// t1 is eliminatable
	struct Test t1;
	t1.one = 1;
	t1.two = 2;

	struct Test *t1nil = NULL; // 0000

	// > is not eliminatable P2

	if (&t1 > t1nil) {
		printf("Test t1: [%d] [%d]\n", t1.one, t1.two);
	} else if (&t1 == t1nil) {
		printf("Not reachable\n");
	}
}