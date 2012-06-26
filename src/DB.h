#ifndef _DB_H
#define _DB_H
#include <cstdio>
#include <xapian.h>
#include <string>
#include <vector>
#include <list>

using namespace std;

class DB
{
	public:
	DB();
	~DB();
	bool isCreated();
	void open();
	void add(string path, string name, string content);
	void close();
	vector <list <string> > search(string query);
	private:
       	Xapian::Database db;
	Xapian::WritableDatabase wdb;
	Xapian::Enquire *enquire;
	Xapian::QueryParser qp;
	Xapian::Query query;
	Xapian::MSet result;
};
#endif
