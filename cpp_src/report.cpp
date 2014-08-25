#include <stdio.h>
#include <time.h>

#include "loadgen.h"
#include "perfstats.h"

/* initialize the class statics	*/
long *perfstats::limits = 0;
int perfstats::num_buckets = 0;

/**
 * generate an activity/bandwidth report
 */
void 
report( int threads, long microseconds, perfstats *s ) {
	
	// print out a date/time header
	time_t now;
	time( &now );
	struct tm *tm = localtime( &now );
	fprintf(stdout, "REPORT date=%02d/%02d/%04d time=%02d:%02d:%02d ",
		tm->tm_mon+1, tm->tm_mday, tm->tm_year+1900,
		tm->tm_hour, tm->tm_min, tm->tm_sec );

	// print out the thread-tag
	if (loadgen_tag)
		fprintf(stdout, "tag=%s ", loadgen_tag );

	// print out the number of threads
	fprintf(stdout, "threads=%d ", threads );

	// print out the achieved throughput
	if (s->total_bytes > 0 || threads > 0) {
		long secs = (microseconds + 500000)/1000000;
		fprintf(stdout, "bytes=%lld seconds=%ld rate=%lld ",
			s->total_bytes, secs, (s->total_bytes * 1000000) / microseconds );

#ifdef OBSOLETE
		fprintf(stdout, "min_us=%llu max_us=%llu ",
			s->min_time, s->max_time );
#endif

		fprintf(stdout, "us_buckets=");
		for( int i = 0; i < perfstats::num_buckets; i++ ) {
			fprintf(stdout, i == 0 ? "%ld" : ",%ld", s->buckets[i] );
		}
		
	} 
	fprintf(stdout, "\n");
	fflush( stdout );
}
