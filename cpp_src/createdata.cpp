#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <fcntl.h>

#include "loadgen.h"
#include "threadstatus.h"
#include "pattern.h"
#include "bufset.h"
#include "debug.h"

// maximum number of discrete threads (for manual creation)
//	if we aren't told how many threads to create, we
//	should expect commands to change this number over
//	the course of the session.  This is the maximum number
//	of threads we can create and destroy in a single
//	session.
#define	MAX_THREADS	100

void *createDataThread( void * );
int writeFile( const char *filename, Bufset *bufs, struct writeParms *myparms, perfstats *stats );
int singleWrite(const char *filename, Bufset *bufs, struct writeParms *parms, int fd, perfstats *stats);

/**
 * parameters for a data creation thread
 */
struct writeParms {
	// whole thread parameters
	const char *	to_directory;		// all work will be done here
	unsigned long	create_opts;		// truncate?
	bool		single_file;		// do one file and then exit
	long long	offset;			// base offset for all file I/O
	int		aio_depth;		// depth for AIO requests

	// per file parameters
	int 		block_size;		// size of all writes
	long long	file_length;		// maximum allowable file size
	long long	bytes_to_write;		// total number of bytes to write

	/**
	 * allocate and initialize a write operation descriptor.
	 *
	 * @param name		name of this thread
	 * @param bsize		write block size
	 * @param length	output file length
	 * @param dir		name of directory to put files in
	 */
	writeParms( char *name, int bsize, long long file_size, const char *dir ) {
		// fill in the basic operation parameters
		block_size = bsize;
		file_length = file_size;
		bytes_to_write = 0;
		to_directory = dir;
		create_opts = O_CREAT;
		if (!loadgen_rewrite)
			create_opts |= O_TRUNC;
		if (loadgen_sync)
			create_opts |= O_DSYNC;
		if (loadgen_direct)
			create_opts |= O_DIRECT;

		// allocate the thread status structure for it
		// 	and add it to the known threads list
		new ThreadStatus( name, this );
	}
};

/**
 * Multi-Thread creation load generator:
 * -	creates a sub-directory per thread under target directory
 * -	each load generation thread creates pattern data under it
 *
 * @param to		directory under which files should be created
 * @param threads	number of initial threads
 *			(if this is zero, don't start yet)
 *
 * @return		exit status (worst exiit status from any thread)
 */
int 
createData_d( char *to, int threads ) {

	// if there is an offset, note it and null it out
	long long offset = getOffset(to);

	// figure out what type of target it is
	bool onefile = false;
	if (checkdev( to ) || checkfile( to )) {
		onefile = true;
	} else {
		// make sure that our assigned working directory exists and is writable
		const char *err = checkdir(to, true);
		if (err) {
			fprintf(stderr, "FATAL: target directory %s: %s\n", to, err );
			loadgen_problem = "target directory access";
			return TARGET_DIRECTORY;
		} 
	}
	
	// pre-define all of the threads we will EVER need
	int max_threads = threads ? threads : MAX_THREADS;
	for( int i = 0; i < max_threads; i++ ) {
		// come up with a name for this thread
		char *threadname = 0;
		asprintf( &threadname, "Creator Thread %04d", i );

		struct writeParms *parms;
		// come up with a target directory for this thread
		if (onefile)
 			parms = new writeParms( threadname, loadgen_bsize, loadgen_fsize, to );
		else {
			char *dir = 0;
			asprintf( &dir, "%s/Thread%04d", to, i );
 			parms = new writeParms( threadname, loadgen_bsize, loadgen_fsize, dir );
		}
		
		if (threadname == 0 || parms == 0) {
			loadgen_problem = "malloc failure";
			return RESOURCE_ERROR;
		}
		parms->offset = offset;
		parms->single_file = onefile;
		parms->bytes_to_write = loadgen_data;
		parms->aio_depth = (loadgen_depth > 0) ? loadgen_depth : 1;
		// FIX on shutdown we should reclaim threadname, to_directory
	}
	
	// we just configure them, the thread manager does the real work
	return ThreadStatus::manageThreads( createDataThread, threads );
}

/**
 * Multi-Thread creation load generator where the target directories are
 *	assigned rather than created.  The primary use of this option is
 *	to permit us to measure aggregate throughput across multiple
 *	spindles.
 *
 * @param list		list of directories under which to create files
 *
 * @return		exit status (worst exiit status from any thread)
 */
