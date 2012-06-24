objects = read_inode.o index.o search.o
CPPFLAGS = -I/usr/local/lib -lxapian -L/usr/local/include
CC = g++

desktop_search: $(objects)
	$(CC) -o desktop_search $(objects) desktop_search.cpp $(CPPFLAGS)

read_inode.o: ostream.h basic_function.h blocknr_vector_type.h

index.o:

search.o:

.PHONY: clean
clean:
	rm desktop_search $(objects)
