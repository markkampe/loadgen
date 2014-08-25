#include <time.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

static bool initialized = false;
/**
 * if no block size is specified, we can choose them at random
 * (a power of two between min_bsize and max_bsize inclusive)
 *
 * @return	a block size for the next file
 */
#define	MIN_BSIZE	256
#define	MAX_BSIZE	(2*1024*1024)
long choose_bsize( long alignment, long long maxsize ) {

	if (!initialized) {
		time_t now = time(0);
		srandom( (unsigned) now );
		initialized = true;
	}
	unsigned long long value = random();

	// figure out how many powers of two we support
	int max_double = 0;
	long min_bsize = MIN_BSIZE;
	if (alignment > 0 && min_bsize < alignment)
		min_bsize = alignment;
	// long max_bsize = MAX_BSIZE;	// FIX: UNUSED
	// if (maxsize > 0 && maxsize <  MAX_BSIZE)
	// 	max_bsize = maxsize;
	for( int size = min_bsize; size < maxsize; size = size << 1 )
		max_double++;

	// use the random number as a power-of-two multilier
	value *= (max_double+1);
	value /= RAND_MAX;
	return min_bsize << value;
}

/**
 * choose a random block within a file
 *
 * @param num_blocks	number of blocks in the file
 *
 * @returns 	block to re-read/write
 */
long long choose_block( long long num_blocks ) {
	if (!initialized) {
		time_t now = time(0);
		srandom( (unsigned) now );
		initialized = true;
	}
	unsigned long long value = random();

	value *= num_blocks;
	value /= RAND_MAX;

	return value;
}

/**
 * return largest supported block size
 */
long max_bsize() {
	return MAX_BSIZE;
}

/**
 * if no file size is specified, we can choose them at random
 *
 *	between 10 and 2K blocks, up to a max size of 64MB
 *
 * @param bsize	block size for the file in question
 * @return	a file size for the next file
 */
#define MIN_BLOCKS	10
#define MAX_BLOCKS	2048
#define MAX_FSIZE	(64*1024*1024)
long long choose_file_size( long bsize ) {

	if (!initialized) {
		time_t now = time(0);
		srandom( (unsigned) now );
		initialized = true;
	}
	unsigned long long value = random();

	long max_blocks = MAX_FSIZE/bsize;
	if (max_blocks > MAX_BLOCKS)
		max_blocks = MAX_BLOCKS;

	value *= (max_blocks - MIN_BLOCKS);
	value /= RAND_MAX;
	return (MIN_BLOCKS + value) * bsize;
}

/**
 * every pattern data block begins with four headers
 *	which add up to 256 bytes
 *	which is the smallest possible block
 */
#define WIDTH	64
struct buf_header {
	char run_header[WIDTH];
	char thread_header[WIDTH];
	char file_header[WIDTH];
	char block_header[WIDTH];
	char data[];
};

/**
 * return the length of a standard header
 * 	  (used for verification reads)
 */
long header_size() {

	return sizeof (struct buf_header);
}

// all headers should begin with the same five bytes and end with a newline
#define HEADER_LEN 5
#define	RUN_FORMAT "#RUN date=%02d/%02d/%04d time=%02d:%02d:%02d tag=%-20s"
#define	RUN_READBK "#RUN date=%02d/%02d/%04d time=%02d:%02d:%02d "
#define	DIR_FORMAT "#DIR dir=%s"
#define	FIL_FORMAT "#FIL name=%s length=%llu"
#define	BLK_FORMAT "#BLK bsize=%ld offset=%llu"

// no read-back name should every be this large
#define	MAX_NAME 64	

/**
 * each sub-section header is padded out to a 64 byte line
 *	and terminated with a newline;
 */
void linepad( char *s, int len ) {
	int i;
	int last = len - 1;
	for( i = 0; i < last && s[i] != 0; i++ );
	while( i < last )
		s[i++] = ' ';
	s[last] = '\n';
}

