#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include <cstdio>
#include "DB.h"
using namespace std;

class GlobalArgs
{
public:
	char *deviceName; 	/* -r */
	char **query; 		/* -s */
	bool isIndex; 		/* -i */
	int numQuery;
	
}globalArgs;

static const char *optString = "r:i:s:vh?";
int read_inode(char *argv, DB db);
int index();
int search(char ** query, int numQuery);

int initGlobalArgs()
{
	globalArgs.isIndex = 1;
	globalArgs.deviceName = NULL;
	globalArgs.query = NULL;
	globalArgs.numQuery = 0;

	return 0;
}

void displayUsage()
{
	printf("Usage\n");
}

int main(int argc, char *argv[])
{
	int opt = initGlobalArgs();

	opt = getopt(argc, argv, optString);
	while (opt != -1 )
	{
		switch( opt )
		{
			case 'r':
				globalArgs.deviceName = optarg;
				break;
			case 'i':
				globalArgs.isIndex = 0;
				break;
			case 'h':
			case '?':
				displayUsage();
			default:
				break;
		}
		opt = getopt(argc, argv, optString);
	}

	globalArgs.query = argv + optind;
	globalArgs.numQuery = argc - optind;
	DB db;
	if(!db.isCreated())
	{
		db.open();
		if(globalArgs.deviceName)
		{
			read_inode(globalArgs.deviceName, db);
		}
		db.close();
	}
	string q;
	while(cout << ">>" && cin >> q && q != "exit")
	{
		db.search(q);
	}
/*
	if (globalArgs.deviceName)
	{
		read_inode(globalArgs.deviceName);
	}
	if (globalArgs.isIndex)
	{
		index();
	}

	if (globalArgs.numQuery > 0)
	{
		search(globalArgs.query, globalArgs.numQuery);
	}
	*/
}
