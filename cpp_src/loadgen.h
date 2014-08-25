/**
 * global information for demo load generation utility
 */
extern bool loadgen_shutdown;	///< shut down the load generation
extern bool loadgen_simulate;	///< only pretend to generate data
extern bool loadgen_delete;	///< remove files/directories after verification
extern bool loadgen_read;	///< read back existing files
extern bool loadgen_rewrite;	///< re-write existing files
extern bool loadgen_verify;	///< verify contents on read-back
extern bool loadgen_halt;	///< stop on error
extern bool loadgen_sync;	///< synchronous writes
extern bool loadgen_zombie;	///< we are under remote control
extern bool loadgen_once;	///< only one directory per thread
extern int  loadgen_direct;	///< direct buffer alignment
extern int  loadgen_rand_blk;	///< random access r/w block size
extern long long loadgen_rate;	///< target generation rate
extern int  loadgen_update;	///< performance update interval
extern int  loadgen_maxfiles;	///< maximum number of files to create
extern const char *loadgen_tag;	///< output tag
extern const char *loadgen_problem;	///< the problem that took us down

extern int copyData( const char *from, char *to, int threads, int bsize );
extern int createData_d( char *to, int threads, int bsize, long long file_size );
extern int createData_l( char **list, int bsize, long long file_size );
extern int readData_d( const char *from, char *to_dir, int threads, int bsize, long long length );
extern int readData_l( char **list, int bsize, long long length );

/**
 * see if we have been told to change the number of threads we are running
 *
 * @param maxSeconds (maximum number of seconds to wait)
 *
 * @return	-1 or a new thread count
 */
extern int changeNumThreads( int maxSeconds );

/**
 * check to make sure that a directory exists and is writeable
 *	(creating it if necessary)
 *
 * @param name		of desired directory
 * @param create	should we create it
 */
extern const char *checkdir( const char *name, bool create );
extern bool checkdev( const char *name );
extern bool checkfile( const char *name );

long long getSizeSpec( const char * );
long long getOffset( char *n );

// exit status bits
#define 	SOURCE_DIRECTORY	0x01	///< could not find/open
#define		TARGET_DIRECTORY	0x02	///< could not find/create
#define		INPUT_FILE_ERROR	0x04	///< could not open/read
#define		OUTPUT_FILE_ERROR	0x08	///< could not create/write
#define		RESOURCE_ERROR		0x80	///< memory, threads, ...
