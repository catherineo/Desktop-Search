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

#define DEBUG 1

typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

// Super block accessors
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

inline Inode& get_inode(int inode);
void init_journal_consts(void);

// Convert Big Endian to Little Endian.
static inline __le32 be2le(__be32 v) { return __be32_to_cpu(v); }
static inline __le16 be2le(__be16 v) { return __be16_to_cpu(v); }
static inline __u8 be2le(__u8 v) { return v; }

static inline __le32 be2le(__s32 const& v) { return be2le(*reinterpret_cast<__be32 const*>(&v)); }

int const SUPER_BLOCK_OFFSET = 1024;

//===========================================================
// Frequently used constant values from the superblock
//
ext3_super_block super_block;
int groups_;
int block_size_;
int block_size_log_;
int inodes_per_group_;
int inode_size_;
uint32_t inode_count_;
uint32_t block_count_;

// The journal super block
journal_superblock_t journal_super_block;
Inode journal_inode;
int journal_block_size_;
int journal_maxlen_;
int journal_first_;
int journal_sequence_;
int journal_start_;

static inline off_t block_to_offset(int block)
{
	  off_t offset = block;
	    offset <<= block_size_log_;
		  return offset;
}

static void dump_hex(unsigned char const* buf, size_t size)
{
	for (size_t addr = 0; addr < size; addr += 16)
	{
		std::cout << std::hex << std::setfill('0') << std::setw(4) << addr << " |";
		int offset;
		for (offset = 0; offset < 16 && addr + offset < size; ++offset)
			std::cout << ' ' << std::hex << std::setfill('0') << std::setw(2) << (int)buf[addr + offset];
		for (; offset < 16; ++offset)
			std::cout << "   ";
		std::cout << " | ";
		for (int offset = 0; offset < 16 && addr + offset < size; ++offset)
		{
			unsigned char c = buf[addr + offset];
			if (!std::isprint(c))
				c = '.';
			std::cout << c;
		}
		std::cout << '\n';
	}
}



//==============================================================
//
// Globally used variables
//

std::ifstream device;
uint64_t** inode_bitmap;
uint64_t** block_bitmap;
Inode** all_inodes;
/*
#if USE_MMAP
void** all_mmaps;
#endif
*/

ext3_group_desc* group_descriptor_table;
char* inodes_buf;
std::set<std::string> accepted_filenames;
int no_filtering = 0;
std::string device_name;
uint32_t wrapped_journal_sequence = 0;

/*
#if USE_MMAP
long page_size_;
int device_fd;
#endif
*/
static std::string const outputdir = "RESTORED_FILES/";

//==============================================================
//
//load_meta_data
//
void load_meta_data(int group);

void load_inodes(int group)
{
	if (!block_bitmap[group])
		load_meta_data(group); 
	// The start block of the inode table.
	int block_number = group_descriptor_table[group].bg_inode_table;
	/*
#if USE_MMAP
int const blocks_per_page = page_size_ / block_size_;
off_t page = block_number / blocks_per_page;
off_t page_aligned_offset = page * page_size_;
off_t offset = block_to_offset(block_number); // Use mmap to avoid running out of memory.
all_mmaps[group] = mmap(NULL, inodes_per_group_ * inode_size_ + (offset - page_aligned_offset),
PROT_READ, MAP_PRIVATE | MAP_NORESERVE, device_fd, page_aligned_offset);
all_inodes[group] = reinterpret_cast<Inode*>((char*)all_mmaps[group] + (offset - page_aligned_offset));
#else 
*/

	// Load all inodes into memory.
	all_inodes[group] = new Inode[inodes_per_group_];
	// sizeof(Inode) == inode_size_
	device.seekg(block_to_offset(block_number));
	//printf("%d\n", inode_size_);
	device.read(reinterpret_cast<char*>(all_inodes[group]), inodes_per_group_ * inode_size_ );
	//printf("the load_inodes is here\n");
	assert(device.good());

	/*
#ifdef DEBUG 
	// We set this, so that we can find back where an inode struct came from 
	// // during debugging of this program in gdb. It is not used anywhere.
	for (int ino = 0; ino < inodes_per_group_; ++ino)
	all_inodes[group][ino].set_reserved2(ino + 1 + group * inodes_per_group_);
#endif
#endif
*/
}

