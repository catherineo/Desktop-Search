/* quickstartsearch.cc: Simplest possible searcher
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
#include <iostream>
#include <cstdlib>
#include <vector>
using namespace std;

const char dataBaseDirName[] = "proverbs";

int search(char ** query, int numQuery)
{

        // Catch any Xapian::Error exceptions thrown
        try {
                // Make the database
                Xapian::Database db(dataBaseDirName);

                // Start an enquire session
                Xapian::Enquire enquire(db);

                // Build the query object
				vector<string> query_vecotr;
				for (int i = 0; i < numQuery; ++i)
					query_vecotr.push_back(string(query[i]));
                Xapian::Query query(Xapian::Query::OP_OR, query_vecotr.begin(), query_vecotr.end());
                cout << "Performing query `" << query.get_description() << "'" << endl;

                // Give the query object to the enquire session
                enquire.set_query(query);

                // Get the top 10 results of the query
                Xapian::MSet matches = enquire.get_mset(0, 10);

                // Display the results
                cout << matches.size() << " results found" << endl;

                for (Xapian::MSetIterator i = matches.begin(); i != matches.end(); ++i) {
                        Xapian::Document doc = i.get_document();
                        cout << "Document ID " << *i << "\t"
                                 << "[" <<
                                doc.get_data() << "]" << endl;
                }
        } catch(const Xapian::Error &error) {
                cout << "Exception: "  << error.get_msg() << endl;
        }
}
