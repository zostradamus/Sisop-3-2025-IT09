# LAPORAN RESMI SOAL MODUL 3 SISOP

## ANGGOTA KELOMPOK
| Nama                           | NRP        |
| -------------------------------| ---------- |
| Shinta Alya Ramadani           | 5027241016 |
| Prabaswara Febrian Winandika   | 5027241069 |
| Muhammad Farrel Rafli Al Fasya | 5027241075 |

### Soal 1
## Deskripsi 
Tugas ini meminta pembuatan dua program, yaitu image_server.c dan image_client.c, yang saling terhubung melalui komunikasi Remote Procedure Call (RPC) menggunakan socket. Tujuannya adalah untuk mengelola proses dekripsi file teks rahasia menjadi gambar JPEG serta memfasilitasi pengambilan (download) gambar tersebut oleh client tanpa menggunakan metode penyalinan atau pemindahan file secara langsung.
```
.
├── client
│   ├── 1744403652.jpeg
│   ├── 1744403687.jpeg
│   ├── image_client
│   └── secrets
│       ├── input_1.txt
│       ├── input_2.txt
│       ├── input_3.txt
│       ├── input_4.txt
│       └── input_5.txt
├── image_client.c
├── image_server.c
└── server
    ├── database
    │   ├── 1744403652.jpeg
    │   └── 1744403687.jpeg
    ├── image_server
    └── server.log

```
## Penjelasan Program

### SOAL 2
## Deskripsi 
Delivery Management
---
Terdapat dua program :
  - Delivery Agent : meng-handle pengiriman paket kategori Express secara otomatis dan paralel menggunakan multithreading.
  - Dispatcher : berinteraksi secara manual untuk menangani paket kategori Reguler dengan pilihan :
      - Menampilkan semua order
      - Mengecek status order tertentu
      - Mengantar order tertentu

Semua program harus menggunakan shared memory untuk berbagi data antar proses.

``` Struktur Data
typedef struct {
    char customer[64];
    char destination[128];
    char category[16];
    int is_delivered;
    char handled_by[64];
} Package;
```
Semua order disimpan dalam array Package berukuran ORDER_LIMIT (100).
## Penjelasan Program
1. delivery_agent.c
   - Membuka/membuat shared memory (shmget) untuk menyimpan data order.
   - Membaca file CSV (delivery_order.csv) dan memuat semua order ke shared memory.
   - Membuat 3 thread (AGENT A, AGENT B, AGENT C) untuk mengantar paket.

Tiap thread :
   - Mencari order kategori Express yang belum diantar (is_delivered == 0).
   - Menandai order sebagai delivered, menyimpan nama agent di handled_by.
   - Mencatat log ke file delivery.log.
   - Mutex (pthread_mutex_t) digunakan untuk sinkronisasi akses shared memory.
Contoh Log :
```
[08/05/2025 14:23:45] [AGENT A] Express package delivered to John Doe in New York
```
2. dispatcher.c
   - Mengakses shared memory (shmget dan shmat) yang telah dibuat oleh delivery_agent.
Berdasarkan argumen:
   - list : menampilkan semua order dan statusnya (Pending/Delivered).
   - status [customer]: mengecek status pengiriman berdasarkan nama customer.
   - deliver [customer]: mengantar order kategori Reguler secara manual, lalu mencatat ke delivery.log.

