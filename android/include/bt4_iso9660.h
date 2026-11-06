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
/* Compare an ISO file's exact bytes against an independently verified program.
 * 1 match, 0 missing/different, -1 malformed image or read error. */
int Bt4Iso_MatchesData(FILE *iso, const char *path, const void *data, size_t size);
#endif
