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
#define GLOBAL_SHM_KEY 0x1111  // kunci shared memory global
#define GLOBAL_SEM_KEY 0x2222  // kunci semaphore global

struct Hunter {
    int id;               
    char name[50];
    int level;
    int exp;
    int atk, hp, def;      
    int init_atk, init_hp, init_def; // stat awal
    int banned;            
    int active;            // aktif 1 or 0 
};

// struktur global mem (shared memory)
struct GlobalMemory {
    int nextHunterId;             
    int numHunters;               
    struct Hunter hunters[MAX_HUNTERS];
    int numDungeons;             
    key_t dungeonKeys[MAX_DUNGEONS]; 
};

// struktur data Dungeon (shared memory terpisah)
struct Dungeon {
    int level;
    int atk;
    int hp;
    int def;
    int exp;
};

//  union semun untuk operasi semctl
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

int shmidGlobal;
struct GlobalMemory *globalMem;
int semid;
bool running = true;  //kontrol thread pembuatan dungeon
bool generateDungeonActive = false;  // Flag untuk mode generate dungeon


// operasi semaphore (lock/unlock)
struct sembuf sem_lock = {0, -1, 0};
struct sembuf sem_unlock = {0, 1, 0};

// fungsi cleanup untuk menghapus semua shared memory dan semaphore saat program berhenti
void cleanup(int signum) {
    // stop thread pembuatan dungeon
    running = false;

    // lock untuk baca daftar dungeon
    semop(semid, &sem_lock, 1);
    // hapus semua shared memory dungeon yang tersisa
    for(int i = 0; i < globalMem->numDungeons; i++) {
        key_t key = globalMem->dungeonKeys[i];
        int shmid = shmget(key, sizeof(struct Dungeon), 0666);
        if(shmid >= 0) {
            shmctl(shmid, IPC_RMID, NULL);
        }
    }
    semop(semid, &sem_unlock, 1);

    // detach dan hapus global shared memory
    shmdt(globalMem);
    shmctl(shmidGlobal, IPC_RMID, NULL);

    // hapus semaphore
    semctl(semid, 0, IPC_RMID);

    printf("\nProgram dihentikan, semua shared memory telah dihapus.\n");
    exit(0);
}

