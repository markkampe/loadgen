
import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import javax.swing.event.*;

public class Console implements ActionListener, ChangeListener, ZombieListener {
	private static final long serialVersionUID = 0x01;

	// font for rendering throughput display (less arbitrary than you might think)
	private static final String METER_FONT = "Lucida Sans Typewriter Regular";
	private static final int METER_SIZE = 32;
	private static final int METER_WIDTH = 10;
	private static final String METER_FORMAT = " %08.1f";

	// throughput graph parameters
	private static final int GRAPH_SAMPLES = 100;
	private static final Color GRAPH_NORMAL = Color.gray;
	private long bandwidthPer;	// full-scale (per zombie)
	private long bandwidthTot;	// full-scale (aggregate)

	// screen layout parameters
	private static final int SEPARATION = 10;

	// the hord
	private final Horde myHorde;
	private final int numZombies;	// number created

	// collected data and status
	private Boolean running;
	private PlotPoints plot;
	private long startTime;

	// control and status widgets
	private final Container mainPane;
	private JComboBox opCombo;
	private JComboBox blockSizeCombo;
	private JComboBox fileSizeCombo;
	private JComboBox bandwidthCombo;
	private JButton startButton;
	private JButton stopButton;
	private JSpinner zSpinners[];
	private JProgressBar zThroughput[];
	private JPanel zStatus[];
	private JSpinner masterSpinner;
	private JTextField aThroughput;
	private BarGraph graph;

	//* max useful threads per client (for scale estimation)
	private static final int MAX_THREADS = 10;

	//* options for operations
	private static final String ops[] =
		{ "create/copy", "verify",  "verify/delete" };

	private static final Zombie.Ops opcodes[] =
		{ Zombie.Ops.CREATE, Zombie.Ops.VERIFY, Zombie.Ops.VERIFY_DELETE };

	//* options for bandwidth
	private static final String bandwidths[] = 
		{ "unlimited", "10K", "100K", "1M", "10M", "100M", "1G" };

	//* options for write block size
	private static final String blockSizes[] = 
		{ "assorted", "256", "512", "1024", "2K", "4K", "8K", "16K", "32K", 
		  "64K", "128K", "256K", "512K", "1M", "2M" };
		// WARNING! extending this range requires code changes
		//	256 is the the basic pattern header size
		//	2M is MAX_BSIZE in pattern.cpp

	//* options for write file size
	private static final String fileSizes[] = 
		{ "assorted", "64K", "128K", "256K", "512K", 
		  "1M", "4M", "16M", "64M", "128M", "256M", "512M", "1G" };

	/**
	 * create a control panel for this horde this horde
	 * 
	 * @param content pane for window
	 * @param horde	the zombies under my control
	 */
	public Console( Container screen, Horde horde ) {
		
		// initialize key parameters
		myHorde = horde;
		mainPane = screen;
		numZombies = myHorde.numZombies;
		running = false;
		startTime = System.currentTimeMillis();

		// create the console widgets
		plot = new PlotPoints( GRAPH_SAMPLES );
		drawConsole();
		setDefaults();
		
		// sign up for status updates
		for( int i = 0; i < numZombies; i++ ) 
			myHorde.zombies[i].addListener( this );

	}

