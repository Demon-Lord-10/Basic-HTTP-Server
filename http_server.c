#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <sys/stat.h>

#define PORT 4221
#define MAX_CLIENTS 5
#define BUFFER_SIZE 4096
#define RESPONSE_SIZE 2048

// Helper to guess content type from file extension
const char* get_mime_type(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return "application/octet-stream";
    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".txt") == 0) return "text/plain";
    if (strcmp(ext, ".jpg") == 0) return "image/jpeg";
    if (strcmp(ext, ".png") == 0) return "image/png";
    // Add more as needed
    return "application/octet-stream";
}

/*-----------------------------------------------------------------------------------------------------------------------------------------------------
 *Send Response
 *-----------------------------------------------------------------------------------------------------------------------------------------------------
 * For the Send Response function we take the following as the input
 *
 * 	1)Client_file_descriptor
 *	2)Satus code
 *	3)Content type
 *	4)Body
 *
 * What this function does:
 * 	-prepares an HTTP response for the required routing done
 *
 * What our responses will look like
 *
 *       "HTTP/1.1 %s\r\n"
 *       "Content-Type: text/{content_type}\r\n"
 *       "Content-Length: {content_length}\r\n"
 *       "Connection: close\r\n"
 *       "\r\n"
 *       {body}
 *
 *	to send the response we use write fn, which takes in the client_fd, response,and response length
 *
 *
 *
 *-----------------------------------------------------------------------------------------------------------------------------------------------------
 * */

