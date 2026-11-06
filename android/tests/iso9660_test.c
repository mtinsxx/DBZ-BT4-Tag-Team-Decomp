#define _POSIX_C_SOURCE 200809L
#include "bt4_iso9660.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern int sceCdSearchFile(void *, char *);
static void both32(unsigned char *p, uint32_t n) {
    unsigned i;
    for (i = 0; i < 4; ++i) { p[i] = (unsigned char)(n >> (i*8)); p[i+4] = (unsigned char)(n >> ((3-i)*8)); }
}
static unsigned entry(unsigned char *p, uint32_t sector, uint32_t size, int directory, const char *name, unsigned n) {
    unsigned bytes = 33 + n + (n % 2 == 0);
    memset(p, 0, bytes);
    p[0] = (unsigned char)bytes;
    both32(p+2, sector); both32(p+10, size);
    p[18] = 126; p[19] = 11; p[20] = 5;
    p[25] = directory ? 2 : 0;
    p[28] = p[31] = 1;
    p[32] = (unsigned char)n;
    memcpy(p+33, name, n);
    return bytes;
}
static void sector_write(FILE *iso, unsigned sector, const unsigned char *data) {
    assert(fseeko(iso, (off_t)sector * 2048, SEEK_SET) == 0);
    assert(fwrite(data, 1, 2048, iso) == 2048);
    assert(fflush(iso) == 0);
}
int main(void) {
    char path[] = "/tmp/bt4-iso-test-XXXXXX";
    int fd = mkstemp(path);
    unsigned char pvd[2048] = {0}, root[2048] = {0}, child[2048] = {0}, zeros[2048] = {0};
    unsigned n;
    FILE *iso;
    Bt4IsoFile file;
    assert(fd >= 0 && (iso = fdopen(fd, "w+b")) != NULL);
    sector_write(iso, 29, zeros);
    pvd[0] = 1; memcpy(pvd+1, "CD001", 5); pvd[6] = 1;
    entry(pvd+156, 20, 2048, 1, "", 1);
    n = entry(root, 22, 8, 0, "SLUS_219.78;1", 13);
    entry(root+n, 21, 2048, 1, "BIN", 3);
    entry(child, 23, 16, 0, "DBZP.BIN;1", 10);
    sector_write(iso, 16, pvd); sector_write(iso, 20, root); sector_write(iso, 21, child);
    assert(Bt4Iso_Find(iso, "cdrom0:\\SLUS_219.78;1", &file) == 1 && file.sector == 22 && file.size == 8);
    assert(file.date[0] == 126 && !strcmp(file.name, "SLUS_219.78;1"));
    assert(Bt4Iso_Find(iso, "/bin/dbzp.bin", &file) == 1 && file.sector == 23 && file.size == 16);
    assert(Bt4Iso_Find(iso, "/SLUS_219.78;2", &file) == 0);
    assert(Bt4Iso_Find(iso, "/MISSING.BIN", &file) == 0);
    assert(Bt4Iso_Find(iso, "/../SLUS_219.78", &file) == 0);
    assert(Bt4Iso_Find(iso, "/BIN", &file) == 0);
    assert(Bt4Iso_MatchesData(iso,"SLUS_219.78",zeros,8)==1);
    assert(Bt4Iso_MatchesData(iso,"SLUS_219.78",zeros,9)==0);
    zeros[0]=1;assert(Bt4Iso_MatchesData(iso,"SLUS_219.78",zeros,8)==0);zeros[0]=0;
    assert(Bt4Iso_MatchesData(iso,"MISSING.BIN",zeros,8)==0);
    /* Compare real sparse-file data beyond 6 GiB without truncating LBA math. */
    sector_write(iso,3u*1024*1024,zeros);
    entry(root,3u*1024*1024,8,0,"SLUS_219.78;1",13);sector_write(iso,20,root);
    assert(Bt4Iso_MatchesData(iso,"SLUS_219.78",zeros,8)==1);
    entry(root,22,8,0,"SLUS_219.78;1",13);sector_write(iso,20,root);
    assert(setenv("BT4_ISO", path, 1) == 0);
    assert(sceCdSearchFile(&file, "\\SLUS_219.78;1") == 1 && file.sector == 22);
    /* Reject mismatched endian copies and extents outside the input. */
    pvd[156+6] = 0xff; sector_write(iso, 16, pvd);
    assert(Bt4Iso_Find(iso, "SLUS_219.78", &file) == -1);
    assert(Bt4Iso_MatchesData(iso,"SLUS_219.78",zeros,8)==-1);
    entry(pvd+156, 20, 2048, 1, "", 1); sector_write(iso, 16, pvd);
    entry(root, UINT32_MAX, 8, 0, "SLUS_219.78;1", 13); sector_write(iso, 20, root);
    assert(Bt4Iso_Find(iso, "SLUS_219.78", &file) == -1);
    unsetenv("BT4_ISO");
    fclose(iso); unlink(path);
    return 0;
}
