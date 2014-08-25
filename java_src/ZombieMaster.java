import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import java.util.Calendar;

/**
 * a ZombieMaster is a console that controls a horde
 * of load generation zombies.
 */
public class ZombieMaster implements WindowListener {
	
	private static final String iconImage = "zombie.jpg";
	private static final String ZOMBIE_HORDE = "LOAD_ZOMBIE_HORDE";	
	private static final String SWITCH_CHAR = "-";

	private Boolean shutdown;
	private Horde myHorde;
	
	/**
	 * process the arguments, figure out where our horde is,
	 * and create a console that will actually drive the horde.
	 * 
	 * @param args	
	 */
	public static void main( String args[]) {
		String scenario = null;
		Options opts = Options.getInstance();
		
		// process the arguments
		for( int i = 1; i < args.length; i++ ) {
			if (args[i].startsWith(SWITCH_CHAR)) {	
				String s = args[i].substring(1);
				if (s.startsWith(SWITCH_CHAR))
					s = s.substring(1);	// may have two dashes
				if( !opts.parseSwitch( s )) {
					// note that now all unrecognized options are passed on
					System.err.println("Unrecognized option: " + args[i]);
					System.exit(-1);
				}
			} else {
				try {
					int n = Integer.parseInt(args[i]);
					opts.numZombies = n;	// number of zombies to simulate
				} catch (NumberFormatException e) {
					scenario = args[i];		// scenario to run
				}
			}
		}
		
		// do we need to go to a default configuration?
		if (scenario == null && System.getenv( ZOMBIE_HORDE ) != null) {
			scenario = System.getenv( ZOMBIE_HORDE );
			System.err.println("Using default configuration file: " + scenario);
		}
		
		// put a time stamp at the front of our log
		if (opts.log) {
			Calendar cal = Calendar.getInstance();
			String date = String.format("%02d/%02d/%04d %02d:%02d:%02d",
				cal.get(Calendar.MONTH),
				cal.get(Calendar.DAY_OF_MONTH),
				cal.get(Calendar.YEAR),
				cal.get(Calendar.HOUR_OF_DAY),
				cal.get(Calendar.MINUTE),
				cal.get(Calendar.SECOND) );
			System.out.println("# Zombie Master Started " + date );
		}

		// assemble the described horde
		Horde horde = (scenario == null) ? new Horde() : new Horde( scenario );
		if (horde == null || horde.numZombies == 0)
			System.exit(-1);
		
		// figure out what to call ourselves
		if (opts.title == null)
			opts.title = (scenario == null) ? 
					"local simulation" : scenario;

		// and instantiate a manager
		new ZombieMaster( horde );

		// when we fall out of the loop, we're done
		System.exit(0);
	}

	/**
	 * main action code for the ZombieMaster
	 * <UL>
	 *	<LI> create a window
	 *	<LI> put a console in it
	 *	<LI> process zombie status reports until we are done
	 * </UL>
	 */
	public ZombieMaster( Horde horde ) 
	{	
		myHorde = horde;
		shutdown = false;
		Options opts = Options.getInstance();

		// create and decorate a window for ourselves
		JFrame f = new JFrame();
		f.setTitle( opts.title );
		java.net.URL url = ZombieMaster.class.getResource(iconImage);
		Image myIcon = Toolkit.getDefaultToolkit().getImage( url );
		f.setIconImage( myIcon );

		// put up all the pretty widgets 
		new Console( f.getContentPane(), horde );
		f.pack();
		f.setVisible( true );
		
		// arrange to get control when the user closes it
		f.setDefaultCloseOperation(WindowConstants.DO_NOTHING_ON_CLOSE);
		f.addWindowListener( this );

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
				Thread.sleep( opts.update/2 );
			} catch (InterruptedException e) { 
				// I don't think we care
			}
		}
	}

	/**
	 * When our window is shut down, we have to tell
	 * all of our zombies to stop.
	 */
	public void windowClosing(WindowEvent arg0) {
		for( int i = 0; i < myHorde.numZombies; i++ )
			myHorde.zombies[i].stop();
		shutdown = true;
		// the actual shutdown will happen when the horde has stopped
		// FIX ... is there a way to disable the STOP button?
	}

	// Window events I don't care about
	public void windowActivated(WindowEvent arg0) {}
	public void windowClosed(WindowEvent arg0) {}
	public void windowDeactivated(WindowEvent arg0) {}
	public void windowDeiconified(WindowEvent arg0) {}
	public void windowIconified(WindowEvent arg0) {}
	public void windowOpened(WindowEvent arg0) {}	
}