Nama pengantar diambil dari nama user Linux (getpwuid(getuid())).
Contoh Log :
```
[08/05/2025 14:25:10] [student] Reguler package delivered to Alice Smith in Los Angeles
```
## Penjelasan Code
#### delivery_agent.c
```
// Include library yang diperlukan
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/shm.h>

// Konstanta shared memory key dan batas order
#define SHM_KEY 1234
#define ORDER_LIMIT 100

// Struktur data Package untuk menyimpan informasi pengiriman
typedef struct {
    char customer[64];
    char destination[128];
    char category[16];
    int is_delivered;
    char handled_by[64];
} Package;

// Global variable
Package *shared_orders;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
```
Fungsi untuk mencatat log pengiriman paket Express ⬇⬇⬇
```
void log_express(const char *agent, const char *customer, const char *destination) {
    FILE *fp = fopen("delivery.log", "a");
    if (!fp) return;

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    fprintf(fp, "[%02d/%02d/%04d %02d:%02d:%02d] [%s] Express package delivered to %s in %s\n",
        tm_info->tm_mday, tm_info->tm_mon + 1, tm_info->tm_year + 1900,
        tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
        agent, customer, destination);

    fclose(fp);
}
```
Fungsi untuk thread agen yang mencari dan mengirim paket kategori Express ⬇⬇⬇
```
void *delivery_worker(void *arg) {
    char *agent_name = (char *) arg;

    while (1) {
        pthread_mutex_lock(&mutex);
        for (int i = 0; i < ORDER_LIMIT; i++) {
            if (strcmp(shared_orders[i].category, "Express") == 0 && shared_orders[i].is_delivered == 0) {
                shared_orders[i].is_delivered = 1;
                snprintf(shared_orders[i].handled_by, sizeof(shared_orders[i].handled_by), "%s", agent_name);
                log_express(agent_name, shared_orders[i].customer, shared_orders[i].destination);
                pthread_mutex_unlock(&mutex);
                sleep(1);
                goto loop_end;
            }
        }
        pthread_mutex_unlock(&mutex);
    loop_end:
        sleep(1);
    }
    return NULL;
}
```
#### dispatcher.c
```
// Include library yang diperlukan
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pwd.h>
#include <sys/ipc.h>
#include <sys/shm.h>

// Konstanta shared memory key dan batas order
#define SHM_KEY 1234
#define ORDER_LIMIT 100

// Struktur data Package untuk menyimpan informasi pengiriman
typedef struct {
    char customer[64];
    char destination[128];
    char category[16];
    int is_delivered;
    char handled_by[64];
} Package;

// Global variable
Package *shared_orders;
```
Fungsi untuk membaca order dari file CSV ke shared memory ⬇⬇⬇
```
void load_orders_from_csv(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening CSV file");
        exit(EXIT_FAILURE);
    }

    char buffer[256];
    int idx = 0;

    while (fgets(buffer, sizeof(buffer), file) && idx < ORDER_LIMIT) {
        char *token = strtok(buffer, ",");
        if (token) strncpy(shared_orders[idx].customer, token, sizeof(shared_orders[idx].customer));

        token = strtok(NULL, ",");
        if (token) strncpy(shared_orders[idx].destination, token, sizeof(shared_orders[idx].destination));

        token = strtok(NULL, ",\n");
        if (token) strncpy(shared_orders[idx].category, token, sizeof(shared_orders[idx].category));

        shared_orders[idx].is_delivered = 0;
        strcpy(shared_orders[idx].handled_by, "-");
        idx++;
    }

    fclose(file);
}
```
Fungsi utama program ⬇⬇⬇
```
int main() {
    int shmid = shmget(SHM_KEY, sizeof(Package) * ORDER_LIMIT, IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("Failed to create shared memory");
        return EXIT_FAILURE;
    }

    shared_orders = (Package *) shmat(shmid, NULL, 0);
    if (shared_orders == (void *) -1) {
        perror("Failed to attach shared memory");
        return EXIT_FAILURE;
    }

    load_orders_from_csv("delivery_order.csv");

    pthread_t agentA, agentB, agentC;
    pthread_create(&agentA, NULL, delivery_worker, "AGENT A");
    pthread_create(&agentB, NULL, delivery_worker, "AGENT B");
    pthread_create(&agentC, NULL, delivery_worker, "AGENT C");

    pthread_join(agentA, NULL);
    pthread_join(agentB, NULL);
    pthread_join(agentC, NULL);

    shmdt(shared_orders);
    return 0;
}
```
Fungsi untuk mencatat log pengiriman paket Reguler ⬇⬇⬇
```
void log_reguler(const char *agent, const char *customer, const char *destination) {
    FILE *fp = fopen("delivery.log", "a");
    if (!fp) return;

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    fprintf(fp, "[%02d/%02d/%04d %02d:%02d:%02d] [%s] Reguler package delivered to %s in %s\n",
        tm_info->tm_mday, tm_info->tm_mon + 1, tm_info->tm_year + 1900,
        tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
        agent, customer, destination);

    fclose(fp);
}
```
Fungsi untuk menampilkan semua order dan status pengiriman ⬇⬇⬇
```
void show_all_orders() {
    printf("List of orders:\n");
    for (int i = 0; i < ORDER_LIMIT; i++) {
        if (strlen(shared_orders[i].customer) > 0) {
            printf("%s - %s\n", shared_orders[i].customer, shared_orders[i].is_delivered ? "Delivered" : "Pending");
        }
    }
}
```
Fungsi untuk mengecek status order tertentu ⬇⬇⬇
```
void check_status(const char *target) {
    for (int i = 0; i < ORDER_LIMIT; i++) {
        if (strcmp(shared_orders[i].customer, target) == 0) {
            if (shared_orders[i].is_delivered)
                printf("Status for %s: Delivered by %s\n", target, shared_orders[i].handled_by);
            else
                printf("Status for %s: Pending\n", target);
            return;
        }
    }
    printf("Order for %s not found.\n", target);
}
```
Fungsi untuk mengantar order Reguler secara manual ⬇⬇⬇
```
void deliver_order(const char *target) {
    for (int i = 0; i < ORDER_LIMIT; i++) {
        if (strcmp(shared_orders[i].customer, target) == 0 &&
            shared_orders[i].is_delivered == 0 &&
            strcmp(shared_orders[i].category, "Reguler") == 0) {

            shared_orders[i].is_delivered = 1;
            struct passwd *pw = getpwuid(getuid());
            if (pw) {
                snprintf(shared_orders[i].handled_by, sizeof(shared_orders[i].handled_by), "%s", pw->pw_name);
                log_reguler(shared_orders[i].handled_by, shared_orders[i].customer, shared_orders[i].destination);
                printf("Successfully delivered order for %s.\n", target);
            }
            return;
        }
    }
    printf("Cannot deliver order for %s. Maybe already delivered or not Reguler.\n", target);
}
```
Fungsi utama program ⬇⬇⬇
```
int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s [-list] | [-status name] | [-deliver name]\n", argv[0]);
        return EXIT_FAILURE;
    }

    int shmid = shmget(SHM_KEY, sizeof(Package) * ORDER_LIMIT, 0666);
    if (shmid == -1) {
        perror("Failed to get shared memory");
        return EXIT_FAILURE;
    }

    shared_orders = (Package *) shmat(shmid, NULL, 0);
    if (shared_orders == (void *) -1) {
        perror("Failed to attach shared memory");
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "-list") == 0) {
        show_all_orders();
    } else if (strcmp(argv[1], "-status") == 0 && argc == 3) {
        check_status(argv[2]);
    } else if (strcmp(argv[1], "-deliver") == 0 && argc == 3) {
        deliver_order(argv[2]);
    } else {
        printf("Invalid command.\n");
    }

    shmdt(shared_orders);
    return 0;
}
```
### Build & Run
```
gcc -o delivery_agent delivery_agent.c -lpthread
gcc -o dispatcher dispatcher.c
```
### Output

