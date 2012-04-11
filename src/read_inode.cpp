#include <algorithm>
#include "sys.h"
#include <sys/types.h>
#include <asm/byteorder.h>
#include "ext3.h"
#include <utime.h>
#include <sys/mman.h>
#include <cassert>
#include <ctime>
#include <cstdlib>
#include <getopt.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cerrno>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <set>
#include <map>
#include <vector>
#include <list>
#include <limits>
#include <sstream>
#include "debug.h"
#include "ostream.h"
#include "locate.h"

typedef ext2_super_block ext3_super_block;

std::ifstream device;
std::string device_name;

void init_consts()
{
	//Frequently used constants.
	groups_ = groups(super_block);
	block_size_ = block_size(super_block);
	block_size_log_ = EXT3_BLOCK_SIZE_BITS(&super_block);
	inodes_per_group_ = inodes_per_group(super_block);
	inode_size_ = inode_size(super_block);
	inode_count_ = inode_count(super_block);
	block_count_ = block_count(super_block);
/*
#if USE_MMAP
page_size_ = sysconf(_SC_PAGESIZE);
#endif
*/

	// Global arrays.
	all_inodes = new Inode* [groups_];
/*
#if USE_MMAP
all_mmaps = new void* [groups_];
#endif
	block_bitmap = new uint64_t* [groups_];
	// We use this array to know of which groups we loaded the metadata. Therefore zero it out.
	std::memset(block_bitmap, 0, sizeof(uint64_t*) * groups_);
	inode_bitmap = new uint64_t* [groups_];
	assert((size_t)inode_size_ <= sizeof(Inode));
	assert((size_t)inode_size_ == sizeof(Inode));inode_size_// This fails if kernel headers are used.
		inodes_buf = new char[inodes_per_group_ * inode_size_];
*/
	// Initialize group_descriptor_table.

/*
	// Calculate the block where the group descriptor table starts.
	int const super_block_block = SUPER_BLOCK_OFFSET / block_size(super_block);
	// The block following the superblock is the group descriptor table.
	int const group_descriptor_table_block = super_block_block + 1;

	// Allocate group descriptor table.
	assert(EXT3_DESC_PER_BLOCK(&super_block) * sizeof(ext3_group_desc) == (size_t)block_size_);
	group_descriptor_table = new ext3_group_desc[groups_];

	device.seekg(block_to_offset(group_descriptor_table_block));
	assert(device.good());
	device.read(reinterpret_cast<char*>(group_descriptor_table), sizeof(ext3_group_desc) * groups_);
	assert(device.good());
*/

}


int main(int arec, char *argv[])
{
	Debug(debug::init());

	device_name = argv[1];

	device.open(device_name.c_str());
	assert( device.good() );

	//Read the first super_block
	device.seekg(SUPERBLOCK_SIZE);
	assert( device.good() );

	device.read(reinterpret_cast<char *>(&super_block), sizeof(ext3_super_block));
	assert( device.good() );

	init_consts();

	std::cout << "groups: " << groups_ << std::endl;

	std::cout << super_block << std::endl;

	assert(super_block.s_magic == 0xEF53);// EXT3.
	assert(super_block.s_creator_os == 0);// Linux.
	assert(super_block.s_block_group_nr == 0);// First super block.
	assert((uint32_t)groups_ * inodes_per_group(super_block) == inode_count_);// All inodes belong to a group.
	// extX does not support block fragments.
	// "File System Forensic Analysis, chapter 14, Overview --> Blocks"
	assert(block_size_ == fragment_size(super_block));
	// The inode bitmap has to fit in a single block.
	assert(inodes_per_group(super_block) <= 8 * block_size_);
	// There should fit exactly an integer number of inodes in one block.
	assert((block_size_ / inode_size_) * inode_size_ == block_size_);
	// Space needed for the inode table should match the returned value of the number of blocks they need.
	assert((inodes_per_group_ * inode_size_ - 1) / block_size_ + 1 == inode_blocks_per_group(super_block));


	std::cout << 0xEF53 << std::endl;
	std::cout << super_block.s_magic << std::endl;
	std::cout << super_block.s_creator_os << std::endl;
	std::cout << super_block.s_block_group_nr << std::endl;
	std::cout << "inode size: " << inode_size(super_block) << std::endl;

	std::cout << "inode_block_per_block: " << inode_blocks_per_group(super_block) << std::endl;


}


