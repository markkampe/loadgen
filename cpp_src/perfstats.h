#include <stdio.h>

/* maximum number of latency buckets	*/
#define	MAX_LATENCY_BUCKETS	24

typedef long long unsigned hires_time_t;

/**
 * performance statistics collected from on load generation thread
 *
 */
class perfstats {
    public:
	long		total_files;	///< total files processed
	long long 	total_bytes;	///< total bytes written
	hires_time_t	total_usecs;	///< total usecs spent doing I/O
	hires_time_t	min_time;	///< minimum time per IO op
	hires_time_t	max_time;	///< maximum time per IO op
	long		*buckets;	///< latency bucket counters

	static int num_buckets;
	static long *limits;

	perfstats() {
		buckets = new long[MAX_LATENCY_BUCKETS];
		reset();
	}

	~perfstats() {
		delete[] buckets;
	}

	void reset() {
		total_files = 0;
		total_bytes = 0;
		total_usecs = 0ULL;
		min_time = 0ULL;
		max_time = 0ULL;
		
		for( int i = 0; i < MAX_LATENCY_BUCKETS; i++ )
			buckets[i] = 0L;
	}


	static void setlimits( long *p ) {
		if (limits == 0)
			limits = new long[MAX_LATENCY_BUCKETS];

		int i = 0;
		while( i < MAX_LATENCY_BUCKETS && *p > 0 )
			limits[i++] = *p++;
		num_buckets = i+1;
		while( i < MAX_LATENCY_BUCKETS )
			limits[i++] = -1;
	}

	// overload assignment operator to copy bucket values
	perfstats &operator=( const perfstats &rhs ) {
		this->total_files = rhs.total_files;
		this->total_bytes = rhs.total_bytes;
		for( int i = 0; i < MAX_LATENCY_BUCKETS; i++ )
			this->buckets[i] = rhs.buckets[i];
		this->max_time = rhs.max_time;
		this->min_time = rhs.min_time;

		return *this;
	}

	// overload += operator to aggregate statistics from multiple threads
	perfstats &operator+=( const perfstats &rhs ) {
		this->total_files += rhs.total_files;
		this->total_bytes += rhs.total_bytes;
		for( int i = 0; i < MAX_LATENCY_BUCKETS; i++ )
			this->buckets[i] += rhs.buckets[i];
		if (rhs.max_time > this->max_time)
			this->max_time = rhs.max_time;
		if (this->min_time == 0 || rhs.min_time < this->min_time)
			this->min_time = rhs.min_time;

		return *this;
	}

	// overload -= operator to compute deltas
	perfstats &operator-=( const perfstats &rhs ) {
		this->total_files -= rhs.total_files;
		this->total_bytes -= rhs.total_bytes;
		for( int i = 0; i < MAX_LATENCY_BUCKETS; i++ )
			this->buckets[i] -= rhs.buckets[i];
		if (rhs.max_time > this->max_time)
			this->max_time = rhs.max_time;
		if (this->min_time == 0 || rhs.min_time < this->min_time)
			this->min_time = rhs.min_time;

		return *this;
	}

	void xfer_done( long long bytes, hires_time_t us ) {
		// note the transfer
		total_bytes += bytes;
		total_usecs += us;

		// update the max/min transfer times
		if (min_time == 0 || min_time > us)
			min_time = us;
		if (max_time < us)
			max_time = us;
	
		// put this operation into a bucket
		int i;
		for( i = 0; i < MAX_LATENCY_BUCKETS && limits[i] > 0; i++ )
			if (us <= (unsigned long) limits[i])
				break;
		buckets[i]++;
	}

	void file_done() {
		total_files++;
	}
};

extern hires_time_t hires_time();
extern int timed_write( int fd, char *buf, int len, perfstats *s, const char *name, long long offset );
extern int  timed_read( int fd, char *buf, int len, perfstats *s, const char *name );
extern void report( int threads, long microseconds, perfstats *s );

