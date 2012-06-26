#include <cstdio>
#include <string>
#include <xapian.h>
#include <time.h>
#include <iostream>
#include <list>
#include <vector>

#include "DB.h"

using namespace std;

DB::DB()
{
	wdb = *new Xapian::WritableDatabase("./db", Xapian::DB_CREATE_OR_OPEN);
	db = *new Xapian::Database("./db");
	enquire = NULL;
}

DB::~DB()
{
	delete enquire;
}

bool DB::isCreated()
{
	if(db.get_metadata("version").empty())
	{
		return false;
	}
	return true;
}

void DB::open()
{
	time_t t;
	time(&t);
	wdb.set_metadata("version", ctime(&t));
}

void DB::add(string path, string name, string content)
{
	Xapian::TermGenerator indexer;
	Xapian::Document doc;
	doc.set_data(path);
	indexer.set_document(doc);
	indexer.index_text(name, 1, "N");
	indexer.index_text(path, 1, "P");
	indexer.index_text(content, 1, "C");
	wdb.add_document(doc);
	wdb.commit();	
}

void DB::close()
{
	time_t t;
	time(&t);
	wdb.set_metadata("version", ctime(&t));
}

vector <list <string> > DB::search(string querystr)
{
	db.reopen();
	delete enquire;
	enquire = new Xapian::Enquire(db);	
	qp.set_database(db);
	query = qp.parse_query(querystr, Xapian::QueryParser::FLAG_PARTIAL | Xapian::QueryParser::FLAG_DEFAULT, "N");
	enquire->set_query(query);
	result = enquire->get_mset(0, 10);
	list <string> name;
	for(Xapian::MSetIterator itr = result.begin(); itr != result.end(); itr++)
	{
		Xapian::Document doc = itr.get_document();
		cout<<doc.get_data()<<endl;
		name.push_back(doc.get_data());
	}	
	query = qp.parse_query(querystr, Xapian::QueryParser::FLAG_DEFAULT, "C");
	enquire->set_query(query);
	result = enquire->get_mset(0, 10);
	list <string> content;
	for(Xapian::MSetIterator itr = result.begin(); itr != result.end(); itr++)
	{
		Xapian::Document doc = itr.get_document();
		cout<<doc.get_data()<<endl;
		content.push_back(doc.get_data());
	}
	vector<list <string> > v;
	v.push_back(name);
	v.push_back(content);
	return v;
}
