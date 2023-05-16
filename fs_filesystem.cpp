#include "fs_client.h"
#include "fs_server.h"
#include "fs_filesystem.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cstring>
#include <string>
#include <sstream>
#include <strstream>
#include <queue>
#include <set>
#include <unordered_map>

#include <arpa/inet.h>		// htons()
#include <stdio.h>		// printf(), perror()
#include <stdlib.h>		// atoi()
#include <cassert>

extern std::queue<uint32_t> free_blocks;
extern std::unordered_map<int, std::mutex> inode_locks;
std::mutex q_lock;
/*--------------------READ/WRITE/CREATE/DELETE------------------------*/

/*	-Called on FS_READBLOCK requests-
	Reads the data at a specified block
	i_node: i_node of the file being read
	data: empty string that will be filled with the data being read
	path_num: disc block of i_node
	block: block in inode data array to read 				*/
bool read_block(fs_inode &i_node, char data[], uint32_t path_num, uint32_t block) {
	if(block >= i_node.size || i_node.type == 'd') {
		return false;
	}
	disk_readblock(i_node.blocks[block], (void*)data);
	return true;
}

/*	-Called on FS_WRITEBLOCK requests-
	Writes the data at a specified block
	i_node: i_node of the file being read
	data: string of data to be written
	path_num: disc block of i_node
	block: block in inode data array to write				*/
bool write_block(fs_inode &i_node, char data[], uint32_t path_num, uint32_t block) {

	//Block does not exist in file
	if(i_node.type == 'd')
	{
		return false;
	}
	if(block == i_node.size) {			//is this check correct: >=
		if(block >= FS_MAXFILEBLOCKS)
		{
			return false;
		}

		Lock_RAII q_mutex(&q_lock);
		if(free_blocks.empty()) {
			return false;
		}
		uint32_t free_block = free_blocks.front();

		disk_writeblock(free_block, (void*)data);
		i_node.blocks[i_node.size++] = free_block;
		free_blocks.pop();
		disk_writeblock(path_num, (void*)&i_node);

		
	}
	else if(block > i_node.size) {
		return false;
	}
	else {
		disk_writeblock(i_node.blocks[block], (void*)data);
	}

	return true;
}

/*	-Called on FS_CREATE requests-
	Creates new file/directory at specified path
	i_node: the directory before the created file (1 level up)
	path_num: i_node's block number
	username: name of the person creating a file
	final_path: name of the file/directory to be deleted
	type: 'f' or 'd' for file or directory					*/
bool create_path(const std::string path, fs_inode &i_node, uint32_t path_num, const std::string &username, char type) {
	Lock_RAII q_mutex(&q_lock);
	if(free_blocks.empty()) {
		return false;
	}
	uint32_t free_block = free_blocks.front();

	//Checks to see if a spot exists
	fs_direntry dir_block[FS_DIRENTRIES];

	//Creates inode for new file/directory
	fs_inode new_node;
	strcpy(new_node.owner, username.c_str());
	new_node.size = 0;
	new_node.type = type;
	unsigned int dir_idx = FS_DIRENTRIES, block_idx;
	fs_direntry final_dirblock[FS_DIRENTRIES];

	for(unsigned int i = 0; i < i_node.size; ++i) {
		disk_readblock(i_node.blocks[i], (void*)dir_block);
		for(unsigned int j = 0; j < FS_DIRENTRIES; ++j) {
			//Empty direntry
			if(dir_block[j].inode_block == 0 && dir_idx == FS_DIRENTRIES) {
				dir_idx = j;
				block_idx = i;
				for(unsigned int w = 0; w < FS_DIRENTRIES; ++w) {
					final_dirblock[w].inode_block = dir_block[w].inode_block;
					strcpy(final_dirblock[w].name, dir_block[w].name);
				}
			}

			if(dir_block[j].inode_block != 0 && strcmp(dir_block[j].name, path.c_str()) == 0) { //TODO: OH file and directory same name
				return false; //TODO looping all the way through, show we use a separate structure?
			}
		}
	}
	if(dir_idx != FS_DIRENTRIES) {
		strcpy(final_dirblock[dir_idx].name, path.c_str());
		disk_writeblock(free_block, (void*)&new_node); 

		final_dirblock[dir_idx].inode_block = free_block;
		disk_writeblock(i_node.blocks[block_idx], (void*)final_dirblock);
		free_blocks.pop();
		return true;
	}
	//Need to create new block
	if(i_node.size >= FS_MAXFILEBLOCKS) {
		return false;
	}
	
	disk_writeblock(free_block, (void*)&new_node);

	//Creates new direntry block to put file inode into
	fs_direntry new_dir_block[FS_DIRENTRIES];
    strcpy(new_dir_block[0].name, path.c_str());
	new_dir_block[0].inode_block = free_block;
	for(unsigned int i = 1; i < FS_DIRENTRIES; ++i) {
		new_dir_block[i].inode_block = 0;
	}
	free_blocks.pop();
	free_block = free_blocks.front();
	disk_writeblock(free_block, (void*)new_dir_block);
	i_node.blocks[i_node.size] = free_block;
	i_node.size++;

	//Updates overall inode
	disk_writeblock(path_num, (void*)&i_node); 
	free_blocks.pop();
	return true;
}

/*	-Called on FS_DELETE requests-
	Deletes specific file/directory and updates inodes/direntries accordingly
	i_node: the directory before the deleted file (1 level up)
	path_num: i_node's block number
	final_path: name of the file/directory to be deleted	*/
