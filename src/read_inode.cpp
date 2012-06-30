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
#include "DB.h"

typedef ext2_super_block ext3_super_block;

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
*/
	block_bitmap = new uint64_t* [groups_];
	// We use this array to know of which groups we loaded the metadata. Therefore zero it out.
	memset(block_bitmap, 0, sizeof(uint64_t*) * groups_);
	inode_bitmap = new uint64_t* [groups_];
	//printf("%d %d\n",(size_t)inode_size_, sizeof(Inode));
	assert((size_t)inode_size_ <= sizeof(Inode));
	assert((size_t)inode_size_ == sizeof(Inode));
	// This fails if kernel headers are used.
	inodes_buf = new char[inodes_per_group_ * inode_size_];
	// Initialize group_descriptor_table.

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

}


int read_inode(char *argv, DB db)
{
	Debug(debug::init());

	device_name = argv;

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


	/*
	std::cout << 0xEF53 << std::endl;
	std::cout << super_block.s_magic << std::endl;
	std::cout << super_block.s_creator_os << std::endl;
	std::cout << super_block.s_block_group_nr << std::endl;
	std::cout << "inode size: " << inode_size(super_block) << std::endl;

	std::cout << "inode_block_per_block: " << inode_blocks_per_group(super_block) << std::endl;
	*/
#if DEBUG
	std::cout << "journal_inum: " << super_block.s_journal_inum << std::endl;
	std::cout << "journal_dev: " << super_block.s_journal_dev << std::endl;
	std::cout << "inode_size: " << super_block.s_inode_size<< std::endl;
#endif

	if (super_block.s_journal_dev == 0 )
	{
		Inode& journal_inode = get_inode(super_block.s_journal_inum);
		std::cout << "super_block.s_journal_inum: " << super_block.s_journal_inum << std::endl;
		int first_block = journal_inode.block()[0];

#if DEBUG
		std::cout << "first_block_number: " << first_block << std::endl;
		std::cout << "sizeof(journal_superblock_s): " << sizeof(journal_superblock_s) << std::endl;
#endif
		assert(first_block);
		device.seekg(block_to_offset(first_block));
		assert(device.good());
		
		device.read(reinterpret_cast<char*>(&journal_super_block), sizeof(journal_superblock_s));
		assert(device.good());

		//std::cout << journal_super_block << std::endl;
		assert(be2le(journal_super_block.s_header.h_magic) == JFS_MAGIC_NUMBER);
		init_journal_consts();
	}
	//dump_hex((unsigned char*)&journal_inode, inode_size_);
	
#if DEBUG
	for (int i = 0; i < groups_; ++i)
	{
		std::cout << "inode_table: " << group_descriptor_table[i].bg_inode_table << std::endl;
	}
#endif
	
	Inode& root_inode = get_inode(2);
	if ( is_directory(root_inode) )
	{
		char root_prefix[2] = "/";
		//read_directory();
		//std::cout << "the block" << root_inode.block()[0] << std::endl;
		
		unsigned char* block = new unsigned char[block_size_];
		device.seekg(block_to_offset(root_inode.block()[0]));
		directory_file.open("directory_file");

		//std::cout << root_inode.block()[0] << std::endl;
		//std::cout << block_to_offset(root_inode.block()[0]) << std::endl;
		device.read(reinterpret_cast<char*>(block),block_size_);
		//dump_hex(block, block_size_);
		std::cout << "file type" << '\t' << "inode" << '\t' << "name" << std::endl;
		read_current_directory(block, block_size_, root_prefix, db);
		directory_file.close();
		std::cout << sum_files  << " files had been indexed" <<std::endl;
	}
}


