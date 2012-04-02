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


