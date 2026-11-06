#define _POSIX_C_SOURCE 200809L
#include "bt4_sdk_ci_query.h"
#include "bt4_sdk_file_fields.h"
#include "bt4_sdk_fields.h"
#include "bt4_guest_abi.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Synthetic imported configuration/resources, not copied executable data. */
uint32_t D_002D4CD0[4], D_002D4E78[4];
char D_002D4CF0[32]="cdrom0:", D_002D4F98[32]="host:", D_002D4E90[256]="DATA";
const char D_002F6090[]="\\", D_002F6098[]=";1", D_002F6160[]="<disc>";
const char D_002F6248[]="host", D_002F6250[]="/";
const char D_002F5EA8[]="dvd-null", D_002F5F00[]="dvd-missing";
const char D_002F8428[]="host-null", D_002F8478[]="host-open-failed";
static uint32_t override_data[64];
static unsigned errors;
static const char *expected_message;
static char root[256], small_path[512];

/* Fixtures for unrelated content services used by the REAL plat_fcache.c.
   Its open/stat/cache/alias precedence code runs unchanged. */
const char *Port_FileRoot(void) { return root; }
int PortStages_Alias(const char *relative, char *out, unsigned capacity) {
    if (strcmp(relative,"DATA/alias.bin")) return 0;
    assert(strlen(small_path)<capacity); strcpy(out,small_path); return 1;
}
int PortSongs_Alias(const char *relative, char *out, unsigned capacity) {
    (void)relative; (void)out; (void)capacity; return 0;
}
int Port_StageFits(const char *relative, long size) { (void)relative; (void)size; return 1; }
int Port_StageBufSize(void) { return 0; }
void Port_StateTouch(void *pointer, unsigned long size) { (void)pointer; (void)size; }

static void error_callback(uint32_t context, const char *message, void *handle) {
    assert(context==0xF1234567u && message==expected_message && handle==NULL);
    ++errors;
}
static void both32(unsigned char *p, uint32_t value) {
    unsigned i; for (i=0;i<4;++i) { p[i]=(unsigned char)(value>>(i*8)); p[i+4]=(unsigned char)(value>>((3-i)*8)); }
}
static void record(unsigned char *p, uint32_t sector, uint32_t size, int directory, const char *name) {
    unsigned n=(unsigned)strlen(name), length=33+n+(n%2==0);
    memset(p,0,length);p[0]=(unsigned char)length;both32(p+2,sector);both32(p+10,size);
    p[25]=directory?2:0;p[28]=p[31]=1;p[32]=(unsigned char)n;memcpy(p+33,name,n);
}
static void write_sector(FILE *iso, unsigned sector, const unsigned char data[2048]) {
    assert(fseeko(iso,(off_t)sector*2048,SEEK_SET)==0 && fwrite(data,1,2048,iso)==2048);
}
static void sparse(const char *path, uint64_t size) {
    FILE *file=fopen(path,"wb"); assert(file);
    assert(ftruncate(fileno(file),(off_t)size)==0 && fclose(file)==0);
}

