/*
=====================================
Assignment 6 Submission
Name: Praveen Kumar
Roll number: 22CS10054
=====================================
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>

#define BUFFER_SIZE 4096

// Function declarations
int connect_to_server(const char *server_ip, int port);
void send_command(int socket, const char *command);
char *receive_response(int socket);
void handle_data_command(int socket);
void print_help();

// Global variables
int server_socket = -1;

// Signal handler for Ctrl+C
void handle_sigint(int sig) {
    printf("\nInterrupted. Sending QUIT command to server...\n");
    if (server_socket != -1) {
        send_command(server_socket, "QUIT");
        char *response = receive_response(server_socket);
        printf("%s\n", response);
        free(response);
        close(server_socket);
    }
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <port>\n", argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];
    int port = atoi(argv[2]);

    // Register signal handler
    signal(SIGINT, handle_sigint);

    // Connect to server
    server_socket = connect_to_server(server_ip, port);
    if (server_socket < 0) {
        fprintf(stderr, "Failed to connect to server\n");
        return 1;
    }

    printf("Connected to My_SMTP server.\n");
    
    // Receive initial server greeting
    char *response = receive_response(server_socket);
    printf("%s", response);
    free(response);

    // Print help information
    print_help();

    // Main command loop
    char command[BUFFER_SIZE];
    while (1) {
        printf("> ");
        if (fgets(command, BUFFER_SIZE, stdin) == NULL) {
            break;
        }

        // Remove trailing newline
        size_t len = strlen(command);
        if (len > 0 && command[len - 1] == '\n') {
            command[len - 1] = '\0';
        }

        // Skip empty commands
        if (strlen(command) == 0) {
            continue;
        }

        // Check for help command
        if (strcmp(command, "HELP") == 0 || strcmp(command, "help") == 0) {
            print_help();
            continue;
        }

        // Handle DATA command specially
        if (strncmp(command, "DATA", 4) == 0) {
            handle_data_command(server_socket);
            continue;
        }

        // Send command to server
        send_command(server_socket, command);

        // Receive and display response
        response = receive_response(server_socket);
        printf("%s", response);
        free(response);

        // Check if the command was QUIT
        if (strncmp(command, "QUIT", 4) == 0) {
            printf("Disconnected from server.\n");
            break;
        }
    }

    // Close connection
    close(server_socket);
    return 0;
}

int connect_to_server(const char *server_ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Error creating socket");
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address or address not supported");
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        return -1;
    }

    return sock;
}

void send_command(int socket, const char *command) {
    char buffer[BUFFER_SIZE];
    snprintf(buffer, BUFFER_SIZE, "%s\r\n", command);
    
    if (send(socket, buffer, strlen(buffer), 0) < 0) {
        perror("Error sending command");
    }
}

char *receive_response(int socket) {
    char *response = malloc(BUFFER_SIZE);
    if (!response) {
        perror("Memory allocation failed");
        return NULL;
    }
    
    memset(response, 0, BUFFER_SIZE);
    
    ssize_t bytes_received = recv(socket, response, BUFFER_SIZE - 1, 0);
    if (bytes_received < 0) {
        perror("Error receiving response");
        free(response);
        return NULL;
    }
    
    response[bytes_received] = '\0';
    return response;
}

void handle_data_command(int socket) {
    // Send DATA command
    send_command(socket, "DATA");
    
    // Get initial response
    char *response = receive_response(socket);
    printf("%s", response);
    free(response);
    
    // Prompt for message input
    printf("Enter your message (end with a single dot '.' on a new line):\n");
    
    char line[BUFFER_SIZE];
    while (1) {
        if (fgets(line, BUFFER_SIZE, stdin) == NULL) {
            break;
        }
        
        // Check for single dot
        if (strcmp(line, ".\n") == 0) {
            send_command(socket, ".");
            break;
        }
        
        // Send the line
        if (send(socket, line, strlen(line), 0) < 0) {
            perror("Error sending data");
            return;
        }
    }
    
    // Get final response
    response = receive_response(socket);
    printf("%s", response);
    free(response);
}

void print_help() {
    printf("\nMy_SMTP Client Commands:\n");
    printf("HELO <client_id>           - Initiate session\n");
    printf("MAIL FROM: <email>         - Specify sender email\n");
    printf("RCPT TO: <email>           - Specify recipient email\n");
    printf("DATA                       - Start message input\n");
    printf("LIST <email>               - List emails for recipient\n");
    printf("GET_MAIL <email> <id>      - Retrieve specific email\n");
    printf("QUIT                       - End session\n");
    printf("HELP                       - Show this help message\n\n");
}