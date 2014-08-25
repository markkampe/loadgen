#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "loadgen.h"
#include "debug.h"

/*
 * routine:	checkdir
 *
 * purpose:	to make sure that a specified directory exists and
 *		is writable, creating it if necessary
 *
 * parameters:	name of desired directory
 *		whether or not we need to create files in it
 *
 * returns:	error string (or NULL if successful)
 */
const char *
checkdir( const char *dirname, bool create ) {

	struct stat statb;
	if (stat( dirname, &statb ) == 0) {
		if (!S_ISDIR(statb.st_mode))
			return( "not a directory" );
		if (access(dirname, R_OK|X_OK) != 0)
			return( "no access" );
		if (create && access(dirname, W_OK) != 0)
			return( "no write access" );
	} else if (create) {
		if (loadgen_simulate == 0 && mkdir( dirname, 0777 ) != 0)
			return( "unable to create" );
		if (loadgen_debug & D_FILES) {
			fprintf(stderr, "# Creating target directory %s\n", dirname );
		}
	} else 
		return( "no such directory" );

	return 0;	// otherwise, it looks good
}

/*
 * routine:	checkdev
 *
 * purpose:	to determine whether or not a name references a special file
 *
 * returns:	true/false
 *
 * Note: only check for block because char disks are deprecated
 */
bool checkdev( const char *devname ) {

	struct stat statb;
	if (stat( devname, &statb ) == 0 && S_ISBLK(statb.st_mode))
		return( true );

	return( false );
}

/*
 * routine:	checkfile
 *
 * purpose:	to determine whether or not a name references an existing file
 *
 * returns:	true/false
 */
bool checkfile( const char *filename ) {

	struct stat statb;
	if (stat( filename, &statb ) == 0 && S_ISREG(statb.st_mode))
		return( true );

	return( false );
}
