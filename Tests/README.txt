DESIGN OF TEST CASES
	each test case has several possible files (where # is the testcase number)
		setup.#		... if present, run before testcase
		stdin.# 	... expected input (with TESTDIR substitution)
		stdout.#	... expected output
				    if none present, a standard sanity test is run
		stderr.#	... expected stderr (with TESTDIR substitution)
		rc.#		... expected rc
		dirs.#		... directories to be listed
		ls.#		... expected ls output
		verify.#	... script for additional verification
		cleanup.#	... if present, run after testcase
		



EXISTING TEST CASES:

1. Zombie mode startup and exit
	verify that the program starts up, prompts for input
	verify the exit command

2. Zombie mode parameter processing
	verify correct parsing of all major parameters
	verify that it dies if the target directory does not exist

3. Zombie mode parallel thread pattern file creation (in directory)
	use parallel threads to create pattern files in multiple directories
	verifying the handling of: target=, threads=, maxfiles=, length=, rate=, update=
	verify disconnect command
	verify that that the report contains well formatted and plausible data

4. Zombie mode parallel thread file copy
	use parallel threads to copy a directory tree
	verifying the handling of: source=, target=, threads=, rate=, update=
	verify that that the report contains well formatted and plausible data
	verify that that the report contains well formatted and plausible data

5. Zombie mode parallel verify/delete data copies
	use parallel threads to verify the copies made in test 4
	verifying the handling of: verify, delete
	program will verify correctness of the copies
	verify that that the report contains well formatted and plausible data

6. Zombie mode parallel verify/delete pattern data (in directory)
	use parallel threads to verify the pattern files written in test 3
	verifying the handling of: verify, delete
	program will verify correctness of the copies
	verify that that the report contains well formatted and plausible data
	

YET TO BE DONE

    EASY AND IMPORTANT
	header verification errors
		pattern data creation followed by sed corruption
	data verification errors
		pattern data creation followed by sed corruption
	data comarison errors
		pattern data creation and copy, followed by corruption of copy

    HARD BUT IMPORTANT
    	correctness of the reported latencies

    PROBABLY EASY BUT UNIMPORTANT
	--halt		... obvious from the output
	--onceonly	... obvious from the output

    UNSURE HOW TO TEST, MAYBE SOMEDAY
	--rewrite	... it should be obvious from the output
    	--direct	... it should be obvious from the performance results
	--synchronous 	... it should be obvious form the performance results
	--random	... it should be obvious from the performance results
    	target directory access
	source directory access
	input file open errors
	file read errors