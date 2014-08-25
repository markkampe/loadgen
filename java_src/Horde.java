import java.io.*;

/**
 * a Horde is a collection of zombies, described by a configuration file
 */
public class Horde {
	
	// default zombie parameters
	private static final String LOCAL_HOST = "127.0.0.1";
	private static final int ZOMBIE_PORT = 8080;

	// strings in the zombie configuration file
	private static final String COMMENT = "#";
	private static final String PARM_TAG = "PARM";
	private static final String ZOMBIE_TAG = "ZOMBIE";

	/** a horde is nothing more than an array of zombies	*/
	public Zombie zombies[];
	public int numZombies;

	/**
	 * create a test horde of local zombies
	 */
	public Horde() {
		Options opts = Options.getInstance();
		numZombies = opts.numZombies;

		zombies = new Zombie[numZombies];
		for( int i = 0; i <numZombies; i++ ) {
			String tag = "z" + (i+1);
			String dir = "--target=/tmp/" + tag;
			zombies[i] = new Zombie( tag, LOCAL_HOST, ZOMBIE_PORT, 
								opts.update, dir );
		}
		opts.simulate = true;
	}

	/**
	 * read the configuration file, 
	 * and set options and create a horde accordingly.
	 * 
	 * @param filename	of horde description file
	 */
	public Horde( String filename ) {
		Options opts = Options.getInstance();
		zombies = null;
		numZombies = 0;

		// see if we can open the input file
		BufferedReader reader;
		try {
			reader = new BufferedReader( new FileReader( filename ));
		} catch (FileNotFoundException e) {
			System.err.println("ERROR: scenario file " + filename + "does not exist");
			return;
		}

		int linenum = 0;
		String line;
		for(;;) {	
			// see if we can read another line
			try { 
				line = reader.readLine(); 
				if (line == null)
					break;
				linenum++;
			} catch (IOException e) {
				System.err.println("File " + filename +
					", line " + linenum + ": read error" );
				return;
			}

			if (line.startsWith(COMMENT) || line.trim().length() == 0)
				continue;
			
			if (line.startsWith(PARM_TAG)) {
				// isolate the parameter value
				String arg = line.substring(PARM_TAG.length()).trim();
				if (!opts.parseSwitch( arg )) {
					System.err.println("File " + filename + 
					", line " + linenum + 
					": unrecognized option - " + arg );
				}
				continue;
			}

			// in pass one, we just count them
			if (line.startsWith(ZOMBIE_TAG)) {
				numZombies++;
				continue;
			}

			// something wrong
			System.err.println("File " + filename + ", line " + linenum + 
					": unrecognized line - " + line );
			numZombies = 0;
			break;
		}

		// in pass 2, we re-read the file, processing the zombie definitions
		if (numZombies == 0)
			return;

		try {
			reader = new BufferedReader( new FileReader( filename ));
		} catch (FileNotFoundException e) {
			System.err.println("ERROR: scenario file " + filename + "does not exist");
			numZombies = 0;
			return;
		}

		zombies = new Zombie[ numZombies ];
		numZombies = 0;
		linenum = 0;
		for(;;) {	
			// see if we can read another line
			try { 
				line = reader.readLine(); 
				if (line == null)
					break;
				linenum++;
			} catch (IOException e) {
				System.err.println("File " + filename +
					", line " + linenum + ": read error" );
				numZombies = 0;
				zombies = null;
				return;
			}

			// include the configuration file in the output log
			if (opts.log)
				System.out.println( line );

			// in pass one we processed everything but the ZOMBIE lines
			if (!line.startsWith(ZOMBIE_TAG)) 
				continue;

			// collect (or default) the per-zombie parameters
			Argument arg = new Argument( line );
			String tag    = arg.stringProperty("tag");
			if (tag == null)
				tag = "z" + (numZombies + 1);
			String zombie = arg.stringProperty("zombie");
			if (zombie == null)
				zombie = LOCAL_HOST;
			int port   = arg.intProperty("port");
			if (port < 0)
				port = ZOMBIE_PORT;

			// and create the zombie
			zombies[numZombies++] = new Zombie( tag, zombie, port, opts.update, line+opts.misc);
		}
	}
}
