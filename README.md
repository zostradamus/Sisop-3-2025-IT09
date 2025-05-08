# LAPORAN RESMI SOAL MODUL 3 SISOP

## ANGGOTA KELOMPOK
| Nama                           | NRP        |
| -------------------------------| ---------- |
| Shinta Alya Ramadani           | 5027241016 |
| Prabaswara Febrian Winandika   | 5027241069 |
| Muhammad Farrel Rafli Al Fasya | 5027241075 |

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

.
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
