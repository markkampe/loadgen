/**
 * module:	main.cpp
 *<P>
 * purpose:
 *	argument handling, initialization, and kick-off
 *
 *	Note that if no command line arguments are passed,
 *	it will read them (in the same format) in the first
 *	line from stdin.
 *
 *<P>
 * argument handling functions, driven by "myargs"
 * -	getArgOpts	generate a getopt_long argspec table
 * -	getArgSpec	generate a getopt_long argument type
 * -	switch_info	print out a list of switches
 * -	usage_info	to print out a usage message
 * -	debugOpts	convert string into debug option bits
 * -	getDebugOopts	convert option bits into string form
 *<P>
 * This is almost entirely boiler-plate, driven by:
 * - myargs: table that defines the supported arguments
 * - dbgopts: table that defines the supported debug options
 *<P>
 * The only interesting code in this module is the case 
 * statement that deals with arguments and the few lines
 * of initialization and kick-off that follow it.
 */

#include <string>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "loadgen.h"
#include "debug.h"
#include "perfstats.h"

debugOptions loadgen_debug = D_OPTS + D_CMDS;

/**
 * supported arguments
 *<p>
 * NOTE:I use my own table (rather than a getopt option
 *	table because I want to know the type/meaning of the
 *	arguments to each switch.  I generate the tables and
 *	strings required for getopt from here.
 *<p>
 * NOTE:while the order is of no processing importance, this
 *	is the order in which they will print out for usage
 *	messages, and so should be chosen with a little care.
 */
static struct myargs {
	const char *fullname;	/* name when fully spelled out	*/
	char letter;		/* single letter switch		*/
	const char *argtype;	/* type/meaning of argument	*/
} myargs[] = {
	/* fullname	letter		argtype		*/
	{"tag",		'T',		"output tag" },
	{"target",	'o',		"target directory (for writes/comparisons)" },
	{"source",	'i',		"source directory (for copies)" },
	{"bsize",	'b',		"write block size" },
	{"length",	'l',		"file length" },
	{"data",	'Z',		"bytes to read/write" },
	{"maxfiles",	'M',		"maximum number of files to create" },
	{"threads",	't',		"initial number of up-load threads" },
	{"update",	'u',		"update interval" },
	{"rate",	'R',		"target bandwidth" },
	{"direct",	'A',		"alignment" },
	{"depth",	'a',		"concurrent ops" },
	{"random",	'z',		"block size" },
	{"read",	'r',		0	 },
	{"verify",	'v',		0	 },
	{"rewrite",	'w',		0	 },
	{"delete",	'd',		0	 },
	{"sync",	's',		0	 },
	{"halt",	'H',		0	 },
	{"simulate",	'S',		0 	 },
	{"onceonly",	'1',		0	 },
	{"help",	'?',		0	 },
	{"debug",	'D',		"options" },
	{0,		0,		0   }
};

static const char herald[] = "Yes Master?";
static const char wilco[]  = "Yes Master!";
static const char rip[]    = "Yes Master.";
static const char argh[]   = "Arg Master.";

static void usage_info( const char * );
static char *getArgSpec( struct myargs * );
static struct option *getArgOpts( struct myargs * );
static char **getTargets( char * );
static const char **read_args();

// default wire speeds
#define	gigE	(1024 * 1024 * 1024 / 10)
#define tenGig	(1024 * 1024 * 1024)

// general control globals
bool loadgen_read = false;	///< read back existing files
bool loadgen_verify = false;	///< verify generated directories/files
bool loadgen_delete = false;	///< delete files/dirs after verification
bool loadgen_rewrite = false;	///< rewrite already existing files
bool loadgen_shutdown = false;	///< shut down the load generation
bool loadgen_simulate = false;	///< only simulate load generation
bool loadgen_halt = false;	///< halt on error
bool loadgen_sync = false;	///< synchronous writes
bool loadgen_zombie = false;	///< under remote control
bool loadgen_once = false;	///< only one directory per thread
int  loadgen_bsize = 0;		///< read/write block size
long long loadgen_rate = 0;	///< target write/read bandwidth
long long loadgen_fsize = 0;	///< size of each created file
long long loadgen_data = 0;	///< how much data to read or write
int loadgen_update = 5;		///< statistics update interval in seconds
int loadgen_maxfiles = 0;	///< maximum number of files to create
int  loadgen_direct = 0;	///< direct buffer alignment
int  loadgen_rand_blk = 0;	///< random access r/w block size
int  loadgen_depth = 0;		///< number of concurrent I/O operations
const char *loadgen_tag = 0;	///< tag for this zombie
const char *loadgen_problem = 0;	///< the problem that shut us down

