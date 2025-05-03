#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>
#include "assets/shm_common.h"

#define MAX_HUNTERS 100
#define MAX_DUNGEONS 100
#define GLOBAL_SHM_KEY 0x1111  // Kunci shared memory global
#define GLOBAL_SEM_KEY 0x2222  // Kunci semaphore global

// Struktur data untuk Hunter (disimpan di shared memory global)
struct Hunter {
    int id;                // ID unik hunter
    char name[50];
    int level;
    int exp;
    int atk, hp, def;      // Statistik saat ini
    int init_atk, init_hp, init_def; // Statistik awal (untuk reset)
    int banned;            // Status banned (1 = banned, 0 = tidak)
    int active;            // Apakah slot aktif (1) atau kosong (0)
};

// Struktur data global (shared memory)
struct GlobalMemory {
    int nextHunterId;             // ID berikutnya yang akan diberikan
    int numHunters;               // Jumlah hunter aktif
    struct Hunter hunters[MAX_HUNTERS];
    int numDungeons;              // Jumlah dungeon aktif
    key_t dungeonKeys[MAX_DUNGEONS]; // Kunci unik setiap dungeon
};

// Struktur data Dungeon (disimpan di shared memory terpisah)
struct Dungeon {
    int level;
    int atk;
    int hp;
    int def;
    int exp;
};

// Definisi union semun untuk operasi semctl
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

int shmidGlobal;
struct GlobalMemory *globalMem;
int semid;
bool running = true;  // Flag kontrol thread pembuatan dungeon

// Operasi semaphore (lock/unlock)
struct sembuf sem_lock = {0, -1, 0};
struct sembuf sem_unlock = {0, 1, 0};

// Fungsi cleanup untuk menghapus semua shared memory dan semaphore saat program berhenti
void cleanup(int signum) {
    // Stop thread pembuatan dungeon
    running = false;

    // Lock untuk baca daftar dungeon
    semop(semid, &sem_lock, 1);
    // Hapus semua shared memory dungeon yang tersisa
    for(int i = 0; i < globalMem->numDungeons; i++) {
        key_t key = globalMem->dungeonKeys[i];
        int shmid = shmget(key, sizeof(struct Dungeon), 0666);
        if(shmid >= 0) {
            shmctl(shmid, IPC_RMID, NULL);
        }
    }
    semop(semid, &sem_unlock, 1);

    // Detach dan hapus global shared memory
    shmdt(globalMem);
    shmctl(shmidGlobal, IPC_RMID, NULL);

    // Hapus semaphore
    semctl(semid, 0, IPC_RMID);

    printf("\nProgram dihentikan, semua shared memory telah dihapus.\n");
    exit(0);
}

// Thread yang membuat dungeon baru setiap 3 detik
void *dungeonThread(void *arg) {
    srand(time(NULL));
    while(running) {
        sleep(3);
        semop(semid, &sem_lock, 1);
        if(globalMem->numDungeons < MAX_DUNGEONS) {
            int idx = globalMem->numDungeons;
            // Buat kunci unik untuk dungeon baru (contoh: offset tetap ditambah index)
            key_t key = GLOBAL_SHM_KEY + 100 + idx;
            int shmid = shmget(key, sizeof(struct Dungeon), IPC_CREAT | 0666);
            if(shmid < 0) {
                perror("Gagal membuat shared memory dungeon");
            } else {
                struct Dungeon *d = (struct Dungeon*) shmat(shmid, NULL, 0);
                if(d == (void*) -1) {
                    perror("Gagal attach shared memory dungeon");
                } else {
                    // Isi dungeon dengan nilai random sesuai spesifikasi
                    d->level = rand() % 5 + 1;      // 1-5
                    d->atk   = rand() % 51 + 100;   // 100-150
                    d->hp    = rand() % 51 + 50;    // 50-100
                    d->def   = rand() % 26 + 25;    // 25-50
                    d->exp   = rand() % 151 + 150;  // 150-300
                    // Tambahkan kunci ke daftar global
                    globalMem->dungeonKeys[idx] = key;
                    globalMem->numDungeons++;

                    printf("Notifikasi: Dungeon baru dibuat - Key: %d, Level: %d, ATK: %d, HP: %d, DEF: %d, EXP: %d\n",
                           (int)key, d->level, d->atk, d->hp, d->def, d->exp);
                    fflush(stdout);

                    shmdt(d);
                }
            }
        }
        semop(semid, &sem_unlock, 1);
    }
    return NULL;
}

// Tampilkan daftar hunter yang aktif
void showHunters() {
    semop(semid, &sem_lock, 1);
    printf("ID\tNama\tLevel\tExp\tATK\tHP\tDEF\tBanned\n");
    for(int i = 0; i < MAX_HUNTERS; i++) {
        if(globalMem->hunters[i].active) {
            struct Hunter *h = &globalMem->hunters[i];
            printf("%d\t%s\t%d\t%d\t%d\t%d\t%d\t%s\n",
                   h->id, h->name, h->level, h->exp,
                   h->atk, h->hp, h->def,
                   (h->banned ? "Ya" : "Tidak"));
        }
    }
    semop(semid, &sem_unlock, 1);
}