void load_meta_data(int group)
{
	if (block_bitmap[group])// Already loaded?
		return; // Load block bitmap.
	block_bitmap[group] = new uint64_t[block_size_ / sizeof(uint64_t)];
	device.seekg(block_to_offset(group_descriptor_table[group].bg_block_bitmap));
	device.read(reinterpret_cast<char*>(block_bitmap[group]), block_size_); // Load inode bitmap.
	inode_bitmap[group] = new uint64_t[block_size_ / sizeof(uint64_t)];
	device.seekg(block_to_offset(group_descriptor_table[group].bg_inode_bitmap));
	device.read(reinterpret_cast<char*>(inode_bitmap[group]), block_size_); // Load all inodes into memory.
	//printf("load_meta_data is good\n");
	load_inodes(group);
}

//=============================================================

inline Inode& get_inode(int inode)
{
	int group = (inode - 1) / inodes_per_group_;
	unsigned int bit = inode - 1 - group * inodes_per_group_;
	assert(bit < 8U * block_size_);
	if (block_bitmap[group] == NULL)
	{
		//printf("block_bitmap[group] is NULL\n");
		load_meta_data(group);
	}
#if DEBUG
	printf("group, bit(all_inodes[group][bit]): %d %d\n", group, bit);
#endif
	return all_inodes[group][bit];
}

void init_journal_consts(void)
{
	//Initialize journal constants
	journal_block_size_ = be2le(journal_super_block.s_blocksize);
	assert(journal_block_size_ == block_size_);
	// Sorry, I'm trying to recover my own data-- have no time to deal with this.
	journal_maxlen_ = be2le(journal_super_block.s_maxlen);
	journal_first_ = be2le(journal_super_block.s_first);
	journal_sequence_ = be2le(journal_super_block.s_sequence);
	journal_start_ = be2le(journal_super_block.s_start);
	journal_inode = get_inode(super_block.s_journal_inum);
}

unsigned char* get_block(int block, unsigned char* block_buf)
{
	device.seekg(block_to_offset(block));
	device.read((char*)block_buf, block_size_);
	assert(device.good());
	return block_buf;
}


struct FileSystemState {
	private:
		__le16 M_state;
	public:
		FileSystemState(__le16 state) : M_state(state) { }
		friend std::ostream& operator<<(std::ostream& os, FileSystemState const& state)
		{
			if ((state.M_state & EXT3_VALID_FS))
				os << "'Unmounted cleanly'";
			else
				os << "Not clean";
			if ((state.M_state & EXT3_ERROR_FS))
				os << " 'Errors detected'";
			return os;
		}
};

