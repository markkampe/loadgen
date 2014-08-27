#include <stdio.h>
/* #define	_XOPEN_SOURCE	600 */
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "loadgen.h"
#include "threadstatus.h"
#include "pattern.h"
#include "debug.h"

	
void *copyThread( void * );

/**
 * parameters for a data creation thread
 */
struct readParms {
	char *		from_directory;
	char *		to_directory;
	int 		block_size;
	unsigned long	create_opts;
};

/**
 * scandir filter function to select only non-hidden directories
 *
 * @param dp	dirent to be validated
 *
 * @return	true if it is a non-hidden directory
 */
static int isDir( const struct dirent *dp ) 
{
	// make sure name doesn't begin with a dot
	const char *s = dp->d_name;
	if (*s == '.')
		return( 0 );

	// stat it to see if it is a directory
	struct stat statb;
	if (stat( s, &statb ) != 0)
		return( 0 );

	return S_ISDIR( statb.st_mode );
}

/*
 * scandir filter function to select only non-hidden files
 *
 * @param dp	dirent to be validated
 *
 * @return	true if it is a non-hidden file
 */
static int isFile( const struct dirent *dp ) {

	// make sure name doesn't begin with a dot
	const char *s = dp->d_name;
	return( *s != '.' );
}
/**
 * Data copying load generator:
 * -	identifies sub-directories in data source
 * -	kicks off data-copy sub-threads in each
 *
 * @param from		directory from which files came
 * @param to		directory under which files should be created
 * @param threads	number of initial threads
 * @param bsize		write block size
 *
 * @return		exit status (worst exiit status from any thread)
 */
int 
copyData( const char *from, char *to, int threads, int bsize ) {

	// make sure that our assigned working directory exists 
	const char *err = checkdir(to, to);
	if (err) {
		fprintf(stderr, "FATAL: target directory %s: %s\n", to, err );
		loadgen_problem = "target directory access";
		return TARGET_DIRECTORY;
	} 
	
	// if there is a from directory, make sure it exists as well
	err = checkdir(from, false);
	if (err) {
		fprintf(stderr, "FATAL: source directory %s: %s\n", from, err );
		loadgen_problem = "source directory access";
		return SOURCE_DIRECTORY;
	} 

	// generate a list of all the sub-directories in the target sub-directory
	chdir( from );		// isDir needs this to be our current working directory
	struct dirent **results;
	int num_subdirs = scandir( from, &results, isDir, alphasort );
	if (num_subdirs <= 0) {
		fprintf(stderr, "FATAL: no sub-directories to verify under %s\n", from );
		loadgen_problem = "no directories to copy";
		return INPUT_FILE_ERROR;
	}

	// create a thread for each 
	for( int i = 0; i < num_subdirs; i++ ) {
		// come up with a name for this thread
		char *threadname;
		if (asprintf( &threadname, "Copier Thread %04d", i ) < 0)  {
			fprintf(stderr, "FATAL: unable to allocate name for thread %d\n", i );
			loadgen_problem = "malloc failure";
			return RESOURCE_ERROR;
		}

		// allocate and initialize a parameter structure
		struct readParms *parms = (struct readParms *) malloc( sizeof (struct readParms) );
		if (parms == 0) {
			fprintf(stderr, "FATAL: unable to allocate parameters for thread %d\n", i );
			loadgen_problem = "malloc failure";
			return RESOURCE_ERROR;
		}

		parms->block_size = bsize;
		parms->create_opts = loadgen_rewrite ? 0 : O_TRUNC|O_CREAT;
		if (loadgen_sync)
			parms->create_opts |= O_DSYNC;
		if (loadgen_direct)
			parms->create_opts |= O_DIRECT;

		if ( asprintf( &parms->to_directory, "%s/%s", to, results[i]->d_name ) < 0 ) {
			fprintf(stderr, "FATAL: unable to allocate to-path for thread %d\n", i );
			loadgen_problem = "malloc failure";
			return RESOURCE_ERROR;
		}

		if ( asprintf( &parms->from_directory, "%s/%s", from, results[i]->d_name ) < 0 ) {
			fprintf(stderr, "FATAL: unable to allocate from-path for thread %d\n", i );
			loadgen_problem = "malloc failure";
			return RESOURCE_ERROR;
		}

		// allocate the thread status structure for it
		// 	and add it to the known threads list
		new ThreadStatus( threadname, parms );
		
		// and free up the dirent
		free( results[i] );
		results[i] = 0;
	}

	// free up the scandir temps
	free( results );
	results = 0;
	
	// we just configure them, the thread manager does the real work
	int ret =  ThreadStatus::manageThreads( copyThread, threads );
	
	return ret;
}

/**
 * this is the routine that each load generation thread runs
 *
 * @param	ThreadStatus structure for this thread
 */
