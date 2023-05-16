CC=g++ -g -Wall -std=c++17 -D_XOPEN_SOURCE

# List of source files for your file server
FS_SOURCES=fs_socket.cpp fs_server.cpp fs_filesystem.cpp helpers.cpp

# Generate the names of the file server's object files
FS_OBJS=${FS_SOURCES:.cpp=.o}

all: fs

# Compile the file server and tag this compilation
fs: ${FS_OBJS} libfs_server.o
	${CC} -o $@ $^ -pthread -ldl

#test
test%: test%.cpp libfs_client.o
	${CC} -o $@ $^ -ldl
# Generic rules for compiling a source file to an object file
%.o: %.cpp
	${CC} -c $<
%.o: %.cc
	${CC} -c $<

clean:
	rm -f ${FS_OBJS} fs app
