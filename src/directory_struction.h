#include <vector>

struct bitmap_ptr {
	int index;
	union {
		uint64_t mask;
		unsigned char byte[8];
	};
};

bitmap_ptr get_bitmap_mask(unsigned int bit)
{
	bitmap_ptr result;
	result.mask = 0;
	result.index = bit>>6;
	result.byte[(bit & 63) >> 3] = 1 << (bit & 7);
	return result;
}


class DirectoryBlock;
class Directory;

struct Index {
	int cur;	// Indicates the order in memory.
	int next;	// The index of the DirEntry that ext3_dir_entry_2::rec_len refers to or zero if it refers to the end.
};

struct DirEntry {
	std::list<DirectoryBlock>::const_iterator M_directory_iterator;	// Pointer to DirectoryBlock containing this entry.
	Directory* M_directory;						// Pointer to Directory, if this is a directory.
	int M_file_type;							// The dir entry file type.
	int M_inode;								// The inode referenced by this DirEntry.
	std::string M_name;							// The file name of this DirEntry.
	union {
		ext3_dir_entry_2 const* dir_entry;					// Temporary pointer into block_buf.
		Index index;							// Ordering index of dir entry.
	};
	bool deleted;								// Copies of values calculated by filter_dir_entry.
	bool allocated;
	bool reallocated;
	bool zero_inode;
	bool linked;
	bool filtered;

	bool exactly_equal(DirEntry const& de) const;
	void print(void) const;
};

bool DirEntry::exactly_equal(DirEntry const& de) const
{
	assert(index.cur == de.index.cur);
	return M_inode == de.M_inode && M_name == de.M_name && M_file_type == de.M_file_type && index.next == de.index.next;
}

class DirectoryBlock {
	private:
		int M_block;
		std::vector<DirEntry> M_dir_entry;
	public:
		void read_block(int block, std::list<DirectoryBlock>::iterator iter);
		void read_dir_entry(ext3_dir_entry_2 const& dir_entry, Inode& inode,
				bool deleted, bool allocated, bool reallocated, bool zero_inode, bool linked, bool filtered, std::list<DirectoryBlock>::iterator iter);

		bool exactly_equal(DirectoryBlock const& dir) const;
		int block(void) const { return M_block; }
		void print(void) const;

		std::vector<DirEntry> const& dir_entries(void) const { return M_dir_entry; }
		std::vector<DirEntry>& dir_entries(void) { return M_dir_entry; }
};

bool DirectoryBlock::exactly_equal(DirectoryBlock const& dir) const
{
	if (M_dir_entry.size() != dir.M_dir_entry.size())
		return false;
	std::vector<DirEntry>::const_iterator iter1 = M_dir_entry.begin();
	std::vector<DirEntry>::const_iterator iter2 = dir.M_dir_entry.begin();
	for (;iter1 != M_dir_entry.end(); ++iter1, ++iter2)
		if (!iter1->exactly_equal(*iter2))
			return false;
	return true;
}

bool read_block_action(ext3_dir_entry_2 const& dir_entry, Inode& inode,
		bool deleted, bool allocated, bool reallocated, bool zero_inode, bool linked, bool filtered, Parent*, void* data)
{
	std::list<DirectoryBlock>::iterator* iter_ptr = reinterpret_cast<std::list<DirectoryBlock>::iterator*>(data);
	DirectoryBlock* directory = &**iter_ptr;
	directory->read_dir_entry(dir_entry, inode, deleted, allocated, reallocated, zero_inode, linked, filtered, *iter_ptr);
	return false;
}

void DirectoryBlock::read_dir_entry(ext3_dir_entry_2 const& dir_entry, Inode& UNUSED(inode),
		bool deleted, bool allocated, bool reallocated, bool zero_inode, bool linked, bool filtered, std::list<DirectoryBlock>::iterator iter)
{
	DirEntry new_dir_entry;
	new_dir_entry.M_directory_iterator = iter;
	new_dir_entry.M_directory = NULL;
	new_dir_entry.M_file_type = dir_entry.file_type & 7;	// Only the last 3 bits are used.
	new_dir_entry.M_inode = dir_entry.inode;
	new_dir_entry.M_name = std::string(dir_entry.name, dir_entry.name_len);
	new_dir_entry.dir_entry = &dir_entry;		// This points directy into the block_buf that we are processing.
	// It will be replaced with the indices before that buffer is destroyed.
	new_dir_entry.deleted = deleted;
	new_dir_entry.allocated = allocated;
	new_dir_entry.reallocated = reallocated;
	new_dir_entry.zero_inode = zero_inode;
	new_dir_entry.linked = linked;
	new_dir_entry.filtered = filtered;
	M_dir_entry.push_back(new_dir_entry);
}