int 
createData_l( char **list ) {

	// create a work thread for each specified directory
	int threads = 0;
	for( int i = 0; list[i] != 0; i++ ) {

		bool single_file = false;

		// if there is an offset, note it and null it out
		long long offset = getOffset(list[i]);

		// see if we have been given a device or a directory
		if (checkdev( list[i]) || checkfile( list[i])) {
			single_file = true;
		} else {
			/* see if the file is a directory	*/
			const char *err = checkdir(list[i], false);
			if (err) {
				fprintf(stderr, "FATAL: target directory %s: %s\n", list[i], err );
				loadgen_problem = "target directory access";
				return TARGET_DIRECTORY;
			} 
		}

		// come up with a name for this thread
		char *threadname = 0;
		asprintf( &threadname, "Creator Thread %04d", i );

		struct writeParms *parms = new writeParms( threadname, loadgen_bsize, loadgen_fsize, list[i] );
		if (threadname == 0 || parms == 0) {
			loadgen_problem = "malloc failure";
			return RESOURCE_ERROR;
		}

		parms->offset = offset;
		parms->single_file = single_file;
		parms->bytes_to_write = loadgen_data;
		parms->aio_depth = (loadgen_depth > 0) ? loadgen_depth : 1;
		threads++;
		// FIX on shutdown we should reclaim threadname, to_directory
	}
	
	// we just configure them, the thread manager does the real work
	return ThreadStatus::manageThreads( createDataThread, threads );
}


/**
 * this is the routine that each load generation thread runs
 *
 * @param	ThreadStatus structure for this thread
 */
void *createDataThread( void *sts ) {
	int status = 0;		// this thread's exit status
	int done = 0;		// number of files created
	Bufset *bufset = 0;	// write buffer set

	struct ThreadStatus *mystatus = (struct ThreadStatus *) sts;
	struct writeParms *myparms = (struct writeParms *) mystatus->parms;
	int alignment = loadgen_direct > 0 ? loadgen_direct : DEFAULT_ALIGNMENT;
	int maxfiles = loadgen_maxfiles;
	int bufsize = myparms->block_size;
	int num_buf = myparms->aio_depth;


	// announce that we are starting up
	mystatus->running = true;	// now set in manageThreads to avoid a race
	if (loadgen_debug & D_THREADS) {
		fprintf(stderr, "# Starting %s in %s\n", mystatus->name, myparms->to_directory );
	}

	// make sure we can create our target directory
	if (!myparms->single_file) {
		const char *err = checkdir( myparms->to_directory, true );
		if (err) {
			fprintf(stderr, 
				"FATAL: target directory %s: %s\n", 
				myparms->to_directory, err );
			status = TARGET_DIRECTORY;
			loadgen_problem = "target directory access";
			goto exit;
		} 
	} else
		maxfiles = 1;

	// allocate and lock down pattern data buffer(s)
	if (bufsize == 0)
		bufsize = max_bsize();
	bufset = new Bufset( num_buf, bufsize, alignment );
	if (bufset->buffers == 0) {
		fprintf(stderr, "Unable to allocate (%d %d byte) data buffer for %s\n",
			num_buf, bufsize, mystatus->name );
		status |= RESOURCE_ERROR;
		loadgen_problem = "malloc failure";
		goto exit;
	}

	// initialize the run and thread headers and the data contents
	for( int i = 0; i < num_buf; i++) {
		char *b = bufset->buffer(i);
		runHeader( b, loadgen_tag );
		threadHeader( b, myparms->to_directory );
		fillData( b, bufsize );
	}


	// create a succession of files
	for( done = 0; status == 0 && mystatus->enable; done++ ) {
		// see if a general shutdown has been declared
		if (loadgen_shutdown)
			break;

		// see if we have hit the maximum # of files to create
		if (maxfiles > 0 && done >= maxfiles)
			break;

		// figure out the name of the next file 
		if (!myparms->single_file) {
			char *fullpath;
			char filename[16];
			snprintf(filename, sizeof filename, "FILE_%06d", done );
			if (!asprintf( &fullpath, "%s/%s", 
					myparms->to_directory, filename )) {
				fprintf(stderr, "Unable to allocate filename for %s\n", 
					mystatus->name);
				status |= RESOURCE_ERROR;
				loadgen_problem = "malloc failure";
				break;
			}
			status = writeFile( fullpath, bufset, myparms, &mystatus->stats );
			free( fullpath );
			fullpath = 0;
		} else {
			status = writeFile( myparms->to_directory, bufset, myparms, &mystatus->stats );
		}
	}

  	// free the pattern data buffers
	delete bufset;

  exit:	
	// update my exit status and exit
	mystatus->running = false;
	mystatus->exit_status = status;
	if (loadgen_debug & D_THREADS || done != maxfiles) {
		// it is often useful to know what caused each thread to exit
		fprintf(stderr, "# Shutting down %s (en=%d, cnt=%d/%d, sts=%x, stop=%d)\n",
			mystatus->name, mystatus->enable,
			done, loadgen_maxfiles, status, loadgen_shutdown );
	}
	pthread_exit(0);
}