/**
 * SIGHUP handler ... shut down the load generation
 */
void hup( int sig ) {
	(void) sig;

	loadgen_shutdown = true;
}

/**
 * SIGTERM handler ... shut down the load generation
 */
void term( int sig ) {
	(void) sig;

	loadgen_shutdown = true;
}

/**
 * SIGINT handler ... shut down the load generation
 */
void intr( int sig ) {
	(void) sig;

	loadgen_shutdown = true;
}

/**
 * routine:	main ... argument handling
 * -	parse command line parameters
 * -	register my hup and term handlers
 * -	loop, calling my main loop handler
 */
int
main( int argc, const char **argv) {

	// default generation parameters
	char  	   **tgts = 0;		// directories where we create/verify data
	const char  *src = 0;		// directory from which we copy data
	int         threads = 1;	// number of load generation threads
	int 	    targets = 0;	// number of specified targets

	// figure out our system name
	{	static struct utsname buf;
		if (uname( &buf ) == 0)
			loadgen_tag = buf.nodename;
	}

	// convert my command line options table into longopts
	struct option *op = getArgOpts( myargs );
	char *ap = getArgSpec( myargs );
	int opt_index = 0;

	// if there were no command line arguments get them from stdin
	if (argc < 2) {
		argv = read_args();
		if (argv == 0 || argv[1] == 0) {
			fprintf(stdout, "%s No arguments!\n", argh );
			fflush(stdout);
			exit( -1 );
		} else if (argv[1][0] == 'x') {
			fprintf(stdout, "%s\n", rip );
			fflush(stdout);
			exit( 0 );
		}
		
		for( argc = 0; argv[argc]; argc++ );
		threads=0;		// we probably want a delayed start
		loadgen_zombie = true;	// we need to say "yes master"
	}
	
	// process the command line arguments
	while(1) {
		int c = getopt_long( argc, (char * const *) argv, ap, op, &opt_index );
		if (c == -1)
			break;

		switch (c) {
		    case 'v':
		    	loadgen_verify = true;
		    case 'r':
			loadgen_read = true;
			continue;

		    case 'd':
		    	loadgen_delete = true;
			continue;

		    case 'o':
			tgts = getTargets( optarg );
			for( targets = 0; tgts && tgts[targets]; targets++ );
			continue;

		    case 'i':
			src = optarg;
			continue;

		    case 'A':
			loadgen_direct = (int) getSizeSpec( optarg );
			continue;

		    case 'u':
			loadgen_update = atoi( optarg );
			continue;
			
		    case 't':
			threads = atoi( optarg );
			continue;

		    case 'M':
			loadgen_maxfiles = atoi( optarg );
			continue;

		    case 'T':
			loadgen_tag = optarg;
			continue;

		    case 'H':
			loadgen_halt = true;
			continue;

		    case 'l':
			loadgen_fsize = getSizeSpec( optarg );
			continue;

		    case 'Z':
		    	loadgen_data = getSizeSpec( optarg );
			continue;

		    case 'b':
			loadgen_bsize = (int) getSizeSpec( optarg );
			continue;

		    case 'a':
		    	loadgen_depth = atoi( optarg );
			continue;

		    case 'D':
		    	loadgen_debug = debugOpts(optarg);
			continue;

		    case 'w':
		    	loadgen_rewrite = true;
			continue;

		    case 's':
		    	loadgen_sync = true;
			continue;

		    case 'S':
		    	loadgen_simulate = true;
			continue;

		    case '1':
			loadgen_once = true;
			continue;

		    case 'z':
		    	loadgen_rand_blk = getSizeSpec(optarg);
			continue;

		    case 'R':
		    	loadgen_rate = getSizeSpec(optarg);
			continue;

		   case '?':
			usage_info( argv[0] );
			exit( 0 );
		}
	}

	// we're through with the synthesized longopt options
	free( op );

	// make sure we know (at least) where to create our data
	//	(having a default would surely be the wrong thing)
	if (tgts == 0 || targets == 0) {
		loadgen_problem = "No target specified";
		if (loadgen_zombie)
			fprintf(stdout, "%s %s!\n", argh, loadgen_problem);
		fprintf(stderr, " %s\n", loadgen_problem);
		exit( -1 );
	}


	// define the latency reporting buckets (micro-seconds)
	long limits[] = { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512,
			1000, 2000, 4000, 8000, 16000, 32000, 
			64000, 128000, 256000, 512000, -1 };
	perfstats::setlimits( limits );

	// see if we are supposed to display our options
	if (loadgen_debug & D_OPTS) {
		fprintf(stderr, "# Options:\n");
		fprintf(stderr, "#   tag      = %s\n", loadgen_tag );
		fprintf(stderr, "#   target(s)= ");
		for( int i = 0; i < targets; i++ ) {
			fprintf(stderr, i == 0 ? "%s" : ",%s", tgts[i] );
		}
		fprintf(stderr, "\n");

		if (src)
			fprintf(stderr, "#   source   = %s\n", src );
		else if (loadgen_fsize)
			fprintf(stderr, "#   length   = %lld bytes\n", loadgen_fsize );
		else
			fprintf(stderr, "#   length   = random\n");

		if (loadgen_data)
			fprintf(stderr, "#   data     = %lld bytes\n", loadgen_data );
		if (loadgen_bsize)
			fprintf(stderr, "#   bsize    = %d bytes\n", loadgen_bsize );
		else
			fprintf(stderr, "#   bsize    = random\n");

		fprintf(stderr, "#   maxfiles = %d\n", loadgen_maxfiles );
		fprintf(stderr, "#   threads  = %d\n", threads );
		fprintf(stderr, "#   read     = %s\n", loadgen_read ? "true" : "false" );
		fprintf(stderr, "#   rewrite  = %s\n", loadgen_rewrite ? "true" : "false" );
		fprintf(stderr, "#   verify   = %s\n", loadgen_verify ? "true" : "false" );
		if (loadgen_rand_blk)
			fprintf(stderr, "#   random   = %d\n", loadgen_rand_blk );
		fprintf(stderr, "#   direct   = %d\n", loadgen_direct );
		if (loadgen_depth)
			fprintf(stderr, "#   depth    = %d\n", loadgen_depth );
		fprintf(stderr, "#   delete   = %s\n", loadgen_delete ? "true" : "false" );
		fprintf(stderr, "#   sync     = %s\n", loadgen_sync ? "true" : "false" );
		if (loadgen_rate > 0)
			fprintf(stderr, "#   rate     = %lld bytes/sec\n", loadgen_rate );
		fprintf(stderr, "#   onceonly = %s\n", loadgen_once ? "true" : "false" );
		fprintf(stderr, "#   halt     = %s\n", loadgen_halt ? "true" : "false" );
		fprintf(stderr, "#   update   = %d\n", loadgen_update );
		fprintf(stderr, "#   simulate = %s\n", loadgen_simulate ? "true" : "false" );
		fprintf(stderr, "#   num_buckets = %d\n", perfstats::num_buckets );
		fprintf(stderr, "#   buckets = (" );
		for( int i = 0; limits[i] > 0; i++ )
			fprintf(stderr, "<=%luus,", limits[i]);
		fprintf( stderr, ">)\n");
		fprintf(stderr, "#   debug    = %s\n", getDebugOpts(loadgen_debug));
		fprintf(stderr, "#\n");
		fflush( stderr );
	}

	// register hup, interrupt and termination handlers
	signal( SIGINT, &intr );
	signal( SIGHUP, &hup );
	signal( SIGTERM, &term );

	if (loadgen_zombie) {
		fprintf(stdout, "%s\n", wilco );
		fflush( stdout );
	} else if (threads < 1) {
		fprintf(stderr, "threads = 0 and no command input\n");
		exit( -1 );
	}

	// kick off the actual load generation
	umask(0);
	int ret = 0;
	if (loadgen_read) {
		if (targets == 1 && checkdir( tgts[0], false) == 0)
			ret = readData_d( src, tgts[0], threads );
		else
			ret = readData_l( tgts );
	} else {
		if (src) {	// copy
			ret = copyData( src, tgts[0], threads );
		} else {	// create
			if (targets == 1)
				ret = createData_d( tgts[0], threads );
			else
				ret = createData_l( tgts );
		}
	}

	if (loadgen_zombie) {
		if (ret == 0)
			fprintf(stdout, "%s\n", rip );
		else if (loadgen_problem)
			fprintf(stdout, "%s %s\n", argh, loadgen_problem );
		else
			fprintf(stdout, "%s Cause unknown!\n", argh );
		fflush( stdout );
	}
	exit( ret );
}

