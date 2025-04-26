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
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>

#define BUFFER_SIZE 4096
#define MAX_CLIENTS 10
#define MAX_EMAIL_SIZE 8192
#define MAILBOX_DIR "mailbox"

// Response codes
#define OK "200 OK\r\n"
#define ERR_SYNTAX "400 ERR Invalid command syntax\r\n"
#define ERR_NOT_FOUND "401 NOT FOUND Requested email does not exist\r\n"
#define ERR_FORBIDDEN "403 FORBIDDEN Action not permitted\r\n"
#define ERR_SERVER "500 SERVER ERROR\r\n"

// Client session state
typedef struct {
    char sender[256];
    char recipient[256];
    int is_authenticated;
    int has_sender;
    int has_recipient;
} ClientState;

// Function to handle client connection
void *handle_client(void *arg);

// Protocol command handlers
void handle_helo(int client_socket, char *client_id, ClientState *state);
void handle_mail_from(int client_socket, char *sender, ClientState *state);
void handle_rcpt_to(int client_socket, char *recipient, ClientState *state);
void handle_data(int client_socket, ClientState *state);
void handle_list(int client_socket, char *email);
void handle_get_mail(int client_socket, char *email, int id);
void handle_quit(int client_socket);

// Helper functions
void create_mailbox_if_not_exists();
void save_email(const char *recipient, const char *sender, const char *content);
char *get_current_date();
void send_response(int client_socket, const char *response);

// Global variables
pthread_mutex_t mailbox_mutex = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t thread_id;

    // Create server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Error creating socket");
        return 1;
    }

    // Set socket options to reuse address
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Error setting socket options");
        close(server_socket);
        return 1;
    }

    // Initialize server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    // Bind socket to the specified port
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error binding socket");
        close(server_socket);
        return 1;
    }

    // Listen for incoming connections
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Error listening");
        close(server_socket);
        return 1;
    }

    printf("Listening on port %d...\n", port);

    // Create mailbox directory if it doesn't exist
    create_mailbox_if_not_exists();

    // Handle SIGINT to gracefully shut down the server
    signal(SIGINT, (void (*)(int))exit);

    // Accept and handle client connections
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("Error accepting connection");
            continue;
        }

        printf("Client connected: %s\n", inet_ntoa(client_addr.sin_addr));

        // Create a new thread to handle the client
        int *client_sock = malloc(sizeof(int));
        *client_sock = client_socket;
        
        if (pthread_create(&thread_id, NULL, handle_client, (void *)client_sock) != 0) {
            perror("Error creating thread");
            free(client_sock);
            close(client_socket);
        } else {
            pthread_detach(thread_id);
        }
    }

    // Close server socket
    close(server_socket);
    return 0;
}

