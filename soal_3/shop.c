#include <stdio.h>
#include <string.h>

#define TOTAL_WEAPONS 5

typedef struct {
    char name[30];
    int price;
    int damage;
    char passive[50];
} Weapon;

Weapon weapons[TOTAL_WEAPONS] = {
    {"Sapu Terbang", 100, 15, "Dodge Attack"},
    {"Ikan Lele Sakti", 120, 18, "Lifesteal"},
    {"Sendal Swallow Plus", 150, 20, "Double Attack"},
    {"Teh Pucuk Bersoda", 110, 17, "Full Heal Chance"},
    {"Guling Sakti", 90, 13, "Enemy Sleep"}
};

void tampilkan_toko() {
    printf("\n=== Weapon Shop ===\n");
    for (int i = 0; i < TOTAL_WEAPONS; i++) {
        printf("[%d] %s - %d Gold | Damage: %d | Passive: %s\n", i+1, weapons[i].name, weapons[i].price, weapons[i].damage, weapons[i].passive);
    }
    printf("===================\n");
}

Weapon ambil_senjata(int idx) {
    if (idx < 0 || idx >= TOTAL_WEAPONS) {
        Weapon kosong = {"", 0, 0, ""};
        return kosong;
    }
    return weapons[idx];
}

