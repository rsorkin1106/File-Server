# File-Server

## 1. Overview
This project is a multi-threaded, secure network file server. Clients that use the file
server will interact with it via network messages

## 2. Client interface to the file server
After initializing the library (by calling fs_clientinit), a client uses the following functions to issue
requests to your file server: fs_clientinit, fs_read, fs_append, fs_create, fs_delete. These functions
are described in fs_client.h and included in libfs_client.a. Each client program should include
fs_client.h and link with libfs_client.a.
Here is an example client that uses these functions. Assume the file server was initialized with a user user1
whose password is password1. This client is run with two arguments:
1. the name of the file server's computer
2. the port on which the file server process is accepting connections from clients.

```
#include <iostream> 
#include <cstdlib>
#include "fs_client.h"
using namespace std;
main(int argc, char *argv[])
{
char *server;
int server_port;
unsigned int session, seq=0;
char buf[10];
if (argc != 3) {
cout << "error: usage: " << argv[0] << " <server> <serverPort>\n";
exit(1);
}
server = argv[1];
server_port = atoi(argv[2]);
fs_clientinit(server, server_port);
fs_session("user1", "password1", &session, seq++);
fs_create("user1", "password1", session, seq++, "tmp");
fs_append("user1", "password1", session, seq++, "tmp", "abc", 3);
fs_read("user1", "password1", session, seq++, "tmp", 1, buf, 2);
fs_delete("user1", "password1", session, seq++, "tmp");
}
```
## 3. Communication protocol between client and file server
The client's side of this protocol is carried out by the functions in libfs_client.a.
There are five types of requests that can be sent over the network from a client to the file server:
FS_CLIENTINIT, FS_READ, FS_APPEND, FS_CREATE, FS_DELETE. 
### 3.1 FS_CLIENTINIT
A client requests a new session with FS_CLIENTINIT (a user can call FS_CLIENTINIT any number of times). Client
requests use a session and sequence number as a unique identifier for the request (a nonce) to thwart replay
attacks. A user may only use session numbers that have been returned by that user's prior FS_CLIENTINIT
requests. A session remains active for the lifetime of the file server.
The first request in a session is the FS_CLIENTINIT that created the session, which uses the specified sequence
number. Each subsequent sequence number for a session must be larger than all prior sequence numbers
used by that session (they may increase by more than 1).
An FS_CLIENTINIT request message is a string of the following format:
FS_CLIENTINIT <session> <sequence><NULL>
  
### 3.2 FS_READBLOCK
A client reads an existing file by sending an FS_READBLOCK request to the file server.
An FS_READ request message is a string of the following format:
FS_READBLOCK <session> <sequence> <filename> <offset> <size><NULL>
<session> is the session number for this request
<sequence> is the sequence number for this request
<filename> is the name of the file being read
<offset> specifies the starting byte of the file portion being read
<size> specifies the number of bytes to read from the file (should be > 0)
<NULL> is the ASCII character '\0' (terminating the string)

### 3.3 FS_WRITEBLOCK
A client appends to an existing file by sending an FS_WRITEBLOCK request to the file server.
An FS_WRITEBLOCK request message is a string of the following format:
FS_WRITEBLOCK <session> <sequence> <filename> <size><NULL><data>
<session> is the session number for this request
<sequence> is the sequence number for this request
<filename> is the name of the file to which the data is being appended
<size> specifies the number of bytes to append to the file (should be > 0)
<NULL> is the ASCII character '\0' (terminating the string)
<data> is that data to append to the file. Note that <data> is outside of the request string (i.e., after
<NULL>). The size of <data> is given in <size>.

### 3.4 FS_CREATE
A client creates a new file by sending an FS_CREATE request to the file server.
An FS_CREATE request message is a string of the following format:
FS_CREATE <session> <sequence> <filename><NULL>
<session> is the session number for this request
<sequence> is the sequence number for this request
<filename> is the name of the file being created
<NULL> is the ASCII character '\0' (terminating the string)

### 3.5 FS_DELETE
A client deletes an existing file by sending an FS_DELETE request to the file server.
An FS_DELETE request message is a string of the following format:
FS_DELETE <session> <sequence> <filename><NULL>
<session> is the session number for this request
<sequence> is the sequence number for this request
<filename> is the name of the file being deleted
<NULL> is the ASCII character '\0' (terminating the string)

## 4. File system structure on disk
This section describes the file system structure on disk that your file server will read and write. fs_param.h
(which is included automatically in both fs_client.h and fs_server.h) defines the basic file system
parameters.
fs_server.h has two typedefs that describe the on-disk data structures:
  ```
/*
* Typedefs for on-disk data structures.
*/
typedef struct {
char name[FS_MAXFILENAME + 1]; /* name of this file */
unsigned int inode_block; /* disk block that stores the inode for
this file */
} fs_direntry;
  
typedef struct {
char owner[FS_MAXUSERNAME + 1];
unsigned int size; /* size of this file or directory
in bytes */
unsigned int blocks[FS_MAXFILEBLOCKS]; /* array of data blocks for this
file or directory */
} fs_inode;
  ```
The file system consists of a single directory of files. It is not a hierarchical file system, so the directory
contain only files (it does not contain other directories).
Each file and directory is described by an inode, which is stored in a single disk block. The structure of an
inode is specified in fs_inode. The owner field is used only for files (it is ignored for the directory); it is the
name of the user that created the file (a string of characters, including the '\0' that terminates the string). The
blocks array lists the disk blocks where this file or directory's data is stored. Entries in the blocks array
that are beyond the end of the file may have arbitrary values. The inode for the directory is stored in disk
block 0.
The data for the directory is an array of fs_direntry entries (one entry per file). Unused directory entries
are identified by inode_block=0. In the array of directory entries, entries that are used may be interspersed
with entries that are unused, e.g., entries 0, 5, and 15 might be used, with the rest of the entries being
unused. Each directory entry contains a file name (including the '\0' that terminates the string) and the disk
block number that stores that file's inode. A file name is a non-empty string of characters (whitespace is not
allowed).
Tip: the typedefs above serve two purposes. The first purpose is to concisely describe the data format on
disk. E.g., an fs_direntry consists of FS_MAXFILENAME+1 bytes for the file name, followed by a 4-byte
unsigned integer (in little-endian byte order on x86 systems). The second purpose is to provide an easy way
to convert the raw data you read from disk into a data structure, viz. through typecasting.
  
## 5. How it works
  
### 5.1 Arguments and input
Your file server should be able to be called with 0 or 1 command-line arguments. The argument, if present,
specifies the port number the file server should use to listen for incoming connections from clients. If there is
no argument, the file server should have the system choose a port.
Your file server will be passed a list of usernames and passwords via stdin.
Each line will contain
`<username> <password>`
For example, the Linux file passwords could contain the following contents:
  ```
user1 password1
user2 password2
user3 password3
user4 password4
  ```
and your file server could be started as:
`fs 8000 < passwords`
or
`fs < passwords`

### 5.2 Initialization
When your file server starts, it should carry out the following tasks:
Read the list of usernames and passwords from stdin.
Initialize the list of free disk blocks by reading the relevant data from the existing file system. Your
file server should be able to start with any valid file system (an empty file system as well as file
systems containing files).
