
enum filename_char_type {
	fnct_ok,
	fnct_illegal,
	fnct_unlikely
};

inline filename_char_type is_filename_char(unsigned char c)
{
	if (c < 32 || c > 126 || c == '/')
		return fnct_illegal;
#if 0
	// These characters are legal... but very unlikely.
	static unsigned char hit[128 - 32] = {			// Mark 22, 2a, 3b, 3c, 3e, 3f, 5c, 60, 7c
		//  0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f
		0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, // 2
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, // 3
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 4
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, // 5
		1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 6
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0  // 7
	};
	// These characters are legal, but very unlikely.
	// Don't reject them when a specific block was requested.
	if (commandline_block == -1 && hit[c - 32])
		return fnct_unlikely;
#endif
	return fnct_ok;
}
