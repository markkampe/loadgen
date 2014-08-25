#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>

#include "perfstats.h"
#include "loadgen.h"
#include "debug.h"

/**
 * get the high resolution time
 *
 *	this is the same info returned by gettimeofday, but in a
 *	form that can be arithmetically manipulated
 *
 * @return long long unsigned
 */
hires_time_t hires_time() {
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return( (tv.tv_sec * 1000000) + tv.tv_usec );
}

/**
 * write out a buffer, time the operation, and update perf stistics
 *
 * @param fd	file descriptor (-1 means simulated write)
 * @param buf	buffer of data to be written
 * @param len	length of buffer to be written
 * @param stats	perfstats buffer to be updated
 * @param name	name of file being written
 *
 * @return int	status bits
 */
int timed_write( int fd, char *buf, int len, perfstats *s, const char *name, long long offset ) 
{
	// note the starting time of this operation
	hires_time_t start = hires_time();

	// write the next block
	ssize_t ret = (fd < 0) ? len : write( fd, buf, len);
	if (ret != len) {
		fprintf(stderr,"write error to file %s: %s\n", 
				name, strerror(errno));
		loadgen_problem = "file write error";
		return( OUTPUT_FILE_ERROR );
	}

	// figure out how long it took
	hires_time_t elapsed = hires_time() - start;
	s->xfer_done( len, elapsed );
	if (loadgen_debug & D_WRITES) {
		fprintf(stderr, "# Write %d bytes to %s(%llu)\n", len,  name, offset );
	}

	// see if we need to stall before the next operation
	if (loadgen_rate > 0) {
		hires_time_t expected_us = (1000000ULL *  (hires_time_t) len) / (hires_time_t) loadgen_rate;
		if (expected_us > elapsed) {
			hires_time_t needed_us = expected_us - elapsed;
			if (loadgen_debug & D_SLEEP) {
				fprintf(stderr, 
					"# sleep %lluus (=%llu-%llu)\n",
					needed_us, expected_us, elapsed );
			}
			usleep( (long) needed_us );
		}
	}

	return( 0 );
}


/**
 * read buffer, time the operation, and update perf stistics
 *
 * @param fd	file descriptor (-1 means simulated read)
 * @param buf	buffer of data to be read
 * @param len	length of buffer to be read
 * @param stats	perfstats buffer to be updated
 * @param name	name of file being read
 *
 * @return int	bytes read
 */
int timed_read( int fd, char *buf, int len, perfstats *s, const char *name ) 
{
	// note the starting time of this operation
	hires_time_t start = hires_time();

	// write the next block
	ssize_t ret = (fd < 0) ? len : read( fd, buf, len);
	if (ret < 0) {
		fprintf(stderr, 
			"Data read error on input file %s: %s\n",
					name, strerror( errno ));
		loadgen_problem = "file read error";
		return( ret );
	}

	// figure out how long it took
	hires_time_t elapsed = hires_time() - start;
	s->xfer_done( ret, elapsed );

	// see if we need to stall before the next operation
	if (loadgen_rate > 0) {
		hires_time_t expected_us = (1000000ULL *  (hires_time_t) len) / (hires_time_t) loadgen_rate;
		if (expected_us > elapsed) {
			hires_time_t needed_us = expected_us - elapsed;
			if (loadgen_debug & D_SLEEP) {
				fprintf(stderr, 
					"# sleep %lluus (=%llu-%llu)\n",
					needed_us, expected_us, elapsed );
			}
			usleep( (long) needed_us );
		}
	}

	return( ret );
}
