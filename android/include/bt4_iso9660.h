#ifndef BT4_ISO9660_H
#define BT4_ISO9660_H
#include <stdint.h>
#include <stdio.h>
typedef struct Bt4IsoFile {
    uint32_t sector;
    uint32_t size;
    char name[16];
    uint8_t date[8];
} Bt4IsoFile;
/* 1 found, 0 absent/invalid path, -1 malformed or unreadable ISO.
 * Metadata lookup only; compatible BT4 profile validation is still required. */
int Bt4Iso_Find(FILE *iso, const char *path, Bt4IsoFile *file);
#endif