### SOAL  3
## Deskripsi
Program RPG Sederhana
---
Program game sederhana :
- player.c = Client tempat kamu main game.
  - Kamu bisa lihat status, beli senjata, equip senjata, masuk arena bertarung.
- shop.c = Data toko senjata
  - Menyediakan list senjata unik dengan harga, damage, dan efek pasif.
- dungeon.c = Server dungeon.
  - Tempat player bertarung.
  - Saat player masuk combat, server akan mengirimkan musuh (dengan HP dan reward coin random).

## Penjelasan Program
1. dungeon.c
- Membuka server TCP socket di port 8080:
  - socket()
  - bind()
  - listen()
  - accept() koneksi baru dari player.
- Membuat thread untuk setiap koneksi client (pthread_create):
  - Setiap player yang masuk dibuat thread khusus.
- Setiap client yang terhubung:
  - Dicatat di file game_server.log:
    - Waktu masuk
    - IP client
    - Event yang terjadi (battle initiated).
- Saat client mengirim "FIGHT":
  - Server:
    - Random musuh (HP 50-200).
    - Random reward (coin 50-150).
    - Kirim data kembali ke client dalam format:
      ```
      OPPONENT <hp> <reward>
      ```
    - Jika pesan selain "FIGHT", server balas error.
- Logging ke file:
  - Semua battle dan event player disimpan ke file game_server.log.

2. player.c
- Membuat karakter player (Adventurer) dengan:
  - Nama
  - Coin awal 300
  - Inventory kosong
  - Belum ada senjata terpakai
- Menampilkan menu utama:
  - View Status
  - Check Inventory
  - Visit Shop
  - Enter Combat
  - Exit Game
- Fungsi-fungsi utama:
  - show_status(): Menampilkan informasi nama, coin, senjata, damage, passive, musuh dikalahkan.
  - view_inventory():
    - Menampilkan senjata yang sudah dimiliki.
    - Memilih senjata untuk di-equip.
  - visit_shop():
    - Menampilkan daftar senjata dari shop.c.
    - Membeli senjata menggunakan coin player.
  - start_combat():
    - Membuka socket ke dungeon.c server.
    - Mengirim pesan "FIGHT".
    - Menerima informasi musuh (HP dan Reward).
    - Bertarung:
      - attack memberikan damage + chance critical 20%.
      - Bisa exit untuk mundur.
      - Menambah coin setelah mengalahkan musuh.
  - HP Bar musuh ditampilkan berwarna:
    - Hijau (>60%)
    - Kuning (30-60%)
    - Merah (<30%)

3. shop.c
- Menyediakan daftar senjata (armory) sebanyak 10 buah senjata, lengkap dengan:
  - Nama
  - Harga
  - Damage
  - Efek pasif
