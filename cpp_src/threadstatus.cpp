#include <stdio.h>
#include <malloc.h>
#include <unistd.h>

#include "loadgen.h"
#include "threadstatus.h"
#include "debug.h"

// real implementations of the ThreadStatus class
ThreadStatus *ThreadStatus::listHead = 0;	// head of list descriptors

/*
 * allocate and initialize a new threadStatus 
 */
ThreadStatus::ThreadStatus( char *threadName, void *threadParms ) {
	// initialize the new descriptor
	name = threadName;
	parms = threadParms;
	enable = false;
	started = false;
	running = false;
	exit_status = 0;
	_next = 0;

	// add us to the end of the chain
	ThreadStatus *t;
	for( t = listHead; t && t->_next; t = t->_next);
	if (t)
		t->_next = this;
	else
		listHead = this;
}

/*
 * discard the descriptor for an ex-thread
 */
ThreadStatus::~ThreadStatus() {
	// find the element that points to us
	ThreadStatus *prev = 0;
	for( ThreadStatus *t = listHead; t && t->_next && t != this; t = t->_next )
		prev = t;

	// take us out of the chain
	if (prev == 0) {
		if (listHead == this) {
			listHead = _next;
		}
	} else if (prev->_next == this) {
		prev->_next = _next;
	} 
	_next = 0;

	// free up the stuff we point to
	free( name );
	name = 0;
	free( parms );
	parms = 0;
}
/**
 * The thread manager doesn't actually understand what these
 * threads are supposed to be doing.  It merely:
 *   - tries to keep the right number of them running
 *   - generates periodic throughput report
 *   - collects exit status
 *<br>
 * What those actually threads do is controlled by:
 *  - the thread service routine
 *  - the supplied parameters
 * both of which come from the caller
 */
int 
ThreadStatus:: manageThreads( void *(*routine)(void *), int threads, int stacksize ) {
	int status = 0;		// return status
	int wanted = threads;	// number of threads I am targeting
	int enabled = 0;	// number of threads currently enabled
	int running = 0;	// number of threads currently running
	int available = 0;	// number of threads ready to be started

	// count up the defined threads
	for( ThreadStatus *t = ThreadStatus::first(); t != 0; t = t->next() )
		available++;
	
	// initialize our throughput computation
	perfstats prev, sum, delta;
	hires_time_t time_now = hires_time();
	hires_time_t time_prev = time_now;

	/*
	 * main loop (until we shut down)
	 *
	 * warning - possible race condition
	 *
	 *	If a thread is started and no longer running, we assume that it has
	 *	terminated and harvest its status.  If the running status were set
	 *	in the thread start-up function, there would be a low likelihood
	 *	race condition where we made it around the main loop before the
	 *	thread started, think it had shut down, disable it and harvest
	 *	its status before it could even start.
	 *
	 *	This problem started happening when I created some very fast test
	 *	cases in detached zombie mode.  We prevent this by setting running 
	 *	when the thread is created (here) rather than in the thread start-up 
	 *	function.  The only down-side is that we cannot detect the 
	 *	(extremely unlikely) situation where (for some reason) the thread 
	 *	start-up function never gets called.
	 */
	do {
		// see if we've been told to shut down
		if (loadgen_shutdown) {
			wanted = 0;		// we don't want any more threads
			available = 0;		// and we don't have any anyway
		}

		// count threads still running and harvest status 
		for( ThreadStatus *t = ThreadStatus::first(); t != 0; t = t->next() ) {
			if (t->started && !t->running) {
				if (t->enable == true) {	// see race condition note
					t->enable = false;
					enabled--;
					if (loadgen_debug & D_THREADS) 
						fprintf(stderr, "# disable and harvest %s\n", t->name);
				}

				// check fo rerrors
				status |= t->exit_status;
				if (status != 0 && loadgen_halt)
					loadgen_shutdown = true;
			} 
		}
		
		// see if we need to start-up any new threads
		while( enabled < wanted && available > 0) {
			ThreadStatus *sts = 0;

			// find the first not-yet-run thread
			for( sts = ThreadStatus::first(); sts != 0 && sts->started; sts = sts->next() );

			// we have already run all of the threads we were asked to manage
			if (sts == 0)
				break;
			
			// kick off a new (detached) thread to serve it
			sts->enable = true;
			pthread_attr_t attr;
			pthread_attr_init( &attr );
			pthread_attr_setstacksize( &attr, stacksize );
			pthread_attr_setdetachstate( &attr, true );
			int ret = pthread_create( &sts->thread, &attr, routine, (void *) sts );
			pthread_attr_destroy( &attr );
			if (ret == 0) {
				enabled++;
				available--;
				sts->started = true;
				sts->running = true;	// this prevents the above race
				if (loadgen_debug & D_THREADS)
					fprintf(stderr, "# enabling new thread %s\n", sts->name);
				continue;
			} 
			fprintf(stderr, "Thread creation failure; enabled=%d, wanted=%d\n",
				enabled, wanted );
			break;
		}

		// see if we need to shut down any threads
		while( enabled > wanted ) {
			// find the first enabled thread
			ThreadStatus *t;
			for( t = ThreadStatus::first(); t != 0 && !t->enable; t = t->next() );
			if ( t && t->enable ) {
				t->enable = false;
				enabled--;
				if (loadgen_debug & D_THREADS)
					fprintf(stderr, "# disabling excess thread %s\n", t->name);
				continue;
			}

			fprintf(stderr, 
				"No enabled threads to shut down; enabled=%d, wanted=%d\n",
				enabled, wanted );
			break;
		}

		// wait for input, a signal, or an update interval
		int rslt = changeNumThreads( loadgen_update );
		if (rslt >= 0) {
			if (loadgen_debug & D_CMDS)
				fprintf(stderr, "# Change number of threads to %d\n", rslt );
			wanted = rslt;
		}

		// take a census and gather throughput data
		running = 0;
		sum.reset();
		time_now = hires_time();
		for( ThreadStatus *t = ThreadStatus::first(); t != 0; t = t->next() ) {
			sum += t->stats;
			if (t->running)
				running++;
		}

		// compute and report the most recently achieved bandwidth
		delta = sum;
		delta -= prev;
		hires_time_t delta_t = time_now - time_prev;
		report( running, (long) delta_t, &delta );
			
		// and reset the counters for next time
		prev = sum;
		time_prev = time_now;
	} while( available > 0 || running > 0 );
		
	// free up our thread status structures
	while( ThreadStatus *t = ThreadStatus::first() )
		delete t;
	
	return status;
}