bool delete_path(fs_inode &i_node, uint32_t path_num, const std::string &final_path, const std::string &username) {

	//i_node points to the path right before the one getting deleted
	fs_direntry dir_block[FS_DIRENTRIES];

    uint32_t direntry_idx, block_idx;
    uint32_t final_block = FS_DISKSIZE;

	for(uint32_t i = 0; i < i_node.size; ++i) {
		disk_readblock(i_node.blocks[i], (void*)dir_block);
		for(unsigned int j = 0; j < FS_DIRENTRIES; ++j) {
			std::string dir_name(dir_block[j].name);
			if(dir_name == final_path && dir_block[j].inode_block != 0) {
				final_block = dir_block[j].inode_block;
				direntry_idx = j;
				block_idx = i;
				break;
			}
		}
		if(final_block < FS_DISKSIZE)
			break;
	}

	if(final_block == FS_DISKSIZE) {
		return false;
	}

	fs_inode victim;
	Lock_RAII victim_lock(&inode_locks[final_block]);
	Lock_RAII q_mutex(&q_lock);

	disk_readblock(final_block, (void*)&victim);
	if(strcmp(victim.owner, username.c_str()) != 0)
	{
		return false;
	}
	if(victim.type == 'f') {
		delete_file(victim); 
	}
	else if(victim.size > 0){
		return false;
	}

    //delete dir entry in both cases, check if direntry array is empty, do writes
	free_blocks.push(final_block);
	dir_block[direntry_idx].inode_block = 0;

	bool empty = true;
	for(unsigned int i = 0; i < FS_DIRENTRIES; ++i) {
		if(dir_block[i].inode_block != 0)
		{
			empty = false;
		}
	}

	if(empty) {
		free_blocks.push(i_node.blocks[block_idx]);
		for(unsigned int i = block_idx; i < i_node.size - 1; ++i) {
			i_node.blocks[i] = i_node.blocks[i+1];
		}
		i_node.size--;
		disk_writeblock(path_num, (void*)&i_node);
	}
	else {
		disk_writeblock(i_node.blocks[block_idx], (void*)dir_block);
	}

	return true;


}


/*----------------------------HELPERS-----------------------------*/

//Deals with deleteing an inode if its a file
void delete_file(fs_inode &file) {
	for(uint32_t i = 0; i < file.size; ++i) {
		std::cout << "freeing data block " << file.blocks[i] << std::endl;
		free_blocks.push(file.blocks[i]);
	}
	file.size = 0;
}

/*
	Uses &path to linearly search from root til the critical part in path
	Create/Delete return block_num for the directory in which specified file/folder is
	Read/Write return block_num to the file they wish to write/read to
	path: vector of strings of the path
	command: command entered by user
	username: name of user
*/
uint32_t pathTraversal(const std::vector<std::string> &path, fs_inode &inode, const std::string &command, const std::string &username, Lock_RAII &lck) {
	// "/dir/beach/pie"
	uint32_t block_num = 0;


	//inode_locks[block_num].lock();
	if(command == "FS_CREATE" && path.size() == 0) {
		return FS_DISKSIZE;
	}

	//inode_locks[block_num].lock();
	disk_readblock(0, (void*)&inode);
	bool is_create_or_delete = (command == "FS_CREATE" || command == "FS_DELETE");
	size_t path_size = path.size(); 
	if(is_create_or_delete) 
	{
		path_size--; //create and delete want the directory one above the dir/file to delete
	}

	//path[i] is path we are currenty trying to find block for
	for(size_t i = 0; i < path_size; ++i) {
		//traverse every block in current path to find path[i]
		if(inode.type == 'd') { 
			block_num = find_node(inode, path[i], username, i, lck);

            if(block_num == FS_DISKSIZE)
                return FS_DISKSIZE;
		}
		else if(!is_create_or_delete && i < path_size - 1) {
			return FS_DISKSIZE;
		}

		if(is_create_or_delete && inode.type == 'f') {
			return FS_DISKSIZE;
		}
	}
	return block_num;
}

//Returns the block number of the directory/file that is to be modified or FS_DISKSIZE if not found
unsigned int find_node(fs_inode &inode, const std::string &path, const std::string &username, size_t idx, Lock_RAII &lck) {

    fs_direntry dir_block[FS_DIRENTRIES];

	for(uint32_t i = 0; i < inode.size; ++i) {
		disk_readblock(inode.blocks[i], (void*)dir_block);
		//Traverse every direntry in block to see if one is path[i]
		for(unsigned int j = 0; j < FS_DIRENTRIES; ++j) {
			if(dir_block[j].inode_block != 0) {
				std::string dir_name(dir_block[j].name);
				assert(inode.type == 'd');
				if(dir_name == path) { 
					//block_num = dir_block[k].inode_block;

					//Updates input parameter
					//inode_locks[dir_block[j].inode_block].lock();
					//inode_locks[block_num].unlock();	//pass in block num

					Lock_RAII new_lck(&inode_locks[dir_block[j].inode_block]);
					std::swap(lck, new_lck);
					
					disk_readblock(dir_block[j].inode_block, (void*)&inode);
					//inode_locks[block_num].unlock();
					if(strcmp(inode.owner, username.c_str()) != 0) {
                        if(idx == 0) {
                            return FS_DISKSIZE; //user does not own the directory
                        }
                        else {
                            assert(false); //file system is incorrect so break
                        }
                    }

					return dir_block[j].inode_block;
				}
			}
		}
	}
	return FS_DISKSIZE;
}