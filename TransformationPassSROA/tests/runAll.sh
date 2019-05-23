run_test()
{
	NAME=$1
	make $NAME-exec
	make $NAME-opt-exec
	echo ""
	echo ""
	echo "-------------$NAME Diff Result-------------"
	diff $NAME-exec.out $NAME-opt-exec.out
	echo ""
	echo ""
	make
}

for entry in $PWD/*Test.c
do
	testName=$(basename $entry .c)
	run_test $testName
done