/*
 * create a file, choose a size, and write it
 */
int
writeFile( const char *filename, 		// output file to create
	Bufset *bufs,				// output buffers to use
	struct writeParms *myparms, 		// test parameters
	perfstats *stats ) {			// performance counters
	// generate a fully qualified path and create the file
	int fd = -1;
	if (!loadgen_simulate) {
		int opts = O_WRONLY | myparms->create_opts;
		fd = open( filename, opts, 0666 );
		if (fd < 0) {
			fprintf(stderr,"Unable to create output file %s: %s\n", 
				filename, strerror( errno ));
			loadgen_problem = "file create failure";
			return OUTPUT_FILE_ERROR;
		}
	}

	// create a local (this test only) parameter set
	struct writeParms parms = *myparms;

	// come up with a block size
	if (parms.block_size == 0) {
		parms.block_size = choose_bsize( loadgen_direct, bufs->size );
	}

	// come up with a file length
	unsigned long long fsize = myparms->file_length;
	if (parms.file_length == 0) {
		struct stat statb;
		fstat( fd, &statb );
		if (S_ISREG(statb.st_mode) && statb.st_size > 0) {
			// existing file ... use current length
			parms.file_length = statb.st_size;
		} else if (S_ISBLK(statb.st_mode) || S_ISCHR(statb.st_mode)) {
			ioctl(fd, BLKGETSIZE64, &parms.file_length);
		} 
	}

	// come up with a file length
	if (parms.file_length == 0)
		parms.file_length = choose_file_size( parms.block_size );

	// now create header(s) for the chosen file size
	for( int i = 0; i < bufs->buffers; i++ ) {
		fileHeader( bufs->buffer(i), filename, parms.file_length );
	}
		
	// figure out how much data to write
	if (parms.bytes_to_write == 0)
		parms.bytes_to_write = fsize;

	// announce our intentions
	if (loadgen_debug & D_FILES) 
		fprintf(stderr, "# %s output file %s, bsize=%d, depth=%d, fsize=%lld/%lld\n", 
			loadgen_rewrite ? "rewriting" : "creating",
			filename, parms.block_size, parms.aio_depth, parms.bytes_to_write, parms.file_length);

	// fill it full of data
	stats->file_done();	// bump the file count
	int status;
	// FIX .. AIO depth=
	status = singleWrite(filename, bufs, &parms, fd,  stats);

	// close the file and free its name
	if (!loadgen_simulate)
		close( fd );

	return( status );
}

/*
 * synchronous writes
 *
 * @param	name of output file (for logging)
 * @param	Bufset for the pattern data write buffers
 * @param	writeParms for this file
 * @param	open write file descriptor
 * @param	perfstats structure to accumulate results
 *
 * @return	status
 */
int singleWrite(const char *filename,
		Bufset *bufs,
		struct writeParms *parms,
		int fd,
		perfstats *stats) {
	int status = 0;
	long long len = 0;
	long long offset = parms->offset;
	char *buf = bufs->buffer(0);

	lseek( fd, offset, SEEK_SET );
	while( len < parms->bytes_to_write && status == 0 ) {
		// update the header for the next block
		blockHeader( buf,  parms->block_size, offset );

		// do the write
		int bytes = (loadgen_rand_blk) ? loadgen_rand_blk : parms->block_size;
		status |= timed_write( fd, buf, bytes, stats, filename, offset );
		len += bytes;

		// see if we should do a seek
		if (loadgen_rand_blk && loadgen_rewrite) {
			long long maxblk = parms->file_length / parms->block_size;
			offset = parms->offset + choose_block( maxblk ) * parms->block_size;
			lseek( fd, offset, SEEK_SET );
		} else
			offset += bytes;
	}

	return( status );
}
