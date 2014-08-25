/**
 * module:	debug.h
 *
 * purpose:
 * 	variables and flags to control diagnostic output
 */

// basic output control ... on by default or used all the time		*/
#define	D_OPTS		0x00000001	///< display enabled options
#define	D_CMDS		0x00000002	///< display session commands
#define	D_THREADS	0x00000004	///< display thread start/stops
#define D_FILES		0x00000008	///< display all file operations
#define	D_WRITES	0x00000010	///< display all write operations
#define	D_VERIFY	0x00000020	///< display verification operations
#define	D_CONNECT	0x00000040	///< display connection events
#define D_SLEEP		0x00000080	///< display sleeps

// low level debug operations ... you need a reason to turn these on


// all reasonable debug options
#define	D_ALL		0x003fffff	///< show everything

typedef	unsigned long debugOptions;	///< word full of debug options
extern debugOptions loadgen_debug;

/**
 * convert string debug option specifications into corresponding bits
 *
 * @param option_spec	string describing debug options
 * @return		word full of corresponding option bits
 */
debugOptions debugOpts( const char *option_spec );

/**
 * render debug option bits into a descriptive string
 *
 * @param bits		word full of debug option bits
 * @return		character string representation of those options
 */
const char *getDebugOpts( debugOptions bits );
