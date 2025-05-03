#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>
#include <arpa/inet.h>

#define SERVER_PORT 8080
#define MAX_CONNECTIONS 10
#define BUFFER_SIZE 1024

void log_event(const char *ip, const char *event) {
    FILE *log_file = fopen("game_server.log", "a");
    if (!log_file) return;

    time_t current_time = time(NULL);
    struct tm *time_info = localtime(&current_time);

    fprintf(log_file, "[%04d-%02d-%02d %02d:%02d:%02d] [%s] %s\n",
        time_info->tm_year + 1900, time_info->tm_mon + 1, time_info->tm_mday,
        time_info->tm_hour, time_info->tm_min, time_info->tm_sec, ip, event);

    fclose(log_file);
}

void *handle_client(void *client_socket_ptr) {
    int client_socket = *(int *)client_socket_ptr;
    free(client_socket_ptr);

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    getpeername(client_socket, (struct sockaddr *)&client_addr, &addr_len);
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));

    char join_message[128];
    snprintf(join_message, sizeof(join_message), "New adventurer entered the realm.");
    log_event(client_ip, join_message);
    printf("[SERVER LOG] %s\n", join_message);

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        bytes_read = read(client_socket, buffer, BUFFER_SIZE);
        if (bytes_read <= 0) break;

        if (strncmp(buffer, "FIGHT", 5) == 0) {
            srand(time(NULL) ^ pthread_self());
            int enemy_hp = rand() % 151 + 50;
            int reward = rand() % 101 + 50;

            char response[BUFFER_SIZE];
            snprintf(response, sizeof(response), "OPPONENT %d %d\n", enemy_hp, reward);
            send(client_socket, response, strlen(response), 0);

            char log_entry[128];
            snprintf(log_entry, sizeof(log_entry), "Battle initiated -> Foe HP %d, Reward %d Gold", enemy_hp, reward);
            log_event(client_ip, log_entry);
        } else {
            const char *error_msg = "ERROR: Invalid request.\n";
            send(client_socket, error_msg, strlen(error_msg), 0);
        }
    }

    close(client_socket);
    return NULL;
}

int main() {
    int server_socket, new_connection;
    struct sockaddr_in server_addr;
    socklen_t addr_length = sizeof(server_addr);
    pthread_t thread_id;

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Address binding failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, MAX_CONNECTIONS) < 0) {
        perror("Listening failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("[GAME SERVER] Realm active on port %d...\n", SERVER_PORT);

    while (1) {
        new_connection = accept(server_socket, (struct sockaddr *)&server_addr, &addr_length);
        if (new_connection < 0) {
            perror("Connection acceptance failed");
            continue;
        }

        int *client_socket = malloc(sizeof(int));
        *client_socket = new_connection;

        if (pthread_create(&thread_id, NULL, handle_client, client_socket) != 0) {
            perror("Thread creation failed");
            close(new_connection);
            free(client_socket);
        } else {
            pthread_detach(thread_id);
        }
    }

    close(server_socket);
    return 0;
}