std::ostream& operator<<(std::ostream& os, ext3_super_block const& super_block)
{
	//This was generated with:
	os << "Inodes count: " << inode_count(super_block) << '\n';
	os << "Blocks count: " << block_count(super_block) << '\n';
	os << "Reserved blocks count: " << reserved_block_count(super_block) << '\n';
	os << "Free blocks count: " << super_block.s_free_blocks_count << '\n';
	os << "Free inodes count: " << super_block.s_free_inodes_count << '\n';
	os << "First Data Block: " << first_data_block(super_block) << '\n';
	os << "Block size: " << block_size(super_block) << '\n';
	os << "Fragment size: " << fragment_size(super_block) << '\n';
	os << "Number of blocks per group: " << blocks_per_group(super_block) << '\n';
	os << "Number of fragments per group: " << super_block.s_frags_per_group << '\n';
	os << "Number of inodes per group: " << inodes_per_group(super_block) << '\n';
	time_t mtime = super_block.s_mtime;
	os << "Mount time: " << std::ctime(&mtime);
	time_t wtime = super_block.s_wtime;
	os << "Write time: " << std::ctime(&wtime);
	os << "Mount count: " << super_block.s_mnt_count << '\n';
	os << "Maximal mount count: " << super_block.s_max_mnt_count << '\n';
	os << "Magic signature: " << std::hex << "0x" << super_block.s_magic << std::dec << '\n';
	os << "File system state: " << FileSystemState(super_block.s_state) << '\n';
	// os << "Behaviour when detecting errors: " << super_block.s_errors << '\n';
	// os << "minor revision level: " << super_block.s_minor_rev_level << '\n';
	// os << "time of last check: " << super_block.s_lastcheck << '\n';
	// os << "max. time between checks: " << super_block.s_checkinterval << '\n';
	// os << "OS: " << super_block.s_creator_os << '\n';
	// os << "Revision level: " << super_block.s_rev_level << '\n';
	// os << "Default uid for reserved blocks: " << super_block.s_def_resuid << '\n';
	// os << "Default gid for reserved blocks: " << super_block.s_def_resgid << '\n';
	// os << "First non-reserved inode: " << super_block.s_first_ino << '\n';
	os << "Size of inode structure: " << super_block.s_inode_size << '\n';
	os << "Block group # of this superblock: " << super_block.s_block_group_nr << '\n';
	// os << "compatible feature set: " << super_block.s_feature_compat << '\n';
	// os << "incompatible feature set: " << super_block.s_feature_incompat << '\n';
	// os << "readonly-compatible feature set: " << super_block.s_feature_ro_compat << '\n';
	// os << "128-bit uuid for volume: " << super_block.s_uuid << '\n';
	// os << "For compression: " << super_block.s_algorithm_usage_bitmap << '\n';
	// os << "Nr to preallocate for dirs: " << super_block.s_prealloc_dir_blocks << '\n';
	os << "Per group desc for online growth: " << super_block.s_reserved_gdt_blocks << '\n';
	os << "UUID of journal superblock:";
	for (int i = 0; i < 16; ++i)
		os << " 0x" << std::hex << std::setfill('0') << std::setw(2) << (int)super_block.s_journal_uuid[i];
	os << std::dec << '\n';
	os << "Inode number of journal file: " << super_block.s_journal_inum << '\n';
	os << "Device number of journal file: " << super_block.s_journal_dev << '\n';
	os << "Start of list of inodes to delete: " << super_block.s_last_orphan << '\n';
	// os << "HTREE hash seed: " << super_block.s_hash_seed << '\n';
	// os << "Default hash version to use: " << super_block.s_def_hash_version << '\n';
	os << "First metablock block group: " << super_block.s_first_meta_bg << '\n';
	// os << "Padding to the end of the block: " << super_block.s_reserved << '\n';
	return os;
}

std::ostream& operator<<(std::ostream& os, ext3_group_desc const& group_desc)
{
	os << "block bitmap at " << group_desc.bg_block_bitmap <<
		", inodes bitmap at " << group_desc.bg_inode_bitmap <<
		", inode table at " << group_desc.bg_inode_table << '\n';
	os << "\t   " << group_desc.bg_free_blocks_count << " free blocks, " <<
		group_desc.bg_free_inodes_count << " free inodes, " <<
		group_desc.bg_used_dirs_count << " used directory";
	return os;
}

std::ostream& operator<<(std::ostream& os, journal_header_t const& journal_header)
{
	os << "Block type: ";
	switch (be2le(journal_header.h_blocktype))
	{
		case JFS_DESCRIPTOR_BLOCK:
			os << "Descriptor block";
			break;
		case JFS_COMMIT_BLOCK:
			os << "Commit block";
			break;
		case JFS_SUPERBLOCK_V1:
			os << "Superblock version 1";
			break;
		case JFS_SUPERBLOCK_V2:
			os << "Superblock version 2";
			break;
		case JFS_REVOKE_BLOCK:
			os << "Revoke block";
			break;
		default:
			os << "*UNKNOWN* (0x" << std::hex << be2le(journal_header.h_blocktype) << std::dec << ')';
			break;
	}
	os << '\n';
	os << "Sequence Number: " << be2le(journal_header.h_sequence);
	return os;
}

