#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>

#include "loadgen.h"
#include "debug.h"

#define STDIN	0

// longest expected input line
#define MAX_LINE	128

// largest reasonable number of threads to run
#define	MAX_THREADS	999

/*
 * wait for a command to change the number of threads
 *
 *	waits no more than maxSeconds for a command
 *
 *	if an x, q, or EOF is entered, it is treated as a shutdown
 *	if a d is entered, disconnect from stdin (it has no more to say)
 #	if a number is entered, return it as a new thread count
 *
 *	returns a new thread count, or -1 if we timed out
 */
int changeNumThreads( int maxSeconds ) {

	static bool disconnect = false;

	// if we aren't a zombie or have no connected input
	if (disconnect || !loadgen_zombie) {
		sleep( maxSeconds );
		return( -1 );
	}
		
	// set up our poll arguments
	struct pollfd pollfds;
	pollfds.fd = STDIN;
	pollfds.events = POLLIN;

	char inbuf[MAX_LINE];

	// wait for input or the specified delay period
	int count = poll( &pollfds, 1, maxSeconds * 1000 );
	if (count == 0)	// timed out
		return( -1 );	

	// did we get a HUP from stdin?
	if (pollfds.revents & POLLHUP) {
		disconnect = true;
		loadgen_shutdown = true;
		if (loadgen_debug & D_CMDS)
			fprintf(stderr, "# STDIN <- HUP\n");
		return 0;
	}

	// did we get character input
	if (pollfds.revents & POLLIN) {
		char c;
		int got = 0;
		do {	// try to read one line from stdin
			if (read(0, &inbuf[got], 1) != 1) 
				break;
			c = inbuf[got++];
		} while (c != '\n' && got < MAX_LINE - 2);
		inbuf[got] = 0;

		// see if it is an EOF or shutdown command
		c = inbuf[0];
		if (got == 0 || c == 'x' || c == 'q' || c == 'X' || c == 'Q') {
			loadgen_shutdown = true;
			if (loadgen_debug & D_CMDS)
				fprintf(stderr, "# STDIN <- quit\n");
			return 0;
		}

		// see if it is a disconnect command
		if (c == 'd' || c == 'D') {
			disconnect = true;
			if (loadgen_debug & D_CMDS)
				fprintf(stderr, "# STDIN <- disconnect\n");
			return( -1 );
		}

		// try to parse it as a number
		int n = atoi(inbuf);
		if (loadgen_debug & D_CMDS)
			fprintf(stderr, "# STDIN <- %d\n", n);
		if (n >= 0 && n < MAX_THREADS)
			return( n );
	}

	// something else seems to have happened here
	fprintf(stderr, "# Unexpected Poll Exit!\n");
	return( -1 );
}