void test_ci_queries(void) {
    char template[]="/tmp/bt4-ci-query-XXXXXX", path[512], iso_path[512], large_path[512], mods_path[512];
    unsigned char pvd[2048]={0}, directory[2048]={0}, zero[2048]={0};
    FILE *iso;
    char long_name[1500];
    Bt4DvdCiWork dvd={0}; Bt4HostCiWork host={0};
    static const Bt4AbiEntry entries[]={{(void *)error_callback,"vipp"}};
    static Bt4AbiModule module={NULL,entries,1};
    assert(mkdtemp(template));strcpy(root,template);
    unsetenv("BT4_PTBR_ROOT");
    snprintf(path,sizeof(path),"%s/DATA",root);assert(mkdir(path,0700)==0);
    snprintf(small_path,sizeof(small_path),"%s/DATA/small.bin",root);sparse(small_path,53);
    snprintf(large_path,sizeof(large_path),"%s/DATA/large.bin",root);sparse(large_path,UINT64_C(0x180000035));
    snprintf(path,sizeof(path),"%s/DATA/empty.bin",root);sparse(path,0);
    assert(func_0028B850("small.bin")==1 && func_0028B8F0("small.bin")==53);
    assert(func_0028B850("host:DATA\\small.bin")==1 && func_0028B850("Small.bin")==0);
    assert(func_0028B850("empty.bin")==1 && func_0028B8F0("empty.bin")==0);
    assert(func_0028B8F0("alias.bin")==53);
    assert((uint32_t)func_0028B8F0("large.bin")==0x80000035u);
    assert(func_0028C288("large.bin",202)==1 && func_0028C288("large.bin",204)==1);
    assert((uint32_t)func_0028C288("large.bin",203)==0x80000035u &&
           (uint32_t)func_0028C288("large.bin",205)==0x80000035u);
    snprintf(path,sizeof(path),"%s/mods",root);assert(mkdir(path,0700)==0);
    snprintf(path,sizeof(path),"%s/mods/DATA",root);assert(mkdir(path,0700)==0);
    snprintf(mods_path,sizeof(mods_path),"%s/mods/DATA/mod.bin",root);sparse(mods_path,71);
    assert(func_0028B8F0("mod.bin")==71);

    snprintf(iso_path,sizeof(iso_path),"%s/disc.iso",root);iso=fopen(iso_path,"w+b");assert(iso);
    write_sector(iso,29,zero);
    pvd[0]=1;memcpy(pvd+1,"CD001",5);pvd[6]=1;record(pvd+156,20,2048,1,"x");
    record(directory,23,53,0,"TEST.BIN;1");
    write_sector(iso,16,pvd);write_sector(iso,20,directory);assert(fclose(iso)==0);
    assert(setenv("BT4_ISO",iso_path,1)==0);
    assert(func_0027BDD8("test.bin")==1 && dvCiGetFileSize("/Test.Bin")==53);
    assert(func_0027C680("TEST.BIN;1",203)==53 && func_0027C680("test.bin",205)==53);
    assert(func_0027BDD8("missing.bin")==0);
    assert(dvCiGetFileSize("12345678.00000002")==4096 && dvCiGetFileSize(D_002F6160)==-1);

    /* Same recovered table layout, different comparison semantics. */
    memset(override_data,0,sizeof(override_data));
    override_data[0]=55;override_data[1]=0x80000001u;
    override_data[2]=66;override_data[3]=17;
    strcpy((char *)override_data+24,"Dir\\Name.BIN");
    strcpy((char *)override_data+56,"Dir\\Second.BIN");
    D_002D4CD0[0]=D_002D4E78[0]=Bt4Android_EncodePointer(override_data);
    D_002D4CD0[1]=D_002D4E78[1]=2;
    D_002D4CD0[2]=D_002D4E78[2]=3;D_002D4CD0[3]=D_002D4E78[3]=31;
    assert((uint32_t)dvCiGetFileSize("dir/name.bin")==0x80000001u);
    assert((uint32_t)func_0028B8F0("Dir/Name.BIN")==0x80000001u);
    assert(func_0028B850("Dir/Name.BIN")==1 && func_0028B850("dir/name.bin")==0);
    assert(func_0028C288("Dir/Name.BIN",202)==-1); /* original LW sign extension */
    assert(dvCiGetFileSize("dir/second.bin")==17 && func_0028B8F0("Dir/Second.BIN")==17);
    override_data[1]=0; /* a zero-size override falls back to the real provider */
    strcpy((char *)override_data+24,"small.bin");
    assert(func_0028B8F0("small.bin")==53);
    memset(D_002D4CD0,0,sizeof(D_002D4CD0));memset(D_002D4E78,0,sizeof(D_002D4E78));

    dvd.completed_sectors=0x400001;dvd.reserved4=0xF1234567u;
    assert(func_0027C680(&dvd,200)==2 && func_0027C680(&dvd,201)==2048);
    assert(func_0027C680(&dvd,301)==0 && (uint32_t)func_0027C680(&dvd,302)==0xF1234567u);
    assert(func_0027C680("ignored",202)==0 && func_0027C680("ignored",204)==0);
    host.file_size=INT64_C(0x180000035);host.completed_sectors=0x400001;
    assert(func_0028C288(&host,200)==1 && (uint32_t)func_0028C288(&host,201)==0x80000035u);
    assert(func_0028C288(&host,301)==1 && (uint32_t)func_0028C288(&host,302)==0x80000035u);
    host.completed_sectors=1;assert(func_0028C288(&host,200)==0 && func_0028C288(&host,201)==2048);
    dvd.completed_sectors=-1;host.completed_sectors=-1;
    assert(func_0027C680(&dvd,200)==-1 && func_0027C680(&dvd,201)==-2048);
    assert(func_0028C288(&host,200)==1 && (uint32_t)func_0028C288(&host,201)==0x80000035u);
    assert(func_0027C680(&dvd,299)==1 && func_0028C288(&host,299)==1);
    assert(func_0027C680(NULL,299)==0 && func_0028C288(NULL,299)==0);
    assert(func_0027C680(&dvd,401)==0 && func_0028C288(&host,-1)==0);

    Bt4Android_RegisterSignatures(&module);
    func_0027BD48(Bt4Android_EncodePointer((void *)error_callback),0xF1234567u);
    func_0028B838(Bt4Android_EncodePointer((void *)error_callback),0xF1234567u);
    expected_message=D_002F5EA8;assert(dvCiGetFileSize(NULL)==0);
    expected_message=D_002F5F00;assert(dvCiGetFileSize("missing.bin")==0);
    expected_message=D_002F8428;assert(func_0028B8F0(NULL)==0);
    expected_message=D_002F8478;assert(func_0028B8F0("missing.bin")==0);
    assert(func_0028C288("missing.bin",202)==0 && func_0028C288("missing.bin",205)==0);
    assert(func_0027C680("missing.bin",203)==0 && errors==6); /* DVD command has no error callback */
    memset(long_name,'x',sizeof(long_name)-1);long_name[sizeof(long_name)-1]=0;
    assert(func_0028B8F0(long_name)==0);
    expected_message=D_002F5F00;assert(dvCiGetFileSize(long_name)==0 && errors==8);
    func_0027BD48(0,0);func_0028B838(0,0);
    unsetenv("BT4_ISO");
    unlink(iso_path);unlink(small_path);unlink(large_path);unlink(mods_path);
    snprintf(path,sizeof(path),"%s/DATA/empty.bin",root);unlink(path);
    snprintf(path,sizeof(path),"%s/mods/DATA",root);rmdir(path);
    snprintf(path,sizeof(path),"%s/mods",root);rmdir(path);
    snprintf(path,sizeof(path),"%s/DATA",root);rmdir(path);rmdir(root);
}