- Membuat fungsi display_shop():
  - Menampilkan semua senjata beserta atributnya ke terminal.
  - Senjata ditampilkan dalam format tabel rapi.
- Membuat fungsi get_weapon(int index):
  - Mengambil satu senjata berdasarkan index.
  - Jika index salah (out of bounds), mengembalikan senjata kosong.

## Penjelasan Code
#### dungeon.c
```
// Import library untuk socket programming dan thread
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <time.h>
#include <arpa/inet.h>

// Definisi konstanta
#define SERVER_PORT 8080
#define MAX_CONNECTIONS 10
#define BUFFER_SIZE 1024
```
Fungsi untuk mencatat event ke dalam file log ⬇⬇⬇
```
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
```
Fungsi untuk menangani komunikasi dengan 1 client (player) ⬇⬇⬇
```
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
        if (bytes_read <= 0) break; // Client disconnect

        if (strncmp(buffer, "FIGHT", 5) == 0) {
            srand(time(NULL) ^ pthread_self());
            int enemy_hp = rand() % 151 + 50;    // HP musuh 50-200
            int reward = rand() % 101 + 50;       // Reward 50-150 coins

            char response[BUFFER_SIZE];
            snprintf(response, sizeof(response), "OPPONENT %d %d\n", enemy_hp, reward);
            send(client_socket, response, strlen(response), 0);

            // Log pertarungan
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
```
Fungsi utama untuk memulai server dungeon ⬇⬇⬇
```
int main() {
    int server_socket, new_connection;
    struct sockaddr_in server_addr;
    socklen_t addr_length = sizeof(server_addr);
    pthread_t thread_id;

    // Membuat socket TCP
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set properti alamat server
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);

    // Binding socket ke address
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Address binding failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Mendengarkan koneksi masuk
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

        // Membuat thread untuk client baru
        if (pthread_create(&thread_id, NULL, handle_client, client_socket) != 0) {
            perror("Thread creation failed");
            close(new_connection);
            free(client_socket);
        } else {
            pthread_detach(thread_id); // Supaya otomatis clean setelah client disconnect
        }
    }

    close(server_socket);
    return 0;
}
```
#### player.c
```
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

// Warna untuk mempercantik tampilan
#define RED     "\033[38;5;196m"
#define GREEN   "\033[38;5;46m"
#define YELLOW  "\033[38;5;226m"
#define BLUE    "\033[38;5;81m"
#define PURPLE  "\033[38;5;135m"
#define RESET   "\033[0m"

// Port server dungeon
#define GAME_PORT 8080
#define BUFFER_SIZE 1024
#define WEAPON_COUNT 10

// Struct senjata
typedef struct {
    char name[30];
    int price;
    int damage;
    char passive[50];
} Weapon;

// Import dari shop.c
extern Weapon armory[WEAPON_COUNT];
extern void display_shop();
extern Weapon get_weapon(int index);

// Struct karakter player
typedef struct {
    char name[50];
    int coins;
    Weapon inventory[10];
    int item_count;
    Weapon equipped;
    int foes_defeated;
} Adventurer;

// Global player
Adventurer player = {.coins = 300, .item_count = 0, .foes_defeated = 0};

// Deklarasi fungsi
void start_combat();
void show_status();
void view_inventory();
void visit_shop();
void display_hp_bar(int current_hp, int max_hp);
```
Main Menu ⬇⬇⬇
```
int main() {
    int choice;
    printf("\n");
    printf("\033[5;36m╔══════════════════════════════════════════════════════════════════════╗\n");
    printf("\033[5;35m║                     🌟 WELCOME TO THE MYSTIC REALM 🌟                ║\n");
    printf("\033[5;33m║  ⚔️  Where legends are born, mysteries unfold, and glory awaits! ⚔️  ║\n");
    printf("\033[5;36m╚══════════════════════════════════════════════════════════════════════╝\n");
    printf(RESET);

    usleep(500000);
    printf("\n\033[38;5;110m\"Beyond this point lies a world unknown...\n");
    usleep(400000);
    printf("Prepare yourself, adventurer, for destiny calls your name.\"\033[0m\n\n");
    usleep(600000);

    printf("\033[38;5;153mEnter your adventurer's name: \033[0m");
    fgets(player.name, sizeof(player.name), stdin);
    player.name[strcspn(player.name, "\n")] = '\0';

    usleep(300000);

    while (1) {
        printf("\n%s\n", BLUE "📜 MAIN MENU" RESET);
        printf("1. View Adventurer Status\n");
        printf("2. Check Inventory\n");
        printf("3. Visit Mystic Shop\n");
        printf("4. Enter Combat Arena\n");
        printf("5. Exit\nChoose (1-5): ");
        scanf("%d", &choice);
        getchar();
        switch (choice) {
            case 1: show_status(); break;
            case 2: view_inventory(); break;
            case 3: visit_shop(); break;
            case 4: start_combat(); break;
            case 5:
                printf("\n%s\n", PURPLE "🌙 Thank you for playing Mystic Realm! Farewell adventurer~" RESET);
                exit(0);
            default: printf("Invalid selection.\n");
        }
    }
}
```
Fungsi menampilkan status player ⬇⬇⬇
```
void show_status() {
    printf("\n== Adventurer Status ==\n");
    printf("Name: %s\n", player.name);
    printf("Coins: %d\n", player.coins);
    printf("Weapon: %s\n", strlen(player.equipped.name) ? player.equipped.name : "None equipped");
    printf("Base Damage: %d\n", player.equipped.damage);
    if (strlen(player.equipped.passive) && strcmp(player.equipped.passive, "-") != 0)
        printf("Passive Ability: %s\n", player.equipped.passive);
    printf("Foes Defeated: %d\n", player.foes_defeated);
}
```
Fungsi untuk cek inventory dan equip senjata ⬇⬇⬇
```
void view_inventory() {
    int selection;
    int valid_input = 0;
    do {
        printf("\n%s\n", YELLOW "🎒 INVENTORY" RESET);
        if (player.item_count == 0) {
            printf("Your inventory is empty... visit the shop first! 🛍️\n");
            return;
        }
        for (int i = 0; i < player.item_count; i++) {
            int is_equipped = strcmp(player.inventory[i].name, player.equipped.name) == 0;
            printf("%d. %s | Damage: %d | Passive: %s%s\n", i+1,
                   player.inventory[i].name, player.inventory[i].damage,
                   player.inventory[i].passive, is_equipped ? " (EQUIPPED)" : "");
        }
        printf("Select weapon number to equip (0 to cancel): ");
        if (scanf("%d", &selection) != 1) {
            printf("Invalid input! Please enter a number between 1-%d or 0 to cancel.\n", player.item_count);
            while(getchar() != '\n');
            continue;
        }
        if (selection == 0) {
            valid_input = 1;
            break;
        }
        if (selection < 1 || selection > player.item_count) {
            printf("Invalid selection!\n");
        } else {
            valid_input = 1;
            player.equipped = player.inventory[selection-1];
            printf("%s is now equipped!\n", player.equipped.name);
        }
    } while (!valid_input);
}
```
Fungsi masuk ke toko dan beli senjata ⬇⬇⬇
```
void visit_shop() {
    int choice;
    int valid_input = 0;
    do {
        display_shop();
        printf("Select weapon number to purchase (0 to cancel): ");
        if (scanf("%d", &choice) != 1) {
            printf("Invalid input! Please enter a number.\n");
            while(getchar() != '\n');
            continue;
        }
        if (choice == 0) {
            valid_input = 1;
            break;
        }
        if (choice < 1 || choice > WEAPON_COUNT) {
            printf("Invalid selection!\n");
        } else {
            valid_input = 1;
            Weapon w = get_weapon(choice-1);
            if (player.coins >= w.price) {
                player.inventory[player.item_count++] = w;
                player.coins -= w.price;
                printf("Successfully purchased %s!\n", w.name);
            } else {
                printf("Not enough coins!\n");
            }
        }
    } while (!valid_input);
}
```
Fungsi untuk menampilkan HP musuh ⬇⬇⬇
```
void display_hp_bar(int current_hp, int max_hp) {
    const char* enemy_type = "👾";
    printf("\033[3F\033[0J"); fflush(stdout);

    float hp_percent = (float)current_hp / max_hp;
    const char* hp_color = hp_percent > 0.6 ? GREEN : (hp_percent > 0.3 ? YELLOW : RED);
    printf("\n%s╔════════════════════════════╗%s\n", hp_color, RESET);
    printf("%s║ %-26s ║%s\n", hp_color,
           current_hp == max_hp ? "ENEMY APPEARED!" : "ENEMY UNDER ATTACK!", RESET);
    printf("%s╚════════════════════════════╝%s\n", hp_color, RESET);

    printf("%s %s HP: ", hp_color, enemy_type);
    int filled = 20 * current_hp / max_hp;
    for (int i = 0; i < 20; i++) {
        printf("%s%s", hp_color, i < filled ? "■" : "□");
    }
    printf(" %d/%d%s\n\n", current_hp, max_hp, RESET);
}
```
Fungsi untuk masuk arena bertarung dan melakukan pertarungan ⬇⬇⬇
```
void start_combat() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(GAME_PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection to server failed");
        return;
    }

    srand(time(0));
    char action[10];
    while (1) {
        send(sock, "FIGHT", strlen("FIGHT"), 0);
        char buffer[BUFFER_SIZE] = {0};
        read(sock, buffer, BUFFER_SIZE);

        int foe_hp, reward;
        sscanf(buffer, "OPPONENT %d %d", &foe_hp, &reward);
        int max_hp = foe_hp;
        printf("\nFoe Appeared!\n");
        display_hp_bar(foe_hp, max_hp);

        while (foe_hp > 0) {
            printf("\nType 'attack' to strike or 'exit' to retreat: ");
            scanf("%s", action);

            if (strcmp(action, "attack") == 0) {
                int damage = player.equipped.damage + (rand() % 5);
                int critical = rand() % 100 < 20 ? 1 : 0;
                if (critical) damage *= 2;
                foe_hp -= damage;
                if (foe_hp < 0) foe_hp = 0;

                printf("Your attack dealt " RED "%d damage" RESET " %s\n", damage, critical ? "(Critical!)" : "");
                if (strlen(player.equipped.passive) > 1 && strcmp(player.equipped.passive, "-") != 0)
                    printf("Passive Effect: %s\n", player.equipped.passive);

                if (foe_hp <= 0) {
                    printf("\n=== \033[1;35mREWARD\033[0m ===\n");
                    printf("You gained \033[1;33m%d coins!\033[0m\n", reward);
                    player.coins += reward;
                    player.foes_defeated++;
                    break;
                } else {
                    display_hp_bar(foe_hp, max_hp);
                }
            } else if (strcmp(action, "exit") == 0) {
                printf("You retreated from combat.\n");
                close(sock);
                return;
            } else {
                printf("Unknown command.\n");
            }
        }

        printf("\n=== \033[1;36m⚔️  NEW FOE EMERGES FROM THE DARKNESS! ⚔️\033[0m ===\n");
    }

    close(sock);
}
```

