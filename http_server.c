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

#define PORT 4221
#define MAX_CLIENTS 5
#define BUFFER_SIZE 4096
#define RESPONSE_SIZE 2048

// Send HTTP response
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

// Handle client requests
void handle_client(int client_fd) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        fprintf(stderr, "Read failed: %s\n", strerror(errno));
        close(client_fd);
        return;
    }
    buffer[bytes_read] = '\0';

    char method[16], path[1024];
    if (sscanf(buffer, "%15s %1023s", method, path) != 2) {
        fprintf(stderr, "Invalid request format\n");
        close(client_fd);
        return;
    }

    // Simple routing
    if (strcmp(path, "/") == 0) {
        send_response(client_fd, "200 OK","html", "<html><h1><b>Hello World!</b></h1></html>");
    } else if (strcmp(path, "/hello") == 0) {
        send_response(client_fd, "200 OK", "html" , "<html>Hello!</html>");
    }
    else if(strncmp(path,"/echo/",6)==0){
	char *body = path+6;
        if (body) {
            send_response(client_fd, "200 OK","plain" , body);
	}
        else {
            send_response(client_fd, "400 Bad Request","html", "<html>No body found</html>");
        }
    }
    else if(strcmp(path,"/user-agent")==0){
	char *headers = strstr(buffer, "\r\n");
    	if (!headers) {
        	send_response(client_fd, "400 Bad Request","html", "No headers found");
        	return;
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
        send_response(client_fd, "200 OK","html", user_agent);
    	} else {
        send_response(client_fd, "404 Not Found","plain", "User-Agent header not found");
    	}
	}
    else {
        send_response(client_fd, "404 Not Found","html", "<html>Not Found</html>");
    }

    printf("Handled %s request for %s\n", method, path);
    close(client_fd);
}

// Create, bind, and listen on socket
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
        handle_client(client_fd);
    }

    close(server_fd);
    return 0;
}
