

import java.net.*;
import java.io.*;

/**
 * a Zombie is local proxy for a load generator instance
 */
public class Zombie {
	
	//* values for (external) zombie status
	public enum Status { NONE, OK, WEDGED, DONE, FAILED }

	//* externally known operations
	public enum Ops { CREATE, COPY, VERIFY, VERIFY_DELETE }
	
	//* internal zombie states
	private enum State { UNKNOWN, READY, STARTED, RUNNING, STOPPED, DONE, FAILED }

	// standard message headers from loadgen
	private static final String ZOMBIE_GREETING = "Yes Master?";
	private static final String ZOMBIE_OBEYS    = "Yes Master!";
	private static final String ZOMBIE_EXITS    = "Yes Master.";
	private static final String ZOMBIE_DIES     = "Arg Master.";
	private static final String ZOMBIE_REPORT   = "REPORT";
	private static final String ZOMBIE_COMMENT	= "#";
	
	public final String tag;		// nick-name for this zombie
	private final String args;		// misc args for this Zombie
	private final int updatePeriod;		// period between updates
	
	private Socket socket;			// communications to this zombie
	private PrintWriter out;		// output port
	private BufferedReader in;		// input port
	
	private Status myStatus;		// current status
	private State myState;			// current state
	private long lastUpdate;		// time of last update
	
	private ZombieListener notify;	// who to notify of changes
	
	private int nominalThreads;		// number of threads we have asked for
	private int actualThreads;		// last number of threads reported
	private long recentThroughput;	// most recent throughput value
	
	/**
	 * create a descriptor for a new Zombie
	 * 
	 * @param nickname	display name for this zombie
	 * @param ip_addr	IP address of this zombie
	 * @param ip_port	port for this zombie
	 * @param update	reporting interval (in seconds)
	 * @param misc		misc args for this zombie
	 */
	public Zombie( String nickname, 
			String ip_addr, int ip_port, int update,
			String misc ) {
		tag = nickname;
		args = misc;
		nominalThreads = 0;
		actualThreads = 0;
		updatePeriod = update;
		notify = null;
		
		myStatus = Status.NONE;
		myState = State.UNKNOWN;
		
		// open the connection
		try {
			socket = new Socket(ip_addr, ip_port);
			out = new PrintWriter(socket.getOutputStream(), true);
			in  = new BufferedReader( new InputStreamReader( socket.getInputStream()));
		} catch (UnknownHostException e) {
			System.err.println(tag + " !! " + ip_addr  + ":" + e);
			myStatus = Status.FAILED;
			myState = State.FAILED;
			return;
		} catch (IOException e) {
			System.err.println(tag + " at " + ip_addr + "(" + ip_port + ") " + e);
			myStatus = Status.FAILED;
			myState = State.FAILED;
			return;
		}
		
		Options opts = Options.getInstance();
		if (opts.log) {
			System.out.println("# Zombie " + tag + " at " + ip_addr + "(" + ip_port + ")");
		}
	}
	
	/**
	 * register a listener for status/performance events
	 * 
	 * @param who	object to notify of status/performance events
	 */
	public void addListener( ZombieListener who ) {
		notify = who;
	}
	
	/**
	 * pass a new zombie the initial parameters to start him running
	 * 
	 * @param block_size	size of each write
	 * @param file_size	maximum file size
	 * @param bandwidth	nominal transfer rate
	 * 
	 * @return 	weather or not Zombie has been started
	 */
	public boolean start( Ops operation, 
		int block_size, long file_size, long bandwidth ) {
		
		// we can only start if we haven't yet started or failed
		if (myState != State.READY && myState != State.UNKNOWN)
			return false;
		
		// start with the parms from the configuraiton file
		String command = args + " --tag=" + tag;

		// add command specific parameters
		switch (operation) {
		    case VERIFY_DELETE:
			command += " --delete";
		    case VERIFY:
			command += " --verify";
			break;
		}

		// optional scenario parameters 
		if (block_size != 0)
			command +=	 " --bsize=" + block_size;
		if (file_size != 0)
			command +=	 " --length=" + file_size;
		if (bandwidth > 0)
			command +=  "  --rate=" + bandwidth;
		
		// interaction/diagnostic parameters
		command += " --update=" + updatePeriod/1000;	// ms -> seconds
		command += " --halt";
		if (Options.getInstance().log) {
			System.out.println(tag + " <- " + command );
		} else {
			command +=  "  --debug=0";
		}
		
		// send the command to loadgen
		out.println( command );
		
		// status is unknown until we get vack a response
		myStatus = Status.NONE;
		myState = State.STARTED;
		if (notify != null)
			notify.statusChange( this );
		return true;
	}
	
