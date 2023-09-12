#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <signal.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdbool.h>

#define PORT 9000
#define DATA_FILE "/var/tmp/aesdsocketdata"

static int server_socket = 0;
static int client_socket = 0;
static bool daemon_mode = false;

void sigint_handler(int signo) {
    if (signo == SIGINT || signo == SIGTERM) {
        syslog(LOG_INFO, "Caught signal, exiting");
        shutdown(server_socket, SHUT_RDWR);
        remove(DATA_FILE);
        closelog();
        exit(0);
    }
}

void handle_connection() {
    struct sockaddr_in client_addr = {0};
    socklen_t client_addr_len = sizeof(client_addr);
    char buffer[1024] = {0};
    FILE *data_file = NULL;

    if (getpeername(client_socket, (struct sockaddr *)&client_addr, &client_addr_len) == 0) {
        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(client_addr.sin_addr));
    }

    while (1) {
        ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received < 0) {
            fprintf(stderr, "Connection closed or error while receiving\n");
            break;
        }
        else if (bytes_received == 0)
        {
            // Listen for incoming connections
            if (listen(server_socket, 5) == -1) {
                perror("Error listening for connections");
                close(server_socket);
                exit(-1);
            }

            struct sockaddr_in client_addr = {0};
            socklen_t client_addr_len = sizeof(client_addr);
            client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
            if (client_socket == -1) {
                perror("Error accepting connection");
                exit(-1);
            }
            continue;
        }

        data_file = fopen(DATA_FILE, "a");
        if (data_file == NULL) {
            perror("Error opening data file");
            exit(-1);
        }

        fwrite(buffer, 1, bytes_received, data_file);

        fclose(data_file);

        // Check for a newline character to determine the end of a packet
        bool newline_found = false;
        for (int i = 0; bytes_received > i; i++)
        {
            if (buffer[i] == '\n')
            {
                newline_found = true;
                break;
            }
        }
        if (newline_found) {
            // Send the content of the data file back to the client
            data_file = fopen(DATA_FILE, "r");
            if (data_file == NULL) {
                perror("Error opening data file");
                exit(-1);
            }
            char *line = NULL;
            size_t length = 0;
            ssize_t bytes_read = 0;
            while ((bytes_read = getline(&line, &length, data_file)) > 0) {
                if (send(client_socket, line, bytes_read, 0) == -1) {
                    perror("Error sending data");
                    break;
                }
            }
            free(line);
            fclose(data_file);
        }
    }

    syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(client_addr.sin_addr));
    close(client_socket);
}

int main(int argc, char *argv[]) {
    openlog("aesd_socket_server", LOG_PID | LOG_NDELAY | LOG_NOWAIT, LOG_LOCAL1);

    // Parse input arguments
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        daemon_mode = true;
        printf("Daemon mode!\n");
    }

    // Create a socket
    server_socket = socket(PF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Error creating socket");
        return 1;
    }

    // Set up the server address struct
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = PF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket to the specified port
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error binding socket");
        close(server_socket);
        return 1;
    }

    // Listen for incoming connections
    if (listen(server_socket, 5) == -1) {
        perror("Error listening for connections");
        close(server_socket);
        return 1;
    }

    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    if (daemon_mode)
    {
        // Handle the connection in a separate thread or process
        if (fork() == 0) {
            struct sockaddr_in client_addr = {0};
            socklen_t client_addr_len = sizeof(client_addr);
            client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
            if (client_socket == -1) {
                perror("Error accepting connection");
                exit(-1);
            }
            handle_connection();
            close(server_socket);
            exit(0);
        } else {
            exit(0);
        }
    }
    else
    {
        // Handle the connection in main thread or process
        struct sockaddr_in client_addr = {0};
        socklen_t client_addr_len = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket == -1) {
            perror("Error accepting connection");
            exit(-1);
        }
        handle_connection();
        close(server_socket);
    }

    return 0;
}
