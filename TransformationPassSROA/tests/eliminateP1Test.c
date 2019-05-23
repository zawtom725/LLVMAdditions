#include <stdlib.h>
#include <stdio.h>

// test the special handling of P1 type getelementptr
// before my pass: -sccp -instcombine

struct RT {
	char A;
	int B[10][20];
	char C;
};

struct ST {
	int X;
	double Y;
	struct RT Z;
};

int main(int argc, char *argv[]){
	struct ST t1;
	t1.X = 1;
	t1.Y = 0.5;
	t1.Z.A = 'a';
	t1.Z.B[0][5] = 2;
	t1.Z.C = 'b';

	printf("t1: [%d] [%f] [%c] [%d] [%c]\n", t1.X, t1.Y, t1.Z.A, t1.Z.B[0][5], t1.Z.C);
	return 0;
}