void send_response(int client_fd, const char *status, const char *content_type, const char *body) {
    char response[RESPONSE_SIZE];
    int resp_len = snprintf(response, sizeof(response),
        "HTTP/1.1 %s\r\n"
        "Content-Type: text/%s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        status, content_type, strlen(body), body);

    ssize_t bytes_sent = write(client_fd, response, resp_len);
    if (bytes_sent < 0) {
        fprintf(stderr, "Write failed: %s\n", strerror(errno));
    }
}


/*-----------------------------------------------------------------------------------------------------------------------------------------------------
 * Send File Response
 *-----------------------------------------------------------------------------------------------------------------------------------------------------
 * For the send_file_response function, we take the following as input:
 *
 *      1) client_file_descriptor
 *      2) filename (the name of the file to be served)
 *
 * What this function does:
 *      - Attempts to open the specified file in binary read mode.
 *      - If the file is not found, sends a 404 Not Found response.
 *      - Uses stat() to determine the file size for the Content-Length header.
 *      - Reads the entire file content into a buffer.
 *      - Determines the correct Content-Type based on the file extension (using get_mime_type).
 *      - Prepares the HTTP response headers as follows:
 *
 *          "HTTP/1.1 200 OK\r\n"
 *          "Content-Type: {mime_type}\r\n"
 *          "Content-Length: {file_size}\r\n"
 *          "Connection: close\r\n"
 *          "\r\n"
 *          {file_content}
 *
 *      - Sends the headers and the file content to the client using the write function.
 *      - Handles errors such as file not found, stat failure, memory allocation failure, or file read errors,
 *        and sends appropriate error responses (404 or 500).
 *
 * To send the response:
 *      - First, write() is used to send the headers.
 *      - Then, write() is used to send the file content buffer.
 *
 *-----------------------------------------------------------------------------------------------------------------------------------------------------
 */


void send_file_response(int client_fd, const char* filename){
	FILE *fp = fopen(filename,"rb");
	if(!fp){
		send_response(client_fd,"404 Not Found","html","<html> File Not Found </html>");
		return;
	}
 	
	//to get size of the file
	struct stat st;

	if(stat(filename,&st) != 0){                      
		fclose(fp);
		send_response(client_fd,"500 Internal Server Error", "html", "<html>Could not stat file</html>");
		return;
	}

	size_t filesize = st.st_size;

	//make buffer and read the file
	
	char *buffer = malloc(filesize);
	if(!buffer){
		fclose(fp);
		send_response(client_fd, "500 Internal Server Error", "html", "<html>Memory error</html>");
		return;
	}
	fread(buffer,1,filesize,fp);   //Reads data from a file: fread takes a file pointer, a buffer to store the data, the size of each element, and the number of elements to read as arguments. 
	
	fclose(fp);

	//Prepare HTTP headers
	char header[1024];
	const char* mime = get_mime_type(filename);
    	snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n", mime, filesize);
	
 	//send header and file content
	send(client_fd, header, strlen(header), 0);
    	send(client_fd, buffer, filesize, 0);

    free(buffer);	


}

/*-----------------------------------------------------------------------------------------------------------------------------------------------------
 * Handling CLients
 * ----------------------------------------------------------------------------------------------------------------------------------------------------
 *
 * Since we r using multithreading the thread fn is of the type void* th_fn(void* arg)
 * we need to get the arg , so we explicitly convert the arg to (int*) [points to int] and then we dereference it
 *     int client_fd = *(int*)arg;
 *
 * dont forget to free the arg as it is allocated on the heap
 *
 * What this function does:
 * 	-Handles clients
 * 	-Reads the HTTP request
 * 	-Routes the HTTP request
 * 	-Sends the appropriate response to the client
 *
 * How do HTTP requests look like? 
 * 	<Request-Line>\r\n
 *	<Header-Name-1>: <Header-Value-1>\r\n
 *	<Header-Name-2>: <Header-Value-2>\r\n
 *	...
 *	\r\n
 *	<Optional-Body>
 *
 *
 *
 * HTTP responses r of this type
 * 	<Status-Line>\r\n
 *	<Header-Name-1>: <Header-Value-1>\r\n
 *	<Header-Name-2>: <Header-Value-2>\r\n
 *	...\r\n
 *	\r\n
 *	<Response Body>
 *	
 *
 * 	So now we need to route to the required path this is done using simple if statements
 *
 * 	sscanf - used to parse the req to get the path and method
 *
 *	#Routing functions
 *	1)/ -> Home page
 *	2)/echo/{something} -> prints {something} as plain text
 *	3)/file/{filename}  -> downloads the file if it exists
 *	4)/hello -> hello page
 *	5)/user-agent -> prints the application , OS ,device type etc
 *
 *	Used Status Codes
 *	200 -> Succesful request
 *	400 -> Bad request
 *	404 -> Not Found
 *	500 -> Internal Server Error
 *
 *
 *-----------------------------------------------------------------------------------------------------------------------------------------------------
 * */
void* handle_client(void* arg) {
    int client_fd = *(int*)arg;
    free(arg);
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        fprintf(stderr, "Read failed: %s\n", strerror(errno));
        close(client_fd);
        return NULL; // since we are returning void*
    }
    buffer[bytes_read] = '\0';

    char method[16], path[1024];
    if (sscanf(buffer, "%15s %1023s", method, path) != 2) {
        fprintf(stderr, "Invalid request format\n");
        close(client_fd);
        return NULL; // returning void*
    }

    // Simple routing
    if (strcmp(path, "/") == 0) {
        send_response(client_fd, "200 OK", "html", "<html><h1><b>Hello World!</b></h1></html>");
    } else if (strcmp(path, "/hello") == 0) {
        send_response(client_fd, "200 OK", "html", "<html>Hello!</html>");
    } else if (strncmp(path, "/echo/", 6) == 0) {
        char *body = path + 6;
        if (body) {
            send_response(client_fd, "200 OK", "plain", body);
        } else {
            send_response(client_fd, "400 Bad Request", "html", "<html>No body found</html>");
        }
    } else if (strcmp(path, "/user-agent") == 0) {
        char *headers = strstr(buffer, "\r\n");
        if (!headers) {
            send_response(client_fd, "400 Bad Request", "html", "No headers found");
            return NULL;
        }
        headers += 2; // Skip the first \r\n

        // Copy headers to a temp buffer for safe tokenizing
        char headers_copy[BUFFER_SIZE];
        strncpy(headers_copy, headers, sizeof(headers_copy) - 1);
        headers_copy[sizeof(headers_copy) - 1] = '\0';

        // Loop through header lines to find User-Agent
        char *line = strtok(headers_copy, "\r\n");
        char *user_agent = NULL;
        while (line != NULL) {
            if (strncmp(line, "User-Agent:", 11) == 0) {
                user_agent = line + 12; // Skip "User-Agent: "
                break;
            }
            line = strtok(NULL, "\r\n");
        }

        if (user_agent && *user_agent) {
            send_response(client_fd, "200 OK", "html", user_agent);
        } else {
            send_response(client_fd, "404 Not Found", "plain", "User-Agent header not found");
        }
    } else if (strncmp(path, "/file/", 6) == 0) {
        char *filename = path + 6; // Get the filename from the path
        if (filename && strlen(filename) > 0) {
            send_file_response(client_fd, filename);
        } else {
            send_response(client_fd, "400 Bad Request", "html", "<html>No filename specified</html>");
        }
    } else {
        send_response(client_fd, "404 Not Found", "html", "<html>Not Found</html>");
    }

    printf("Handled %s request for %s\n", method, path);
    close(client_fd);
    return NULL;
}

