// image_client.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>

#define SOCKET_PATH "/tmp/image_rpc_socket"
#define BUFFER_SIZE 8192

// Menyimpan string hex ke file sebagai byte
int save_hex_to_file(const char* hex, const char* output_filename) {
    FILE* out = fopen(output_filename, "wb");
    if (!out) return -1;

    for (size_t i = 0; hex[i] && hex[i+1]; i += 2) {
        if (!isxdigit(hex[i]) || !isxdigit(hex[i+1])) continue;

        unsigned int byte;
        sscanf(hex + i, "%2x", &byte);
        fputc(byte, out);
    }

    fclose(out);
    return 0;
}

void menu() {
    printf("\n===== Image RPC Client =====\n");
    printf("1. Kirim file teks terenkripsi untuk didekripsi menjadi JPEG\n");
    printf("2. Minta file JPEG dari server\n");
    printf("3. Keluar\n");
    printf("Pilih: ");
}

int connect_socket() {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock == -1) return -1;

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        close(sock);
        return -1;
    }

    return sock;
}

void send_decrypt() {
    char filename[128];
    printf("Masukkan nama file teks terenkripsi (misal: input_1.txt): ");
    scanf("%s", filename);

    int sock = connect_socket();
    if (sock == -1) {
        printf("[ERROR] Gagal connect ke server\n");
        return;
    }

    char filepath[256];
    snprintf(filepath, sizeof(filepath), "client/secrets/%s", filename);

    FILE *f = fopen(filepath, "r");
    if (!f) {
    	printf("File not found");
    	return;
    }

        char text[BUFFER_SIZE] = {0};
        fread(text, 1, sizeof(text), f);
        fclose(f);

    char request[BUFFER_SIZE];
    snprintf(request, sizeof(request), "DECRYPT|%s", text);
    send(sock, request, strlen(request), 0);

    char response[BUFFER_SIZE] = {0};
    int r = recv(sock, response, sizeof(response), 0);
    response[r] = '\0';

    if (strncmp(response, "ERR", 3) == 0) {
        printf("[Server Error] %s\n", response);
    } else {
        char path[256];
        snprintf(path, sizeof(path), "client/%s", response);
        char fullfile[BUFFER_SIZE];

        snprintf(fullfile, sizeof(fullfile), "server/database/%s", response);
        int fsrc = open(fullfile, O_RDONLY);
        int fdest = open(path, O_CREAT | O_WRONLY, 0644);
        char buffer[BUFFER_SIZE];
        int bytes;
        while ((bytes = read(fsrc, buffer, BUFFER_SIZE)) > 0) {
            write(fdest, buffer, bytes);
        }
        close(fsrc);
        close(fdest);

        printf("[INFO] File JPEG diterima: %s\n", response);
    }
    close(sock);
}

void send_download() {
    char filename[128];
    printf("Masukkan nama file JPEG (misal: 1744401282.jpeg): ");
    scanf("%s", filename);

    int sock = connect_socket();
    if (sock == -1) {
        printf("[ERROR] Gagal connect ke server\n");
        return;
    }

    char request[256];
    snprintf(request, sizeof(request), "DOWNLOAD|%s", filename);
    send(sock, request, strlen(request), 0);

    char response[BUFFER_SIZE] = {0};
    int r = recv(sock, response, sizeof(response), 0);

    if (strncmp(response, "ERR", 3) == 0) {
        response[r] = '\0';
        printf("[Server Error] %s\n", response);
    } else {
        char out_path[256];
        snprintf(out_path, sizeof(out_path), "client/%s", filename);
        save_hex_to_file(response, out_path);
        printf("[INFO] File JPEG diterima dan disimpan: %s\n", out_path);
    }
    close(sock);
}

void send_exit() {
    int sock = connect_socket();
    if (sock == -1) return;
    char *exit_msg = "EXIT";
    send(sock, exit_msg, strlen(exit_msg), 0);
    close(sock);
}

int main() {
    while (1) {
        menu();
        int pilihan;
        scanf("%d", &pilihan);
        getchar();

        switch (pilihan) {
            case 1:
                send_decrypt();
                break;
            case 2:
                send_download();
                break;
            case 3:
                send_exit();
                printf("Keluar dari program.\n");
                return 0;
            default:
                printf("Pilihan tidak valid.\n");
        }
    }
}