#### shop.c
```
// Import library dasar
#include <stdio.h>
#include <string.h>

// Definisikan jumlah senjata di toko
#define WEAPON_COUNT 10

// Struct untuk menyimpan data senjata
typedef struct {
    char name[30];
    int price;
    int damage;
    char passive[50];
} Weapon;

// Daftar senjata yang tersedia di toko
Weapon armory[WEAPON_COUNT] = {
    {"💥 Explosive Crocs", 120, 18, "Blast on Hit"},
    {"🍌 Yeeted Banana", 90, 14, "Slippery Strike"},
    {"👡 Cursed Flip-Flop", 100, 16, "Stun Chance"},
    {"🐓 Aggressive Rooster", 130, 20, "Berserk Mode"},
    {"◔◔ Suspicious Bubble Tea", 110, 17, "Random Buff"},
    {"🐟 Flying Sardine", 115, 19, "Piercing Attack"},
    {"🍜 Quantum Noodles", 140, 21, "Time Skip"},
    {"🦆 Haunted Rubber Duck", 95, 15, "Fear Aura"},
    {"🧈 Shouting Tofu", 105, 16, "Sonic Boom"},
    {"🦟 Killer Mosquito", 125, 19, "Life Leech"},
};
```
Fungsi untuk menampilkan semua senjata yang tersedia di toko ⬇⬇⬇
```
void display_shop() {
    printf("\n");
    printf("     ╔════════════════════════════════════════════════╗\n");
    printf("     ║            🛒  CRAZY WARRIOR MART  🛒          ║\n");
    printf("     ╚════════════════════════════════════════════════╝\n\n");

    // Loop untuk print semua senjata
    for (int i = 0; i < WEAPON_COUNT; i++) {
        printf("[%d] %s - %d Coins | Damage: %d | Passive: %s\n", 
              i+1, armory[i].name, armory[i].price, 
              armory[i].damage, armory[i].passive);
    }
    printf("=========================================================================\n");
}
```
Fungsi untuk mengambil senjata berdasarkan index ⬇⬇⬇
```
Weapon get_weapon(int index) {
    if (index < 0 || index >= WEAPON_COUNT) {
        // Senjata kosong
        Weapon empty = {"", 0, 0, ""};
        return empty;
    }
    return armory[index];
}
```
### Build & Run
### Output

