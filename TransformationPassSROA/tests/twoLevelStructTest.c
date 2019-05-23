#include <stdlib.h>
#include <stdio.h>

// the following tests struct within struct
// it will take my pass at least two iterations to handle

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

int main(int argc, char *argv[]){
	// s1 is not eliminateable until S2 is replaced
	struct S1 s1;
	s1.a = 'a';
	s1.b = 'b';

	// s2 can be immediately replaced
	struct S2 s2;
	s2.x = 1;
	s2.y = 2;
	s2.s1 = &s1;

	printf("S2: [%c] [%c] [%d] [%d]\n", s2.s1->a, s2.s1->b, s2.x, s2.y);
}