#include "bt4_iso_source.h"
/* Recovered DVD/host metadata and extended-command contracts. Native reads
   reuse the existing ISO metadata reader and extracted-file cache. */
#include "bt4_sdk_ci_query.h"
#include "bt4_sdk_file_fields.h"
#include "bt4_sdk_cri.h"
#include "bt4_iso9660.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern uint32_t D_002D4CD0[4], D_002D4E78[4];
extern uint32_t D_002D3FE4, D_002D3FE8, D_002E6F5C, D_002E6F60;
extern char D_002D4CF0[], D_002D4F98[], D_002D4E90[];
extern const char D_002F6090[], D_002F6098[], D_002F6160[], D_002F6248[], D_002F6250[];
extern const char D_002F5EA8[], D_002F5F00[], D_002F8428[], D_002F8478[];
extern long Port_FileSize(const char *relative_path);

static void error(int dvd, const char *message) {
    Bt4Cri_DriverError(dvd ? D_002D3FE4 : D_002E6F5C,
                       dvd ? D_002D3FE8 : D_002E6F60, message, NULL);
}
static unsigned normalize(unsigned byte, int dvd) {
    if (byte == '\\') byte = '/';
    if (dvd && byte >= 'a' && byte <= 'z') byte -= 'a' - 'A';
    return byte;
}
static int equal(const char *a, const char *b, int dvd) {
    size_t i, size = strlen(a);
    if (size != strlen(b)) return 0;
    for (i=0;i<size;++i) if (normalize((unsigned char)a[i],dvd) != normalize((unsigned char)b[i],dvd)) return 0;
    return 1;
}
static void override(const uint32_t config[4], const char *name, int dvd, uint32_t result[2]) {
    int32_t i, count = (int32_t)config[1];
    result[0] = result[1] = 0;
    if (!config[0]) return;
    for (i=0;i<count;++i) {
        uint32_t pair[2];
        uint32_t address = config[0] + config[2]*8 + (config[3]+1)*(uint32_t)i;
        if (!equal(name,Bt4Cri_Pointer(address),dvd)) continue;
        memcpy(pair,Bt4Cri_Pointer(config[0]+8*(uint32_t)i),sizeof(pair));
        result[0]=pair[0]; result[1]=pair[1];
        return;
    }
}
static int append(char *out, size_t capacity, const char *text) {
    size_t n = strlen(out), size = strlen(text);
    if (size >= capacity-n) return 0;
    memcpy(out+n,text,size+1);
    return 1;
}
static int dvd_path(const char *name, char *out, size_t capacity) {
    size_t i,n,suffix;
    out[0]=0;
    if (!append(out,capacity,D_002D4CF0)) return 0;
    if (*name != '/' && *name != '\\' && !append(out,capacity,D_002F6090)) return 0;
    if (!append(out,capacity,name)) return 0;
    n=strlen(out); suffix=strlen(D_002F6098);
    if ((n<suffix || strcmp(out+n-suffix,D_002F6098)) && !append(out,capacity,D_002F6098)) return 0;
    for (i=0;out[i];++i) {
        unsigned c=(unsigned char)out[i];
        if (c=='/') c='\\';
        if (c>='a' && c<='z') c-='a'-'A';
        out[i]=(char)c;
    }
    return 1;
}
static int host_path(const char *name, char *out, size_t capacity) {
    size_t i,n;
    char device[6]={0};
    out[0]=0;
    for (i=0;i<16 && name[i] && name[i]!=':';++i) {}
    if ((i==16 || name[i]!=':') && !append(out,capacity,D_002D4F98)) return 0;
    memcpy(device,out,strlen(out)<4 ? strlen(out) : 4);
    if (!strcmp(device,D_002F6248)) {
        if (!append(out,capacity,D_002D4E90)) return 0;
        n=strlen(D_002D4E90);
        if (n && D_002D4E90[n-1]!='/' && D_002D4E90[n-1]!='\\' &&
            *name!='/' && *name!='\\' && !append(out,capacity,D_002F6250)) return 0;
    }
    if (!append(out,capacity,name)) return 0;
    for (i=0;out[i];++i) if (out[i]=='\\') out[i]='/';
    return 1;
}
int Bt4Ci_DvdLocate(const char *name, Bt4IsoFile *file, int *overridden) {
    uint32_t result[2];
    char path[1024], *relative;
    FILE *iso;
    Bt4IsoFile info;
    int found;
    if (overridden) *overridden=1;
    if (!strcmp(name,D_002F6160)) { file->sector=0; file->size=UINT32_MAX; return 1; }
    if (strlen(name)==17 && name[8]=='.') {
        char *end;
        result[0]=(uint32_t)strtoull(name,&end,16);
        if (*end) ++end;
        result[1]=(uint32_t)strtoull(end,NULL,16)<<11;
    } else override(D_002D4CD0,name,1,result);
    if (result[1]) { file->sector=result[0]; file->size=result[1]; return 1; }
    if (overridden) *overridden=0;
    if (!dvd_path(name,path,sizeof(path))) return 0;
    relative=strchr(path,':'); relative=relative ? relative+1 : path;
    if (!(iso=Bt4Iso_Open())) return 0;
    found=Bt4Iso_Find(iso,relative,&info);
    fclose(iso);
    if (found!=1) return 0;
    *file=info;
    return 1;
}
int Bt4Ci_HostOverride(const char *name, uint32_t pair[2]) {
    override(D_002D4E78,name,0,pair);
    return pair[1]!=0;
}
int Bt4Ci_HostRelativePath(const char *name, char *out, size_t capacity) {
    char *relative;
    if (!host_path(name,out,capacity)) return 0;
    relative=strchr(out,':'); relative=relative ? relative+1 : out;
    while (*relative=='/') ++relative;
    memmove(out,relative,strlen(relative)+1);
    return 1;
}
int64_t Bt4Ci_HostSize(const char *name, int report) {
    uint32_t result[2];
    char path[1024], *relative;
    long size;
    if (!name) { if (report) error(0,D_002F8428); return 0; }
    override(D_002D4E78,name,0,result);
    if (result[1]) return (int32_t)result[1]; /* original LW -> signed 64-bit */
    if (!host_path(name,path,sizeof(path))) { if (report) error(0,D_002F8478); return 0; }
    /* PS2 host device paths map to the native port's existing data-root and
       mod/alias provider; no new cache or fake descriptor is introduced. */
    relative=strchr(path,':'); relative=relative ? relative+1 : path;
    while (*relative=='/') ++relative;
    size=Port_FileSize(relative);
    if (size<0) { if (report) error(0,D_002F8478); return 0; }
    return size;
}
int32_t dvCiGetFileSize(const char *name) {
    Bt4IsoFile info;
    if (!name) { error(1,D_002F5EA8); return 0; }
    if (!Bt4Ci_DvdLocate(name,&info,NULL)) { error(1,D_002F5F00); return 0; }
    return (int32_t)info.size;
}
int32_t func_0027BDD8(const char *name) { Bt4IsoFile info; return name ? Bt4Ci_DvdLocate(name,&info,NULL) : 0; }
int32_t func_0028B850(const char *name) {
    uint32_t result[2];
    char path[1024], *relative;
    if (!name) return 0;
    override(D_002D4E78,name,0,result);
    if (result[1]) return 1;
    if (!host_path(name,path,sizeof(path))) return 0;
    relative=strchr(path,':'); relative=relative ? relative+1 : path;
    while (*relative=='/') ++relative;
    return Port_FileSize(relative)>=0;
}
int32_t func_0028B8F0(const char *name) { return (int32_t)(uint32_t)Bt4Ci_HostSize(name,1); }
int32_t func_0027C680(const void *target, int32_t command) {
    const Bt4DvdCiWork *file=target;
    Bt4IsoFile info;
    uint64_t bytes;
    if (!target) return 0;
    switch (command) {
    case 200: bytes=(uint64_t)(int64_t)file->completed_sectors<<11; return (int32_t)(uint32_t)(bytes>>32);
    case 201: return (int32_t)((uint32_t)file->completed_sectors<<11);
    case 202: case 204: case 301: return 0; /* complete original constant leaves */
    case 203: case 205: return Bt4Ci_DvdLocate(target,&info,NULL) ? (int32_t)info.size : 0;
    case 302: return (int32_t)file->reserved4;
    case 299: return 1;
    default: return 0;
    }
}
int32_t func_0028C288(const void *target, int32_t command) {
    const Bt4HostCiWork *file=target;
    uint64_t bytes,size;
    if (!target) return 0;
    switch (command) {
    case 200: case 201:
        bytes=(uint64_t)(int64_t)file->completed_sectors<<11;
        size=(uint64_t)file->file_size;
        if (size<bytes) bytes=size;
        return command==200 ? (int32_t)(uint32_t)(bytes>>32) : (int32_t)(uint32_t)bytes;
    case 202: case 204: size=(uint64_t)Bt4Ci_HostSize(target,1); return (int32_t)(uint32_t)(size>>32);
    case 203: case 205: return (int32_t)(uint32_t)Bt4Ci_HostSize(target,1);
    case 301: return (int32_t)(uint32_t)((uint64_t)file->file_size>>32);
    case 302: return (int32_t)(uint32_t)file->file_size;
    case 299: return 1;
    default: return 0;
    }
}