### SOAL  4
## Deskripsi
system.c dan hunter.c untuk simulasi hunter dan dungeon
---


```markdown
# Dungeon Hunter Game

Game berbasis **shared memory** & **semaphore** di C, dengan dua komponen utama:

- **system.c**: “Server” yang mengelola shared memory, semaphore, auto‐generate dungeon, dan fitur admin (ban/unban/reset hunter).
- **hunter.c**: “Client” yang dijalankan player—registrasi/login, bertarung dengan dungeon, battle antar hunter, dan manajemen stat.

---

## 📂 Struktur Direktori

```


├── system.c
└── hunter.c

````

---

## 🔧 Prasyarat

- OS: Linux/Unix  
- Compiler: GCC  
- Header: `<sys/ipc.h>`, `<sys/shm.h>`, `<sys/sem.h>`, `<pthread.h>`, `<signal.h>`

---

## 🖥️ system.c

### 1. Definisi & Inisialisasi

```c
#define MAX_HUNTERS 100
#define MAX_DUNGEONS 100
#define GLOBAL_SHM_KEY 0x1111
#define GLOBAL_SEM_KEY 0x2222

struct Hunter { /* id, name, level, exp, atk/hp/def, init_*, banned, active */ };
struct GlobalMemory {
    int nextHunterId, numHunters;
    struct Hunter hunters[MAX_HUNTERS];
    int numDungeons;
    key_t dungeonKeys[MAX_DUNGEONS];
};
struct Dungeon { int level, atk, hp, def, exp; };

// Shared memory & semaphore
shmidGlobal = shmget(GLOBAL_SHM_KEY, sizeof(GlobalMemory), IPC_CREAT|0666);
globalMem   = shmat(shmidGlobal, NULL, 0);
semid       = semget(GLOBAL_SEM_KEY, 1, IPC_CREAT|0666);
union semun arg = { .val = 1 };
semctl(semid, 0, SETVAL, arg);
````

### 2. Cleanup (SIGINT)

```c
void cleanup(int sig) {
  running = false;
  semop(semid, &lock, 1);
  for(int i=0;i<globalMem->numDungeons;i++){
    int shmid = shmget(globalMem->dungeonKeys[i], sizeof(Dungeon),0666);
    shmctl(shmid, IPC_RMID, NULL);
  }
  semop(semid, &unlock, 1);
  shmdt(globalMem);
  shmctl(shmidGlobal, IPC_RMID, NULL);
  semctl(semid,0,IPC_RMID);
  exit(0);
}
signal(SIGINT, cleanup);
```

### 3. Auto‐Generate Dungeon

```c
void *dungeonThread(void *_) {
  while(running) {
    sleep(3);
    if(!generateDungeonActive) continue;

    semop(semid,&lock,1);
    if(globalMem->numDungeons < MAX_DUNGEONS) {
      key_t key = GLOBAL_SHM_KEY + 100 + globalMem->numDungeons;
      int shmid = shmget(key, sizeof(Dungeon), IPC_CREAT|0666);
      Dungeon *d = shmat(shmid,NULL,0);
      d->level = rand()%5+1; /* atk/hp/def/exp random */
      globalMem->dungeonKeys[ globalMem->numDungeons++ ] = key;
      shmdt(d);
    }
    semop(semid,&unlock,1);
  }
}
pthread_create(&tid,NULL,dungeonThread,NULL);
```

### 4. Fitur Admin & Menu

```c
void showHunters() {
  semop(semid,&lock,1);
  printf("ID  Nama  Lv  Exp  ATK  HP  DEF  Banned\n");
  for(int i=0;i<MAX_HUNTERS;i++)
    if(globalMem->hunters[i].active)
      printf("%d  %s  %d  %d  %d  %d  %d  %s\n",
        h->id,h->name,h->level,h->exp,h->atk,h->hp,h->def,
        h->banned?"Ya":"Tidak");
  semop(semid,&unlock,1);
}

