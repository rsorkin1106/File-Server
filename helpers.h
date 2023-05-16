#include <arpa/inet.h>		// htons(), ntohs()
#include <netdb.h>		// gethostbyname(), struct hostent
#include <netinet/in.h>		// struct sockaddr_in
#include <stdio.h>		// perror(), fprintf()
#include <string.h>		// memcpy()
#include <sys/socket.h>		// getsockname()
#include <unistd.h>		// stderr


int make_server_sockaddr(struct sockaddr_in *addr, int port);

int make_client_sockaddr(struct sockaddr_in *addr, const char *hostname, int port);

 int get_port_number(int sockfd);