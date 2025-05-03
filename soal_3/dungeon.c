#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>
#include <arpa/inet.h>

#define PORT 9000
#define MAKS_CLIENT 10
#define UKURAN_BUFFER 1024

void catat_log(const char *ip, const char *pesan) {
    FILE *f = fopen("dungeon.log", "a");
    if (!f) return;

    time_t sekarang = time(NULL);
    struct tm *t = localtime(&sekarang);

    fprintf(f, "[%04d-%02d-%02d %02d:%02d:%02d] [%s] %s\n",
        t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
        t->tm_hour, t->tm_min, t->tm_sec, ip, pesan);

    fclose(f);
}

void *layani_client(void *arg) {
    int sock = *(int *)arg;
    free(arg);

    struct sockaddr_in alamat_client;
    socklen_t len = sizeof(alamat_client);
    getpeername(sock, (struct sockaddr *)&alamat_client, &len);
    char ip_client[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &alamat_client.sin_addr, ip_client, sizeof(ip_client));

    char pesan_masuk[128];
    snprintf(pesan_masuk, sizeof(pesan_masuk), "Pemain bergabung ke dungeon.");
    catat_log(ip_client, pesan_masuk);
    printf("[INFO] %s\n", pesan_masuk);

    char buffer[UKURAN_BUFFER];
    ssize_t baca;

    while (1) {
        memset(buffer, 0, UKURAN_BUFFER);
        baca = read(sock, buffer, UKURAN_BUFFER);
        if (baca <= 0) break;

        if (strncmp(buffer, "BATTLE", 6) == 0) {
            srand(time(NULL) ^ pthread_self());
            int hp_musuh = rand() % 151 + 50;
            int emas = rand() % 101 + 50;

            char kirim[UKURAN_BUFFER];
            snprintf(kirim, sizeof(kirim), "ENEMY %d %d\n", hp_musuh, emas);
            send(sock, kirim, strlen(kirim), 0);

            char catatan[128];
            snprintf(catatan, sizeof(catatan), "Aksi: BATTLE -> Musuh HP %d, Hadiah %d Gold", hp_musuh, emas);
            catat_log(ip_client, catatan);
        } else {
            const char *pesan = "ERROR: Permintaan tidak dikenali.\n";
            send(sock, pesan, strlen(pesan), 0);
        }
    }

    close(sock);
    return NULL;
}

int main() {
    int server_fd, koneksi_baru;
    struct sockaddr_in alamat;
    socklen_t panjang_alamat = sizeof(alamat);
    pthread_t thread_id;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Gagal membuat soket");
        exit(EXIT_FAILURE);
    }

    alamat.sin_family = AF_INET;
    alamat.sin_addr.s_addr = INADDR_ANY;
    alamat.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&alamat, sizeof(alamat)) < 0) {
        perror("Gagal bind alamat");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAKS_CLIENT) < 0) {
        perror("Gagal listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("[SERVER] Dungeon aktif di port %d...\n", PORT);

    while (1) {
        koneksi_baru = accept(server_fd, (struct sockaddr *)&alamat, &panjang_alamat);
        if (koneksi_baru < 0) {
            perror("Gagal menerima koneksi");
            continue;
        }

        int *sock_client = malloc(sizeof(int));
        *sock_client = koneksi_baru;

        if (pthread_create(&thread_id, NULL, layani_client, sock_client) != 0) {
            perror("Gagal membuat thread");
            close(koneksi_baru);
            free(sock_client);
        } else {
            pthread_detach(thread_id);
        }
    }

    close(server_fd);
    return 0;
}
