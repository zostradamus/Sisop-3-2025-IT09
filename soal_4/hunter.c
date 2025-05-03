#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <time.h>


#define MAX_HUNTERS 100
#define MAX_DUNGEONS 100
#define GLOBAL_SHM_KEY 0x1111  // Harus sama dengan di system.c
#define GLOBAL_SEM_KEY 0x2222  // Harus sama dengan di system.c

// Struktur data Hunter (sama seperti di system.c)
struct Hunter {
    int id;
    char name[50];
    int level;
    int exp;
    int atk, hp, def;
    int init_atk, init_hp, init_def;
    int banned;
    int active;
};

// Struktur data GlobalMemory (sama seperti di system.c)
struct GlobalMemory {
    int nextHunterId;
    int numHunters;
    struct Hunter hunters[MAX_HUNTERS];
    int numDungeons;
    key_t dungeonKeys[MAX_DUNGEONS];
};

struct Dungeon {
    int level;
    int atk;
    int hp;
    int def;
    int exp;
};

// Operasi semaphore (lock/unlock)
struct sembuf sem_lock = {0, -1, 0};
struct sembuf sem_unlock = {0, 1, 0};

void levelUp(struct Hunter *hunter) {
    hunter->level += 1;
    hunter->atk += 2;
    hunter->def += 2;
    hunter->hp += 10;

    printf("\n>> %s naik ke level %d!\n", hunter->name, hunter->level);
    printf("   ATK bertambah menjadi %d\n", hunter->atk);
    printf("   DEF bertambah menjadi %d\n", hunter->def);
    printf("   HP bertambah menjadi %d\n\n", hunter->hp);
}

