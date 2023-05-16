#include "fs_client.h"
#include "fs_server.h"

#include <string>
#include <vector>

class Lock_RAII
{
	public:

		std::mutex *lock;
		Lock_RAII(std::mutex *m)
		{
			lock = m;
			lock->lock();
		}
		void raii_unlock()
		{
			lock->unlock();
		}
		~Lock_RAII()
		{
			lock->unlock();
		}
};
/*--------------------READ/WRITE/CREATE/DELETE------------------------*/
//All return false if instruction cannot be completed, else true

/*	-Called on FS_READBLOCK requests-
	Reads the data at a specified block
	i_node: i_node of the file being read
	data: empty string that will be filled with the data being read
	path_num: disc block of i_node
	block: block in inode data array to read 				*/
bool read_block(fs_inode &i_node, char data[], uint32_t path_num, uint32_t block);

/*	-Called on FS_WRITEBLOCK requests-
	Writes the data at a specified block
	i_node: i_node of the file being read
	data: string of data to be written
	path_num: disc block of i_node
	block: block in inode data array to write				*/
bool write_block(fs_inode &i_node, char data[], uint32_t path_num, uint32_t block);

/*	-Called on FS_CREATE requests-
	Creates new file/directory at specified path
	i_node: the directory before the created file (1 level up)
	path_num: i_node's block number
	username: name of the person creating a file
	final_path: name of the file/directory to be deleted
	type: 'f' or 'd' for file or directory					*/
bool create_path(const std::string path, fs_inode &i_node, uint32_t path_num, const std::string &username, char type);


/*	-Called on FS_DELETE requests-
	Deletes specific file/directory and updates inodes/direntries accordingly
	i_node: the directory before the deleted file (1 level up)
	path_num: i_node's block number
	final_path: name of the file/directory to be deleted	*/
bool delete_path(fs_inode &i_node, uint32_t path_num, const std::string &final_path, const std::string &username);


/*----------------------------HELPERS-----------------------------*/

//Deals with deleteing an inode if its a file
void delete_file(fs_inode &file);

/*
	Uses &path to linearly search from root til the critical part in path
	Create/Delete return block_num for the directory in which specified file/folder is
	Read/Write return block_num to the file they wish to write/read to
*/
uint32_t pathTraversal(const std::vector<std::string> &path, fs_inode &inode, const std::string &command, const std::string &username, Lock_RAII &lck);

unsigned int find_node(fs_inode &inode, const std::string &path, const std::string &username, size_t idx, Lock_RAII &lck);