	/**
	 * create and display all of the console widgetry
	 * <UL>
	 *   <LI> at the center: throughput over time bar-graph
	 *   <LI> on the bottom:
	 *   <UL>
	 *   	<LI> block size and file length selectors, start/stop buttons, master thread-count spinner
	 *   	<LI> a control/status stack for each zombie
	 *   	<UL>
	 *   		<LI> name
	 *   		<LI> most recent throughput
	 *   		<LI> thread count spinner
	 *  	<LI> scrollable list of recent reports
	 *   </UL>
	 * </UL>
	 */
	private void drawConsole( ) {
		// basic set-up options and start button on the left
		// (each combobox in its own Box to prevent gratuitous resizing
		JPanel controls = new JPanel( new GridLayout( 6, 2, SEPARATION, SEPARATION));

		// operation selector
		controls.add( new JLabel("operation", JLabel.LEFT));
		opCombo = new JComboBox( ops );
		opCombo.setSelectedItem(ops[0]);
		controls.add( opCombo );

		// bandwidth selector
		controls.add( new JLabel("max (bytes/sec)", JLabel.LEFT));
		bandwidthCombo = new JComboBox( bandwidths );
		bandwidthCombo.setSelectedIndex(0);
		controls.add( bandwidthCombo );

		// block size selector
		controls.add( new JLabel("write (bytes)", JLabel.LEFT));
		blockSizeCombo = new JComboBox( blockSizes );
		blockSizeCombo.setSelectedIndex(0);
		controls.add( blockSizeCombo );

		// file size selector
		fileSizeCombo = new JComboBox( fileSizes );
		fileSizeCombo.setSelectedIndex(0);
		controls.add( new JLabel("file (bytes)", JLabel.LEFT));
		controls.add( fileSizeCombo );

		startButton = new JButton("START");
		controls.add(startButton);
		startButton.addActionListener( this );

		stopButton = new JButton("STOP");
		controls.add(stopButton);
		stopButton.addActionListener( this );

		// put all of these into a vertical box on the left side
		// (we introduce the extra FlowLayout JPanel to prevent 
		// gratuitous resizing of these widgets)
		JPanel left = new JPanel( new FlowLayout());	
		left.add(controls);
		mainPane.add( left, BorderLayout.WEST );

		// create a pannel to hold the lower controls
		JPanel lower = new JPanel(new GridLayout( 1, numZombies+3));

		// create a master thread spinner 
		SpinnerModel bank   = new SpinnerNumberModel( 0, 0, MAX_THREADS * numZombies, numZombies);
		masterSpinner = new JSpinner( bank );
		masterSpinner.addChangeListener( this );
		masterSpinner.setEnabled(false);
		lower.add( masterSpinner );
		lower.add( new JSeparator() );

		// create a panel for each zombie
		zStatus = new JPanel[numZombies];
		zSpinners = new JSpinner[numZombies];
		zThroughput = new JProgressBar[numZombies];
		for( int i = 0; i < numZombies; i++ ) {
			// create the control stack for this zombie
			JPanel zStack = new JPanel();
			zStack.setLayout( new BoxLayout( zStack, BoxLayout.Y_AXIS));

			// status indicators  are a little bit complicated
			// 	what I want is a named red/yellow/green indicator
			// it is comprised of
			//    an outer assembly JPanel (FlowLayout to prevent expansion)
			//    to which a (color changing) JPanel is added
			//    on top of which a JLabel is added
			JPanel assy = new JPanel(new FlowLayout()); // the whole assembly
			zStatus[i] = new JPanel();		// the status indicator
			zStatus[i].add( new JLabel( myHorde.zombies[i].tag, JLabel.CENTER ));
			assy.add( zStatus[i] );
			zStack.add( Box.createVerticalStrut( SEPARATION ));
			zStack.add( assy );

			// throughput progress meter
			zStack.add( Box.createVerticalStrut( SEPARATION ));
			zThroughput[i] = new JProgressBar( JProgressBar.VERTICAL, 0, 100 );
			zStack.add( zThroughput[i] );

			// per-zombie thread count selector
			//	in a FlowLayout JPanel to prevent stretching
			zStack.add( Box.createVerticalStrut( SEPARATION ));
			JPanel b = new JPanel( new FlowLayout());
			zSpinners[i] = new JSpinner(new SpinnerNumberModel( 0, 0, MAX_THREADS, 1 ));
			zSpinners[i].setEnabled(false);
			zSpinners[i].addChangeListener( this );
			b.add( zSpinners[i] );
			zStack.add( b );

			// and add this stack to the zombie status panel
			lower.add( zStack );
		}

		lower.add( new JSeparator());

		// add the lower status/controls to the bottom of the screen
		mainPane.add( lower, BorderLayout.SOUTH );

		// aggregate throughput on the right
		JPanel top = new JPanel();
		aThroughput = new JTextField(METER_WIDTH);
		aThroughput.setText( String.format(METER_FORMAT, 0.0));
		aThroughput.setFont( new Font( METER_FONT, Font.PLAIN, METER_SIZE ));
		aThroughput.setEditable(false);
		top.add(new JLabel("Aggregate Throughput (MB/s)    "));
		top.add(aThroughput);
		mainPane.add( top, BorderLayout.NORTH );

		// performance graph in the center
		graph = new BarGraph( GRAPH_SAMPLES, GRAPH_NORMAL );
		mainPane.add( graph, BorderLayout.CENTER);
	}

