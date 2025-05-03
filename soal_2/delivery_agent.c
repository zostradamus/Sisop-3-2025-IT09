#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
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
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

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
