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

	
void *readThread( void * );
int readFile( const char *filename, char *buf, int bufsize, int bsize, long long read_bytes, long long offset, perfstats *stats );
void *compareThread( void * );

/**
 * parameters for a data creation thread
 */
struct readParms {
	const char *	from_directory;
	const char *	to_directory;
	int 		block_size;
	int		aio_depth;
	long long	bytes_to_read;
	long long	offset;
	bool            one_file;

	/**
	 * allocate and initialize a read operation descriptor.
	 *
	 * @param name		name of this thread
	 * @param bsize		write block size
	 * @param src		name of directory files came from
	 * @param cpy		name of directory files were copied to
	 */
	readParms( char *name, int bsize, const char *src_dir, const char *cpy_dir ) {
		// fill in the basic operation parameters
		block_size = bsize;
		bytes_to_read = 0LL;
		offset = 0LL;
		from_directory = src_dir;
		to_directory = cpy_dir;
		one_file = false;

		// allocate the thread status structure for it
		// 	and add it to the known threads list
		new ThreadStatus( name, this );

		// FIX - on exit from_directory, to_directory and name should free
	}
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
 * Data reader
 * -	spawn off threads to read/verify every file in every directory
 *
 * @param from		directory from which files came
 * @param to		directory under which files should be created
 * @param threads	number of initial threads
 *
 * @return		exit status (worst exiit status from any thread)
 */
int 
readData_d( const char *from, char *to, int threads ) {

	// make sure that our assigned working directory exists 
	const char *err = checkdir(to, false);
	if (err) {
		fprintf(stderr, "FATAL: target directory %s: %s\n", to, err );
		loadgen_problem = "target directory access";
		return TARGET_DIRECTORY;
	} 
	
	// if there is a from directory, make sure it exists as well
	if (from) {
		err = checkdir(from, false);
		if (err) {
			fprintf(stderr, "FATAL: source directory %s: %s\n", from, err );
			loadgen_problem = "source directory access";
			return SOURCE_DIRECTORY;
		} 
	}

	// generate a list of all the sub-directories in the target sub-directory
	chdir( to );		// isDir needs this to be our current working directory
	struct dirent **results;
	int num_subdirs = scandir( to, &results, isDir, alphasort );
	if (num_subdirs <= 0) {
		fprintf(stderr, "# no sub-directories to verify under %s\n", to );
		return 0;
	}

	// we may want to limit the number of directories to # threads
	if (threads > 0 && num_subdirs > threads && loadgen_once)
		num_subdirs = threads;

	// create a thread for each 
	for( int i = 0; i < num_subdirs; i++ ) {
		// come up with a name for this thread
		char *threadname = 0;
		asprintf( &threadname, "Verifier Thread %04d", i );
	
		// figure out where the data will reside
		char *cpy = 0;
		asprintf( &cpy, "%s/%s", to, results[i]->d_name );

		// figure out where the original source is
		char *src = 0;
		if (from)
			asprintf( &src, "%s/%s", from, results[i]->d_name );

		// allocate and initialize a parameter structure
		struct readParms *parms = new readParms( threadname, loadgen_bsize, src, cpy );
		if (threadname == 0 || cpy == 0 || parms == 0) {
			fprintf(stderr, "FATAL: malloc failure\n");
			loadgen_problem = "malloc failure";
			return RESOURCE_ERROR;
		}
		parms->bytes_to_read = loadgen_data;
		parms->aio_depth = loadgen_depth;

		// and free up the dirent
		free( results[i] );
		results[i] = 0;
	}

	// free up the scandir temps
	free( results );
	results = 0;
	
	// we just configure them, the thread manager does the real work
	int ret =  ThreadStatus::manageThreads( from ? compareThread : readThread, threads );
	
	return ret;
}

/**
 * Data reader ... list of directories or files
 * -	spawn off threads to read/verify specified files/directories
 *
 * @param list		list of directories to be verified
 *
 * @return		exit status (worst exiit status from any thread)
 */
int 
readData_l( char **list ) {

	// create a work thread for each specified directory
	int threads = 0;
	for( int i = 0; list[i] != 0; i++ ) {
		bool single_file;
	
		// if there is an offset, note it and null it out
		long long offset = getOffset(list[i]);

		// make sure the assigned working directory exists 
		if (checkdev(list[i])) {
			if (loadgen_fsize == 0) {
				fprintf(stderr, "FATAL: device %s requires length specified\n",
					list[i] );
				loadgen_problem = "target device w/o length";
				return TARGET_DIRECTORY;
			}
			single_file = true;
		} else if (checkfile(list[i])) {
			single_file = true;
		} else {
			const char *err = checkdir(list[i], false);
			if (err) {
				fprintf(stderr, "FATAL: target directory %s: %s\n", 
					list[i], err );
				loadgen_problem = "target directory access";
				return TARGET_DIRECTORY;
			} 
			single_file = false;
		}
		
		// come up with a name for this thread
		char *threadname = 0;
		asprintf( &threadname, "Verifier Thread %04d", i );
	
		// allocate and initialize a parameter structure
		struct readParms *parms = new readParms( threadname, loadgen_bsize, 0, list[i] );
		if (threadname == 0 || parms == 0) {
			fprintf(stderr, "FATAL: malloc failure\n");
			loadgen_problem = "malloc failure";
			return RESOURCE_ERROR;
		}

		// plug in a few additional parameters
		parms->bytes_to_read = loadgen_data;
		parms->aio_depth = loadgen_depth;
		parms->offset = offset;
		parms->one_file = single_file;

		threads++;
	}

	// we just configure them, the thread manager does the real work
	int ret =  ThreadStatus::manageThreads( readThread, threads );
	
	return ret;
}

/**
 * this is the routine that each load generation thread runs 
 *	to verify pattern data.
 *
 * @param	ThreadStatus structure for this thread
 */
void *readThread( void *sts ) {
	int status = 0;		// this thread's exit status
	char *data = 0;		// the buffer we read into
	int count = 0;		// number of directory entries to process
	int done = 0;		// number of files actually processed
	struct dirent **results = 0;	// returned directory entries

	// pick up ponter to my status structure
	struct ThreadStatus *mystatus = (struct ThreadStatus *) sts;
	struct readParms *myparms = (struct readParms *) mystatus->parms;
	int alignment = loadgen_direct > 0 ? loadgen_direct : DEFAULT_ALIGNMENT;
	long bufsize = myparms->block_size;
	if (bufsize == 0)
		bufsize = max_bsize();

	// announce that we are starting up
	mystatus->running = true;	// now set in manageThreads to avoid a race
	if (loadgen_debug & D_THREADS) {
		fprintf(stderr, "# Starting %s in %s\n", mystatus->name, myparms->to_directory );
	}

	// make sure our target directory exists
	if (!myparms->one_file) {
		const char *err = checkdir( myparms->to_directory, false );
		if (err) {
			fprintf(stderr, 
				"FATAL: target directory %s: %s\n", 
				myparms->to_directory, err );
			status = TARGET_DIRECTORY;
			loadgen_problem = "target directory access";
			goto exit;
		} 
	}

	// allocate a read buffer
	if (posix_memalign( (void **) &data, alignment, bufsize ) != 0) {
		fprintf(stderr, "Unable to allocate (%ld byte) data buffer for %s\n",
			bufsize, mystatus->name );
		status |= RESOURCE_ERROR;
		loadgen_problem = "malloc failure";
		goto exit;
	}
	mlock( data, bufsize );

	// find and verify each file in this directory
	if (myparms->one_file) {
		count = 0;
		status = readFile( myparms->to_directory, data, bufsize,
				myparms->block_size, myparms->bytes_to_read, myparms->offset,
				&mystatus->stats );
		if (status == 0 && loadgen_delete) {
			if (unlink( myparms->to_directory ) != 0) {
				fprintf(stderr,
					"Unable to delete file %s: %s\n",
					myparms->to_directory, strerror( errno ) );
				status |= INPUT_FILE_ERROR;
				loadgen_problem = "file deletion error";
			} else if (loadgen_debug & D_FILES) {
				fprintf(stderr, "# Delete file %s ... OK\n", 
					myparms->to_directory );
			}
		}
	} else 
		count = scandir(  myparms->to_directory, &results, isFile, alphasort );

	for( done = 0; done < count && status == 0 && mystatus->enable; done++ ) {
		if (loadgen_shutdown)
			break;
		
		// get the next file to verify
		char *path;
		asprintf( &path, "%s/%s", myparms->to_directory, results[done]->d_name );
		
		// read (and verify) this file
		status = readFile( path, data, bufsize,
				myparms->block_size, myparms->bytes_to_read, myparms->offset, 
				&mystatus->stats );
		if (status == 0) {
			mystatus->stats.file_done();
			if (loadgen_delete) {
				if (unlink( path ) != 0) {
					fprintf(stderr,
						"Unable to delete file %s: %s\n",
						path, strerror( errno ) );
					status |= INPUT_FILE_ERROR;
					loadgen_problem = "file deletion error";
				} else if (loadgen_debug & D_FILES) {
					fprintf(stderr, "# Delete file %s ... OK\n", path );
				}
			} 
		} else {
			fprintf(stderr, "# %s of %s completed w/status=%d\n",
				loadgen_verify ? "verify" : "read", path, status);
		}
		
		// free the stuff we allocated for this file
		free( path );
		path = 0;
		free( results[done] );
		results[done] = 0;
	}

  	// free stuff we allocated
	if (data) {
		munlock( data, bufsize );
		free( data );
	}
	if (results)
		free( results );
	
	if (status == 0 && loadgen_delete && !myparms->one_file) {
		if (rmdir( myparms->to_directory ) != 0) {
			fprintf(stderr,
				"Unable to remove directory %s: %s\n",
				myparms->to_directory, strerror( errno ) );
			status |= INPUT_FILE_ERROR;
			loadgen_problem = "directory deletion error";
		} else if (loadgen_debug & D_FILES) {
			fprintf(stderr, "# Remove directory %s ... OK\n", myparms->to_directory );
		}
	}

  exit:	
	// update my exit status and exit
	mystatus->running = false;
	mystatus->exit_status = status;
	if (loadgen_debug & D_THREADS || done != count) {
		fprintf(stderr, "# Shutting down %s (en=%d, cnt=%d/%d sts=%x, stop=%d)\n",
			mystatus->name, mystatus->enable,
			done, count, status, loadgen_shutdown );
	}
	pthread_exit(0);
}

/**
 * readFile ... read (and verify) a single file (of pattern data)
 *
 * @param	file name to process
 * @param	input buffer (locked down and aligned)
 * @param	length of input buffer
 * @param	expected file block size (0 = no expectations)
 * @param 	total number of bytes to read
 * @param	base (byte) offset for all I/O to this file
 * @param	stats structure to update
 *
 * @return	error mask
 */
int readFile( const char *filename, char *inbuf, int bufsize, 
		int bsize, long long read_bytes, long long base_offset,
		perfstats *stats ) {

	// open the file
	int opts = loadgen_direct ? O_DIRECT : 0;
	int fd = open( filename, opts );
	if (fd < 0) {
		fprintf(stderr, "Unable to open input file %s: %s\n", 
			filename, strerror( errno ) );
		loadgen_problem = "file open failure";
		return INPUT_FILE_ERROR;
	} 

	// read in the first block
	long firstblk = loadgen_direct ? loadgen_direct : header_size();

	if (read( fd, inbuf, firstblk ) != firstblk) {
		fprintf(stderr, "Header read error on input file %s: %s\n", 
			filename, strerror( errno ));
		loadgen_problem = "file read error";
		close(fd);
		return INPUT_FILE_ERROR;
	}

	// check the headers and extract size information
	long long file_size = 0;
	if (loadgen_verify || bsize == 0 || file_size == 0) {
		const char *err = checkHeaders( inbuf, bsize, 0ULL );
		if (err) {
			fprintf(stderr, 
				"Header verification error on input file %s: %s\n",
				filename, err );
			loadgen_problem = "header verification error";
			close(fd);
			return INPUT_FILE_ERROR;
		} 
		if (loadgen_verify && loadgen_debug & D_VERIFY) {
			fprintf(stderr, "# Verify HEADERS for %s ... OK\n", filename );
		}

		
		// default file and block sizes come from the headers
		file_size = get_file_size(inbuf);
		if (bsize == 0)
			bsize = get_block_size( inbuf );
	}
	long long max_block = file_size/bsize;
	if (read_bytes == 0)
		read_bytes = file_size;

	// make sure we have a reasonable block size
	if (bsize == 0 || bsize > max_bsize() || bsize > bufsize) {
		fprintf(stderr,
			"FATAL: file %s: illegal bsize (%d): supported max (%ld)\n",
			filename, bsize, max_bsize() );
		loadgen_problem = "illegal block size";
		close(fd);
		return INPUT_FILE_ERROR;
	}

	// verify the file name and size
	const char *err = loadgen_verify ? checkFile( inbuf, filename ) : 0;
	if (err) {
		fprintf(stderr, 
			"File verification error on input file %s: %s\n",
			filename, err );
		loadgen_problem = "data verification error";
		close(fd);
		return INPUT_FILE_ERROR;
	} else if (loadgen_verify && loadgen_debug & D_VERIFY) {
		fprintf(stderr, "# Verify LENGTH for %s ... OK\n", filename );
	}


	if (loadgen_debug & D_FILES) {
		fprintf(stderr, "# %sing file %s, using bsize=%d\n", 
			loadgen_verify ? "verify" : "read", filename, bsize );
	}

	long long offset = base_offset;
	lseek( fd, offset, SEEK_SET );
	int status = 0;
	while( status == 0 && read_bytes > 0 ) {
		// figure out how much to read
		int bytes = (loadgen_rand_blk == 0) ? bsize : loadgen_rand_blk;

		bytes = timed_read( fd, inbuf, bytes, stats, filename );
		if (bytes <= 0) {
			status |= INPUT_FILE_ERROR;
			break;
		}

		// headers are verified against the actual block size
		err = loadgen_verify ? checkHeaders( inbuf, bsize, offset ) : 0;
		if (err) {
			fprintf(stderr, 
				"Header verification error on input file %s at offset %llu: %s\n",
				filename, offset, err );
			status |= INPUT_FILE_ERROR;
			loadgen_problem = "header verification error";
			break;
		}

		// data is verified against the re-read block size
		err = loadgen_verify ? checkData( inbuf, bytes ) : 0;
		if (err) {
			fprintf(stderr, 
				"Data verification error on input file %s at offset %llu: %s\n",
				filename, offset, err );
			status |= INPUT_FILE_ERROR;
			loadgen_problem = "data verification error";
			break;
		} else if (loadgen_debug & D_VERIFY) {
			fprintf(stderr, "# %s CONTENTS(%d) for %s(%llu) ... OK\n", 
				loadgen_verify ? "Verify" : "Read",
				bytes, filename, offset );
		} 

		// note that we have knocked off some of our quota
		read_bytes -= bytes;

		// and figure out where the next read should come from
		if (loadgen_rand_blk) {
			offset = base_offset + choose_block( max_block) * bsize;
			lseek( fd, offset, SEEK_SET );
		} else
			offset += bytes;
	}

	close( fd );
	return status;
}

/**
 * this is the routine that each load generation thread runs 
 *	to compare copied files
 *
 * @param	ThreadStatus structure for this thread
 */
void *compareThread( void *sts ) {
	int status = 0;		// this thread's exit status
	char *data1 = 0;	// the orginal data
	char *data2 = 0;	// the copy data
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
	const char *err = checkdir( myparms->to_directory, false );
	if (err) {
		fprintf(stderr, 
			"FATAL: target directory %s: %s\n", 
			myparms->to_directory, err );
		status = TARGET_DIRECTORY;
		loadgen_problem = "target directory access";
		goto exit;
	} 

	// make sure our source directory exists
	err = checkdir( myparms->from_directory, false );
	if (err) {
		fprintf(stderr, 
			"FATAL: source directory %s: %s\n", 
			myparms->from_directory, err );
		status = SOURCE_DIRECTORY;
		loadgen_problem = "source directory access";
		goto exit;
	} 

	// allocate our buffers
	if (posix_memalign( (void **) &data1, alignment, bsize ) != 0 ||
	    posix_memalign( (void **) &data2, alignment, bsize ) != 0 ) {
		fprintf(stderr, "Unable to allocate (%ld byte) data buffer for %s\n",
			bsize, mystatus->name );
		status |= RESOURCE_ERROR;
		loadgen_problem = "malloc failure";
		goto exit;
	}

	// find and verify each file in this directory
	struct dirent **results;
	count = scandir(  myparms->to_directory, &results, isFile, alphasort );
	for( done = 0; done < count && status == 0 && mystatus->enable; done++ ) {
		if (loadgen_shutdown)
			break;
		

		// open the original file
		char *from_path;
		asprintf( &from_path, "%s/%s", myparms->from_directory, results[done]->d_name );
		int opts = loadgen_direct ? O_DIRECT : 0;
		int fd_from = open( from_path, opts );
		if (fd_from < 0) {
			fprintf(stderr, "Unable to open source file %s: %s\n", 
				from_path, strerror( errno ) );
			status |= INPUT_FILE_ERROR;
			loadgen_problem = "file open failure";
			break;
		} 

		// open the copy file
		char *to_path;
		asprintf( &to_path, "%s/%s", myparms->to_directory, results[done]->d_name );
		int fd_to = open( to_path, opts );
		if (fd_to < 0) {
			fprintf(stderr, "Unable to open copy file %s: %s\n", 
				to_path, strerror( errno ) );
			status |= OUTPUT_FILE_ERROR;
			loadgen_problem = "file open failure";
			break;
		} 


		// figure out what block size to use for the comparison
		//	no smaller than the alignment
		//	no greater than the file size
		bsize = myparms->block_size;
		if (bsize == 0)
			bsize = choose_bsize( loadgen_direct, max_bsize() );

		if (loadgen_debug & D_FILES) {
			fprintf(stderr, "# compare file %s in %s to %s, bsize=%ld\n",
				results[done]->d_name, myparms->from_directory,
				myparms->to_directory, bsize);
		}

		// verify the file contents
		while( status == 0 ) {
			// read and verify another block
			int bytes = read( fd_from, data1, bsize );
			if (bytes == 0)
				break;
			if (bytes < 0) {
				fprintf(stderr, 
					"Data read error on source file %s: %s\n",
					from_path, strerror( errno ));
				status |= INPUT_FILE_ERROR;
				loadgen_problem = "file read error";
				break;
			}

			int bytes2 = timed_read( fd_to, data2, bytes, &mystatus->stats, from_path );
			if (bytes2 < 0) {
				fprintf(stderr, 
					"Data read error on copy file %s: %s\n",
					to_path, strerror( errno ));
				status |= INPUT_FILE_ERROR;
				loadgen_problem = "file read error";
				break;
			} else if (bytes2 != bytes) {
				fprintf(stderr, 
					"Short read on copy file %s\n", to_path );
				status |= INPUT_FILE_ERROR;
				loadgen_problem = "file read error";
				break;
			}

			if (memcmp(data1, data2, bytes) != 0) {
				fprintf(stderr,
					"Data comparison error on file %s\n", to_path );
				status |= INPUT_FILE_ERROR;
				loadgen_problem = "copy comparison error";
				break;
			}
		}

		close( fd_to );
		close( fd_from );

		if (status == 0) {
			mystatus->stats.file_done();
			if (loadgen_delete) {
				if (unlink( to_path ) != 0) {
					fprintf(stderr,
						"Unable to delete file %s: %s\n",
						to_path, strerror( errno ) );
					status |= INPUT_FILE_ERROR;
					loadgen_problem = "file deletion error";
				} else if (loadgen_debug & D_FILES) {
					fprintf(stderr, "# Delete file %s ... OK\n", to_path );
				}
			}
		}
		
		// free the stuff we allocated for this file
		free( to_path );
		to_path = 0;
		free( from_path );
		from_path = 0;
		free( results[done] );
		results[done] = 0;
	}

  	// free stuff we allocated
	if (data1)
		free( data1 );
	if (data2)
		free( data2 );
	if (results)
		free( results );
	
	if (status == 0 && loadgen_delete) {
		if (rmdir( myparms->to_directory ) != 0) {
			fprintf(stderr,
				"Unable to remove directory %s: %s\n",
				myparms->to_directory, strerror( errno ) );
			status |= INPUT_FILE_ERROR;
			loadgen_problem = "directory deletion error";
		} else if (loadgen_debug & D_FILES) {
			fprintf(stderr, "# Remove directory %s ... OK\n", myparms->to_directory );
		}
	}

  exit:	
	// update my exit status and exit
	mystatus->running = false;
	mystatus->exit_status = status;
	if (loadgen_debug & D_THREADS || done != count) {
		fprintf(stderr, "# Shutting down %s (en=%d, cnt=%d/%d sts=%x, stop=%d)\n",
			mystatus->name, mystatus->enable,
			done, count, status, loadgen_shutdown );
	}
	pthread_exit(0);
}

