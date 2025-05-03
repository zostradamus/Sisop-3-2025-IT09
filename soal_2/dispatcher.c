#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pwd.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define SHM_KEY 1234
#define ORDER_LIMIT 100

typedef struct {
    char customer[64];
    char destination[128];
    char category[16];
    int is_delivered;
    char handled_by[64];
} Package;

Package *shared_orders;

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

void show_all_orders() {
    printf("List of orders:\n");
    for (int i = 0; i < ORDER_LIMIT; i++) {
        if (strlen(shared_orders[i].customer) > 0) {
            printf("%s - %s\n", shared_orders[i].customer, shared_orders[i].is_delivered ? "Delivered" : "Pending");
        }
    }
}

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
