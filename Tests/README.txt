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

5. Zombie mode parallel compare/delete data copies
	use parallel threads to compare copies made in test 4 with originals
	verifying the handling of: compare, delete
	program will verify correctness of the copies
	verify that that the report contains well formatted and plausible data

6. Zombie mode parallel verify/delete pattern data (in directory)
	use parallel threads to verify the pattern files written in test 3
	verifying the handling of: verify, delete
	program will verify correctness of the copies
	verify that that the report contains well formatted and plausible data
	
7. Zombie mode parallel single file sequential pattern write
	using parallel threads to sequentially write individual files
	verify handling of existing file target (and once-only) data creation

8. Zombie mode parallel single file sequential pattern verify
	using parallel threads to sequentially verify individual files
	verify handling of single file pattern verification (files created in 7)

9. Zombie mode parallel single file random pattern write
	using parallel threads to do random block writes to individual files
	verify random rewrites (of files created in 7).

10. Zombie mode parallel single file random pattern verify and delete
	using parallel threads to verify random block reads 
	verify random reads (of files created in 7 and 9) and single file deletes

YET TO BE DONE

    ADDITIONAL REPORT SANITY CHECKING
    	rate ... non zero values should be 64K +/- 4K
	report intervals ... should be one second apart
	latency bucket counts should all be sub 100us

    EASY AND IMPORTANT
	induced header verification errors
		pattern data creation followed by sed corruption
	induced data verification errors
		pattern data creation followed by sed corruption
	induced data comarison errors
		pattern data creation and copy, followed by corruption of copy

    PROBABLY EASY BUT UNIMPORTANT
    	induced target directory access errors
	induced source directory access errors
	induced input file open errors
	induced file read errors
	--halt

    UNSURE HOW TO TEST, MAYBE SOMEDAY
    	--direct	... it should be obvious from the performance results
	--synchronous 	... it should be obvious form the performance results