std::ostream& operator<<(std::ostream& os, journal_superblock_t const& journal_super_block)
{
	os << journal_super_block.s_header << '\n';
	os << "Journal block size: " << be2le(journal_super_block.s_blocksize) << '\n';
	os << "Number of journal blocks: " << be2le(journal_super_block.s_maxlen) << '\n';
	os << "Journal block where the journal actually starts: " << be2le(journal_super_block.s_first) << '\n';
	os << "Sequence number of first transaction: " << be2le(journal_super_block.s_sequence) << '\n';
	os << "Journal block of first transaction: " << be2le(journal_super_block.s_start) << '\n';
	os << "Error number: " << be2le(journal_super_block.s_errno) << '\n';
	if (be2le(journal_super_block.s_header.h_blocktype) != JFS_SUPERBLOCK_V2)
		return os;
	os << "Compatible Features: " << be2le(journal_super_block.s_feature_compat) << '\n';
	os << "Incompatible features: " << be2le(journal_super_block.s_feature_incompat) << '\n';
	os << "Read only compatible features: " << be2le(journal_super_block.s_feature_ro_compat) << '\n';
	os << "Journal UUID:";
	for (int i = 0; i < 16; ++i)
		os << std::hex << " 0x" << std::setfill('0') << std::setw(2) << (int)be2le(journal_super_block.s_uuid[i]);
	os << std::dec << '\n';
	int32_t nr_users = be2le(journal_super_block.s_nr_users);
	os << "Number of file systems using journal: " << nr_users << '\n';
	assert(nr_users <= 48);
	os << "Location of superblock copy: " << be2le(journal_super_block.s_dynsuper) << '\n';
	os << "Max journal blocks per transaction: " << be2le(journal_super_block.s_max_transaction) << '\n';
	os << "Max file system blocks per transaction: " << be2le(journal_super_block.s_max_trans_data) << '\n';
	os << "IDs of all file systems using the journal:\n";
	for (int u = 0; u < nr_users; ++u)
	{
		os << (u + 1) << '.';
		for (int i = 0; i < 16; ++i)
			os << std::hex << " 0x" << std::setfill('0') << std::setw(2) << (int)be2le(journal_super_block.s_users[u * 16 + i]);
		os << std::dec << '\n';
	}
	return os;
}

std::ostream& operator<<(std::ostream& os, journal_block_tag_t const& journal_block_tag)
{
	os << "File system block: " << be2le(journal_block_tag.t_blocknr) << '\n';
	os << "Entry flags:";
	uint32_t flags = be2le(journal_block_tag.t_flags);
	if ((flags & JFS_FLAG_ESCAPE))
		os << " ESCAPED";
	if ((flags & JFS_FLAG_SAME_UUID))
		os << " SAME_UUID";
	if ((flags & JFS_FLAG_DELETED))
		os << " DELETED";
	if ((flags & JFS_FLAG_LAST_TAG))
		os << " LAST_TAG";
	os << '\n';
	return os;
}

std::ostream& operator<<(std::ostream& os, journal_revoke_header_t const& journal_revoke_header)
{
	os << journal_revoke_header.r_header << '\n';
	uint32_t count = be2le(journal_revoke_header.r_count);
	os << "Bytes used: " << count << '\n';
	assert(sizeof(journal_revoke_header_t) <= count && count <= (size_t)block_size_);
	count -= sizeof(journal_revoke_header_t);
	assert(count % sizeof(__be32) == 0);
	count /= sizeof(__be32);
	__be32* ptr = reinterpret_cast<__be32*>((unsigned char*)&journal_revoke_header + sizeof(journal_revoke_header_t));
	int c = 0;
	for (uint32_t b = 0; b < count; ++b)
	{
		std::cout << std::setfill(' ') << std::setw(8) << be2le(ptr[b]);
		++c;
		c &= 7;
		if (c == 0)
			std::cout << '\n';
	}
	return os;
}

