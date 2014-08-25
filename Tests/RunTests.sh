#!/bin/bash
#	basic functional test suite for the load generator
#

#
# parameters:	test to run
#		directory in which to run it
#
if [ -n "$1" ]
then			# run only the specified test
	MIN=$1
	MAX=$1
	allpass=0	# all tests cannot possibly pass
	shift
else			# run the default range of tests
	MIN=1
	MAX=100
	allpass=1
fi

if [ -n "$1" ]
then
	TESTDIR=$1
else
	TESTDIR="/tmp/loadgenTest.$$"
	mkdir $TESTDIR
fi

LOADGEN=../loadgen		# someday make this a parameter

# figure out what the default messages should be
nodename=`uname -n`
herald="loadgen (tag=$nodename): Yes Master?"

#
# sanity check a log
#
#	this function implements a bunch of sanity checks on each log
#
logcheck() {
	dir=$1		# working directory
	test=$2		# test number

	# confirm it contains some reports
	if ! grep "^REPORT date=" $dir/stdout.$test > $dir/reports
	then
		echo "stdout contains no valid REPORT lines"
		return 1
	fi

	# comfirm they all have the correct test tag
	if grep -v "tag=test_tag" $dir/reports > /dev/null
	then
		echo "incorrect tag in REPORTs"
		return 1
	fi

	# confirm it contains a reasonable number of reports
	#	four 16K files at 16K/sec should take 4 seconds
	#	we expect ~5 reports
	lines=`wc -l $dir/reports | cut "-d " -f1`
	lines=$((lines+0))
	if [ $lines -lt 4 ]
	then
		echo implausibly few REPORT lines: $lines
		return 1
	fi
	if [ $lines -gt 6 ]
	then
		echo implausibly many REPORT lines: $lines
		return 1
	fi

	# find the reports from the full number of threads
	if ! grep "threads=4" $dir/reports > $dir/reports4
	then
		echo output contained no threads=4 reports
		return 1
	fi

	# confirm that at least one of them shows data
	if ! grep -v -m 1 "rate=0" $dir/reports4 > $dir/reports_wdata
	then
		echo no non-zero throughput reports
		return 1
	fi

	# confirm it has a non-zero byte count too
	if grep "bytes=0" $dir/reports_wdata > /dev/null
	then
		echo non-zero throughput with zero bytecount
		return 1
	fi

	# FIX: verify date is today
	# FIX: verify time is within the last few seconds
	# FIX: verify some non-zero bucket values
	# FIX: verify plausibility of bucket values

	# confirm there were no threads=5 reports
	if grep "threads=5" $dir/reports > /dev/null
	then
		echo output contained threads=5 reports
		return 1
	fi

	# delete the scratch files
	rm -f $dir/reports $dir/reports4 $dir/reportsw_data
	return 0
}

#
# each test case may have any of the following files:
#	summary.#	one line description of this test case
#	setup.#		script to be run prior to test
#	stdin.#		input to be passed to load generator
#	stdout.#	stdout expected from load generator (modulo herald)
#	stderr.#	stderr expected from load generator
#	rc.#		exit code expected from load generator
#	verify.#	verification script
#	cleanup.#	script to be run after test finishes
#
for (( test=MIN; test <=MAX; test++ ))
do
	if [ -f summary.$test ]
	then
		echo "Test $test:" `cat summary.$test`
	else
		break
	fi

	# figure out the expected input and output file names
	stdin=$TESTDIR/stdin.$test
	stdout=$TESTDIR/stdout.$test
	stderr=$TESTDIR/stderr.$test
	expected=$TESTDIR/expected.$test
	rc=$TESTDIR/rc.$test

	# plug the test node name into the expected output
	echo $herald > $expected
	if [ -f stdout.$test ]
	then
		cat stdout.$test >> $expected
	fi

	# if there is a setup script, run it
	if [ -f setup.$test ]
	then
		sh setup.$test $test $TESTDIR
	fi

	# see whether or not we need an input file
	if [ -f stdin.$test ]
	then
		# plug the test directory name into the test input
		sed -e "s#TESTDIR#$TESTDIR#g" < stdin.$test > $stdin
		$LOADGEN > $stdout 2> $stderr < $stdin
		echo $? > $rc
	else
		# run the test with no input
		$LOADGEN > $stdout 2> $stderr < /dev/null
		echo $? > $rc
	fi

	errors=0
	# see if the results were what we expected
	if ! cmp rc.$test $rc > /dev/null
	then
		echo test $test: expected rc=`cat rc.$test` got `cat $rc`
		errors=1
	fi

	# if there is a verifier, run it
	#	(it has the opportunity to tweak stdout and stderr)
	if [ -f verify.$test ]
	then
		if ! sh verify.$test $test $TESTDIR
		then
			# verifier will generate its own error messages
		 	errors=1
		fi
	fi

	# if we expected specific stdout, verify it
	if [ -f stdout.$test ]
	then
		if ! diff $expected $stdout
		then
			echo test $test: stdout mismatch
			errors=1
		fi
	else	# if we don't do a general log sanity check
		err=`logcheck $TESTDIR $test`
		if [ -n "$err" ]
		then
			echo test $test: $err
			errors=1
		fi
	fi

	# if we expected specific stderr, verify it
	if [ -f stderr.$test ]
	then
		# sanitize (variable) references to the test directory
		mv $stderr $TESTDIR/scratch
		sed -e "s#$TESTDIR#TESTDIR#g" < $TESTDIR/scratch > $stderr
		rm $TESTDIR/scratch
			
		# then compare it with the golden output
		if ! diff stderr.$test $stderr
		then
			echo test $test: stderr mismatch
			errors=1
		fi
	fi

	# if we have directory contents to verify, verify them
	if [ -f dirs.$test ]
	then
		DIRS=`cat dirs.$test`
		( cd $TESTDIR; ls -Rs $DIRS > ls.$test )
		if ! diff ls.$test $TESTDIR/ls.$test
		then
			echo test $test: directory listing mismatch
			errors=1
		fi
	fi

	if [ $errors = 0 ]
	then
		echo " ... passed"
		# if there is a cleanup script run it
		if [ -f cleanup.$test ]
		then
			sh cleanup.$test $test $TESTDIR
		fi
	else
		allpass=0
		break
	fi
done

# if we succeded, we can delete the test directory
if [ $allpass = 1 ]
then
	echo All tests passed.
	rm -rf $TESTDIR
	exit 0
else
	echo Test results available for inspsection in $TESTDIR
	exit 255
fi
