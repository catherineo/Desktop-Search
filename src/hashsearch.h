#include <cstring>
#include <cstdlib>
#include <cassert>

unsigned int BKDRHash(char *str)
{
	unsigned int seed = 131; // 31 131 1313 13131 131313 etc..
	unsigned int hash = 0;

	while (*str)
	{
		hash = hash * seed + (*str++);
	}

	return (hash & 0x0000FFFF);
}

class HashContent;
class HashUnit;

class HashUnit
{
public:
	char * name;
	HashContent * content;
	HashUnit * next;
	HashUnit(){}
	HashUnit(char * name)
	{
		this -> name = new char[strlen(name) + 1 ];
		strcpy(this -> name, name);
		next = NULL;
		content = NULL;
	}
};

class HashContent
{
public:
	char * oneContent;
	HashContent * next;
	HashContent(){}
	HashContent(char *content)
	{
		oneContent = new char[strlen(content) + 1];
		strcpy(oneContent, content);
		next = NULL;
	}
};

HashUnit * hashList[0x10000];

bool hashClear()
{
	memset(hashList, NULL, sizeof(hashList));
	return 0;
}

bool hashInsert(char *name, char *content)
{
	unsigned hashCode = BKDRHash(name);
	HashUnit * p;
	for (p = hashList[hashCode]; p; p = p -> next)
	{
		if (!strcmp(p -> name, name) )
			break;
	}
	if (!p )
		p = new HashUnit(name);


	assert(p != NULL);
	HashContent *q;
	for (q = p -> content; q; q = q -> next)
	{
		if (!strcmp(q -> oneContent, content) )
			return 0;
	}
	assert(q == NULL);

	q = new HashContent(content);

}

bool hashSearch(char * name, HashContent* point)
{
	if (point )
	{
		point = (HashContent*)point -> next;
		return 0;
	}


	unsigned int hashCode = BKDRHash(name);
	HashUnit *p;
	for (p = hashList[hashCode]; p; p = p -> next)
	{
		if (!strcmp(p -> name, name) )
			break;
	}
	if (!p )
		return 0;

	point = p -> content;
	return 1;
}
