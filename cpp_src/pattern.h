/*
 * routines to generate and verify pattern data
 *	runHeader:	header to describe the creating zombie instance
 *	threadHeader:	header to describe the creating thread
 *	fileHeader:	header to describe the specific file
 *	blockHeader:	header to describe each block
 *	
 *	fillData:	fill the remainder of a block with pattern data
 *	
 *	checkData	validate the correctness of a block
 *	fileLength	expected length of this file
 */
void runHeader( char *buf, const char *tag );
void threadHeader( char *buf, const char *name );
void fileHeader( char *buf, const char *name, long long len );
void blockHeader( char *buf, long bsize, long long offset );

// fill out the remainder of the block with pattern data
void fillData( char *buf, int bsize );

// check the correctness of the headers in a block
const char *checkHeaders( const char *buf, int bsize, long long offset );

// check correctness of the file description
const char * checkFile( const char *buf, const char *path );

// check the correctness of the data in a block
const char *checkData( const char *buf, int bsize );

// length of the standard header set
long header_size();

// length of the block size used for this file
long get_block_size( const char *buf );

// length of the block size used for this file
long long get_file_size( const char *buf );

// largest supported block size
long max_bsize();

// choose a random block size
long choose_bsize( long alignment, long long maxlen );

// choose a random block of a file
long choose_block( long long max_blocks );

// choose a random file size
long long choose_file_size( long bsize );

#define	DEFAULT_ALIGNMENT	8192
