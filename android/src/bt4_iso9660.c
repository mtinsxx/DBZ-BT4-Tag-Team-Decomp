#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "bt4_iso9660.h"
#include "bt4_iso_source.h"
#include <sys/types.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define SECTOR 2048u
#define MAX_DIRECTORY (16u * 1024u * 1024u)
_Static_assert(sizeof(Bt4IsoFile) == 32, "PS2 sceCdlFILE layout");

static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static uint32_t be32(const uint8_t *p) {
    return (uint32_t)p[3] | (uint32_t)p[2] << 8 | (uint32_t)p[1] << 16 | (uint32_t)p[0] << 24;
}
static int read_at(FILE *iso, uint64_t offset, void *out, size_t size, uint64_t length) {
    if (offset > length || size > length - offset || offset > INT64_MAX) return -1;
    if (fseeko(iso, (off_t)offset, SEEK_SET) != 0 || fread(out, 1, size, iso) != size) return -1;
    return 0;
}
static int record(const uint8_t *r, size_t available, Bt4IsoFile *out, uint64_t length) {
    size_t n;
    uint64_t offset;
    if (available < 34 || r[0] < 34 || r[0] > available || r[32] > r[0] - 33 ||
        r[1] != 0 || (r[25] & 0x80) != 0) return -1; /* no extended/multi-extent records */
    out->sector = le32(r + 2);
    out->size = le32(r + 10);
    if (out->sector != be32(r + 6) || out->size != be32(r + 14)) return -1;
    offset = (uint64_t)out->sector * SECTOR;
    if (offset > length || out->size > length - offset) return -1;
    memset(out->name, 0, sizeof(out->name));
    n = r[32] < sizeof(out->name) - 1 ? r[32] : sizeof(out->name) - 1;
    memcpy(out->name, r + 33, n);
    memcpy(out->date, r + 18, 7);
    out->date[7] = 0;
    return 0;
}
static int match(const uint8_t *name, unsigned size, const char *want) {
    unsigned i = 0;
    if (strchr(want, ';')) {
        if (strlen(want) != size) return 0;
        for (; i < size; ++i) if (toupper(name[i]) != (unsigned char)want[i]) return 0;
        return 1;
    }
    while (i < size && name[i] != ';' && want[i] != '\0') {
        if (toupper(name[i]) != (unsigned char)want[i]) return 0;
        ++i;
    }
    return want[i] == '\0' && (i == size || name[i] == ';');
}

int Bt4Iso_MatchesData(FILE *iso,const char *path,const void *data,size_t size) {
    Bt4IsoFile file;
    unsigned char buffer[32768];
    const unsigned char *expected=data;
    size_t offset=0;
    int found;
    if (!iso || (!data && size)) return -1;
    found=Bt4Iso_Find(iso,path,&file);
    if (found!=1) return found;
    if (file.size!=size) return 0;
    if (fseeko(iso,(off_t)((uint64_t)file.sector*SECTOR),SEEK_SET)) return -1;
    while (offset<size) {
        size_t n=size-offset<sizeof(buffer)?size-offset:sizeof(buffer);
        if (fread(buffer,1,n,iso)!=n) return -1;
        if (memcmp(buffer,expected+offset,n)) return 0;
        offset+=n;
    }
    return 1;
}

int Bt4Iso_Find(FILE *iso, const char *path, Bt4IsoFile *file) {
    uint8_t sector[SECTOR];
    char normalized[256];
    size_t n = 0;
    off_t end;
    Bt4IsoFile directory;
    char *component;
    if (!iso || !path || !file) return 0;
    if (strncmp(path, "cdrom0:", 7) == 0 || strncmp(path, "cdrom:", 6) == 0)
        path = strchr(path, ':') + 1;
    while (*path == '/' || *path == '\\') ++path;
    for (; *path; ++path) {
        unsigned char c = (unsigned char)*path;
        if (n + 1 >= sizeof(normalized) || c < 32 || c == ':') return 0;
        normalized[n++] = c == '\\' ? '/' : (char)toupper(c);
    }
    normalized[n] = '\0';
    if (n == 0 || normalized[n-1] == '/') return 0;
    if (fseeko(iso, 0, SEEK_END) != 0 || (end = ftello(iso)) < 0) return -1;
    if (read_at(iso, 16u * SECTOR, sector, sizeof(sector), (uint64_t)end) != 0 ||
        sector[0] != 1 || memcmp(sector + 1, "CD001", 5) || sector[6] != 1 ||
        !(sector[156 + 25] & 2) || record(sector + 156, SECTOR - 156, &directory, (uint64_t)end)) return -1;
    component = normalized;
    for (;;) {
        char *slash = strchr(component, '/');
        uint32_t position = 0;
        int found = 0;
        Bt4IsoFile next;
        uint8_t flags = 0;
        if (slash) *slash = '\0';
        if (!*component || !strcmp(component, ".") || !strcmp(component, "..")) return 0;
        if (directory.size > MAX_DIRECTORY) return -1;
        while (position < directory.size) {
            uint32_t within = position % SECTOR;
            unsigned length;
            if (within == 0 && read_at(iso, (uint64_t)directory.sector * SECTOR + position,
                                      sector, SECTOR, (uint64_t)end)) return -1;
            length = sector[within];
            if (length == 0) { position += SECTOR - within; continue; }
            if (length > SECTOR - within || length > directory.size - position || length < 34) return -1;
            if (record(sector + within, length, &next, (uint64_t)end)) return -1;
            if (match(sector + within + 33, sector[within + 32], component)) {
                flags = sector[within + 25];
                found = 1;
                break;
            }
            position += length;
        }
        if (!found) return 0;
        if (!slash) {
            if (flags & 2) return 0;
            *file = next;
            return 1;
        }
        if (!(flags & 2)) return 0;
        directory = next;
        component = slash + 1;
    }
}

#if defined(__ANDROID__) || defined(BT4_ANDROID_ABI_TEST)
/* Actual SDK metadata, including the original ISO sector, rather than a
 * fabricated disc-present result. The application supplies its retained ISO
 * path only after validating the compatible BT4 profile. */
int sceCdSearchFile(void *out, char *name) {
    FILE *iso;
    Bt4IsoFile file;
    int result;
    if (!out || !(iso = Bt4Iso_Open())) return 0;
    result = Bt4Iso_Find(iso, name, &file);
    fclose(iso);
    if (result != 1) return 0;
    memcpy(out, &file, sizeof(file));
    return 1;
}
#endif
