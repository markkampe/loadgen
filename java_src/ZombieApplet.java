
import javax.swing.*;

/**
 * a ZombieApplet is an applet form of a ZombieMaster
 * (which is an application).  The differences are between
 * the ZombieApplet and the ZombieMaster application are:
 * <OL type=a>
 * 	<LI> life-cycle entry points
 * 	<LI> how we find our configuration file
 * 	<LI> management of the root window
 *  <LI> the creation of a message processing thread
 * </OL>
 */
public class ZombieApplet extends JApplet implements Runnable {
	private static final long serialVersionUID = 1L;
	
	private Boolean shutdown;
	private Horde myHorde;
	private Thread messageThread;

	public ZombieApplet() {
		shutdown = false;
		myHorde = null;
	}
	
	/**
	 * called when we are started
	 * <UL>
	 * 	<LI> get our scenario file
	 * 	<LI> instantiate the horde
	 * 	<LI> create the GUI
	 * 	<LI> crate a thread to process status reports
	 * </UL>
	 */
	 public void start() {
		// find our horde configuration file
		String scenario = getParameter("scenario");
		if (scenario == null) {
			System.err.println("No scenario!");
			return;
		} 
		
		// instantiate the horde
		showStatus("Instantiating horde from " + scenario );
		myHorde = new Horde(scenario);
		if (myHorde == null || myHorde.numZombies == 0) {
			System.err.println("Unable to instantiate horde from " + scenario);
			return;
		}
		
		// instantiate the GUI
		new Console( getContentPane(), myHorde );
		
		// start up a message processing thread
		messageThread = new Thread(this);
		messageThread.start();
	}
	
	/**
	 * called when the user is through with this applet
	 * (just send a stop to all of the zombies in the horde)
	 */
	public void stop() {
		showStatus("Shutting down horde");
		for( int i = 0; i < myHorde.numZombies; i++ )
			myHorde.zombies[i].stop();
		shutdown = true;
	}

	/**
	 * main message processing loop
	 */
	public void run() 
	{	
		// figure out how often we should poll
		int pollWait = Options.getInstance().update/2;

		// gather reports as long as I have horde to generate them
		//	and leave them up until the GUI is closed down
		boolean someRunning = true;
		while( someRunning || !shutdown ) {
			someRunning = false;
			for( int i = 0; i < myHorde.numZombies; i++ ) {
				// check for input
				if (myHorde.zombies[i].check())
					someRunning = true;
			}
			try {
				Thread.sleep( pollWait );
			} catch (InterruptedException e) { 
				// I don't think we care
			}
		}
	}
}