	/**
	 * process any input, and update status/performance accordingly
	 * 
	 * @return	whether or not this zombie is still functional
	 */
	public boolean check() {
		// after we're done, we expect no further input
		if (myStatus == Status.DONE || myStatus == Status.FAILED)
			return false;
		
		// get our debugging options
		Options opts = Options.getInstance();
		
		// process input as long as we have it
		while( true ) {
			try {
				if (in.ready()) {
					String report = in.readLine();
					if (report == null)
						break;
					
					
					// ignore informational comments
					if (report.startsWith(ZOMBIE_COMMENT)) {
						if (opts.log)
							System.out.println(tag + " -> " + report );
						continue;
					}
			
					// note recentness of this update
					lastUpdate = System.currentTimeMillis();
					
					// is it a greeting?
					if (report.indexOf(ZOMBIE_GREETING) >= 0) {
						if (opts.log)
							System.out.println("# ZOMBIE " + tag + " ready" );
						if (myState == State.UNKNOWN)
							myState = State.READY;
						if (myStatus == Status.NONE)
							myStatus = Status.OK;
						if (notify != null)
							notify.statusChange( this );
						continue;
					}
					
					// is it a start-up ACK?
					if (report.indexOf(ZOMBIE_OBEYS) >= 0) {
						if (opts.log)
							System.out.println("# ZOMBIE " + tag + " started" );
						if (myState == State.STARTED)
							myState = State.RUNNING;
						myStatus = Status.OK;
						if (notify != null)
							notify.statusChange( this );
						continue;
					}
					
					// is it a shutdown report
					if (report.indexOf(ZOMBIE_EXITS) >= 0) {
						if (opts.log)
							System.out.println("# ZOMBIE " + tag + " finished" );
						myState = State.DONE;
						myStatus = Status.DONE;
						recentThroughput = 0;
						closem();
						if (notify != null) {
							notify.statusChange( this );	
							notify.perfReport( this, lastUpdate + updatePeriod );
						}
						break;
					}
					
					// is it a failure report
					if (report.indexOf(ZOMBIE_DIES) >= 0) {
						myStatus = Status.FAILED;
						myState = State.FAILED;
						recentThroughput = 0;
						closem();
						if (notify != null) {
							notify.statusChange( this );
							notify.perfReport( this, lastUpdate + updatePeriod );
						}
							
						// pass on the error
						String cause = report.substring(ZOMBIE_DIES.length());
						System.err.println( "ZOMBIE " + tag + 
								" has failed: " + cause );
						break;
					}
					
					if (!throughput_report( report )) {
						System.err.println( "Unrecognized message from " + tag + 
								": " + report );
					}
					continue;
				} else if (myState == State.RUNNING || myState == State.STOPPED){
					// see if it has been a long time since we heard from him
					long now = System.currentTimeMillis();
					long sinceLast = now - lastUpdate;
					if (sinceLast > 2 * updatePeriod && myStatus != Status.NONE) {
						System.err.println( "ZOMBIE " + tag + " is incommunicado" );
						myStatus = Status.NONE;
						recentThroughput = 0;
						if (notify != null) {
							notify.statusChange( this );
							notify.perfReport( this, now );
						}
					} 
				} 
				break;
			} catch (IOException e) {
				System.err.println("ZOMBIE " + tag + " Lost connection" );
				myStatus = Status.FAILED;
				closem();
				if (notify != null)
					notify.statusChange( this );
				break;
			}	
		}
		
		// return whether or not we are still kicking
		return( myState != State.DONE && myState != State.FAILED );
	}
	
	
	/**
	 * tell a zombie to shut down all of his threads and exit
	 */
	public void stop() {
		// can't stop if we have already stopped
		if (myState == State.DONE || myState == State.STOPPED || myState == State.FAILED)
			return;
		
		
		// send a shutdown command
		out.println("x");
		
		Options opts = Options.getInstance();
		if (opts.log) {
			System.out.println( "# sent shutdown to " + tag );
		}
		
		// status us unknown until the ack comes in
		myState = State.STOPPED;
		myStatus = Status.NONE;
		if (notify != null)
			notify.statusChange( this );
	}

