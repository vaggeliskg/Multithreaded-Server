#include "header.h"

void perror_exit(char *message);

// function used to send a message to the server 
void send_message(int socket, const char *message) {
    uint32_t size = htonl(strlen(message) + 1); 
    if (write(socket, &size, sizeof(size)) < 0) {
        perror_exit("write size");
    }
    if (write(socket, message, strlen(message) + 1) < 0) { 
        perror_exit("write message");
    }
}

// function used for receiving messages from the server
void receive_message(int socket) {
    uint32_t size;
    if (read(socket, &size, sizeof(size)) < 0) {
        perror_exit("read size");
    }
    size = ntohl(size);

    char *buffer = (char *)malloc(size);
    if (buffer == NULL) {
        perror_exit("malloc");
    }

    if (read(socket, buffer, size) < 0) {
        perror_exit("read message");
    }

    printf("%s\n", buffer);
    free(buffer);
}



int main(int argc, char *argv[]) {
    int total_length = 0;
    for (int i = 3; i < argc; i++) {
        total_length += strlen(argv[i]) + 1; 
    }

    char *command = malloc(total_length * sizeof(char));
    int command_length = strlen(command);
    char *buf = (char*) malloc( command_length * sizeof (char));

    int port, sock, secret_number;

    struct sockaddr_in server;
    struct sockaddr *serverptr = (struct sockaddr *)&server;
    struct hostent *rem;
   
    /* Create socket */
    if (( sock = socket (AF_INET, SOCK_STREAM, 0) ) < 0)
        perror_exit("socket");


    /* Find server address */
    if (( rem = gethostbyname (argv[1]) ) == NULL ) {
    herror("gethostbyname"); exit(1);
    }

    port = atoi(argv[2]) ; /* Convert port number to integer */

    server.sin_family = AF_INET; /* Internet domain */
    memcpy(&server.sin_addr, rem->h_addr, rem->h_length );
    server.sin_port = htons(port); /* Server port */

    if (connect(sock ,serverptr ,sizeof (server)) < 0) {
        perror_exit("connect");
    }

    printf("Connecting to %s port %d \n", argv[1] ,port);


    // store the message
    buf[0] = '\0';
	for (int i = 3; i < argc; i++)
	{
		strcat(buf, argv[i]);
		strcat(buf, " ");
	}

    size_t len = strlen(buf);
    for( int i = 0; i < len; i ++) {
        if( buf[i] == '\n') {
            buf[i] = '\0';
            break;
        }
    }
    // if exit is sent send a secret number indicating that main must exit 
    // after completing the neccessary tasks
    if(strcmp(argv[3], "exit") == 0) {
        secret_number = 1;
        if (write(sock, &secret_number, sizeof(secret_number)) < 0) {
            perror_exit("write size");
        }

    }
    else {
        // else: send the default secret number = 0 
        secret_number = 0;
        if (write(sock, &secret_number, sizeof(secret_number)) < 0) {
            perror_exit("write size");
        }
    }

    send_message(sock, buf);

    // Receive response from the server
    receive_message(sock);

    if(strcmp(argv[3], "issueJob") == 0) {
        receive_message(sock);
    }


    close(sock);
    return 0;
}

void perror_exit(char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}