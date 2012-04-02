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

#include "locate.h"

typedef ext2_super_block ext3_super_block;

int inode_count(ext3_super_block const& super_block) { return super_block.s_inodes_count; }
int block_count(ext3_super_block const& super_block) { return super_block.s_blocks_count; }
int reserved_block_count(ext3_super_block const& super_block) { return super_block.s_r_blocks_count; }
int first_data_block(ext3_super_block const& super_block) { return super_block.s_first_data_block; }
int block_size(ext3_super_block const& super_block) { return EXT3_BLOCK_SIZE(&super_block); }
int fragment_size(ext3_super_block const& super_block) { return EXT3_FRAG_SIZE(&super_block); }
int blocks_per_group(ext3_super_block const& super_block) { return EXT3_BLOCKS_PER_GROUP(&super_block); }
int inodes_per_group(ext3_super_block const& super_block) { return EXT3_INODES_PER_GROUP(&super_block); }
int first_inode(ext3_super_block const& super_block) { return EXT3_FIRST_INO(&super_block); }
int inode_size(ext3_super_block const& super_block) { return EXT3_INODE_SIZE(&super_block); }
int inode_blocks_per_group(ext3_super_block const& super_block) { return inodes_per_group(super_block) * inode_size(super_block) / block_size(super_block); }
int groups(ext3_super_block const& super_block) { return inode_count(super_block) / inodes_per_group(super_block); }


// Frequently used constant values from the superblock
ext3_super_block super_block;

std::ifstream device;
std::string device_name;

int main(int arec, char *argv[])
{
	Debug(debug::init());

	device_name = argv[1];

	device.open(device_name.c_str());
	assert( device.good() );

	// Read the first super_block
	device.seekg(SUPERBLOCK_SIZE);
	assert( device.good() );

	device.read(reinterpret_cast<char *>(&super_block), sizeof(ext3_super_block));
	assert( device.good() );

	int groups_ = groups(super_block);

	std::cout << super_block << std::endl;
}
