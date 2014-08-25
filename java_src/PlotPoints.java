

/**
 * A set of 0-100 data values over a period of time
 */
public class PlotPoints  {
	
	public int data[];			// the data set
	private int firstSample;	// time value of first sample
	private final int numpoints;// number of points in the data set
	
	/**
	 * manage an updating bar graph
	 * 
	 * @param graph		underlying bar graph
	 * @param numPoints	number of data points to maintain
	 * @param updatePeriod	miliseconds per update
	 */
	public PlotPoints( int numPoints ) {
		data = new int[numPoints];
		numpoints = numPoints;
		firstSample = 0;
		
		// initialize the data
		for( int i = 0; i < numpoints; i++ ) {
			data[i] = 0;
		}
	}
	
	/**
	 * update the data to record a new sample
	 * 
	 * @param when	tick time-stamp for this sample
	 * @param value	(0-100) for this sample
	 */
	public void newSample( int when, int value ) {
		
		// make sure this sample fits on the plot
		int lastSample = firstSample + numpoints - 1;
		if (when > lastSample) 
			shift( when - lastSample );
			
		data[when - firstSample] = value;
	}
	
	/**
	 * shift the sample array left by some number of samples
	 * 
	 * @param n	number of samples to shift the data
	 */
	private void shift( int n ) {
		// shift the data left by n slots
		for( int i = 0; i < numpoints-1; i++ ) {
			data[i] = (i + n < numpoints) ? data[i+n] : data[numpoints-1];
		}
		
		// shift the start time accordingly
		firstSample += n;
	}
}
