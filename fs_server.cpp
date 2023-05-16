#include "fs_client.h"
#include "fs_server.h"
#include "fs_socket.h"
#include "fs_filesystem.h"

#include <queue>
#include <mutex>
#include <unordered_map>

std::queue<uint32_t> free_blocks;
std::unordered_map<int, std::mutex> inode_locks; //can we keep a lock for eveyr inode if needed

void init()
{
    //root init
	std::vector<bool> full_blocks;
	full_blocks.resize(FS_DISKSIZE, false);
    std::queue<int> q;
    fs_inode root;
    q.push(0);
    while(!q.empty())
    {
        uint32_t top = q.front();
		full_blocks[top] = true;
		disk_readblock(top, (void*)&root);
        q.pop();
		if(root.type == 'f') {
			for(unsigned int i = 0; i < root.size; ++i) {
				full_blocks[root.blocks[i]] = true;
			}
		}
		else {
			fs_direntry dir_block[FS_DIRENTRIES];
			for(size_t i = 0; i < root.size; ++i)
			{
				full_blocks[root.blocks[i]] = true;
				disk_readblock(root.blocks[i], (void*)dir_block);
				for(unsigned int j = 0; j < FS_DIRENTRIES; ++j) {
					if(dir_block[j].inode_block != 0) {
						q.push(dir_block[j].inode_block);
					}
				}
			}
		}

    }

	for(size_t i = 0; i < full_blocks.size(); ++i) {
		if(!full_blocks[i]) {
			free_blocks.push(i);
		}
	}
}

int main(int argc, const char **argv) {
	// Parse command line arguments
	if (argc > 2) {
		printf("Usage: ./server port_num\n");
		return 1;
	}
	int port = (argc == 2) ? atoi(argv[1]) : 0;

	init();

	//calls driver function that runs indefinitely
	if (run_server(port, 30) == -1) {
		return 1;
	}
	return 0;
}