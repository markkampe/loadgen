

/**
 * This is a singleton class to parse and maintain command line options
 */
public class Options {
	/** parameters to the GUI console				*/
	public String title;		// session title
	public int numZombies;		// default # of zombies
	public long scale;			// full scale for aggregate TP
	public int update;			// display update interval
	public long wireSpeed;		// full scale for per-zombie TP
	public boolean log;		// produce an activity log
	public boolean simulate;	// don't actually do any file I/O
	public String misc;		// misc args to pass on to zombies

	/** default session values (to be passed to loadgen)		*/
	public long rate;		// target per thread TP
	public int bsize;		// default block size
	public long length;		// default file length

	/** reference to the singleton instance				*/
	private static Options instance;
	
	private Options() {
		// actual defaults
		update = 3000;		// three second display updates

		// diagnostic options
		simulate = false;
		log = false;
		numZombies = 10;		
		
		// non-defaults
		bsize = 0;
		length = 0;
		rate = 0;
		wireSpeed = 0;
		scale = 0;
		misc = "";
	}
	
	/** return a reference to the singleton instance		*/
	public static Options getInstance() {
		
		if (instance == null)
			instance = new Options();
		return instance;
	}
	
	/**
	 * parse a switch specification and set options accordingly
	 * 
	 * @param arg	String to be parsed (less the switch character)
	 * 
	 * @return	whether or not the argument was recognized
	 */
	public boolean parseSwitch( String arg ) {
		// figure out what the option is
		if (arg.startsWith("zombies=")) {
			Argument a = new Argument(arg);
			numZombies = a.intProperty("zombies");
			return true;
		} else if (arg.startsWith("wire=")) {
			Argument a = new Argument(arg);
			wireSpeed = a.longProperty("wire");
			return true;
		} else if (arg.startsWith("scale=")) {
			Argument a = new Argument(arg);
			scale = a.longProperty("scale");
			return true;
		} else if (arg.startsWith("bsize=")) {
			Argument a = new Argument(arg);
			bsize = a.intProperty("bsize");
			return true;
		} else if (arg.startsWith("length=")) {
			Argument a = new Argument(arg);
			length = a.longProperty("length");
			return true;
		} else if (arg.startsWith("rate=")) {
			Argument a = new Argument(arg);
			rate = a.longProperty("rate");
			return true;
		} else if (arg.startsWith("title=")) {
			Argument a = new Argument(arg);
			title = a.stringProperty("title");
			return true;
		} else if (arg.startsWith("update=")) {
			Argument a = new Argument(arg);
			update = a.intProperty("update");
			return true;
		} else if (arg.startsWith("DEBUG")) {
			log = true;
			return true;
		} else {
			misc += " --" + arg;
			return true;
		}
	}
}
