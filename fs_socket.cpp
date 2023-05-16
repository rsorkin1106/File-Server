#include "fs_socket.h"
#include "fs_filesystem.h"

#include <stdio.h>		// printf(), perror()
#include <stdlib.h>
#include <arpa/inet.h>		// htons()
#include <unistd.h>		// close()


#include "helpers.h"		// make_server_sockaddr(), get_port_number()
#include "fs_param.h"

#include <sstream>
#include <strstream>
#include <cstring>
#include <queue>
#include <unordered_map>
#include<thread>

extern std::mutex q_lock;
extern std::queue<uint32_t> free_blocks;
extern std::unordered_map<int, std::mutex> inode_locks;

//cout lock when printing
//extern std::unordered_map<int, std::mutex> inode_locks;


int run_server(int port, int queue_size) {
    
	// (1) Create socket
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		perror("Error opening stream socket");
		return -1;
	}

	// (2) Set the "reuse port" socket option
	int yesval = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yesval, sizeof(yesval)) == -1) {
		perror("Error setting socket options");
		return -1;
	}

	// (3) Detect which port was chosen.


	// (3) Create a sockaddr_in struct for the proper port and bind() to it.
	struct sockaddr_in addr;
	if (make_server_sockaddr(&addr, port) == -1) {
		return -1;
	}

	// (3) Bind to the port.
	if (bind(sockfd, (sockaddr *) &addr, sizeof(addr)) == -1) {
		perror("Error binding stream socket");
		return -1;
	}

	port = get_port_number(sockfd);

	//printf("Server listening on port %d...\n", port);

	std::cout << "\n@@@ port " << port << std::endl;

	// (4) Begin listening for incoming connections.
	listen(sockfd, queue_size);
	// (5) Serve incoming connections one by one forever
	while (true) {
		int connectionfd = accept(sockfd, 0, 0);

		if(connectionfd != -1) {
			try {
				std::thread first(handle_connection, connectionfd);
				first.detach();
			}
			catch(...) {
				std::cout << "handle_connection failed" << std::endl;
			}
		}
	
	}
}

size_t receiveBytes(char msg[], int connectionfd, bool is_write) {
	// Call recv() enough times to consume all the data the client sends.
	size_t recvd = 0;
	ssize_t rval;
	do {
		rval = recv(connectionfd, msg + recvd, 1, 0);
		if(is_write && recvd + rval == FS_BLOCKSIZE) {
			return FS_BLOCKSIZE;
		}
		if(!is_write && msg[recvd + rval-1] == '\0')
		{
			return recvd + rval-1;
		}
		
		//int start = msg + recvd;
		if (rval == -1) {
			perror("Error reading stream message");
			return MAX_MESSAGE_SIZE + 1;
		}
		//std::cout << "msg rec: " << msg << std::endl;
		recvd += rval;
	} while (rval > 0);  // recv() returns 0 when client closes

	return recvd;
}

int handle_connection(int connectionfd) {

	//printf("New connection %d\n", connectionfd);

	// (1) Receive message from client.

	char msg[MAX_MESSAGE_SIZE + 1];
	memset(msg, 0, sizeof(msg));

	size_t recvd = receiveBytes(msg, connectionfd, false);
	//std::cout << "prerecv" << std::endl;

	if(recvd == MAX_MESSAGE_SIZE + 1) {
		return -1;
	}

	//std::cout << "postrecv" << std::endl;

    //call parsing and validating function
	std::vector<std::string> paths;
    if(!parse_request(msg, recvd, paths)) {
		close(connectionfd);
		//R TODO: return -1 for error??? or is that a different error type
		return -1;
	}
	//std::cout << "postParse" << std::endl;

	// (2) Print out the message
	printf("Client %d says '%s'\n", connectionfd, msg);

    std::string data = generate_response(msg, recvd, paths, connectionfd);
	if(data == "")
	{
		close(connectionfd);
		return -1;
	}

    send(connectionfd, data.c_str(), data.size(), MSG_NOSIGNAL);

	// (4) Close connection
	close(connectionfd);

	return 0;
}

