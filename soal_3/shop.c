#include <stdio.h>
#include <string.h>

#define WEAPON_COUNT 10

typedef struct {
    char name[30];
    int price;
    int damage;
    char passive[50];
} Weapon;

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

void display_shop() {
    printf("\n");
    printf("     ╔════════════════════════════════════════════════╗\n");
    printf("     ║            🛒  CRAZY WARRIOR MART  🛒          ║\n");
    printf("     ╚════════════════════════════════════════════════╝\n\n");
    for (int i = 0; i < WEAPON_COUNT; i++) {
        printf("[%d] %s - %d Coins | Damage: %d | Passive: %s\n", 
              i+1, armory[i].name, armory[i].price, 
              armory[i].damage, armory[i].passive);
    }
    printf("=========================================================================\n");
}

Weapon get_weapon(int index) {
    if (index < 0 || index >= WEAPON_COUNT) {
        Weapon empty = {"", 0, 0, ""};
        return empty;
    }
    return armory[index];
}