/**
 * parse a size specification (which can end in K/M/G) and
 * return the resulting number.
 */
long long 
getSizeSpec( const char *string ) {
	long long num = 0ULL;
	char *suffix;

	num = strtoull( string, &suffix, 10 );

	// see if there was a unit suffix
	switch( *suffix ) {
		case 'k':
		case 'K':
			return num << 10;
		case 'm':
		case 'M':
			return num << 20;
		case 'g':
		case 'G':
			return num << 30;
		case 't':
		case 'T':
			return num << 40;	// who am I kidding?
	}

	return num;
}

/*
 * see if a file name is followed by an offset, and 
 * if so, lex it off and null it out
 */
long long getOffset( char *n ) {
	
	// see if string contains a colon
	while( *n && *n != ':' ) n++;
	if (*n == 0)
		return( 0LL );

	// see if what follows it is a number
	if (n[1] < '0' || n[1] > '9')
		return( 0LL );

	// null the colon leaving only the name
	*n++ = 0;
	return getSizeSpec( n );
}


/**
 * scan my table of supported arguments, and generate
 * a getopt argument specification string (letter for
 * each switch and a colon for each required argument).
 *
 * @param	a myargs table entry
 * @return	a corresponding longopt specification
 */
static char *
getArgSpec( struct myargs *myp ) {
	int i;
	static char spec[64];
	char *s = spec;

	for( i = 0; myp[i].fullname; i++ ) {
		*s++ = myp[i].letter;
		if (myp[i].argtype)
			*s++ = ':';
	}

	*s++ = 0;
	return( spec );
}

