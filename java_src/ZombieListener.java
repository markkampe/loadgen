

/**
 * this interface enables Zombies to report updates in their
 * status or new performance reports.
 *
 */
public interface ZombieListener {
	
	/**
	 * Announce a status change
	 * 
	 * @param who		which Zombie has changed status
	 */
	public void statusChange( Zombie who );
	
	/**
	 * Announce a new performance report
	 * 
	 * @param who		which Zombie filed this report
	 * @param when		timestamp of the report
	 */
	public void perfReport( Zombie who, long when );

}
