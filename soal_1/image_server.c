// image_server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>

#define SOCKET_PATH "/tmp/image_rpc_socket"
#define LOG_FILE "server/server.log"
#define DATABASE_DIR "server/database/"
#define BUFFER_SIZE 8192

void write_log(const char *source, const char *action, const char *info) {
    FILE *log = fopen(LOG_FILE, "a");
    if (!log) return;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);
    fprintf(log, "[%s][%s]: [%s] [%s]\n", source, timestamp, action, info);
    fclose(log);
}

char *encode_file_to_hex(const char *filepath) {
    FILE *file = fopen(filepath, "rb");
    if (!file) return NULL;

    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    rewind(file);

    unsigned char *buffer = malloc(filesize);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    fread(buffer, 1, filesize, file);
    fclose(file);

    // Setiap byte akan jadi 2 karakter hex + null terminator
    char *hex_string = malloc(filesize * 2 + 1);
    if (!hex_string) {
        free(buffer);
        return NULL;
    }

    for (long i = 0; i < filesize; i++) {
        sprintf(hex_string + i * 2, "%02x", buffer[i]);
    }
    hex_string[filesize * 2] = '\0';

    free(buffer);
    return hex_string;
}

unsigned char *reverse_and_decode_hex(const char *hex_data, size_t *out_size) {
    int length = strlen(hex_data);
    if (length % 2 != 0) return NULL;  // harus genap

    // Balik string hex
    char *reversedStr = malloc(length + 1);
    if (!reversedStr) return NULL;

    strcpy(reversedStr, hex_data);
    for (int i = 0, j = length - 1; i < j; i++, j--) {
        char temp = reversedStr[i];
        reversedStr[i] = reversedStr[j];
        reversedStr[j] = temp;
    }

    // Konversi ke byte array
    size_t byteArraySize = length / 2;
    unsigned char *byteArray = malloc(byteArraySize);
    if (!byteArray) {
        free(reversedStr);
        return NULL;
    }

    for (size_t i = 0; i < byteArraySize; i++) {
        sscanf(reversedStr + 2 * i, "%2hhx", &byteArray[i]);
    }

    free(reversedStr);
    *out_size = byteArraySize;
    return byteArray;  // jangan lupa free di pemanggil
}

void handle_client(int client_sock) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    int len = recv(client_sock, buffer, sizeof(buffer), 0);

    if (len <= 0) return;

    char *command = strtok(buffer, "|");
    if (!command) return;

    if (strcmp(command, "DECRYPT") == 0) {
        char *data = strtok(NULL, "|");
        if (!data) {
            send(client_sock, "ERR:Invalid data", 21, 0);
            return;
        }

        /*char filepath[256];
        snprintf(filepath, sizeof(filepath), "client/secrets/%s", filename);

        FILE *f = fopen(filepath, "r");
        if (!f) {
            send(client_sock, "ERR:File not found", 19, 0);
            write_log("Client", "DECRYPT", "Text data (file not found)");
            return;
        }*/

        //char text[BUFFER_SIZE] = {0};
        //fread(text, 1, sizeof(text), data);

        write_log("Client", "DECRYPT", "Text data");

        //send(client_sock, data, strlen(data), 0);
        //return;

	size_t size;
        unsigned char *jpeg_data = reverse_and_decode_hex(data, &size);
        if (!jpeg_data) {
            send(client_sock, "ERR:Decrypt failed", 20, 0);
            return;
        }

        time_t now = time(NULL);
        char out_path[256];
        snprintf(out_path, sizeof(out_path), DATABASE_DIR "%ld.jpeg", now);

        FILE *out = fopen(out_path, "wb");
        fwrite(jpeg_data, 1, size, out);
        fclose(out);
        free(jpeg_data);

        char msg[128];
        snprintf(msg, sizeof(msg), "%ld.jpeg", now);
        send(client_sock, msg, strlen(msg), 0);
        write_log("Server", "SAVE", msg);

    } else if (strcmp(command, "DOWNLOAD") == 0) {
        char *filename = strtok(NULL, "|");
        if (!filename) {
            send(client_sock, "ERR:Invalid request", 21, 0);
            return;
        }

        char filepath[256];
        snprintf(filepath, sizeof(filepath), DATABASE_DIR "%s", filename);
       /* FILE *f = fopen(filepath, "rb");
        if (!f) {
            send(client_sock, "ERR:File not found", 19, 0);
            return;
        }*/

        char *hex_data = encode_file_to_hex(filepath);
	if (!hex_data) {
            send(client_sock, "ERR:File not found", 19, 0);
            return;
    	}
        /*char filedata[BUFFER_SIZE] = {0};
        size_t r = fread(filedata, 1, sizeof(filedata), f);
        fclose(f);*/
        send(client_sock, hex_data, strlen(hex_data), 0);

        write_log("Client", "DOWNLOAD", filename);
        write_log("Server", "UPLOAD", filename);
    } else if (strcmp(command, "EXIT") == 0) {
        write_log("Client", "EXIT", "Client requested to exit");
    }
}

void daemonize() {
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    umask(0);
    setsid();

    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
}

int main() {
    daemonize();
    unlink(SOCKET_PATH);

    int server_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);

    bind(server_sock, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_sock, 5);

    mkdir("server/database", 0755);

    while (1) {
        int client_sock = accept(server_sock, NULL, NULL);
        if (client_sock >= 0) {
            handle_client(client_sock);
            close(client_sock);
        }
    }

    return 0;
}
