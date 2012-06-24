/* quickstartindex.cc: Simplest possible indexer
 *
 * ----START-LICENCE----
 * Copyright 1999,2000,2001 BrightStation PLC
 * Copyright 2003,2004 Olly Betts
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 * -----END-LICENCE-----
 */

#include <xapian.h>
//包含头文件,此头文件通常安装在/usr/local/include
#include <iostream>
#include <fstream>
#include <cstdlib>
using namespace std;

const char directory_file_name[] = "directory_file";
const char dataBaseDirName[] = "proverbs";


int index()
{
	ifstream directoryFile(directory_file_name);
	// Catch any Xapian::Error exceptions thrown
	try {
		// Make the database
		Xapian::WritableDatabase database(dataBaseDirName, Xapian::DB_CREATE_OR_OVERWRITE);

		/*
		// Make the document
		Xapian::Document newdocument;

		// Put the data in the document
		newdocument.set_data(string(argv[2]));

		// Put the terms into the document
		for (int i = 3; i < argc; ++i) {
			newdocument.add_posting(argv[i], i - 2);
		}

		// Add the document to the database
		database.add_document(newdocument);
		*/

		string path, name;
		while (directoryFile >> path >> name)
		{
			Xapian::Document newdocument;
			newdocument.set_data(path);
			int cnt = 1;
			for (int i = 0; i < name.size(); ++i)
			{
				for (int j = 1; j + i - 1 < name.size(); ++j)
				newdocument.add_posting(name.substr(i, j), cnt++);
			}
			database.add_document(newdocument);

		}
	} catch(const Xapian::Error &error) {
		cout << "Exception: "  << error.get_msg() << endl;
	}
	
	directoryFile.close();
	cout << endl << "the directory have been indexed" << endl << endl;
	
}
