#include <pthread.h>
#include "perfstats.h"

/**
 * Each load generation thread is represented by one of these
 * structures, which both passes control information and captures
 * current execution status.  The descriptors for the running 
 * threads are kept in a linked list.
 *<P>
 * Note that no serialization is required.
 *      The list is only accessed by a single thread, and never
 *	from a signal handler.
 */
class ThreadStatus {
   public:
	// identification/configuraiton information
	char 	*name;		///< display name of this thread
	void	*parms;		///< control parameters for this thread

	// enable/status information
	bool	enable;		///< enable/shut-down indication
	bool	started;	///< this thread has been started
	bool	running;	///< running/terminated indication
	int	exit_status;	///< exit status after termination
	perfstats stats;	///< how much data has been transferred

	pthread_t thread;	///< ID of this thread (if running)

	/**
	 * allocate and initialize a new threadStatus 
	 */
	ThreadStatus( char *threadName, void *threadParms );

	/**
	 * discard the descriptor for an ex-thread
	 */
	~ThreadStatus();

	/**
	 *  after that caller has created a list of ThreadStatus objects
	 *  (big enough to handle any order), this manager is called to
	 *  keep the right number running and generate reports.
	 *
	 * @return exit status (or of status bits)
	 */
	static int manageThreads( void *(*routine)(void *), int initThreads = 1, int stacksize = (64*1024) );

   private:
	/** 
	 * descriptor list traversal
	 *
	 * @return	descriptor for the first thread
	 */
	static ThreadStatus *first() { return listHead; }

	/** 
	 * descriptor list traversal
	 *
	 * @return	descriptor for the next thread in the list;
	 */
	ThreadStatus *next() { return _next; }
	
	ThreadStatus *_next;		///< next descriptor in list
	static ThreadStatus *listHead;	///< head of list of descriptors
};