void *handle_client(void *arg) {
    int client_socket = *((int *)arg);
    free(arg);
    
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    ClientState state = {0};

    // Send welcome message
    send_response(client_socket, OK);

    while ((bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';
        
        // Remove trailing newline
        char *newline = strchr(buffer, '\n');
        if (newline) *newline = '\0';
        
        // Remove carriage return if present
        newline = strchr(buffer, '\r');
        if (newline) *newline = '\0';

        printf("Received: %s\n", buffer);

        // Parse command
        char command[16] = {0};
        char argument[BUFFER_SIZE] = {0};
        
        if (sscanf(buffer, "%15s %[^\n]", command, argument) < 1) {
            send_response(client_socket, ERR_SYNTAX);
            continue;
        }

        // Handle commands
        if (strcmp(command, "HELO") == 0) {
            handle_helo(client_socket, argument, &state);
        } else if (strcmp(command, "MAIL") == 0) {
            // Extract email from "MAIL FROM: <email>"
            char email[256] = {0};
            if (sscanf(argument, "FROM: %255s", email) == 1) {
                handle_mail_from(client_socket, email, &state);
            } else {
                send_response(client_socket, ERR_SYNTAX);
            }
        } else if (strcmp(command, "RCPT") == 0) {
            // Extract email from "RCPT TO: <email>"
            char email[256] = {0};
            if (sscanf(argument, "TO: %255s", email) == 1) {
                handle_rcpt_to(client_socket, email, &state);
            } else {
                send_response(client_socket, ERR_SYNTAX);
            }
        } else if (strcmp(command, "DATA") == 0) {
            handle_data(client_socket, &state);
        } else if (strcmp(command, "LIST") == 0) {
            handle_list(client_socket, argument);
        } else if (strcmp(command, "GET_MAIL") == 0) {
            char email[256] = {0};
            int id;
            if (sscanf(argument, "%255s %d", email, &id) == 2) {
                handle_get_mail(client_socket, email, id);
            } else {
                send_response(client_socket, ERR_SYNTAX);
            }
        } else if (strcmp(command, "QUIT") == 0) {
            handle_quit(client_socket);
            break;
        } else {
            send_response(client_socket, ERR_SYNTAX);
        }
    }

    if (bytes_read <= 0) {
        if (bytes_read < 0) {
            perror("Error reading from socket");
        }
        printf("Client disconnected\n");
    }

    close(client_socket);
    return NULL;
}

void handle_helo(int client_socket, char *client_id, ClientState *state) {
    printf("HELO received from %s\n", client_id);
    state->is_authenticated = 1;
    send_response(client_socket, OK);
}

void handle_mail_from(int client_socket, char *sender, ClientState *state) {
    if (!state->is_authenticated) {
        send_response(client_socket, ERR_FORBIDDEN);
        return;
    }

    printf("MAIL FROM: %s\n", sender);
    strcpy(state->sender, sender);
    state->has_sender = 1;
    send_response(client_socket, OK);
}

void handle_rcpt_to(int client_socket, char *recipient, ClientState *state) {
    if (!state->is_authenticated || !state->has_sender) {
        send_response(client_socket, ERR_FORBIDDEN);
        return;
    }

    printf("RCPT TO: %s\n", recipient);
    strcpy(state->recipient, recipient);
    state->has_recipient = 1;
    send_response(client_socket, OK);
}

void handle_data(int client_socket, ClientState *state) {
    if (!state->is_authenticated || !state->has_sender || !state->has_recipient) {
        send_response(client_socket, ERR_FORBIDDEN);
        return;
    }

    printf("DATA received...\n");
    
    // Tell client to start sending data
    send_response(client_socket, "354 Start mail input; end with a single dot '.'\r\n");
    
    char email_content[MAX_EMAIL_SIZE] = {0};
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    int content_length = 0;
    
    // Add metadata to email
    char date[64];
    strcpy(date, get_current_date());
    content_length += snprintf(email_content + content_length, 
                              MAX_EMAIL_SIZE - content_length,
                              "From: %s\nDate: %s\n", 
                              state->sender, date);
    
    // Read email content until single dot
    while ((bytes_read = recv(client_socket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';
        
        // Check for single dot
        if (strcmp(buffer, ".\r\n") == 0 || strcmp(buffer, ".\n") == 0) {
            break;
        }
        
        // Append to email content
        if (content_length + bytes_read < MAX_EMAIL_SIZE) {
            strcat(email_content, buffer);
            content_length += bytes_read;
        } else {
            // Email too large
            send_response(client_socket, ERR_SERVER);
            return;
        }
    }
    
    // Save the email
    save_email(state->recipient, state->sender, email_content);
    
    printf("Message stored.\n");
    send_response(client_socket, "200 Message stored successfully\r\n");
    
    // Reset state for next email
    state->has_sender = 0;
    state->has_recipient = 0;
    memset(state->sender, 0, sizeof(state->sender));
    memset(state->recipient, 0, sizeof(state->recipient));
}

void handle_list(int client_socket, char *email) {
    printf("LIST %s\n", email);
    
    // Construct the path to the mailbox file
    char mailbox_path[512];
    snprintf(mailbox_path, sizeof(mailbox_path), "%s/%s.txt", MAILBOX_DIR, email);
    
    // Open the mailbox file
    FILE *mailbox = fopen(mailbox_path, "r");
    if (!mailbox) {
        if (errno == ENOENT) {
            // Mailbox doesn't exist
            send_response(client_socket, "200 OK\r\nNo emails found.\r\n");
        } else {
            perror("Error opening mailbox");
            send_response(client_socket, ERR_SERVER);
        }
        return;
    }
    
    char response[BUFFER_SIZE] = "200 OK\r\n";
    char line[BUFFER_SIZE];
    int email_id = 0;
    char sender[256];
    char date[64];
    int found = 0;
    
    while (fgets(line, sizeof(line), mailbox)) {
        if (strncmp(line, "--- Email ID:", 13) == 0) {
            sscanf(line, "--- Email ID: %d ---", &email_id);
            found = 1;
        } else if (strncmp(line, "From:", 5) == 0 && found) {
            sscanf(line, "From: %s", sender);
            found = 2;
        } else if (strncmp(line, "Date:", 5) == 0 && found == 2) {
            sscanf(line, "Date: %[^\n]", date);
            
            char email_info[512];
            snprintf(email_info, sizeof(email_info), "%d: Email from %s (%s)\r\n", 
                    email_id, sender, date);
            strcat(response, email_info);
            
            found = 0;
        }
    }
    
    fclose(mailbox);
    
    if (strlen(response) <= 10) { // Just "200 OK\r\n"
        strcat(response, "No emails found.\r\n");
    }
    
    printf("Emails retrieved; list sent.\n");
    send_response(client_socket, response);
}

void handle_get_mail(int client_socket, char *email, int id) {
    printf("GET_MAIL %s %d\n", email, id);
    
    // Construct the path to the mailbox file
    char mailbox_path[512];
    snprintf(mailbox_path, sizeof(mailbox_path), "%s/%s.txt", MAILBOX_DIR, email);
    
    // Open the mailbox file
    FILE *mailbox = fopen(mailbox_path, "r");
    if (!mailbox) {
        if (errno == ENOENT) {
            // Mailbox doesn't exist
            send_response(client_socket, ERR_NOT_FOUND);
        } else {
            perror("Error opening mailbox");
            send_response(client_socket, ERR_SERVER);
        }
        return;
    }
    
    char line[BUFFER_SIZE];
    char email_content[MAX_EMAIL_SIZE] = "200 OK\r\n";
    int current_id = 0;
    int found = 0;
    int in_email = 0;
    
    while (fgets(line, sizeof(line), mailbox)) {
        if (strncmp(line, "--- Email ID:", 13) == 0) {
            sscanf(line, "--- Email ID: %d ---", &current_id);
            if (current_id == id) {
                found = 1;
                in_email = 1;
            } else if (in_email) {
                break; // We've moved past the requested email
            }
        } else if (found && in_email) {
            if (strncmp(line, "--- End Email", 13) == 0) {
                in_email = 0;
            } else {
                strcat(email_content, line);
            }
        }
    }
    
    fclose(mailbox);
    
    if (found) {
        printf("Email with id %d sent.\n", id);
        send_response(client_socket, email_content);
    } else {
        printf("Email with id %d not found.\n", id);
        send_response(client_socket, ERR_NOT_FOUND);
    }
}

void handle_quit(int client_socket) {
    printf("Client requested QUIT\n");
    send_response(client_socket, "200 Goodbye\r\n");
}

void create_mailbox_if_not_exists() {
    struct stat st = {0};
    if (stat(MAILBOX_DIR, &st) == -1) {
        if (mkdir(MAILBOX_DIR, 0700) == -1) {
            perror("Error creating mailbox directory");
            exit(1);
        }
        printf("Created mailbox directory\n");
    }
}

void save_email(const char *recipient, const char *sender, const char *content) {
    pthread_mutex_lock(&mailbox_mutex);
    
    // Construct the path to the mailbox file
    char mailbox_path[512];
    snprintf(mailbox_path, sizeof(mailbox_path), "%s/%s.txt", MAILBOX_DIR, recipient);
    
    // Open the mailbox file in append mode
    FILE *mailbox = fopen(mailbox_path, "a+");
    if (!mailbox) {
        perror("Error opening mailbox");
        pthread_mutex_unlock(&mailbox_mutex);
        return;
    }
    
    // Determine the email ID
    int email_id = 1;
    char line[BUFFER_SIZE];
    
    // Go to the beginning of the file
    fseek(mailbox, 0, SEEK_SET);
    
    while (fgets(line, sizeof(line), mailbox)) {
        if (strncmp(line, "--- Email ID:", 13) == 0) {
            int id;
            if (sscanf(line, "--- Email ID: %d ---", &id) == 1) {
                if (id >= email_id) {
                    email_id = id + 1;
                }
            }
        }
    }
    
    // Go to the end of the file
    fseek(mailbox, 0, SEEK_END);
    
    // Write the email with ID and delimiter
    fprintf(mailbox, "\n--- Email ID: %d ---\n", email_id);
    fprintf(mailbox, "%s", content);
    fprintf(mailbox, "\n--- End Email ID: %d ---\n", email_id);
    
    fclose(mailbox);
    pthread_mutex_unlock(&mailbox_mutex);
}

char *get_current_date() {
    static char date_str[64];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    
    strftime(date_str, sizeof(date_str), "%d-%m-%Y", t);
    return date_str;
}

void send_response(int client_socket, const char *response) {
    if (send(client_socket, response, strlen(response), 0) < 0) {
        perror("Error sending response");
    }
}