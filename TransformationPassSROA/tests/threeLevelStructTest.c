#include <stdlib.h>
#include <stdio.h>

// the following tests struct within struct within struct
// it will take my pass at least three iterations to handle

// before my pass: -sccp

struct S1 {
	char a;
	char b;
};

struct S2 {
	int x;
	int y;
	struct S1 *s1;
};

struct S3 {
	double n;
	double m;
	struct S2 *s2;
};

int main(int argc, char *argv[]){
	// s1 is not eliminateable until s2 and s3 is replaced
	struct S1 s1;
	s1.a = 'a';
	s1.b = 'b';

	// s2 is not eliminateable until s3 is replaced
	struct S2 s2;
	s2.x = 1;
	s2.y = 2;
	s2.s1 = &s1;

	// s3 can be immediately replaced
	struct S3 s3;
	s3.n = 0.5;
	s3.m = 1.0;
	s3.s2 = &s2;

	printf("S2: [%c] [%c] [%d] [%d] [%f] [%f]\n", s3.s2->s1->a, s3.s2->s1->b,
		s3.s2->x, s3.s2->y, s3.n, s3.m);
}