/**
 *	scan my table of supported arguments, and generate
 *	a corresponding getopt_long argument specification table.
 *
 * @param	myargs table (slightly better abstracted
 *		form of longopt table)
 * @return	a corresponding longopt table
 */
static struct option *
getArgOpts( struct myargs *myp )
{	int i;
	int numargs;
	int size;
	struct option *op;

	/* figure out how large a table I need	*/
	for( numargs = 0; myp[numargs].fullname; numargs++ );
	size = (numargs + 1) * sizeof (struct option);

	/* allocate it				*/
	op = (struct option *) malloc( size );
	if (op == 0) {
		fprintf(stderr, "%s Unable to malloc option table\n", argh);
		exit( 1 );
	}
	memset( op, 0, size );

	/* initialize it from my argument table	*/
	for( i = 0; myp[i].fullname; i++ ) {
		op[i].name 	= myp[i].fullname;
		op[i].has_arg	= myp[i].argtype ? 
					required_argument : no_argument;
		op[i].val	= myp[i].letter;
	}

	return( op );
}

/**
 * read a line of input and parse it as command line arguments
 *
 * returns:
 *	an argv array
 *
 * note:
 *	I can't use stdio because it might eat input intended for others,
 *	who use poll/read to get their input.
 */
static const char **
read_args() {
	// prompt for input
	fprintf(stdout, "loadgen (tag=%s): %s\n", loadgen_tag, herald );
	fflush(stdout);

#define	MAX_LINE 256
	char *inbuf = (char *) malloc(MAX_LINE);	// memory leak
	if (inbuf == 0)
		return( 0 );

	// read one line of arguments
#define	STDIN 0
	char c;
	int got = 0;
	do {
		if (read(STDIN, &inbuf[got], 1) != 1) 
			break;
		c = inbuf[got++];
	} while (c != '\n' && got < MAX_LINE - 2);
	inbuf[got] = 0;

	// scan past any introductory trash
	char *s = inbuf;
	while( *s && *s <= ' ')
		s++;

	// make sure there is something left to read
	if (*s == 0)
		return( 0 );

	// count the arguments
	int argc = 0;
	while( *s && *s != '\n' && *s != '\r') {
		// skip to start of nex argument
		while( *s == ' ' || *s == '\t' )
			s++;

		// skip to end of this argument
		argc++;
		while( *s && *s != ' ' && *s != '\t' && *s != '\n' && *s != '\r' )
			s++;
	}

	// allocate an array to hold the arguments	// memory leak
	const char **argv = (const char **) malloc( (argc+2) * sizeof (char *) );
	if (argv == 0)
		return( 0 );

	argc = 0;
	argv[argc++] = "LOADZOMBIE";
	s = inbuf;
	while( *s ) {
		// skip to start of nex argument
		while( *s == ' ' || *s == '\t' )
			s++;

		// add this one to the array
		if (*s && *s != '\n' && *s != '\r') {
			argv[argc++] = s;

			while( *s && *s != ' ' && *s != '\t' && *s != '\n' && *s != '\r' )
				s++;

			if (*s == '\n' || *s == '\r')
				*s = 0;
			else
				*s++ = 0;
		}

	}

	// zero terminate the list
	argv[argc] = 0;

	return( argv );
}