void showDungeons() {
  semop(semid,&lock,1);
  printf("Key  Lv  ATK  HP  DEF  EXP\n");
  key_t keys[MAX_DUNGEONS];
  int n=globalMem->numDungeons;
  memcpy(keys,globalMem->dungeonKeys,sizeof(key_t)*n);
  semop(semid,&unlock,1);
  for(int i=0;i<n;i++){
    int shmid = shmget(keys[i],sizeof(Dungeon),0666);
    Dungeon *d = shmat(shmid,NULL,0);
    printf("%d  %d  %d  %d  %d  %d\n",
      (int)keys[i],d->level,d->atk,d->hp,d->def,d->exp);
    shmdt(d);
  }
}

while(1){
  printf("1.InfoHunter 2.InfoDungeon 3.ToggleAutoDungeon\n"
         "4.Ban 5.Unban 6.ResetHunter 7.Exit\nPilihan: ");
  scanf("%d",&c);
  switch(c){
    case 1: showHunters(); break;
    case 2: showDungeons(); break;
    case 3: generateDungeonActive = !generateDungeonActive; break;
    case 4: /* ban hunter by ID */; break;
    case 5: /* unban hunter */; break;
    case 6: /* reset stats to init_* */; break;
    case 7: cleanup(0); break;
  }
}
```

* **Ban/Unban Hunter**: ubah `hunters[i].banned`.
* **Reset Hunter**: set `level=1, exp=0, atk=init_atk, …`.

---

## 🧑‍💻 hunter.c

### 1. Inisialisasi Client

```c
shmid = shmget(GLOBAL_SHM_KEY,sizeof(GlobalMemory),0666);
globalMem = shmat(shmid,NULL,0);
semid   = semget(GLOBAL_SEM_KEY,1,0666);
```

### 2. Registrasi & Login

```c
while(userId<0){
  printf("1.Register 2.Login 3.Exit: "); scanf("%d",&ch);
  if(ch==1){
    scanf("%s",name);
    semop(semid,&lock,1);
    /* cek duplicate name & slot kosong */
    /* assign id=nextHunterId++ dan init stats */
    semop(semid,&unlock,1);
  }
  else if(ch==2){
    scanf("%d",&id);
    semop(semid,&lock,1);
    /* cari hunter[id], cek banned */
    semop(semid,&unlock,1);
    userId=id;
  }
  else exit(0);
}
```

### 3. defeatDungeon & levelUp

```c
void defeatDungeon(Hunter *h, Dungeon *d){
  printf("%s defeat lvl%d\n",h->name,d->level);
  h->atk+=d->atk; h->def+=d->def; h->hp+=d->hp;
}
void levelUp(Hunter *h){
  h->level++; h->atk+=2; h->def+=2; h->hp+=10; h->exp=0;
  printf("%s lvl up to %d\n",h->name,h->level);
}
```

### 4. Menu Hunter

```c
while(1){
  printf("1.Tmplk Dungeon 2.Conquer 3.Battle Hunter\n"
         "4.Stat Saya 5.Logout 6.Reset Stat: ");
  scanf("%d",&opt);
  switch(opt){
    case 1: /* list dungeon <= level */; break;
    case 2: /* conquer: attach dungeon, add exp, while(exp>=500)levelUp, defeatDungeon, remove dungeon */; break;
    case 3: /* battle hunter lain: compare power, update stats, remove loser */; break;
    case 4: /* tampil stat hunter */; break;
    case 5: shmdt(globalMem); exit(0);
    case 6: /* reset atk/hp/def to init_* berdasarkan pilihan */; break;
  }
}
```

#### a. Tampilkan Dungeon:

```c
semop(semid,&lock,1);
int lvl = hunter->level;
key_t keys[globalMem->numDungeons];
copy keys...
semop(semid,&unlock,1);
for(each key){
  attach, if(lvl>=d->level) printf(...);
  detach;
}
```

#### b. Conquer Dungeon:

```c
scanf("%d",&key);
attach dungeon d;
semop(&lock);
Hunter *h=find hunter;
if(h->level<d->level){ /* gagal */ }
h->exp+=d->exp;
while(h->exp>=500){ h->exp-=500; levelUp(h); }
defeatDungeon(h,d);
remove dungeon key & shmctl(...,IPC_RMID);
semop(&unlock);
```

#### c. Battle Hunter Lain:

```c
semop(&lock);
list semua hunter aktif ≠ userId, ≠ banned;
scanf("%d",&eid);
Hunter a=*me, b=*enemy;
int pa=a.atk+a.hp+a.def, pb=b.atk+b.hp+b.def;
if(pa>=pb){
  printf("You win"); update me.stats+=enemy.stats; deactivate enemy;
} else {
  printf("You lose"); update enemy.stats+=me.stats; deactivate me;
}
semop(&unlock);
```

#### d. Reset Stat:

```c
printf("1.ATK 2.HP 3.DEF: "); scanf("%d",&ch);
semop(&lock);
if(ch==1) h->atk=h->init_atk;
...
semop(&unlock);
```

---

## 🚀 Build & Run

```bash
# Server
gcc system.c -o system -pthread
./system

# Client (bisa banyak)
gcc hunter.c -o hunter
./hunter
```

* Jalankan `./system` **terlebih dahulu**.
* Buka beberapa terminal, jalankan `./hunter` untuk multi‐player.

---

## ℹ️ Cleanup Manual

```bash
ipcs
ipcrm -m <shmid>    # hapus shared memory
ipcrm -s <semid>    # hapus semaphore
```

---
## Revisi 
memasukkan fitur info dungeon, info hunter, dan generated dungeon ke dalam menu.

sebelumnya info hunter dan dungeon menyatu.

generated dungeon dan info hunter dan juga info dungeon langsung berjalan ketika menjalankan system.

sekarang udah dibuatkan menunya jadi akan berjalan ketika menunya dipilih
---