int main() {
    srand(time(NULL) ^ getpid());

    // Attach ke shared memory global
    int shmid = shmget(GLOBAL_SHM_KEY, sizeof(struct GlobalMemory), 0666);
    if(shmid < 0) {
        perror("Shared memory global tidak ditemukan, pastikan system sudah dijalankan");
        exit(1);
    }
    struct GlobalMemory *globalMem = (struct GlobalMemory*) shmat(shmid, NULL, 0);
    if(globalMem == (void*) -1) {
        perror("shmat global");
        exit(1);
    }

    // Attach ke semaphore global
    int semid = semget(GLOBAL_SEM_KEY, 1, 0666);
    if(semid < 0) {
        perror("Semaphore tidak ditemukan");
        exit(1);
    }

    int userId = -1; // ID hunter yang login

    // Menu registrasi / login
    while(userId < 0) {
        printf("\n--- Menu Hunter ---\n");
        printf("1. Register\n");
        printf("2. Login\n");
        printf("3. Exit\n");
        printf("Masukkan pilihan: ");
        int choice;
        scanf("%d", &choice);
        if(choice == 3) {
            // Keluar program
            shmdt(globalMem);
            printf("Keluar...\n");
            exit(0);
        }
        else if(choice == 1) {
            // Registrasi
            char name[50];
            printf("Masukkan nama: ");
            scanf("%s", name);

            semop(semid, &sem_lock, 1);
            // Periksa nama unik
            int nameExists = 0;
            for(int i = 0; i < MAX_HUNTERS; i++) {
                if(globalMem->hunters[i].active && strcmp(globalMem->hunters[i].name, name) == 0) {
                    nameExists = 1;
                    break;
                }
            }
            if(nameExists) {
                printf("Nama sudah digunakan!\n");
                semop(semid, &sem_unlock, 1);
                continue;
            }
            // Periksa kapasitas
            if(globalMem->numHunters >= MAX_HUNTERS) {
                printf("Jumlah hunter maksimum tercapai.\n");
                semop(semid, &sem_unlock, 1);
                continue;
            }
            // Cari slot kosong
            int idx = -1;
            for(int i = 0; i < MAX_HUNTERS; i++) {
                if(!globalMem->hunters[i].active) {
                    idx = i;
                    break;
                }
            }
            if(idx == -1) {
                printf("Tidak ada slot kosong untuk hunter baru.\n");
                semop(semid, &sem_unlock, 1);
                continue;
            }
            // Tambahkan hunter baru
            int id = globalMem->nextHunterId++;
            globalMem->hunters[idx].id = id;
            strncpy(globalMem->hunters[idx].name, name, 49);
            globalMem->hunters[idx].name[49] = '\0';
            globalMem->hunters[idx].level = 1;
            globalMem->hunters[idx].exp = 0;
            // Statistik awal acak
            globalMem->hunters[idx].atk = 10;  // 10-20
            globalMem->hunters[idx].hp  = 100; // 100-120
            globalMem->hunters[idx].def = 5;  // 10-20
            globalMem->hunters[idx].init_atk = globalMem->hunters[idx].atk;
            globalMem->hunters[idx].init_hp  = globalMem->hunters[idx].hp;
            globalMem->hunters[idx].init_def = globalMem->hunters[idx].def;
            globalMem->hunters[idx].banned = 0;
            globalMem->hunters[idx].active = 1;
            globalMem->numHunters++;
            semop(semid, &sem_unlock, 1);

            printf("Registrasi berhasil. ID Anda: %d\n", id);
        }
        else if(choice == 2) {
            // Login
            printf("Masukkan ID: ");
            int id;
            scanf("%d", &id);
            semop(semid, &sem_lock, 1);
            int idx = -1;
            for(int i = 0; i < MAX_HUNTERS; i++) {
                if(globalMem->hunters[i].active && globalMem->hunters[i].id == id) {
                    idx = i;
                    break;
                }
            }
            if(idx == -1) {
                printf("ID tidak ditemukan.\n");
                semop(semid, &sem_unlock, 1);
                continue;
            }
            if(globalMem->hunters[idx].banned) {
                printf("Akun Anda dibanned. Tidak dapat login.\n");
                semop(semid, &sem_unlock, 1);
                continue;
            }
            userId = id;
            semop(semid, &sem_unlock, 1);
            printf("Login berhasil. Selamat datang, %s (Level %d)!\n",
                   globalMem->hunters[idx].name, globalMem->hunters[idx].level);
        }
        else {
            printf("Pilihan tidak valid.\n");
        }
    }

    // Menu utama hunter setelah login
    while(1) {
        printf("\n=== Menu Hunter (ID %d) ===\n", userId);
        printf("1. Tampilkan Dungeon (sesuai level)\n");
        printf("2. Conquer Dungeon\n");
        printf("3. Battle Hunter Lain\n");
        printf("4. Tampilkan Stat Saya\n");
        printf("5. Logout\n");
        printf("6. Reset Stat\n");
        printf("Pilihan: ");
        int opt;
        scanf("%d", &opt);
        if(opt == 5) {
            printf("Logout...\n");
            shmdt(globalMem);
            exit(0);
        }
        else if(opt == 1) {
            // Tampilkan dungeon sesuai level hunter
            semop(semid, &sem_lock, 1);
            int myLevel = 0;
            for(int i = 0; i < MAX_HUNTERS; i++) {
                if(globalMem->hunters[i].active && globalMem->hunters[i].id == userId) {
                    myLevel = globalMem->hunters[i].level;
                    break;
                }
            }
            int count = globalMem->numDungeons;
            key_t keys[MAX_DUNGEONS];
            for(int i = 0; i < count; i++) {
                keys[i] = globalMem->dungeonKeys[i];
            }
            semop(semid, &sem_unlock, 1);

            printf("Dungeon untuk Level %d:\n", myLevel);
            printf("Key\tLevel\tATK\tHP\tDEF\tEXP\n");
            for(int i = 0; i < count; i++) {
                int shmid_d = shmget(keys[i], sizeof(struct Dungeon), 0666);
                if(shmid_d >= 0) {
                    struct Dungeon *d = (struct Dungeon*) shmat(shmid_d, NULL, 0);
                    if(d != (void*) -1) {
                        if(myLevel >= d->level) {
                            printf("%d\t%d\t%d\t%d\t%d\t%d\n",
                                   (int)keys[i], d->level, d->atk, d->hp, d->def, d->exp);
                        }
                        shmdt(d);
                    }
                }
            }
        }
        else if(opt == 2) {
    // Conquer dungeon (by key)
    printf("Masukkan Key dungeon yang ingin ditaklukkan: ");
    int key;
    scanf("%d", &key);
    semop(semid, &sem_lock, 1);
    int idxDungeon = -1;
    for(int i = 0; i < globalMem->numDungeons; i++) {
        if((int)globalMem->dungeonKeys[i] == key) {
            idxDungeon = i;
            break;
        }
    }
    if(idxDungeon == -1) {
        printf("Dungeon dengan key %d tidak ditemukan.\n", key);
        semop(semid, &sem_unlock, 1);
        continue;
    }
    // Hapus dungeon dari daftar global
    for(int i = idxDungeon; i < globalMem->numDungeons - 1; i++) {
        globalMem->dungeonKeys[i] = globalMem->dungeonKeys[i+1];
    }
    globalMem->numDungeons--;
    semop(semid, &sem_unlock, 1);

    // Baca exp dan stats dari dungeon tersebut, lalu hapus shared memory-nya
    int shmid_d = shmget(key, sizeof(struct Dungeon), 0666);
    int expGain = 0;
    int atkGain = 0;
    int hpGain = 0;
    int defGain = 0;
    if(shmid_d >= 0) {
        struct Dungeon *d = (struct Dungeon*) shmat(shmid_d, NULL, 0);
        if(d != (void*) -1) {
            expGain = d->exp;
            atkGain = d->atk;
            hpGain = d->hp;
            defGain = d->def;
            shmdt(d);
        }
        shmctl(shmid_d, IPC_RMID, NULL);
    }

    // Tambahkan exp dan stats ke hunter
    semop(semid, &sem_lock, 1);
    for(int i = 0; i < MAX_HUNTERS; i++) {
        if(globalMem->hunters[i].active && globalMem->hunters[i].id == userId) {
            globalMem->hunters[i].exp += expGain;
            globalMem->hunters[i].atk += atkGain;
            globalMem->hunters[i].hp += hpGain;
            globalMem->hunters[i].def += defGain;

            // Cek apakah hunter naik level
            if(globalMem->hunters[i].exp >= 500) {
                globalMem->hunters[i].exp = 0;
                levelUp(&globalMem->hunters[i]);
                printf("Selamat! Anda naik level.\n");
            }

            printf("Anda memperoleh %d exp, +%d ATK, +%d HP, +%d DEF dari dungeon.\n",
                   expGain, atkGain, hpGain, defGain);
            break;
        }
    }
    semop(semid, &sem_unlock, 1);
}

        else if(opt == 3) {
            int myIdx = -1;

            semop(semid, &sem_lock, 1);
            
            // Cari index hunter user
            for(int i = 0; i < MAX_HUNTERS; i++) {
                if(globalMem->hunters[i].active && globalMem->hunters[i].id == userId) {
                    myIdx = i;
                    break;
                }
            }
            if(myIdx == -1) {
                printf("Data hunter tidak ditemukan.\n");
                semop(semid, &sem_unlock, 1);
                continue;
            }
            
            // Tampilkan daftar hunter aktif
            printf("\n--- Daftar Hunter Aktif ---\n");
            int found = 0;
            for(int i = 0; i < MAX_HUNTERS; i++) {
                if(globalMem->hunters[i].active && globalMem->hunters[i].id != userId && !globalMem->hunters[i].banned) {
                    printf("ID: %d | Nama: %s | Level: %d\n", globalMem->hunters[i].id, globalMem->hunters[i].name, globalMem->hunters[i].level);
                    found = 1;
                }
            }
            if (!found) {
                printf("Tidak ada hunter lain yang bisa ditantang saat ini.\n");
                semop(semid, &sem_unlock, 1);
                continue;
            }
            
            printf("Masukkan ID lawan yang ingin kamu tantang: ");
            int enemyId;
            scanf("%d", &enemyId);
            
            // Cek jika user memilih dirinya sendiri
            if(enemyId == userId) {
                printf("Kamu tidak bisa menantang dirimu sendiri!\n");
                semop(semid, &sem_unlock, 1);
                continue;
            }
            
            // Cari index musuh
            int enemyIdx = -1;
            for(int i = 0; i < MAX_HUNTERS; i++) {
                if(globalMem->hunters[i].active && globalMem->hunters[i].id == enemyId && !globalMem->hunters[i].banned) {
                    enemyIdx = i;
                    break;
                }
            }
            if(enemyIdx == -1) {
                printf("Lawan tidak ditemukan atau tidak aktif.\n");
                semop(semid, &sem_unlock, 1);
                continue;
            }
            
            // Tampilkan stat
            struct Hunter myHunter = globalMem->hunters[myIdx];
            struct Hunter enemy = globalMem->hunters[enemyIdx];
            
            printf("\n--- Pertarungan Dimulai ---\n");
            printf(">> Kamu: %s (Level %d)\n", myHunter.name, myHunter.level);
            printf("   ATK: %d | HP: %d | DEF: %d\n", myHunter.atk, myHunter.hp, myHunter.def);
            
            printf(">> Musuh: %s (Level %d)\n", enemy.name, enemy.level);
            printf("   ATK: %d | HP: %d | DEF: %d\n", enemy.atk, enemy.hp, enemy.def);
            
           // Hitung kekuatan masing-masing hunter
int myPower = myHunter.atk + myHunter.hp + myHunter.def;
int enemyPower = enemy.atk + enemy.hp + enemy.def;

printf("\n--- Pertarungan Dimulai ---\n");
printf(">> Kamu (%s) : Power = %d\n", myHunter.name, myPower);
printf(">> Musuh (%s): Power = %d\n", enemy.name, enemyPower);

if (myPower >= enemyPower) {
    // Kamu menang
    printf(">> Kamu menang!\n");
    globalMem->hunters[myIdx].atk += enemy.atk;
    globalMem->hunters[myIdx].hp  += enemy.hp;
    globalMem->hunters[myIdx].def += enemy.def;

    // Musuh dihapus
    globalMem->hunters[enemyIdx].active = 0;
    printf(">> Hunter %s telah tereliminasi dari sistem.\n", enemy.name);
} else {
    // Kamu kalah
    printf(">> Kamu kalah!\n");
    globalMem->hunters[enemyIdx].atk += myHunter.atk;
    globalMem->hunters[enemyIdx].hp  += myHunter.hp;
    globalMem->hunters[enemyIdx].def += myHunter.def;

    // Kamu dihapus
    globalMem->hunters[myIdx].active = 0;
    printf(">> Kamu telah tereliminasi dari sistem.\n");
}

            
            // Cek level up
            if(globalMem->hunters[myIdx].exp >= 500) {
                globalMem->hunters[myIdx].exp -= 500;
                globalMem->hunters[myIdx].level++;
                printf(">> Selamat! Kamu naik ke level %d!\n", globalMem->hunters[myIdx].level);
            }
            if(globalMem->hunters[enemyIdx].exp >= 500) {
                globalMem->hunters[enemyIdx].exp -= 500;
                globalMem->hunters[enemyIdx].level++;
                printf(">> Musuh %s naik ke level %d!\n", enemy.name, globalMem->hunters[enemyIdx].level);
            }
            
            semop(semid, &sem_unlock, 1);            
        }
        
        else if(opt == 4) {
            // Tampilkan statistik diri sendiri
            semop(semid, &sem_lock, 1);
            for(int i = 0; i < MAX_HUNTERS; i++) {
                if(globalMem->hunters[i].active && globalMem->hunters[i].id == userId) {
                    struct Hunter *h = &globalMem->hunters[i];
                    printf("ID: %d\nNama: %s\nLevel: %d\nExp: %d\nATK: %d\nHP: %d\nDEF: %d\n",
                           h->id, h->name, h->level, h->exp, h->atk, h->hp, h->def);
                    break;
                }
            }
            semop(semid, &sem_unlock, 1);
        }
        else if(opt == 6) {
            // Reset stat tertentu
            printf("Stat yang ingin di-reset:\n");
            printf("1. ATK\n");
            printf("2. HP\n");
            printf("3. DEF\n");
            printf("Pilihan: ");
            int statChoice;
            scanf("%d", &statChoice);
        
            semop(semid, &sem_lock, 1);
            for(int i = 0; i < MAX_HUNTERS; i++) {
                if(globalMem->hunters[i].active && globalMem->hunters[i].id == userId) {
                    if(statChoice == 1) {
                        globalMem->hunters[i].atk = globalMem->hunters[i].init_atk;
                        printf("ATK direset ke %d\n", globalMem->hunters[i].atk);
                    }
                    else if(statChoice == 2) {
                        globalMem->hunters[i].hp = globalMem->hunters[i].init_hp;
                        printf("HP direset ke %d\n", globalMem->hunters[i].hp);
                    }
                    else if(statChoice == 3) {
                        globalMem->hunters[i].def = globalMem->hunters[i].init_def;
                        printf("DEF direset ke %d\n", globalMem->hunters[i].def);
                    }
                    else {
                        printf("Pilihan tidak valid.\n");
                    }
                    break;
                }
            }
            semop(semid, &sem_unlock, 1);
        }
        
        else {
            printf("Pilihan tidak valid.\n");
        }
    }

    return 0;
}