void *copyThread( void *sts ) {
	int status = 0;		// this thread's exit status
	char *data = 0;		// the buffer we read into
	int count = 0;		// number of directory entries to process
	int done = 0;		// number of directory entries processed

	// pick up ponter to my status structure
	struct ThreadStatus *mystatus = (struct ThreadStatus *) sts;
	struct readParms *myparms = (struct readParms *) mystatus->parms;
	int alignment = loadgen_direct > 0 ? loadgen_direct : DEFAULT_ALIGNMENT;
	long bsize = myparms->block_size;
	if (bsize == 0)
		bsize = max_bsize();

	// announce that we are starting up
	mystatus->running = true;
	if (loadgen_debug & D_THREADS) {
		fprintf(stderr, "# Starting %s in %s\n", mystatus->name, myparms->to_directory );
	}

	// make sure our target directory exists
	const char *err = checkdir( myparms->to_directory, true );
	if (err) {
		fprintf(stderr, 
			"FATAL: target directory %s: %s\n", 
			myparms->to_directory, err );
		status = TARGET_DIRECTORY;
		loadgen_problem = "target directory access";
		goto exit;
	} 

	// allocate a read buffer
	if (posix_memalign( (void **) &data, alignment, bsize ) != 0) {
		fprintf(stderr, "Unable to allocate (%ld byte) data buffer for %s\n",
			bsize, mystatus->name );
		status |= RESOURCE_ERROR;
		loadgen_problem = "malloc failure";
		goto exit;
	}
	mlock( data, bsize );

	// find and verify each file in this directory
	struct dirent **results;
	count = scandir(  myparms->from_directory, &results, isFile, alphasort );
	for( done = 0; done < count && status == 0 && mystatus->enable; done++ ) {
		if (loadgen_shutdown)
			break;
		
		// open the input file
		char *from_path;
		asprintf( &from_path, "%s/%s", myparms->from_directory, results[done]->d_name );
		int fd_from = open( from_path, 0 );
		if (fd_from < 0) {
			fprintf(stderr, "Unable to open input file %s: %s\n", 
				from_path, strerror( errno ) );
			status |= INPUT_FILE_ERROR;
			loadgen_problem = "file open failure";
			break;
		} 

		// open the output file
		char *to_path;
		asprintf( &to_path, "%s/%s", myparms->to_directory, results[done]->d_name );
		int opts = O_WRONLY|myparms->create_opts;
		int fd_to = open( to_path, opts, 0666 );
		if (fd_to < 0) {
			fprintf(stderr, "Unable to create output file %s: %s\n", 
				to_path, strerror( errno ) );
			status |= INPUT_FILE_ERROR;
			loadgen_problem = "file create failure";
			break;
		} 

		// come up with a block size 
		bsize = myparms->block_size;
		if (bsize == 0)
			bsize = choose_bsize( loadgen_direct, 0LL );


		if (loadgen_debug & D_FILES) {
			fprintf(stderr, "# copying file %s from %s to %s, bsize=%ld\n",
				results[done]->d_name, myparms->from_directory,
				myparms->to_directory, bsize);
		}

		// no go back and copy the next block
		long long unsigned len = 0;
		while( status == 0 ) {
			// note when this operation starts
			// hires_time_t elapsed = hires_time();	FIX: UNUSED

			// read another block
			int bytes = read( fd_from, data, bsize );
			if (bytes == 0)
				break;
			if (bytes < 0) {
				fprintf(stderr, 
					"Data read error on input file %s at offset %llu: %s\n",
					from_path, len, strerror( errno ));
				status |= INPUT_FILE_ERROR;
				loadgen_problem = "file read error";
				break;
			}

			// write it out to the target file
			status |= timed_write( fd_to, data, bytes, 
				&mystatus->stats, to_path, len );
			len += bytes;
		}

		close( fd_from );
		close( fd_to );

		if (status == 0) {
			mystatus->stats.file_done();
		}
		
		// free the stuff we allocated for this file
		free( from_path );
		from_path = 0;
		free( to_path );
		to_path = 0;
		free( results[done] );
		results[done] = 0;
	}

  	// free stuff we allocated
	if (data) {
		munlock( data, bsize );
		free( data );
	}
	if (results)
		free( results );
	
	// free our parameters
	free( myparms->to_directory );
	myparms->to_directory = 0;
	free( myparms->from_directory );
	myparms->from_directory = 0;

  exit:	
	// update my exit status and exit
	mystatus->running = false;
	mystatus->exit_status = status;
	if (loadgen_debug & D_THREADS || done != count) {
		// it is often useful to know what caused a thread to shut down
		fprintf(stderr, "# Shutting down %s (en=%d, cnt=%d/%d, sts=%x, stop=%d)\n",
			mystatus->name, mystatus->enable,
			done, count, status, loadgen_shutdown );
	}
	pthread_exit(0);
}

