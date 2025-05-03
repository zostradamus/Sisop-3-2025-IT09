#ifndef SHM_COMMON_H
#define SHM_COMMON_H

// Jika kamu mau pakai MAX_HUNTERS/MAX_DUNGEONS dari header,
// aktifkan salah satu baris ini (atau biarkan saja, karena
// kamu mendefinisikannya di .c):
// #ifndef MAX_HUNTERS
// #define MAX_HUNTERS 100
// #endif
//
// #ifndef MAX_DUNGEONS
// #define MAX_DUNGEONS 100
// #endif

// --- Forward declarations saja: ---
typedef struct Hunter Hunter;
typedef struct Dungeon Dungeon;

// (Di sini kamu bisa declare shared-memory keys/structs lain,
// fungsi-fungsi utility, dsb., tapi *jangan* ulangi isi struct)

#endif // SHM_COMMON_H