/**
 * runHeader ... initialize the run-header
 *
 * @param buf	data buffer
 * @param tag	creating loadgen instance
 */
void runHeader( char *buf, const char *tag ) {
	// figure out where the run header goes
	struct buf_header *b = (struct buf_header *) buf;
	char *p = b->run_header;

	// figure out what time it is
	time_t now;
	time( &now );
	struct tm *tm = localtime( &now );

	snprintf( p, sizeof b->run_header, RUN_FORMAT,
		tm->tm_mon+1, tm->tm_mday, tm->tm_year+1900,
		tm->tm_hour, tm->tm_min, tm->tm_sec, 
		tag );

	linepad( (char *) p, sizeof b->run_header );
}

/**
 * threadHeader ... initialize the thread-header
 *
 * @param buf	data buffer
 * @param name	directory in which thread operates
 */
void threadHeader( char *buf, const char *name ) {
	// figure out where the run header goes
	struct buf_header *b = (struct buf_header *) buf;
	char *p = b->thread_header;

	snprintf( p, sizeof b->thread_header, DIR_FORMAT, name );
	linepad( (char *) p, sizeof b->thread_header );
}

/**
 * fileHeader ... initialize the file-header
 *
 * @param buf	data buffer
 * @param name	file name
 * @param len	intended length of file
 */
void fileHeader( char *buf, const char *name, long long len ) {
	// figure out where the run header goes
	struct buf_header *b = (struct buf_header *) buf;
	char *p = b->file_header;

	// find the final part of the name
	const char *s = name;
	while(s[1]) s++;
	while(s > name && s[-1] != '/') s--;

	snprintf( p, sizeof b->thread_header, FIL_FORMAT, s, len );
	linepad( (char *) p, sizeof b->file_header );
}

/**
 * blockHeader ... initialize the block-header
 *
 * @param buf	data buffer
 * @param bsize	length of this block
 * @param offset of this block in file
 */
void blockHeader( char *buf, long bsize, long long offset ) {
	// figure out where the run header goes
	struct buf_header *b = (struct buf_header *) buf;
	char *p = b->block_header;

	snprintf( p, sizeof b->block_header, BLK_FORMAT, bsize, offset);
	linepad( (char *) p, sizeof b->thread_header );
}

// 64 bytes of pattern data to be written out 63 bytes at a time
static char pattern[] = "123456789 abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

/**
 * fillData ... fill in the data payload for the block
 *
 * @param buf	data buffer
 * @param bsize	file block size
 */
void fillData ( char *buf, int bsize ) {
	int i = sizeof (struct buf_header);

	// fill the remainder of the block with a staggered data pattern
	for( int x = 0; i < bsize; i++ ) {
		buf[i] = ((i%64) == 63) ?  '\n' : pattern[(x++)%64];
	}
}

/**
 * checkHeaders ... confirm the correctness of the various headers
 *
 * @param buf	buffer containing the block to be validated
 * @param bsize	expected read/write block size
 *
 * @return	NULL if data is correct, else an error string
 */
const char *
checkHeaders( const char *buf, int bsize, long long offset ) {
	struct buf_header *b = (struct buf_header *) buf;

	// start by ensuring that each header is present
	//	we do a better job of validating the headers
	//	on the first block, but since all but the
	//	block header is a constant, there isn't much
	//	point in revalidating all the other headers
	//	on every block (beyond seeing if they are
	//	there at all)
	if (strncmp( b->run_header, RUN_FORMAT, HEADER_LEN ))
		return "No RUN header";
	if (strncmp( b->thread_header, DIR_FORMAT, HEADER_LEN ))
		return "No DIR header";
	if (strncmp( b->file_header, FIL_FORMAT, HEADER_LEN ))
		return "No FILE header";
	if (strncmp( b->block_header, BLK_FORMAT, HEADER_LEN ))
		return "No BLOCK header";

	// we should also make sure that each header ends with a newline
	if (b->run_header[WIDTH-1] != '\n')
		return "un-terminated RUN header";
	if (b->thread_header[WIDTH-1] != '\n')
		return "un-terminated DIR header";
	if (b->file_header[WIDTH-1] != '\n')
		return "un-terminated FILE header";
	if (b->block_header[WIDTH-1] != '\n')
		return "un-terminated BLOCK header";

	// make sure the block sizes and offsets agree
	long this_bsize;
	long long this_offset;
	if (sscanf( b->block_header, BLK_FORMAT, &this_bsize, &this_offset ) != 2) 
		return "mal-formatted BLOCK header";
	if (bsize != 0 && this_bsize != bsize)
		return "block-size mis-match";
	if (this_offset != offset)
		return "offset mis-match";

	// it might be correct
	return NULL;
}

