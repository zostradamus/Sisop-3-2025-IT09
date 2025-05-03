#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>

#define RED     "\033[38;5;196m"
#define GREEN   "\033[38;5;46m"
#define YELLOW  "\033[38;5;226m"
#define BLUE    "\033[38;5;81m"
#define PURPLE  "\033[38;5;135m"
#define RESET   "\033[0m"

#define GAME_PORT 8080
#define BUFFER_SIZE 1024
#define WEAPON_COUNT 10

typedef struct {
    char name[30];
    int price;
    int damage;
    char passive[50];
} Weapon;

extern Weapon armory[WEAPON_COUNT];
extern void display_shop();
extern Weapon get_weapon(int index);

typedef struct {
    char name[50];
    int coins;
    Weapon inventory[10];
    int item_count;
    Weapon equipped;
    int foes_defeated;
} Adventurer;

Adventurer player = {.coins = 300, .item_count = 0, .foes_defeated = 0};

void start_combat();
void show_status();
void view_inventory();
void visit_shop();
void display_hp_bar(int current_hp, int max_hp);

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
            printf("Invalid selection! Please choose between 1-%d or 0 to cancel.\n", player.item_count);
        } else {
            valid_input = 1;
            player.equipped = player.inventory[selection-1];
            printf("%s is now equipped!\n", player.equipped.name);
        }

    } while (!valid_input);
}

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
            printf("Invalid selection! Please choose between 1-%d or 0 to cancel.\n", WEAPON_COUNT);
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

void display_hp_bar(int current_hp, int max_hp) {
    const char* enemy_type = "👾";

    printf("\033[3F");
    printf("\033[0J");
    fflush(stdout);

    float hp_percent = (float)current_hp / max_hp;
    const char* hp_color = hp_percent > 0.6 ? GREEN :
                         hp_percent > 0.3 ? YELLOW : RED;

    // No color on \n
    printf("\n");
    printf("%s╔════════════════════════════╗%s\n", hp_color, RESET);
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