bool parse_request(char msg[], size_t recvd, std::vector<std::string> &paths) {
    std::istrstream in(msg, recvd);
    std::string command = "", username = "", pathname = "", data = "", block_string = "";
    char null, type;
    uint32_t block;

    in >> command >> username >> pathname;
    std::string correct_format = command + " " + username + " " + pathname;

    if(command == "FS_WRITEBLOCK")
    {
        in >> block_string;
		try {
			block = std::stoi(block_string);
		}
		catch (...) {
			return false;
		}
		if(block >= FS_MAXFILEBLOCKS) {
			return false;
		}
        in >> null >> data;
        correct_format = correct_format + " " + std::to_string(block)  + data;    //right way to take in data???
    }
    else if(command == "FS_READBLOCK")
    {
        in >> block_string;
		try {
			block = std::stoi(block_string);
		}
		catch (...) {
			return false;
		}
		if(block >= FS_MAXFILEBLOCKS) {
			return false;
		}
        correct_format = correct_format + " " + std::to_string(block);
    }
    else if(command == "FS_CREATE")
    {
        //check space on disk
        in >> type;
		if(type != 'd' && type != 'f') {
			return false;
		}
        correct_format = correct_format + " " + type;
    }
    else if(command == "FS_DELETE")
    {
        correct_format = correct_format;
    }
    else
    {
        return false;
    }

	if(!check_size(paths, command, username, pathname, data) || paths[0]=="")
	{
		//std::cout << "check_size fail" << std::endl;
		return false;
	}


    //check correctness
    if((std::string)msg != correct_format){
		//std::cout << "Not same output";
		return false;
	}
    return true;
}

std::string generate_response(char msg[], size_t recvd, const std::vector<std::string> &paths, int connectionfd) {
    std::istrstream in(msg, recvd);
    std::string command, username, pathname, data;
    char null, type;
    uint32_t block;
	fs_inode i_node;
	//fs_direntry dir_block[FS_DIRENTRIES];

    in >> command >> username >> pathname;

    std::string correct_format = command + " " + username + " " + pathname;


	//std::unique_lock<Lock_RAII> lck(Lock_RAII(inode_locks[0]));
	Lock_RAII lck(&(inode_locks[0]));

	uint32_t path_num = pathTraversal(paths, i_node, command, username, lck);

	//std::cout << "path returned " << path_num << std::endl;
	if(path_num == FS_DISKSIZE) {
		return "";
	}
	//disk_readblock(path_num, (void*)&i_node);

    if(command == "FS_WRITEBLOCK")
    {
        in >> block >> null;
		char data[FS_BLOCKSIZE];
		if (receiveBytes(data, connectionfd, true) == MAX_MESSAGE_SIZE + 1) {
			return "";
		}
		std::string user_string = (std::string)i_node.owner;
		if(!write_block(i_node, data, path_num, block)) {
			return "";
		}


        correct_format = correct_format + " " + std::to_string(block) + '\0';
    }
    else if(command == "FS_READBLOCK")
    {
        in >> block;
		char read_data[FS_BLOCKSIZE];
		if(!read_block(i_node, read_data, path_num, block)) {
			return "";
		}
		//std::cout << "passed read_block" << std::endl;
		//Check data size when converting
		correct_format = correct_format + " " + std::to_string(block) + '\0';
		for(unsigned int i = 0; i < FS_BLOCKSIZE; ++i) {
			correct_format = correct_format + read_data[i];
		}
    }
    else if(command == "FS_CREATE")
    {
        in >> type;
		//std::cout << "type " << type << std::endl;
		//std::cout << "current string " << correct_format << std::endl;

		//i_node currently holds the path where we want to create
		if(!create_path(paths[paths.size() - 1], i_node, path_num, username, type))
		{
			//std::cout << "create path fail" << std::endl;
			return "";
		}
        correct_format = correct_format + " " + type + '\0';
    }
    else if(command == "FS_DELETE")
    {
		if(!delete_path(i_node, path_num, paths[paths.size()-1], username)) {
			return "";
		}
        correct_format = correct_format + '\0';
    }
    return correct_format;
}

bool check_size(std::vector<std::string> &paths, std::string command, std::string username, std::string pathname, std::string data) {
    //if(command.size() > ) return false;
    //TODO check block size?
    if(username.size() > FS_MAXUSERNAME) return false;
	if(username == "" || username.find(" ") != std::string::npos) return false;

	/*for(size_t i = 0; i < username.size(); ++i) {
		if(!isalnum(username[i]))
			return false;
	}*/
    if(pathname.size() > FS_MAXPATHNAME) return false;
	if(pathname[0] != '/') {
		return false;
	}

	std::string path = "";
	for(size_t i = 0; i < pathname.size(); ++i) {
		if(pathname[i] == '/') {
			for(size_t j = i + 1; j < pathname.size(); ++j) {
				if(pathname[j] == '/') {
					//2 slashes in a row
					if(path.size() == 0) {
						return false;
					}
					paths.push_back(path);
					path = "";
					i = j - 1;
					break;
				}
				if(j - i > FS_MAXFILENAME)
					return false;
				path += pathname[j];
			}
		}
	}
	paths.push_back(path);
    return true;
}