	/**
	 * Announce a change in status.
	 * 
	 * @param who		which Zombie changed status
	 * @param newStatus	(OK/FAILED/...)
	 */
	public void statusChange( Zombie who ) {
		boolean someRunning = false;

		// figure out which zombie it was
		for (int i = 0; i < numZombies; i++ ) {
			Zombie.Status sts = myHorde.zombies[i].status();

			if (myHorde.zombies[i] == who ) {
				switch( sts ) {
					case OK:
						zStatus[i].setBackground(Color.GREEN);
						break;
					case DONE:
						zStatus[i].setBackground(Color.LIGHT_GRAY);
						zSpinners[i].setValue(0);
						zSpinners[i].setEnabled(false);
						break;
					case FAILED:
						zStatus[i].setBackground(Color.RED);
						zSpinners[i].setValue(0);
						zSpinners[i].setEnabled(false);
						break;
					case WEDGED:
						zStatus[i].setBackground(Color.YELLOW);
						break;
					default:
						zStatus[i].setBackground(Color.WHITE);
				}
			}

			// and figure out if anyone is left
			if (sts != Zombie.Status.DONE && sts != Zombie.Status.FAILED)
				someRunning = true;
		}

		// if they're all dead, we can disable all remaining controls
		if (!someRunning) {
			masterSpinner.setValue(0);
			masterSpinner.setEnabled(false);
			startButton.setEnabled(false);
			stopButton.setEnabled(false);
			opCombo.setEnabled(false);
			bandwidthCombo.setEnabled(false);
			fileSizeCombo.setEnabled(false);
			blockSizeCombo.setEnabled(false);
		}
	}

	/**
	 * Announce a new performance report
	 * 
	 * @param who		which Zombie filed this report
	 * @param when		timestamp of the report
	 * @param bytesPerSecond	reported throughput
	 */
	public void perfReport( Zombie who, long when ) {

		
		// if anyone ever moves more bytes than this per second
		// they are pretty clearly not on Gig-E
		final int tooFast = (1024*1024*1024)/8;
		final int tenGig  = (1024*1024*1024);

		// no throughput displays until we are officially started
		if (!running)
			return;

		// for now, we recompute everyone's throughput every time
		//	and check for the possibility that we under-estimated
		// 	the per-zombie wire-speed.
		long tp = 0;
		for( int i = 0; i < numZombies; i++ ) {
			// update the per-zombie throughput
			long bytes = myHorde.zombies[i].bytesPerSecond();
			if (bytes > tooFast)	
				bandwidthPer = tenGig;
			long throughput = 100 * bytes / bandwidthPer;
			zThroughput[i].setValue( (int) throughput );

			tp += bytes;	// aggregate total throughput
		}

		// FIX ... I should be displaying # of running threads

		// update the aggregate throughput value and graph
		aThroughput.setText(String.format(METER_FORMAT, (double) tp/(1024*1024) ));
		if (tp > 0) {
			long ticks = (when - startTime)/Options.getInstance().update;
			long value = 100 * tp / bandwidthTot;
			plot.newSample( (int) ticks, (int) value );
			graph.plot( plot.data );
		}
	}

	/**
	 * action handler for controls and selectors
	 * 
	 * NOTE: no work required for block/size file combo changes
	 * 	     because we read them when the start button is pressed.
	 */
	public void actionPerformed(ActionEvent e) {

		if (e.getSource() == startButton && !running) {
			// pull out the currently selected parameters
			long ratePer = getBandwidth();
			long fsize = getFileSize();
			int  bsize = getBlockSize();
			Zombie.Ops operation = opcodes[opCombo.getSelectedIndex()];
			Options opts = Options.getInstance();

			// there may be a size that is not in the comboBox
			if (fsize == 0 && opts.length > 0) {
				fsize = opts.length;
			}
			
			// calibration of per-zombie throughput
			if (ratePer == 0) {	// default: GigE wire speed
				if (opts.wireSpeed == 0) {
					if (opts.scale == 0) 
						bandwidthPer = (1024*1024*1024)/10;
					else
						bandwidthPer = opts.scale / numZombies;
				}  else
					bandwidthPer = opts.wireSpeed;
			} else
				bandwidthPer = ratePer;

			// calibration of aggregate throughput
			if (opts.scale == 0) {
				bandwidthTot = bandwidthPer * numZombies;
			} else {
				bandwidthTot = opts.scale;
			}
			
			// start up each of the zombies
			for( int i = 0; i < numZombies; i++ ) {
				myHorde.zombies[i].start( operation, bsize, fsize, ratePer );
			}
		
			// note that we are now started
			running = true;
			opCombo.setEnabled(false);
			bandwidthCombo.setEnabled(false);
			fileSizeCombo.setEnabled(false);
			blockSizeCombo.setEnabled(false);
			startButton.setEnabled(false);

			masterSpinner.setEnabled(true);
			for( int i = 0; i < numZombies; i++ )
				zSpinners[i].setEnabled(true);
		}

		if (e.getSource() == stopButton) {
			for( int i = 0; i < numZombies; i++ ) {
				zSpinners[i].setEnabled(false);
				myHorde.zombies[i].stop();
			}

			// and disable all the other controls
			opCombo.setEnabled(false);
			bandwidthCombo.setEnabled(false);
			fileSizeCombo.setEnabled(false);
			blockSizeCombo.setEnabled(false);
			startButton.setEnabled(false);
			stopButton.setEnabled(false);
			masterSpinner.setEnabled(false);
		}
	}