/*-----------------------------------------------------------------------------------------------------------------------------------------------------	
 *	SET UP THE SERVER SOCKET
 *-----------------------------------------------------------------------------------------------------------------------------------------------------
 * 	To set up a sever socket we need to do the following
 * 	create socket -> bind socket to address and port -> Listen -> Accept clients -> do something -> close socket
 *	
 *	# CREATE SOCKET
 * 	we can use the socket(domain,type,protocol)
 * 	domain -> specifies the communication domain , the address family , here we use AF_INET - IPv4
 * 	type -> type of socket , we use SOCK_STREAM - TCP (more reliable) else we can use SOCK_DGRAM - UDP(less reliable)
 * 	protocol -> we use 0 here which lets OS pick the protocol
 *
 * 	socket returns an integer , -1 if its unsuccesful amd non negative if successful 
 *
 * 	setsockopt -> used to reuse the same address 
 *	
 *	struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,	------------->IPV4 Address family
        .sin_port = htons(PORT),------------->Port network byte order
        .sin_addr = { htonl(INADDR_ANY) }---->IPV4 address in network byte order , INADDR_ANY -> local host
    };
 *	
 *	# BINDING
 *	bind(int sockfd , const struct sockaddr *addr , socklen_t addrlen);
 *	sockfd -> file descriptor of the socket
 *	addr -> pointer to struct sockaddr
 *	addrlen -> size of sockaddr
 *
 *	# LISTENING
 *	listen(int serverfd, number of clients)
 *	serverfd -> file descriptor 
 *	number of clients -> maximum number of clients
 *
 *	#ACCEPT
 *      accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
 *	
 *	#Enabling MULTI-THREADING
 *	To do that we use pthread library , pthread -> POSIX thread (OS interface)
 *
 *	we follow the following steps
 *	1) create a fn of the signature -> void* thread_fn(void* arg)
 *		void* -> generic pointer , can point to anything
 *	2) pthread_t tid -> thread id
 *	3) pthread_create() -> create thread returns 0 if fails
 *		pthread_create(&tid, NULL, thread_fn, arguements for the fn)    ,NULL->thread attributes
 *	4) Detach or Join threads
 *
 *	Detach -> cleans the resources once the function is done executing
 *	
 *
 *-----------------------------------------------------------------------------------------------------------------------------------------------------
*/
int setup_server_socket() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        fprintf(stderr, "Socket creation failed: %s\n", strerror(errno));
        return -1;
    }

    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        fprintf(stderr, "SO_REUSEADDR failed: %s\n", strerror(errno));
        close(server_fd);
        return -1;
    }

    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr = { htonl(INADDR_ANY) }
    };

    if (bind(server_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) != 0) {
        fprintf(stderr, "Bind failed: %s\n", strerror(errno));
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, MAX_CLIENTS) != 0) {
        fprintf(stderr, "Listen failed: %s\n", strerror(errno));
        close(server_fd);
        return -1;
    }

    return server_fd;
}

int main() {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    int server_fd = setup_server_socket();
    if (server_fd == -1) return 1;

    printf("HTTP server running on http://localhost:%d\n", PORT);

    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    for (;;) {
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_fd < 0) {
            fprintf(stderr, "Accept failed: %s\n", strerror(errno));
            continue;
        }
	//Adding Multi Threading
	int* client_fd_ptr = (int*)malloc(sizeof(int));      
	*client_fd_ptr = client_fd;				//Needed to send the arguments
	
	pthread_t tid;                //Thread id
	
	if(pthread_create(&tid, NULL, handle_client, client_fd_ptr) != 0){
		
		free(client_fd_ptr);
		close(client_fd);
	}
	else{
		pthread_detach(tid);
	}
    }

    close(server_fd);
    return 0;
}
