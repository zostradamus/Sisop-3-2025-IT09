#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include "shop.c"

#define WARNA_MERAH     "\033[1;31m"
#define WARNA_HIJAU     "\033[1;42m"
#define WARNA_KUNING    "\033[1;33m"
#define WARNA_BIRU      "\033[1;36m"
#define WARNA_UNGU      "\033[1;35m"
#define RESET_WARNA     "\033[0m"

#define PORT 9000
#define UKURAN_BUFFER 1024

typedef struct {
    int gold;
    Weapon tas[10];
    int jumlah_item;
    Weapon aktif;
    int musuh_dikalahkan;
} Pemain;

Pemain hero = {.gold = 300, .jumlah_item = 0, .musuh_dikalahkan = 0};

void mulai_pertarungan();
void tampilkan_status();
void lihat_tas();
void menu_toko();
void tampilkan_bar_hp(int hp, int maks_hp);

int main() {
    int pilihan;
        printf("\n\033[5;36m╔══════════════════════════════════════════════════════════╗\033[0m\n");
        printf("\033[5;35m║           🌟 SELAMAT DATANG DI LOST DUNGEON 🌟           ║\033[0m\n");
        printf("\033[5;33m║   ⚔️ Tempat di mana keberanian diuji & emas dicari! ⚔️   ║\033[0m\n");
        printf("\033[5;36m╚══════════════════════════════════════════════════════════╝\033[0m\n\n");
    usleep(300000);

    while (1) {
        printf("\n%s\n", WARNA_BIRU "📜 MENU UTAMA" RESET_WARNA);
        printf("1. Lihat Status Pemain\n");
        printf("2. Cek Tas\n");
        printf("3. Kunjungi Toko\n");
        printf("4. Masuk Mode Pertarungan\n");
        printf("5. Keluar\nPilih (1-5) : ");
        scanf("%d", &pilihan);
        getchar();
        switch (pilihan) {
            case 1: tampilkan_status(); break;
            case 2: lihat_tas(); break;
            case 3: menu_toko(); break;
            case 4: mulai_pertarungan(); break;
            case 5:
                printf("\n%s\n", WARNA_UNGU "🌙 Terima kasih telah bermain di Lost Dungeon! Sampai jumpa petualang~" RESET_WARNA);
                exit(0);
            default: printf("Pilihan tidak tersedia.\n");
        }
    }
}

void tampilkan_status() {
    printf("\n== Status Pemain ==\n");
    printf("Gold: %d\n", hero.gold);
    printf("Senjata: %s\n", strlen(hero.aktif.name) ? hero.aktif.name : "Belum Dilengkapi");
    printf("Damage Dasar: %d\n", hero.aktif.damage);
    if (strlen(hero.aktif.passive) && strcmp(hero.aktif.passive, "-") != 0)
        printf("Kemampuan Pasif: %s\n", hero.aktif.passive);
    printf("Total Musuh Dikalahkan: %d\n", hero.musuh_dikalahkan);
}

void lihat_tas() {
    printf("\n%s\n", WARNA_KUNING "🎒 ISI TAS" RESET_WARNA);
    if (hero.jumlah_item == 0) {
        printf("Tas kamu kosong... ayo belanja di toko dulu! 🛍️\n");
        return;
    }
    for (int i = 0; i < hero.jumlah_item; i++) {
    	int aktif = strcmp(hero.tas[i].name, hero.aktif.name) == 0;
    	printf("%d. %s | Damage: %d | Pasif: %s%s\n", i+1, hero.tas[i].name, hero.tas[i].damage, hero.tas[i].passive, aktif ? " (TERPAKAI)" : "");
    }
    printf("Pilih nomor senjata untuk digunakan (0 untuk batal): ");
    int pilih;
    scanf("%d", &pilih);
    if (pilih > 0 && pilih <= hero.jumlah_item) {
        hero.aktif = hero.tas[pilih-1];
        printf("%s sekarang dipakai!\n", hero.aktif.name);
    }
}

void menu_toko() {
    tampilkan_toko();
    printf("Pilih nomor senjata yang ingin dibeli (0 untuk batal): ");
    int pilihan;
    scanf("%d", &pilihan);
    pilihan--;
    if (pilihan >= 0 && pilihan < 5) {
        Weapon w = ambil_senjata(pilihan);
        if (hero.gold >= w.price) {
            hero.tas[hero.jumlah_item++] = w;
            hero.gold -= w.price;
            printf("Berhasil membeli %s!\n", w.name);
  	    } else {
            printf("Gold tidak mencukupi!\n");
        }
    }
}

void tampilkan_bar_hp(int hp, int maks_hp) {
    int panjang = 25;
    int isi = (hp * panjang) / maks_hp;

    printf("%s👾 MUSUH MUNCUL DARI KEGELAPAN!%s", WARNA_MERAH, RESET_WARNA);
    printf("❤️  [");

    for (int i = 0; i < panjang; i++) {
        float ratio = (float)i / panjang;
        if (i < isi) {
            if (ratio < 0.33)
                printf(WARNA_MERAH "░" RESET_WARNA);
            else if (ratio < 0.66)
                printf(WARNA_KUNING "▓" RESET_WARNA);
            else
                printf(WARNA_HIJAU "█" RESET_WARNA);
        } else {
            printf(" ");
        }
    }

    printf("] %d/%d", hp, maks_hp);
}

void mulai_pertarungan() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Gagal terhubung ke server");
        return;
    }

    srand(time(0));
    char aksi[10];

    while (1) {
        send(sock, "BATTLE", strlen("BATTLE"), 0);
        char buffer[UKURAN_BUFFER] = {0};
        read(sock, buffer, UKURAN_BUFFER);

        int nyawa_musuh, hadiah;
        sscanf(buffer, "ENEMY %d %d", &nyawa_musuh, &hadiah);
        int nyawa_max = nyawa_musuh;

        printf("\nMusuh Muncul!\n");
        tampilkan_bar_hp(nyawa_musuh, nyawa_max);

        while (nyawa_musuh > 0) {
            printf("\nKetik 'attack' untuk menyerang atau 'exit' untuk mundur: ");
            scanf("%s", aksi);

            if (strcmp(aksi, "attack") == 0) {
                int serang = hero.aktif.damage + (rand() % 5);
                int kritikal = rand() % 100 < 20 ? 1 : 0;
                if (kritikal) serang *= 2;
                nyawa_musuh -= serang;
                if (nyawa_musuh < 0) nyawa_musuh = 0;

                printf("Seranganmu menyebabkan " WARNA_MERAH "%d damage" RESET_WARNA " %s\n", serang, kritikal ? "(Critical!)" : "");
                if (strlen(hero.aktif.passive) > 1 && strcmp(hero.aktif.passive, "-") != 0)
                    printf("Efek Pasif Aktif: %s\n", hero.aktif.passive);

                if (nyawa_musuh <= 0) {
                    printf("\n=== \033[1;35mHADIAH\033[0m ===\n");
                    printf("Kamu mendapatkan \033[1;33m%d gold!\033[0m\n", hadiah);
                    hero.gold += hadiah;
                    hero.musuh_dikalahkan++;
                    break;
                } else {
                    tampilkan_bar_hp(nyawa_musuh, nyawa_max);
                }
            } else if (strcmp(aksi, "exit") == 0) {
                printf("Kamu mundur dari pertarungan.\n");
                close(sock);
                return;
            } else {
                printf("Perintah tidak dikenal.\n");
            }
        }

        printf("\n=== \033[1;36m⚔️  MUSUH BARU MUNCUL DARI BAYANGAN! ⚔️\033[0m ===\n");
    }

    close(sock);
}
