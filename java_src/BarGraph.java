

import java.awt.*;
import javax.swing.JComponent;

/**
 * a bar-graph for a specified number of (0-100) samples
 */
public class BarGraph extends JComponent {
	
	private static final long serialVersionUID = 1L;
	
	// maximum and minimum desired sizes
	private static final int MIN_BORDER = 5;	// distance of plot from panel edge
	private static final int MAX_BORDER = 20;	// distance of plot from panel edge
	private static final int MIN_BAR_WIDTH = 3;	// pixels per bar
	private static final int MAX_BAR_WIDTH = 10;// pixels per bar
	private static final int MIN_HEIGHT = 50;	// pixels
	private static final int MAX_HEIGHT = 200;	// pixels
	private static final int BEST_HEIGHT = 150;	// pixels
	private static final int PERCENT = 100;		// granularity of values
	
	// display layout parameters
	private final Color bar_color;	// color for rendering bars
	private final int num_bars;		// number of data points on chart
	private int plot_height;		// display area height
	private int plot_width;			// display area width
	private int bar_width;			// width (in pixels) of each bar
	private int h_border;			// border width (in pixels)
	private int v_border;			// border height (in pixels)
	
	// the plotted values
	private int data[];

	/**
	 * note the number of points we are to handle
	 * (everything else we learn at paint time)
	 * 
	 * @param numPoints		number of data points for the chart
	 * @param color			for bars
	 */
	public BarGraph(int numPoints, Color color) {
		num_bars = numPoints;
		bar_color = color;
		data = new int[num_bars];
		for( int i = 0; i < num_bars; i++ )
			data[i] = 0;
	}
	

	/**
	 * figure out how large our window is 
	 * figure out a set of plot scaling parameters that will fit in it
	 * put up the basic frame for the bar graph
	 * 
	 * @param g graphics component on which we draw
	 */
	public void paint(Graphics g) {
		// get our graphics context
		Graphics2D g2d = (Graphics2D) g;
		int height = getHeight();
		int width = getWidth();
		
		// figure out how wide a bar we can get fit into this window
		int pixels = (width - 2*MIN_BORDER)/num_bars;
		if (pixels == 0)
			pixels = 1;
		bar_width = (pixels < MAX_BAR_WIDTH) ? pixels : MAX_BAR_WIDTH;
		plot_width = bar_width * num_bars;
		h_border = (width - plot_width)/2;
		
		// figure out a reasonable height for the plotting area
		int min_ticks = PERCENT/2;
		int u = (height > PERCENT) ? (height-2*MAX_BORDER)/min_ticks : 1;
		plot_height = u * min_ticks;
		v_border = (height - plot_height)/2;
		
		// clear our frame and put up our border
		//	Note that the border is inset from the plot by a blank
		//	pixel so that that it always remains distinct from the
		//	data being plotted.
		g2d.clearRect(h_border-2, v_border-2, plot_width+4, plot_height+4 );
		g2d.drawRect(h_border-2, v_border-2, plot_width+4, plot_height+4 );
		
		// now re-display the data plot in the new size
		for( int i = 0; i < num_bars; i++ ) {
			drawBar(g2d, i*bar_width, bar_width, (plot_height * data[i])/PERCENT, bar_color, false);
		}
	}
	/**
	 * plot an array of (percentage) values on the bar graph
	 * (we keep a copy of the most recently plotted values 
	 * and only re-draw values that have changed)
	 * 
	 * @param points	array of values in the range of 0-100
	 * @param color		of bars
	 */
	public void plot( int points[] ) {
		// get the graphics component on which we draw
		Graphics g = getGraphics();
		Graphics2D g2d = (Graphics2D) g;
		
		// figure out how many bars we are going to draw
		int n = points.length;
		if (n > num_bars)
				n = num_bars;
		
		for( int i = 0; i < n; i++ ) {
			// see if the current value is correct
			if (points[i] == data[i])
				continue;
			
			// clear and re-draw this line
			data[i] = points[i];
			int height = (plot_height * data[i])/PERCENT;
			drawBar(g2d, i*bar_width, bar_width, height, bar_color, true );
		}
	}

	/**
	 * draw a single bar on the chart
	 * 
	 * @param g2d		graphics context
	 * @param offset	(pixel) offset of this bar into chart
	 * @param width		(pixel) width of this bar
	 * @param height	(pixel) height of this bar
	 * @param c			color of bar
	 * @param clear		do we need to clear the canvas behind this bar?
	 */
	private void drawBar(Graphics2D g2d, int offset, int width, int height, Color c, boolean clear ) {
		
		if (height > plot_height)
			height = plot_height;
		
		g2d.setColor(c);
		g2d.fillRect(h_border + offset, v_border+plot_height-height, width, height);
		
		if (clear)
			g2d.clearRect(h_border + offset, v_border, width, plot_height-height);
	}
	
	/**
	 * @return 	ideal size for our component
	 */
	public Dimension getPreferredSize() {
		int width = 2*MAX_BORDER;
		width += num_bars * (MIN_BAR_WIDTH + MAX_BAR_WIDTH)/2;
		int height = 2*MAX_BORDER + BEST_HEIGHT;
		return new Dimension(width, height);
	}

	/**
	 * @return 	minimum size for our component
	 */
	public Dimension getMinimumSize() {
		int width = 2*MIN_BORDER;
		width += num_bars * MIN_BAR_WIDTH;
		int height = MIN_HEIGHT + 2*MIN_BORDER;
		return new Dimension(width, height);
	}
	
	/**
	 * @return 	maximum size for our component
	 */
	public Dimension getMaximumSize() {
		int width = 2*MAX_BORDER;
		width += num_bars * MAX_BAR_WIDTH;
		int height = MAX_HEIGHT + 2*MAX_BORDER;
		return new Dimension(width, height);
	}

}
