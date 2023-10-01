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
#include <pthread.h>


#define PORT 9000
#ifdef USE_AESD_CHAR_DEVICE
#define DATA_FILE "/dev/aesdchar"
#else
#define DATA_FILE "/var/tmp/aesdsocketdata"
#endif /* USE_AESD_CHAR_DEVICE */
#define MAX_CLIENTS 500
#define TIMESTAMP_INTERVAL 10

static int server_socket = 0;
static bool daemon_mode = false;
static pthread_t* thread_list[MAX_CLIENTS] = {NULL};
static pthread_mutex_t mutex;

typedef struct {
    pthread_t thread;
    int client_socket;
} thread_info_t;

void sigint_handler(int signo) {
    if (signo == SIGINT || signo == SIGTERM) {
        syslog(LOG_INFO, "Caught signal, exiting");

        int i;
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (thread_list[i] != NULL) {
                pthread_cancel(*thread_list[i]);
                pthread_join(*thread_list[i], NULL);
            }
        }

        pthread_mutex_destroy(&mutex);

        shutdown(server_socket, SHUT_RDWR);
        #ifndef USE_AESD_CHAR_DEVICE
        remove(DATA_FILE);
        #endif /* USE_AESD_CHAR_DEVICE */
        closelog();
        exit(0);
    }
}

void *handle_connection(void *arg) {
    struct sockaddr_in client_addr = {0};
    socklen_t client_addr_len = sizeof(client_addr);
    char buffer[1024] = {0};
    FILE *data_file = NULL;
    thread_info_t *thread_info = (thread_info_t *)arg;

    if (getpeername(thread_info->client_socket, (struct sockaddr *)&client_addr, &client_addr_len) == 0) {
        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(client_addr.sin_addr));
    }

    while (1) {
        ssize_t bytes_received = recv(thread_info->client_socket, buffer, sizeof(buffer), 0);
        if (bytes_received < 0) {
            fprintf(stderr, "Connection closed or error while receiving\n");
            break;
        }
        else if (bytes_received == 0)
        {
            // Listen for incoming connections
            if (listen(server_socket, MAX_CLIENTS) == -1) {
                perror("Error listening for connections");
                close(server_socket);
                free(thread_info);
                pthread_exit(NULL);
                exit(-1);
            }

            struct sockaddr_in client_addr = {0};
            socklen_t client_addr_len = sizeof(client_addr);
            thread_info->client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
            if (thread_info->client_socket == -1) {
                perror("Error accepting connection");
                free(thread_info);
                pthread_exit(NULL);
                exit(-1);
            }
            continue;
        }

        pthread_mutex_lock(&mutex);
        data_file = fopen(DATA_FILE, "a");
        if (data_file == NULL) {
            perror("Error opening data file");
            free(thread_info);
            pthread_mutex_unlock(&mutex);
            pthread_exit(NULL);
            exit(-1);
        }

        fwrite(buffer, 1, bytes_received, data_file);

        fclose(data_file);
        pthread_mutex_unlock(&mutex);

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
            pthread_mutex_lock(&mutex);
            data_file = fopen(DATA_FILE, "r");
            if (data_file == NULL) {
                perror("Error opening data file");
                free(thread_info);
                pthread_exit(NULL);
                exit(-1);
            }
            char *line = NULL;
            size_t length = 0;
            ssize_t bytes_read = 0;
            while ((bytes_read = getline(&line, &length, data_file)) > 0) {
                if (send(thread_info->client_socket, line, bytes_read, 0) == -1) {
                    perror("Error sending data");
                    break;
                }
            }
            free(line);
            fclose(data_file);
            pthread_mutex_unlock(&mutex);
        }
    }

    syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(client_addr.sin_addr));
    close(thread_info->client_socket);
    free(thread_info);
    pthread_exit(NULL);
}

void *add_timestamps(void *arg) {
    while (1) {
        // Avoid compilation warning
        if (arg == NULL)
        {
            // Get current hour
            time_t current_time;
            struct tm *time_info;
            char timestamp_str[64];
            
            time(&current_time);
            time_info = localtime(&current_time);

            strftime(timestamp_str, sizeof(timestamp_str), "timestamp:%a, %d %b %Y %H:%M:%S %z", time_info);
            
            // Abre el archivo y escribe el timestamp
            pthread_mutex_lock(&mutex);
            FILE *data_file = fopen(DATA_FILE, "a");
            if (data_file != NULL) {
                fprintf(data_file, "%s\n", timestamp_str);
                fclose(data_file);
            }
            pthread_mutex_unlock(&mutex);
            
            sleep(TIMESTAMP_INTERVAL);
        }
    }
}

static int handle_thread(void)
{
    #ifndef USE_AESD_CHAR_DEVICE
    // Handle timestamp
    pthread_t timestamp_thread;
    if (pthread_create(&timestamp_thread, NULL, add_timestamps, NULL) != 0) {
        perror("Error creating timestamp thread");
        exit(-1);
    }
    #endif /* USE_AESD_CHAR_DEVICE */

    while (1) {
        // Listen for incoming connections
        if (listen(server_socket, MAX_CLIENTS) == -1) {
            perror("Error listening for connections");
            close(server_socket);
            exit(-1);
        }

        thread_info_t *thread_info = (thread_info_t *)malloc(sizeof(thread_info_t));

        struct sockaddr_in client_addr = {0};
        socklen_t client_addr_len = sizeof(client_addr);
        thread_info->client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (thread_info->client_socket == -1) {
            perror("Error accepting connection");
            free(thread_info);
            exit(-1);
        }

        if (pthread_create(&thread_info->thread, NULL, handle_connection, thread_info) != 0) {
            perror("Error creating thread");
            free(thread_info);
            close(thread_info->client_socket);
            exit(-1);
        } else {
            int i;
            for (i = 0; i < MAX_CLIENTS; i++) {
                if (thread_list[i] == NULL) {
                    thread_list[i] = &thread_info->thread;
                    break;
                }
            }
        }

    }
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
        return -1;
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
        return -1;
    }

    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    pthread_mutex_init(&mutex, NULL);

    if (daemon_mode)
    {
        // Handle the connection in a separate thread or process
        if (fork() == 0) {
            int ret = handle_thread();
            close(server_socket);
            exit(ret);
        } else {
            exit(0);
        }
    }
    else
    {
        // Handle the connection in main thread or process
        int ret = handle_thread();
        close(server_socket);
        exit(ret);
    }
}
