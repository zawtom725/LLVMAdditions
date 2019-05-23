# CS 526 Project 1 Test
# Author: Ziang Wan

# below are pathes to needed binary executable for this makefile
# This makefile is not portable. You need to modify the following 
# variables correspondingly
CC=clang
OPT=../build/bin/opt -load ../build/lib/LLVMMP1.so
LLVM-DIS=../build/bin/llvm-dis
LLI=../build/bin/lli
LLVM-AS=../build/bin/llvm-as

# fill in the name of the pass you want to test below
OPTS-BEFORE=-sccp
OPTS=-scalarrepl-ziangw2 -verify

.SILENT:

# rules to make a .bc file from a .c file
%.bc: %.c
	$(CC) -c -O0 -Xclang -disable-O0-optnone -emit-llvm $< -o $@-before.bc
	$(OPT) $(OPTS-BEFORE) $(opts) < $@-before.bc > $@
	rm $@-before.bc

# rules to generate the final optimized .bc
%-opt.bc: %.bc
	$(OPT) $(OPTS) < $< > $@

# rules to generate .ll from a .bc file
%.ll: %.bc
	$(LLVM-DIS) < $< > $@

# rules to execute a specific .ll file
%-exec: %.ll
	$(LLVM-AS) $< -o $@.bc
	$(LLI) $@.bc > $@.out
	rm $@.bc
	
clean:
	rm -f *.bc *.ll *.out

