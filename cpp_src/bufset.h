/*
 * operations for the management of aligned I/O buffers
 */
class Bufset {
    public:
	int buffers;	///< number of buffers in this set
	int size;	///< size of each buffer

	/**
	 * constructor for buffer set
	 *
	 * @param	number of desired buffer
	 * @param	size of each buffer
	 * @param	required memory alignment
	 */
	Bufset( int numbufs, int bufsize, int alignment );

	~Bufset();

	/**
	 * return the address of a buffer in the set
	 *
	 * @param	index of the desired buffer
	 * @return	byte address of the desired buffer
	 *		NULL if index is invalid
	 */
	char *buffer( int i );

    private:
	char *_bufstart;
};