/**
 * read a list of filenames into a null terminated pointer array
 *
 * @parm s	argument to be parsed
 * 
 * @return	address of an array of name pointers
 */
static char **getTargets( char *s ) {
	// make sure we have a reasonable set of names
	int chars = 0;
	int commas = 0;
	while( s[chars] ) {
		if (s[chars] == ',')
			commas++;
		chars++;
	}
	
	if (chars < 1 && chars <= (2*commas))
		return 0;

	// allocate a suitable array of pointers
	char **ptrs = new char*[commas+2];
	for( int i = 0; i <= commas+1; i++ )
		ptrs[i] = 0;
	
	// copy the successive path names into the array
	for( int i = 0; *s; i++ ) {
		ptrs[i] = s;

		while( *s && *s != ',' )
			s++;

		if (*s == ',')
			*s++ = 0;
	}

	return ptrs;
}

/**
 * print out a usage list of switch options
 * 	by interpreting the long_options table
 */
static void 
switch_info()
{	int i;
	const char *type;

	fprintf(stderr, "    switch options:\n");
	for( i = 0; myargs[i].fullname; i++ ) {
		if((type = myargs[i].argtype) == 0)
			fprintf(stderr, "\t--%s\n", myargs[i].fullname );
		else
			fprintf(stderr, "\t--%s=<%s>\n", myargs[i].fullname, type);
	}
}

/**
 * print out a complete list of usage messages
 */
static void 
usage_info( const char *cmdname )
{	
	fprintf(stderr, "Usage: %s [switches]\n", cmdname);
	switch_info();
}

/**
 * Debug options definitions:<br>
 *
 * 	debugging features are controlled by a mask
 * 	this table associates names (and first letters)
 * 	with bits in the mask
 */
static struct dbgopt {
	char* 	descr;	/* first letter is how you specify this option	*/
	int	mask;	/* bits to turn on for this option		*/
} dbgopts[] = {
	// basic output control options
	{ (char *) "Options",	D_OPTS },
	{ (char *) "Commands",	D_CMDS },
	{ (char *) "Threads",	D_THREADS },
	{ (char *) "Files",	D_FILES },

	// low level debugging options
	{ (char *) "writes", 	D_WRITES },
	{ (char *) "verify", 	D_VERIFY },
	{ (char *) "sleeps",	D_SLEEP },

	{ (char *) "ALL",	D_ALL },
	{ (char *) "0",		0 },
	{ 0,	0 }
};

/*
 * turn a string of debug option letters into a debug bit mask
 * (used in argument processing).
 */
debugOptions 
debugOpts( const char *arg )
{	int ret = 0;
	int i;
	char c;
	int errors = 0;

	while( *arg ) {
		/* for each character of the argument string	*/
		c = *arg++;
		/* see if it matches a debugging option		*/
		for( i = 0; dbgopts[i].descr; i++ ) {
			/* if so, turn on appropriate mask bit	*/
			if (c == dbgopts[i].descr[0]) {
				ret |= dbgopts[i].mask;
				break;
			}
		}

		// see if we fell off the end
		if (dbgopts[i].descr == 0)
			errors++;
	}

	/* if no valid choice, print out a usage message	*/
	if (errors) {
		fprintf(stderr, "Debug options:\n");
		for( i = 0; dbgopts[i].descr; i++ ) {
			if ((dbgopts[i].mask & D_ALL) == 0)	// don't show testing opts
				continue;
			if (dbgopts[i].mask == D_ALL)		// don't show ALL option
				continue;			
			fprintf(stderr, "\t%c ... %s\n",
				dbgopts[i].descr[0], dbgopts[i].descr );
		}
	}

	return( ret );
}

/*
 * turn a set of debug option bits into a displayable
 * character string (used in diagnostic output to show
 * what debugs are enabled)
 */
const char *
getDebugOpts( debugOptions bits ) {
	static std::string opts;
	int numopts = 0;

	opts = "";

	for( int i = 0; dbgopts[i].descr; i++ ) {
		if (bits & dbgopts[i].mask) {
			// all doesn't count
			if ((dbgopts[i].mask & 0xffff) == 0xffff)
				continue;
			if (numopts++)
				opts += ",";
			opts += dbgopts[i].descr;
		}
	}

	return( opts.c_str() );
}