// thread yang membuat dungeon baru setiap 3 detik
void *dungeonThread(void *arg) {
    srand(time(NULL));
    while (running) {
        sleep(3);
        if (!generateDungeonActive) continue;  // Hanya generate dungeon saat flag aktif

        semop(semid, &sem_lock, 1);
        if(globalMem->numDungeons < MAX_DUNGEONS) {
            int idx = globalMem->numDungeons;
            key_t key = GLOBAL_SHM_KEY + 100 + idx;
            int shmid = shmget(key, sizeof(struct Dungeon), IPC_CREAT | 0666);
            if(shmid >= 0) {
                struct Dungeon *d = (struct Dungeon*) shmat(shmid, NULL, 0);
                if(d != (void*) -1) {
                    d->level = rand() % 5 + 1;
                    d->atk   = rand() % 51 + 100;
                    d->hp    = rand() % 51 + 50;
                    d->def   = rand() % 26 + 25;
                    d->exp   = rand() % 151 + 150;
                    globalMem->dungeonKeys[idx] = key;
                    globalMem->numDungeons++;

                    printf("Dungeon baru dibuat - Key: %d, Level: %d, ATK: %d, HP: %d, DEF: %d, EXP: %d\n",
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

void generateDungeonOnce() {
    semop(semid, &sem_lock, 1);
    if(globalMem->numDungeons < MAX_DUNGEONS) {
        int idx = globalMem->numDungeons;
        key_t key = GLOBAL_SHM_KEY + 100 + idx;
        int shmid = shmget(key, sizeof(struct Dungeon), IPC_CREAT | 0666);
        if(shmid < 0) {
            perror("Gagal membuat shared memory dungeon");
        } else {
            struct Dungeon *d = (struct Dungeon*) shmat(shmid, NULL, 0);
            if(d == (void*) -1) {
                perror("Gagal attach shared memory dungeon");
            } else {
                d->level = rand() % 5 + 1;
                d->atk   = rand() % 51 + 100;
                d->hp    = rand() % 51 + 50;
                d->def   = rand() % 26 + 25;
                d->exp   = rand() % 151 + 150;

                globalMem->dungeonKeys[idx] = key;
                globalMem->numDungeons++;

                printf("Dungeon baru dibuat - Key: %d, Level: %d, ATK: %d, HP: %d, DEF: %d, EXP: %d\n",
                    (int)key, d->level, d->atk, d->hp, d->def, d->exp);
                shmdt(d);
            }
        }
    } else {
        printf("Jumlah maksimum dungeon telah tercapai.\n");
    }
    semop(semid, &sem_unlock, 1);
}


// tampilkan daftar hunter yang aktif
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

// tampilkan daftar dungeon yang aktif beserta statistiknya
void showDungeons() {
    semop(semid, &sem_lock, 1);
    printf("Key\tLevel\tATK\tHP\tDEF\tEXP\n");
    int count = globalMem->numDungeons;
    key_t keys[MAX_DUNGEONS];
    for(int i = 0; i < count; i++) {
        keys[i] = globalMem->dungeonKeys[i];
    }
    semop(semid, &sem_unlock, 1);

    // baca setiap dungeon dari shared memory terpisah
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
    // tangani SIGINT (Ctrl+C) untuk cleanup sebelum exit
    signal(SIGINT, cleanup);

    // buat atau attach shared memory global
    shmidGlobal = shmget(GLOBAL_SHM_KEY, sizeof(struct GlobalMemory), IPC_CREAT | 0666);
    if(shmidGlobal < 0) { perror("shmget global"); exit(1); }
    globalMem = (struct GlobalMemory*) shmat(shmidGlobal, NULL, 0);
    if(globalMem == (void*) -1) { perror("shmat global"); exit(1); }

    // buat atau attach semaphore global
    semid = semget(GLOBAL_SEM_KEY, 1, IPC_CREAT | 0666);
    if(semid < 0) { perror("semget"); exit(1); }
    // inisialisasi semaphore bila baru dibuat
    union semun arg;
    struct semid_ds buf;
    arg.buf = &buf;
    // jika berhasil IPC_STAT, berarti semaphore ada (setiap kali program baru dimulai,
    // nilai semaphore kita inisialisasi 1).
    if(semctl(semid, 0, IPC_STAT, arg) == 0) {
        arg.val = 1;
        semctl(semid, 0, SETVAL, arg);
    }

    // inisialisasi data global saat pertama kali dibuat
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

    // mulai thread untuk pembuatan dungeon otomatis
    pthread_t tid;
    pthread_create(&tid, NULL, dungeonThread, NULL);

    // menu utama sistem (loop sampai exit)
    while(1) {
        printf("\n--- Sistem Utama ---\n");
        printf("1. Info Hunter\n");
        printf("2. Info Dungeon\n");
        printf("3. Generate Dungeon Sekali\n");
        printf("4. Ban Hunter\n");
        printf("5. Unban Hunter\n");
        printf("6. Reset Hunter\n");
        printf("7. Keluar\n");
        printf("Masukkan pilihan (1-7): ");
        int choice;
        scanf("%d", &choice);
    
        if(choice == 1) {
            showHunters();
        } else if(choice == 2) {
            showDungeons();
        } else if(choice == 3) {
            generateDungeonActive = !generateDungeonActive;
            printf("Generate Dungeon sekarang: %s\n", generateDungeonActive ? "AKTIF" : "NONAKTIF");
        } else if(choice == 4) {
            int id;
            printf("Masukkan ID hunter yang akan diban: ");
            scanf("%d", &id);
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
            else printf("Hunter tidak ditemukan.\n");
        } else if(choice == 5) {
            int id;
            printf("Masukkan ID hunter yang akan di-unban: ");
            scanf("%d", &id);
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
            if(found) printf("Hunter ID %d telah di-unban.\n", id);
            else printf("Hunter tidak ditemukan.\n");
        } else if(choice == 6) {
            int id;
            printf("Masukkan ID hunter yang akan di-reset: ");
            scanf("%d", &id);
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
            if(found) printf("Hunter ID %d telah direset ke stat awal.\n", id);
            else printf("Hunter tidak ditemukan.\n");
        } else if(choice == 7) {
            cleanup(0);
        } else {
            printf("Pilihan tidak valid.\n");
        }
    }
    
    return 0;
}
