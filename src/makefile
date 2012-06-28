objects = DB.o read_inode.o
CPPFLAGS = -I/usr/local/lib -lxapian -L/usr/local/include
CC = g++

desktop_search: $(objects) desktop_search.cpp
	$(CC) -o desktop_search $(objects) desktop_search.cpp $(CPPFLAGS)

read_inode.o: ostream.h basic_function.h blocknr_vector_type.h

.PHONY: clean
clean:
	rm desktop_search $(objects)
