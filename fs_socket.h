#include "fs_client.h"
#include "fs_server.h"

#include <sys/socket.h>     // socket(), bind(), listen(), accept(), send(), recv()
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>


#include <string>
#include <vector>

static const size_t MAX_MESSAGE_SIZE = 256;

/**
 * Endlessly runs a server that listens for connections and serves
 * them _synchronously_.
 *
 * Parameters:
 *		port: 		The port on which to listen for incoming connections.
 *		queue_size: 	Size of the listen() queue
 * Returns:
 *		-1 on failure, does not return on success.
 */
int run_server(int port, int queue_size);

/**
 * Called when run_server receives a request
 * Receives a string message from the client and prints it to stdout.
 *
 * Parameters:
 * 		connectionfd: 	File descriptor for a socket connection
 * 				(e.g. the one returned by accept())
 * Returns:
 *		0 on success, -1 on failure.
 */
int handle_connection(int connectionfd);

bool parse_request(char msg[], size_t recvd, std::vector<std::string> &paths);

std::string generate_response(char msg[], size_t recvd, const std::vector<std::string> &paths, int connectionfd);

//Uses paths by reference so if check goes through, every index is a valid path
bool check_size(std::vector<std::string> &paths, std::string command, std::string username, std::string pathname, std::string data);

size_t receiveBytes(char msg[], int connectionfd, bool is_write);