	/**
	 * close our input and output ports
	 */
	private void closem() {
		out.close();
		try { in.close(); } catch (IOException e) { /* don't care */ }
		try { socket.close(); } catch (IOException e) { /* don't care */ }
	}
	
	/**
	 * tell a zombie to set/change the number of threads he is running
	 * 
	 * @param numThread	desired number of threads
	 */
	public void setThreads( int numThread ) {
		// can't do this unless we're running
		if (myState != State.STARTED && myState != State.RUNNING)
			return;
		
		// see if this represents a change
		nominalThreads = numThread;
		if (actualThreads != nominalThreads) {
			out.println( nominalThreads );
			Options opts = Options.getInstance();
			if (opts.log)
				System.out.println("# sent threads=" + nominalThreads + " to " + tag);
		}
	}
	
	/**
	 * return the number of threads last reported by this zombie
	 * 
	 * @return	number of threads last reported
	 */
	public int getThreads() {
		return actualThreads;
	}
	
	/**
	 * return the current status of this Zombie
	 * 
	 * @return	current status (OK, DONE, FAILED, ...)
	 */
	public Status status() {
		return myStatus;
	}
	
	/**
	 * return the last reported throughput for this zombie
	 * 
	 * @return last reported througput in bytes per second
	 */
	public long bytesPerSecond() {
		if (myStatus == Status.DONE || myStatus == Status.FAILED)
			return 0;
		else
			return recentThroughput;
	}
	
	/**
	 * see if a message appears to be a throughput report,
	 * and if so, process it
	 * 
	 * @param msg	message to be examined
	 * 
	 * @return		true if it is a throughput report
	 */
	private boolean throughput_report( String msg ) {
		// throughput reports begin with a standard string
		if (!msg.startsWith(ZOMBIE_REPORT))
			return false;

		// valid performance reports have my tag in them
		Argument a = new Argument(msg);
		String tagValue = a.stringProperty("tag");
		if (tagValue == null || !tag.equals(tagValue))
			return false;
		
		// they probably want this message logged
		// FIX ... and I should be affixing my own time stamp
		if (Options.getInstance().log)
			System.out.println( msg );

		// pull out the number of threads actually running
		actualThreads = a.intProperty( "threads" );

		// time/bytes values are only present if work is being done
		Status newStatus = Status.OK;
		recentThroughput = 0;
		String timeValue = a.stringProperty( "seconds" );
		String bytesValue = a.stringProperty( "bytes" );
		String rateValue = a.stringProperty( "rate" );
		if (timeValue != null && bytesValue != null && rateValue != null) {
			recentThroughput = Long.parseLong(rateValue);
			if (recentThroughput == 0)
				newStatus = Status.WEDGED;
		} 

		// see if our status has changed
		if (newStatus != myStatus) {
			myStatus = newStatus;
			if (notify != null)
				notify.statusChange( this );
		}

		// and deliver the performance update
		if (notify != null) {
			long now = System.currentTimeMillis();
			notify.perfReport(this, now);
		}
		
		return true;
	}
}
