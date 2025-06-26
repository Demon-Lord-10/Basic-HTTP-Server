#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

int main(){

	int server_fd,client_addr_len;
	struct sockaddr_in client_addr;


	setbuf(stdout,NULL);
	setbuf(stderr,NULL);


	// creating a socket
	// if socket creation is successful then returns 0 else -1
	
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) {
		printf("Socket creation failed: %s...\n", strerror(errno));
		return 1;
	}


	//Helps with quick restart of server and reuses the same address and port

	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		printf("SO_REUSEADDR failed: %s \n", strerror(errno));
		return 1;
	}
	
	// AF_INET - ipv4
	// stores port and address
	struct sockaddr_in serv_addr = {
		.sin_family = AF_INET ,
		.sin_port = htons(4221),			//port in netwrok byte order
		.sin_addr = { htonl(INADDR_ANY) },           //accepts any connections on any local network address
	};

	//BINDING
	//Attaches the socket to a specific IP adress and port
	if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
		printf("Bind failed: %s \n", strerror(errno));
		return 1;
	}

	//LISTEN
	int connection_backlog = 5;				//number of clients
	if (listen(server_fd, connection_backlog) != 0) {
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}

	printf("Waiting for a client to connect...\n");
	client_addr_len = sizeof(client_addr);
	
	//ACCEPT
	int client_fd=accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
	
	if(client_fd<0){
		printf("Accept Failed: %s \n",strerror(errno));
		close(server_fd);
		return 1;
	}

	printf("Client connected\n");
	close(client_fd);
	close(server_fd);
	

}
