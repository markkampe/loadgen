
/**
 * arguments are strings that can be searched for parameter=value,
 * where values can have multipliers.  This can be used to pull
 * numbers out of individual (command line) parameters, or to
 * pull values out of a line full of name=value assignments
 */
public class Argument {
	
	private String str;
	
	/** 
	 * Create an Argument through which we can inspect a string
	 * 
	 * @param s
	 */
	public Argument( String s ) {
		str = s;
	}
	
	/**
	 * parse off am int property/value (with optional multiplier)
	 * 
	 * @param name	name of desired property
	 * 
	 * @return	value or -1
	 */
	public int intProperty( String property ) {
		String s = stringProperty( property );
		if (s == null)
			return -1;
		return( (int) sizeParse( s ) );
	}
	
	/**
	 * parse off a long property/value (with optional multiplier)
	 * 
	 * @param name	name of desired property
	 * 
	 * @return	value or -1
	 */
	public long longProperty( String property ) {
		String s = stringProperty( property );
		if (s == null)
			return -1;
		return( sizeParse( s ) );
	}
	
	/**
	 * parse off a string property/value
	 * 
	 * I am unhappy with this parsing for a couple of reasons:
	 *	it doesn't handle escaped delimiter characters in strings
	 *	it doesn't work for command line parameters (for which
	 *	   the shell strips off the quotes)
	 * 	it seems to me cubersome (like sewing with mittens on)
	 * 
	 * @param property	name of desired property
	 * 
	 * @return	string or null
	 */
	public String stringProperty( String property ) {
		
		// find the leading name=
		String lead = property + "=";
		int x;
		if (str.startsWith(lead))	// at beginning
			x = 0;	
		else {	// perhaps in mid-line after a space
			lead = " " + property + "=";
			x = str.indexOf(lead);
			if (x < 0) {	// perhaps after a tab?
				lead = "\t" + property + "=";
				x = str.indexOf(lead);
			}
		}

		// property is not in this line
		if (x < 0)
			return null;

		// pull off the part after the = sign
		String s = str.substring(x + lead.length());
		if (s.length() < 1)
			return null;

		// try to find the end of the value
		char c = s.charAt(0);
		if (c == '\'' || c =='"') {
			x = s.indexOf(c,1);
			return x > 0 ? s.substring(1,x) : null;
		} else if (c == ' ' || c == '\t' || c == '\n') {
			return null;		// no argument
		} else {			// find the delimiting white space
			x = -1;
			String delimiters = " \t\n,";
			for( int i = 0; i < delimiters.length(); i++ ) {
				int xi = s.indexOf( delimiters.charAt(i) );
				if (xi > 0 && (x < 0 || xi < x))
					x = xi;
			}
			return (x <= 0) ? s : s.substring(0,x);
		}
	}

	/**
	 * parse a number with an optional mutipler
	 *
	 * @param value	string to be parsed
	 *
	 * @return	numerical value (or -1)
	 */
	static public long sizeParse( String value ) {

		// figure out the multiplier ... if any
		long multiplier = 1;
		if (value.matches("[0-9]*[kKmMgGtTpP]")) {
			char c = value.charAt(value.length()-1);
			value = value.substring(0, value.length()-1);
			switch ( c ) {
				case 't':
				case 'T':
					multiplier *= 1024;
				case 'g':
				case 'G':
					multiplier *= 1024;
				case 'm':
				case 'M':
					multiplier *= 1024;
				case 'k':
				case 'K':
					multiplier *= 1024;
			}
		}
		
		try {
			long number = Long.parseLong( value );
			return number * multiplier;
		} catch ( NumberFormatException e ) {
			return -1;
		}
	}
}
