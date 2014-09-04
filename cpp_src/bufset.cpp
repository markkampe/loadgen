#include <stdlib.h>
#include <malloc.h>
#include <sys/mman.h>

#include "bufset.h"
/**
 * a buffer set is a managed set of I/O buffers with a specified
 * count, size, and alignment
 */

/**
 * allocate and lock down a buffer set
 *
 * @param	number of desired buffer
 * @param	size of each buffer
 * @param	required memory alignment
 */
Bufset::Bufset( int numbufs, int bufsize, int alignment ) {

	int totsize = numbufs * bufsize;

	if (posix_memalign( (void **) &_bufstart, alignment, totsize) == 0) {
		buffers = numbufs;
		size = bufsize;
		mlock( &_bufstart, totsize );
	} else {
		buffers = 0;
		size = 0;
		_bufstart = 0;
	}
}

/**
 * unlock and free a buffer set
 */
Bufset::~Bufset() {
	if (_bufstart != 0 && buffers != 0 && size != 0) {
		munlock( _bufstart, buffers * size );
		free( _bufstart );
	}
}

/**
 * return the address of a buffer in the set
 *
 * @param	index of the desired buffer
 * @return	byte address of the desired buffer
 *		NULL if index is invalid
 */
char *Bufset::buffer( int i ) {
	if (_bufstart != 0 && i >= 0 && i < buffers)
		return( &_bufstart[i * size] );
	else
		return( 0 );
}