/**
 * get_block_size ... return the block size used for this file
 *		      (assuming headers have already been verified)
 *
 * @param buf	buffer containing headers for this file
 * @return	block size
 */
long
get_block_size( const char * buf ) {
	struct buf_header *b = (struct buf_header *) buf;
	long this_bsize;
	long long this_offset;
	if (sscanf( b->block_header, BLK_FORMAT, &this_bsize, &this_offset ) == 2) 
		return this_bsize;
	else
		return 0;
}

/**
 * get_file_size ... return the file size used for this file
 *		      (assuming headers have already been verified)
 *
 * @param buf	buffer containing headers for this file
 * @return	file size
 */
long long
get_file_size( const char * buf ) {
	struct buf_header *b = (struct buf_header *) buf;
	long long this_fsize;
	char name[256];
	if (sscanf( b->file_header, FIL_FORMAT, name, &this_fsize ) == 2) 
		return this_fsize;
	else
		return 0;
}

/**
 * checkFile ... confirm the file is the one we expected
 *		 (to be called after checkHeaders)
 *
 * @param buf	buffer containing the first block of the file
 * @param path	fully qualified path to the file
 *
 * @return	NULL if file checks out, else error message
 */
const char *
checkFile( const char *buf, const char *path ) {
	struct buf_header *b = (struct buf_header *) buf;

	// read off the creation time
	//	if it parses, I'm going to call it right for now
	//	but if I were cool I'd make sure the file was no
	//	older than this date/time
	int mon, day, year, hour, min, sec;
	if (sscanf( b->run_header, RUN_READBK, &mon, &day, &year, &hour, &min, &sec  ) != 6) 
		return "mal-formatted RUN header";

	// read the directory name
	// 	but don't validate it because we might want
	//	over-constrain our ability to validate data
	//	long after we forgot how we generated it
	char dir[MAX_NAME];
	if (sscanf( b->thread_header, DIR_FORMAT, dir  ) != 1) 
		return "mal-formatted DIR header";

	// read the file name and length
	char file[MAX_NAME];
	long long len;
	if (sscanf( b->file_header, FIL_FORMAT, file, &len ) != 2) 
		return "mal-formatted FILE header";

	// validate the name
	const char *s;
	for(s = path; *s && s[1]; s++);
	while( s > path && s[-1] != '/' ) s--;
	if (strcmp(s, file)) {
		return "file name mis-match";
	}

	// validate the length
	struct stat statb;
	if (stat( path, &statb ) < 0)
		return "unable to stat";
	if (statb.st_size > len)
		return "file too long";
	if (statb.st_size < len)
		return "file too short";
	
	return NULL;
}

/**
 * checkData ... confirm the correctness of the data in a block
 *		(to be called after checkHeaders)
 *
 * @param buf	buffer containing the block to be validated
 * @param bsize	expected read/write block size
 *
 * @return	NULL if data is correct, else an error string
 */
const char *
checkData( const char *buf, int bsize ) {
	int i = sizeof (struct buf_header);
	for( int x = 0; i < bsize; i++ ) {
		if (buf[i] == (((i%64) == 63) ? '\n' : pattern[(x++)%64]))
			continue;
		return "incorrect pattern data";
	}

	return NULL;
}
