#define _POSIX_C_SOURCE 200809L
#include "bt4_iso9660.h"
#include "bt4_iso_source.h"
#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "port_read_file.h"
/* Host fixtures supply presentation/mod services; all source reads below use
   the actual runtime file cache and ISO/AFS implementation. */
int PortStages_Alias(const char *r,char *o,unsigned n) { (void)r;(void)o;(void)n;return 0; }
int PortSongs_Alias(const char *r,char *o,unsigned n) { (void)r;(void)o;(void)n;return 0; }
const char *Port_FileRoot(void) { return "/__tenkaichi_no_extracted_fixture__"; }
void Port_StateTouch(void *p,unsigned long n) { (void)p;(void)n; }
int Port_StageFits(const char *r,long size) { (void)r;(void)size;return 1; }
int Port_StageBufSize(void) { return 0; }
extern long Port_FileSize(const char *);
extern size_t Port_FileReadAt(const char *,long,void *,size_t);

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
    unsigned char pvd[2048] = {0}, root[2048] = {0}, child[2048] = {0}, zeros[2048] = {0},data[2048]={0},afs[2048]={0};
    unsigned n;
    FILE *iso;
    Bt4IsoFile file;
    assert(fd >= 0 && (iso = fdopen(fd, "w+b")) != NULL);
    sector_write(iso, 29, zeros);
    pvd[0] = 1; memcpy(pvd+1, "CD001", 5); pvd[6] = 1;
    entry(pvd+156, 20, 2048, 1, "", 1);
    n = entry(root, 22, 8, 0, "SLUS_219.78;1", 13);
    n+=entry(root+n, 21, 2048, 1, "BIN", 3);
    entry(root+n,24,2048,1,"DATA",4);
    entry(child, 23, 16, 0, "DBZP.BIN;1", 10);
    entry(data,25,2048,0,"PZS4US0.AFS;1",13);
    memcpy(afs,"AFS\0",4);afs[4]=3;afs[8]=128;afs[12]=3;afs[17]=1;afs[20]=5;
    memcpy(afs+128,"abc",3);memcpy(afs+256,"HELLO",5);
    sector_write(iso,24,data);sector_write(iso,25,afs);
    sector_write(iso, 16, pvd); sector_write(iso, 20, root); sector_write(iso, 21, child);
    assert(Bt4Iso_Find(iso, "cdrom0:\\SLUS_219.78;1", &file) == 1 && file.sector == 22 && file.size == 8);
    assert(file.date[0] == 126 && !strcmp(file.name, "SLUS_219.78;1"));
    assert(Bt4Iso_Find(iso, "/bin/dbzp.bin", &file) == 1 && file.sector == 23 && file.size == 16);
    assert(Bt4Iso_Find(iso, "/SLUS_219.78;2", &file) == 0);
    assert(Bt4Iso_Find(iso, "/MISSING.BIN", &file) == 0);
    assert(Bt4Iso_Find(iso, "/../SLUS_219.78", &file) == 0);
    assert(Bt4Iso_Find(iso, "/BIN", &file) == 0);
    {
        char bytes[32]={0};int source=dup(fileno(iso)),pipes[2];FILE *member,*other,*raw,*slice;
        assert(source>=0 && Bt4Iso_MountFd(source,0,-1)==0);close(source);
        member=Bt4Iso_OpenFile("pzs4us0/00001.bin");other=Bt4Iso_OpenFile("pzs4us0/00000.bin");
        assert(member && other);
        FILE *empty=Bt4Iso_OpenFile("pzs4us0/00002.bin");
        assert(empty && fread(bytes,1,32,empty)==0 && !ferror(empty));fclose(empty);
        assert(Port_FileSize("pzs4us0/00001.bin")==5);
        assert(Port_FileReadAt("pzs4us0/00001.bin",3,bytes,30)==2 && !memcmp(bytes,"LO",2));
        FILE *media=Port_ReadFile("iso://pzs4us0/00001.bin");
        assert(media && fread(bytes,1,32,media)==5 && !memcmp(bytes,"HELLO",5));fclose(media);
        assert(fread(bytes,1,sizeof(bytes),member)==5 && !memcmp(bytes,"HELLO",5));
        assert(fread(bytes,1,sizeof(bytes),other)==3 && !memcmp(bytes,"abc",3));
        assert(!fseek(member,-2,SEEK_END) && fread(bytes,1,2,member)==2 && !memcmp(bytes,"LO",2));
        assert(!fseek(member,1,SEEK_END) && fread(bytes,1,1,member)==0);
        assert(!Bt4Iso_OpenFile("pzs4us0/-1.bin") && !Bt4Iso_OpenFile("pzs4us0/42949672960.bin"));
        assert(!Bt4Iso_OpenFile("pzs4us0/00003.bin") && !Bt4Iso_OpenFile("disc/../DATA/PZS4US0.AFS"));
        assert(!pipe(pipes));assert(Bt4Iso_MountFd(pipes[0],0,100)<0 && errno==ESPIPE);close(pipes[0]);close(pipes[1]);
        assert(Bt4Iso_IsMounted());
        Bt4Iso_Unmount();assert(!Bt4Iso_IsMounted());
        assert(!fseek(member,0,SEEK_SET) && fread(bytes,1,5,member)==5 && !memcmp(bytes,"HELLO",5));
        fclose(member);fclose(other);
        FILE *asset=tmpfile();assert(asset);unsigned char prefix[123]={0};
        assert(fwrite(prefix,1,sizeof(prefix),asset)==sizeof(prefix));
        assert(!fseek(iso,0,SEEK_SET));size_t got;
        while((got=fread(zeros,1,sizeof(zeros),iso))>0)assert(fwrite(zeros,1,got,asset)==got);
        assert(!fflush(asset));long asset_size=ftell(asset);
        assert(!Bt4Iso_MountFd(fileno(asset),123,asset_size-123));
        raw=Bt4Iso_Open();assert(raw && Bt4Iso_Find(raw,"SLUS_219.78",&file)==1 && file.size==8);fclose(raw);
        slice=Bt4Iso_OpenFile("pzs4us0/00001.bin");assert(slice && fread(bytes,1,32,slice)==5 && !memcmp(bytes,"HELLO",5));fclose(slice);
        raw=Bt4Iso_Open();assert(raw);
        assert(!ftruncate(fileno(asset),123));
        assert(fread(bytes,1,1,raw)==0 && ferror(raw));fclose(raw);
        Bt4Iso_Unmount();fclose(asset);memset(zeros,0,sizeof(zeros));
        afs[21]=255;sector_write(iso,25,afs);
        assert(!Bt4Iso_MountFd(fileno(iso),0,-1));
        assert(!Bt4Iso_OpenFile("pzs4us0/00001.bin") && errno==EIO);Bt4Iso_Unmount();
        afs[21]=0;sector_write(iso,25,afs);
    }
    assert(Bt4Iso_MatchesData(iso,"SLUS_219.78",zeros,8)==1);
    assert(Bt4Iso_MatchesData(iso,"SLUS_219.78",zeros,9)==0);
    zeros[0]=1;assert(Bt4Iso_MatchesData(iso,"SLUS_219.78",zeros,8)==0);zeros[0]=0;
    assert(Bt4Iso_MatchesData(iso,"MISSING.BIN",zeros,8)==0);
    /* Compare real sparse-file data beyond 6 GiB without truncating LBA math. */
    sector_write(iso,3u*1024*1024,zeros);
    entry(root,3u*1024*1024,8,0,"SLUS_219.78;1",13);sector_write(iso,20,root);
    assert(Bt4Iso_MatchesData(iso,"SLUS_219.78",zeros,8)==1);
    assert(!Bt4Iso_MountFd(fileno(iso),0,-1));
    FILE *large=Bt4Iso_OpenFile("disc/SLUS_219.78");
    assert(large && fread(afs,1,8,large)==8 && !memcmp(afs,zeros,8));fclose(large);Bt4Iso_Unmount();
    entry(root,22,8,0,"SLUS_219.78;1",13);sector_write(iso,20,root);
    assert(setenv("BT4_ISO", path, 1) == 0);
    assert(sceCdSearchFile(&file, "\\SLUS_219.78;1") == 1 && file.sector == 22);
    /* Reject mismatched endian copies and extents outside the input. */
    pvd[156+6] = 0xff; sector_write(iso, 16, pvd);
    assert(Bt4Iso_Find(iso, "SLUS_219.78", &file) == -1);
    assert(Bt4Iso_MatchesData(iso,"SLUS_219.78",zeros,8)==-1);
    assert(!Bt4Iso_MountFd(fileno(iso),0,-1));
    assert(!Bt4Iso_OpenFile("disc/SLUS_219.78") && errno==EIO);Bt4Iso_Unmount();
    entry(pvd+156, 20, 2048, 1, "", 1); sector_write(iso, 16, pvd);
    entry(root, UINT32_MAX, 8, 0, "SLUS_219.78;1", 13); sector_write(iso, 20, root);
    assert(Bt4Iso_Find(iso, "SLUS_219.78", &file) == -1);
    unsetenv("BT4_ISO");
    fclose(iso); unlink(path);
    return 0;
}