// Tampilkan daftar dungeon yang aktif beserta statistiknya
void showDungeons() {
    semop(semid, &sem_lock, 1);
    printf("Key\tLevel\tATK\tHP\tDEF\tEXP\n");
    int count = globalMem->numDungeons;
    key_t keys[MAX_DUNGEONS];
    for(int i = 0; i < count; i++) {
        keys[i] = globalMem->dungeonKeys[i];
    }
    semop(semid, &sem_unlock, 1);

    // Baca setiap dungeon dari shared memory terpisah
    for(int i = 0; i < count; i++) {
        int shmid = shmget(keys[i], sizeof(struct Dungeon), 0666);
        if(shmid >= 0) {
            struct Dungeon *d = (struct Dungeon*) shmat(shmid, NULL, 0);
            if(d != (void*) -1) {
                printf("%d\t%d\t%d\t%d\t%d\t%d\n",
                       (int)keys[i], d->level, d->atk, d->hp, d->def, d->exp);
                shmdt(d);
            }
        }
    }
}

int main() {
    // Tangani SIGINT (Ctrl+C) untuk cleanup sebelum exit
    signal(SIGINT, cleanup);

    // Buat atau attach shared memory global
    shmidGlobal = shmget(GLOBAL_SHM_KEY, sizeof(struct GlobalMemory), IPC_CREAT | 0666);
    if(shmidGlobal < 0) { perror("shmget global"); exit(1); }
    globalMem = (struct GlobalMemory*) shmat(shmidGlobal, NULL, 0);
    if(globalMem == (void*) -1) { perror("shmat global"); exit(1); }

    // Buat atau attach semaphore global
    semid = semget(GLOBAL_SEM_KEY, 1, IPC_CREAT | 0666);
    if(semid < 0) { perror("semget"); exit(1); }
    // Inisialisasi semaphore bila baru dibuat
    union semun arg;
    struct semid_ds buf;
    arg.buf = &buf;
    // Jika berhasil IPC_STAT, berarti semaphore ada (setiap kali program baru dimulai,
    // nilai semaphore kita inisialisasi 1).
    if(semctl(semid, 0, IPC_STAT, arg) == 0) {
        arg.val = 1;
        semctl(semid, 0, SETVAL, arg);
    }

    // Inisialisasi data global saat pertama kali dibuat
    semop(semid, &sem_lock, 1);
    if(globalMem->nextHunterId == 0) {
        globalMem->nextHunterId = 1;
        globalMem->numHunters = 0;
        globalMem->numDungeons = 0;
        for(int i = 0; i < MAX_HUNTERS; i++) {
            globalMem->hunters[i].active = 0;
            globalMem->hunters[i].banned = 0;
        }
    }
    semop(semid, &sem_unlock, 1);

    // Mulai thread untuk pembuatan dungeon otomatis
    pthread_t tid;
    pthread_create(&tid, NULL, dungeonThread, NULL);

    // Menu utama sistem (loop sampai exit)
    while(1) {
        printf("\n--- Sistem Utama ---\n");
        showHunters();
        showDungeons();
        printf("\nPilihan:\n");
        printf("1. Ban Hunter\n");
        printf("2. Unban Hunter\n");
        printf("3. Reset Hunter\n");
        printf("4. Keluar\n");
        printf("Masukkan pilihan (1-4): ");
        int choice;
        scanf("%d", &choice);
        if(choice == 4) {
            cleanup(0);
        }

        if(choice == 1) {
            // Ban hunter berdasarkan ID
            printf("Masukkan ID hunter yang akan diban: ");
            int id; scanf("%d", &id);
            semop(semid, &sem_lock, 1);
            int found = 0;
            for(int i = 0; i < MAX_HUNTERS; i++) {
                if(globalMem->hunters[i].active && globalMem->hunters[i].id == id) {
                    globalMem->hunters[i].banned = 1;
                    found = 1;
                    break;
                }
            }
            semop(semid, &sem_unlock, 1);
            if(found) printf("Hunter ID %d telah diban.\n", id);
            else printf("Hunter dengan ID %d tidak ditemukan.\n", id);
        }
        else if(choice == 2) {
            // Unban hunter berdasarkan ID
            printf("Masukkan ID hunter yang akan diunban: ");
            int id; scanf("%d", &id);
            semop(semid, &sem_lock, 1);
            int found = 0;
            for(int i = 0; i < MAX_HUNTERS; i++) {
                if(globalMem->hunters[i].active && globalMem->hunters[i].id == id) {
                    globalMem->hunters[i].banned = 0;
                    found = 1;
                    break;
                }
            }
            semop(semid, &sem_unlock, 1);
            if(found) printf("Hunter ID %d telah diunban.\n", id);
            else printf("Hunter dengan ID %d tidak ditemukan.\n", id);
        }
        else if(choice == 3) {
            // Reset hunter (kembali ke statistik awal)
            printf("Masukkan ID hunter yang akan direset: ");
            int id; scanf("%d", &id);
            semop(semid, &sem_lock, 1);
            int found = 0;
            for(int i = 0; i < MAX_HUNTERS; i++) {
                struct Hunter *h = &globalMem->hunters[i];
                if(h->active && h->id == id) {
                    h->level = 1;
                    h->exp = 0;
                    h->atk = h->init_atk;
                    h->hp  = h->init_hp;
                    h->def = h->init_def;
                    found = 1;
                    break;
                }
            }
            semop(semid, &sem_unlock, 1);
            if(found) printf("Stats hunter ID %d telah direset ke awal.\n", id);
            else printf("Hunter dengan ID %d tidak ditemukan.\n", id);
        }
        else {
            printf("Pilihan tidak valid.\n");
        }
    }

    return 0;
}
