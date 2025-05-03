// shm_common.c

#include "shm_common.h"
#include <sys/ipc.h>

key_t get_system_key() {
    return ftok("assets/shm_common.h", 1);  // Pastikan path sesuai
}