	/**
	 * see if any command-line or scenario parameters have
	 * overridden default option settings.
	 *
	 * NOTE: if I were cooler, and they entered an option that
	 *	 was not on the combobox menu, I would add it to the
	 *	 combobox menu in a (collation sequence) appropriate
 	 *	 place.
 	 */
	private void setDefaults() {
		Options opts = Options.getInstance();
		int i;
		
		// default block size?
		if (opts.bsize != 0) {
			for( i = 0; i < blockSizes.length; i++ ) {
				if (opts.bsize == Argument.sizeParse(blockSizes[i])) {
					blockSizeCombo.setSelectedIndex(i);
					break;
				}
			}

			// for now, just gripe if the value isn't on the list
			if (i >= blockSizes.length)
				System.err.println(
					"WARNING: unsupported block size (" +
					opts.bsize + ")" );
					
		}

		// default file length?
		if (opts.length != 0) {
			for( i = 0; i < fileSizes.length; i++ ) {
				if (opts.length == Argument.sizeParse(fileSizes[i])) {
					fileSizeCombo.setSelectedIndex(i);
					break;
				}
			}

			// for now, just gripe if the value isn't on the list
			if (i >= fileSizes.length)
				System.err.println(
					"WARNING: unsupported file size (" +
					opts.length + ")" );
		}

		// default per thread generation rate?
		//	note that rate[0] is "unlimited"
		if (opts.rate != 0) {
			for( i = 1; i < bandwidths.length; i++ ) {
				if (opts.rate == Argument.sizeParse(bandwidths[i])) {
					bandwidthCombo.setSelectedIndex(i);
					break;
				}
			}

			// for now, just gripe if the value isn't on the list
			if (i >= bandwidths.length)
				System.err.println(
					"WARNING: unsupported rate (" +
					opts.rate + ")" );
		}
	}

	/** @return the control-panel-selected block size */
	private int getBlockSize() {
		if (blockSizeCombo.getSelectedIndex() == 0)
			return 0;
		return (int) Argument.sizeParse( (String) blockSizeCombo.getSelectedItem() );
	}

	/** @return the control-panel-selected file size */
	private long getFileSize() {
		if (fileSizeCombo.getSelectedIndex() == 0)
			return 0;
		return Argument.sizeParse( (String) fileSizeCombo.getSelectedItem() );
	}

	/** @return the control-panel-selected bandwidth */
	private long getBandwidth() {
		if (bandwidthCombo.getSelectedIndex() == 0)
			return 0;
		return Argument.sizeParse( (String) bandwidthCombo.getSelectedItem() );
	}

	/**
	 * action handler for thread count spinners
	 */
	public void stateChanged(ChangeEvent e) {
		// masterSpinner controls all the zombies in unison
		if (e.getSource() == masterSpinner) {
			int threads = (Integer) masterSpinner.getValue()/numZombies;
			for( int i = 0; i < numZombies; i++ ) {
				Zombie.Status sts = myHorde.zombies[i].status();
				if (sts != Zombie.Status.DONE && sts != Zombie.Status.FAILED)
					zSpinners[i].setValue(threads);
			}
		} else {
			// these are the calls that actually cause changes
			for( int i = 0; i < numZombies; i++ ) {
				if (e.getSource() == zSpinners[i]) {
					Zombie.Status sts = myHorde.zombies[i].status();
					if (sts != Zombie.Status.DONE && sts != Zombie.Status.FAILED) {
						int threads = (Integer) zSpinners[i].getValue();
						myHorde.zombies[i].setThreads( threads );
					}
				}
			}
		}
	}
}