struct DirEntrySortPred {
	bool operator()(DirEntry const& de1, DirEntry const& de2) const { return de1.dir_entry < de2.dir_entry; }
};

void DirectoryBlock::read_block(int block, std::list<DirectoryBlock>::iterator list_iter)
{
	M_block = block;
	static bool using_static_buffer = false;
	assert(!using_static_buffer);
	static unsigned char block_buf[EXT3_MAX_BLOCK_SIZE];
	get_block(block, block_buf);
	using_static_buffer = true;
	iterate_over_directory(block_buf, block, read_block_action, NULL, &list_iter);
	// Sort the vector by dir_entry pointer.
	std::sort(M_dir_entry.begin(), M_dir_entry.end(), DirEntrySortPred());
	int size = M_dir_entry.size();
	assert(size >= 2);	// We should have at least the '.' and '..' entries.
	// Make a temporary backup of the dir_entry pointers.
	// At the same time, overwrite the pointers in the vector with with the index.
	ext3_dir_entry_2 const** index_to_dir_entry = new ext3_dir_entry_2 const* [M_dir_entry.size()];
	int i = 0;
	for (std::vector<DirEntry>::iterator iter = M_dir_entry.begin(); iter != M_dir_entry.end(); ++iter, ++i)
	{
		index_to_dir_entry[i] = iter->dir_entry;
		iter->index.cur = i;
	}
	// Assign a value to index.next, if any.
	for (std::vector<DirEntry>::iterator iter = M_dir_entry.begin(); iter != M_dir_entry.end(); ++iter)
	{
		ext3_dir_entry_2 const* dir_entry = index_to_dir_entry[iter->index.cur];
		ext3_dir_entry_2 const* next_dir_entry = (ext3_dir_entry_2 const*)(reinterpret_cast<char const*>(dir_entry) + dir_entry->rec_len);
		int next = 0;
		for (int j = 0; j < size; ++j)
			if (index_to_dir_entry[j] == next_dir_entry)
			{
				next = j;
				break;
			}
		// Either this entry points to another that we found, or it should point to the end of this block.
		assert(next > 0 || (unsigned char*)next_dir_entry == block_buf + block_size_);
		// If we didn't find anything, use the value 0.
		iter->index.next = next;
	}
	delete [] index_to_dir_entry;
	using_static_buffer = false;
}



class Directory {
	private:
		uint32_t M_inode_number;
		std::list<DirectoryBlock> M_blocks;

	public:
		Directory(uint32_t inode_number) : M_inode_number(inode_number) { }
		Directory(uint32_t inode_number, int first_block);

		std::list<DirectoryBlock>& blocks(void) { return M_blocks; }
		std::list<DirectoryBlock> const& blocks(void) const { return M_blocks; }

		uint32_t inode_number(void) const { return M_inode_number; }
		int first_block(void) const { assert(!M_blocks.empty()); return M_blocks.begin()->block(); }
};

//-----------------------------------------------------------------------------
//
// Directory printing
//

void DirEntry::print(void) const
{
	if (filtered)
		return;
	std::cout << std::setfill(' ') << std::setw(4) << index.cur << ' ';
	if (index.next)
		std::cout << std::setfill(' ') << std::setw(4) << index.next << ' ';
	else
		std::cout << " end ";
	std::cout << dir_entry_file_type(M_file_type, true);
	std::cout << std::setfill(' ') << std::setw(8) << M_inode << "  ";
	std::cout << (zero_inode ? 'Z' : deleted ? reallocated ? 'R' : 'D' : ' ');
	Inode* inode = NULL;
	if (!zero_inode)
	{
		inode = &get_inode(M_inode);
		if (deleted && !reallocated)
		{
			time_t dtime = inode->dtime();
			std::string dtime_str(ctime(&dtime));
			std::cout << ' ' << std::setw(10) << inode->dtime() << ' ' << dtime_str.substr(0, dtime_str.length() - 1);
		}
	}
	if (zero_inode && linked)
		std::cout << " * LINKED ENTRY WITH ZERO INODE *   ";
	else if (zero_inode || !deleted || reallocated)
		std::cout << std::string(36, ' ');
	if (zero_inode || reallocated)
		std::cout << "  ??????????";
	else
		std::cout << "  " << FileMode(inode->mode());
	std::cout << "  " << M_name;
	if (!(reallocated || zero_inode) && M_file_type == EXT3_FT_SYMLINK)
	{
		std::cout << " -> ";
		print_symlink(std::cout, *inode);
	}
	std::cout << '\n';
}
