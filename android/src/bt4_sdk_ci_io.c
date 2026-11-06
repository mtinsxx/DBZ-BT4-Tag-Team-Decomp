#include "bt4_iso_source.h"
#define _POSIX_C_SOURCE 200809L
#include "bt4_sdk_ci_io.h"
#include "bt4_sdk_ci_query.h"
#include "bt4_sdk_cri.h"
#include "bt4_guest_abi.h"
#include "port_host.h"
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(Bt4CiRequest)==0x38, "shared EE request size");
_Static_assert(offsetof(Bt4CiRequest,bytes)==0x20, "host byte-count offset");
_Static_assert(offsetof(Bt4CiRequest,descriptor)==0x30, "host descriptor offset");
_Static_assert(offsetof(Bt4DvdCiWork,lsn)==0x20, "DVD metadata offset");
extern Bt4CiRequest D_002D3320;
extern Bt4DvdCiWork D_002D4060[40];
extern Bt4HostCiWork D_002E6FD0[13];
extern uint32_t D_002D3380,D_002D3384,D_002D3FF0,D_002E6F64,D_00321838;
extern uint32_t D_002D4CE4,D_002D4E88;
extern unsigned char D_002D4CE0[4];
extern uint32_t D_002D3FE4,D_002D3FE8,D_002E6F5C,D_002E6F60;
extern const char D_002F5F30[],D_002F5F58[],D_002F5F80[],D_002F5FD8[],D_002F6008[],D_002F6020[],D_002F6040[],D_002F6160[];
extern const char D_002F8598[],D_002F85C0[],D_002F85E8[],D_002F8640[],D_002F8680[],D_002F8698[],D_002F86B8[],D_002F86E0[];
extern long Port_FileSize(const char *path);
extern size_t Port_FileReadAt(const char *path,long offset,void *buffer,size_t bytes);

/* Stable logical descriptors reference immutable paths in the existing cache,
   not OS descriptors that would become stale after a game-state restore.
   The cache continues to own/evict FILE objects. Bindings survive work Close. */
typedef struct Binding { struct Binding *next; int32_t id; char path[]; } Binding;
PORT_HOST static Binding *bindings;
PORT_HOST static int last_io_error;
int Bt4Ci_LastIoError(void) { return __atomic_load_n(&last_io_error,__ATOMIC_RELAXED); }
static Binding *binding(int32_t id) {
    Binding *b; for (b=bindings;b;b=b->next) if (b->id==id) return b;
    return NULL;
}
static int bind(int32_t id,const char *path) {
    Binding *b=binding(id);
    size_t size;
    if (b) return strcmp(b->path,path) ? -1 : 0;
    size=strlen(path)+1;
    b=malloc(sizeof(*b)+size);
    if (!b) return -1;
    b->id=id;memcpy(b->path,path,size);b->next=bindings;bindings=b;
    return 0;
}
int Bt4Ci_BindHostDescriptor(int32_t id,const char *path) {
    int result=-1;
    if (!path || strlen(path)>=1024 || Port_FileSize(path)<0) return -1;
    Bt4Cri_Lock();result=bind(id,path);Bt4Cri_Unlock();return result;
}
static int native_id(const char *path,int32_t *id) {
    const unsigned char *p=(const unsigned char *)path;
    uint32_t hash=2166136261u;
    while (*p) { hash^=*p++;hash*=16777619u; }
    *id=(int32_t)((hash&0x3FFFFFFFu)|0x40000000u);
    /* A collision is a real open error, never an alias to another file. */
    return bind(*id,path)==0;
}
static int32_t add(int32_t a,int32_t b) { return (int32_t)((uint32_t)a+(uint32_t)b); }
static int32_t sub(int32_t a,int32_t b) { return (int32_t)((uint32_t)a-(uint32_t)b); }
static void error(int dvd,const char *message,void *handle) {
    Bt4Cri_DriverError(dvd?D_002D3FE4:D_002E6F5C,dvd?D_002D3FE8:D_002E6F60,message,handle);
}
static uint32_t request_word(void) { return Bt4Android_EncodePointer(&D_002D3320); }
static int claim(void) {
    if (D_002D3320.used) return 0;
    memset(&D_002D3320,0,sizeof(D_002D3320));
    D_002D3320.used=1;D_002D3320.reserved3=255;++D_002D3380;
    return 1;
}
static void release(void) { memset(&D_002D3320,0,sizeof(D_002D3320));++D_002D3384; }
static int server_enter(uint32_t *guard) {
    if (D_00321838) {
        void *callback=Bt4Android_DecodePointer(D_00321838);
        if (Bt4Android_CallKind(callback,1)=='i')
            return ((int32_t (*)(uint32_t))callback)(Bt4Android_EncodePointer(guard))!=0;
        return ((int32_t (*)(void *))callback)(guard)!=0;
    }
    if (*guard==1) return 0;
    *guard=1;return 1;
}
static int read_dvd(Bt4CiRequest *r) {
    FILE *iso;
    uint64_t offset=(uint64_t)r->lsn*2048,bytes=(uint64_t)r->sectors*2048;
    int result;
    if (((r->mode>>16)&255)!=0) { errno=ENOTSUP;return 0; } /* ISO has 2048-byte sectors */
    if (bytes>SIZE_MAX || bytes>UINT64_C(0x100000000)-r->dvd_destination) { errno=EOVERFLOW;return 0; }
    if (!bytes) return 1;
    if (!(iso=Bt4Iso_Open())) { if (!errno) errno=ENOENT;return 0; }
    result=fseeko(iso,(off_t)offset,SEEK_SET)==0 &&
        fread(Bt4Android_DecodePointer(r->dvd_destination),1,(size_t)bytes,iso)==bytes;
    if (!result && !ferror(iso)) errno=EIO;
    { int saved=errno;fclose(iso);errno=saved; }
    return result;
}
static int read_host(Bt4CiRequest *r) {
    Binding *b=binding(r->descriptor);
    uint64_t offset=(uint64_t)r->position*2048;
    if (!b || r->position<0 || r->bytes<0 || offset>LONG_MAX) { errno=EINVAL;return 0; }
    if (!r->bytes) return 1;
    if (Port_FileReadAt(b->path,(long)offset,Bt4Android_DecodePointer(r->host_destination),(size_t)r->bytes)!=(uint64_t)r->bytes) {
        errno=EIO;return 0;
    }
    return 1;
}
static void execute(void) {
    Bt4CiRequest *r=&D_002D3320;
    int ok;
    if (r->state!=1) return;
    if (r->abort_requested) { r->state=0;return; }
    errno=0;ok=r->type==1 ? read_dvd(r) : read_host(r);
    __atomic_store_n(&last_io_error,ok?0:-(errno?errno:EIO),__ATOMIC_RELAXED);r->state=ok?3:9;
}
static void dvd_complete(Bt4DvdCiWork *file) {
    unsigned state=D_002D3320.state;
    if (state!=0 && state!=3 && state!=9) return;
    if (state==3) { file->completed_sectors=file->requested_sectors;file->position=add(file->position,file->requested_sectors);file->status=1; }
    else file->status=state==9?3:0;
    release();file->request=0;
}
static void host_complete(Bt4HostCiWork *file) {
    unsigned state=D_002D3320.state;
    if (state!=0 && state!=3 && state!=9) return;
    if (state==3) { file->completed_sectors=(int32_t)file->reserved24;file->position=add(file->position,(int32_t)file->reserved24);file->status=1; }
    else file->status=state==9?3:0;
    release();file->request=0;
}
void func_0027BCD0(void) {
    unsigned i;
    Bt4Cri_Lock();
    if (server_enter(&D_002D3FF0)) {
        for (i=0;i<40;++i) if (D_002D4060[i].used==1 && D_002D4060[i].request && D_002D3320.used && D_002D3320.type==1) {
            execute();dvd_complete(&D_002D4060[i]);
        }
        D_002D3FF0=0;
    }
    Bt4Cri_Unlock();
}
void func_0028B7C0(void) {
    unsigned i;
    Bt4Cri_Lock();
    if (server_enter(&D_002E6F64)) {
        for (i=0;i<13;++i) if (D_002E6FD0[i].used==1 && D_002E6FD0[i].request && D_002D3320.used && D_002D3320.type==2) {
            execute();host_complete(&D_002E6FD0[i]);
        }
        D_002E6F64=0;
    }
    Bt4Cri_Unlock();
}
Bt4DvdCiWork *dvCiOpen(const char *name,uint32_t unused,uint32_t attributes) {
    Bt4DvdCiWork *file=NULL;
    Bt4IsoFile info;
    int kind;
    unsigned i;
    (void)unused;
    if (!name) { error(1,D_002F5F30,NULL);return NULL; }
    if (attributes) { error(1,D_002F5F58,NULL);return NULL; }
    Bt4Cri_Lock();
    for (i=0;i<40;++i) if (!D_002D4060[i].used) file=&D_002D4060[i];
    if (!file) { error(1,D_002F5F80,NULL);Bt4Cri_Unlock();return NULL; }
    memcpy(&info,(unsigned char *)file+offsetof(Bt4DvdCiWork,lsn),sizeof(info));
    if (!Bt4Ci_DvdLocate(name,&info,&kind)) { error(1,D_002F5FD8,NULL);memset(file,0,sizeof(*file));Bt4Cri_Unlock();return NULL; }
    memcpy((unsigned char *)file+offsetof(Bt4DvdCiWork,lsn),&info,sizeof(info));file->reserved1=(uint8_t)kind;
    file->reserved4=info.size;file->sectors=(int32_t)((info.size+2047u)>>11);
    if (!strcmp(name,D_002F6160)) file->sectors=INT32_MAX;
    file->used=1;file->status=0;file->position=file->requested_sectors=file->completed_sectors=0;
    file->destination=file->request=0;
    Bt4Cri_Unlock();return file;
}
Bt4HostCiWork *htCiOpen(const char *name,uint32_t unused,uint32_t attributes) {
    Bt4HostCiWork *file=NULL;
    uint32_t pair[2];
    char path[1024];
    int32_t descriptor;
    int64_t size;
    unsigned i;
    int kind;
    (void)unused;
    if (!name) { error(0,D_002F8598,NULL);return NULL; }
    if (attributes) { error(0,D_002F85C0,NULL);return NULL; }
    Bt4Cri_Lock();
    for (i=0;i<13;++i) if (!D_002E6FD0[i].used) file=&D_002E6FD0[i];
    if (!file) { error(0,D_002F85E8,NULL);Bt4Cri_Unlock();return NULL; }
    kind=Bt4Ci_HostOverride(name,pair);
    if (kind) {
        descriptor=(int32_t)pair[0];size=(int32_t)pair[1];
        if (!binding(descriptor)) goto failed;
    } else {
        size=Bt4Ci_HostSize(name,1);
        if (!size) { Bt4Cri_Unlock();return NULL; } /* genuine original zero-size rejection */
        if (!Bt4Ci_HostRelativePath(name,path,sizeof(path)) || !native_id(path,&descriptor)) goto failed;
    }
    file->used=1;file->kind=(uint8_t)kind;file->status=0;file->file_size=size;
    file->sectors=(int32_t)(uint32_t)(((uint64_t)size+2047)>>11);file->descriptor=descriptor;
    file->position=file->completed_sectors=0;file->reserved14=file->reserved20=file->reserved24=file->request=0;
    Bt4Cri_Unlock();return file;
failed:
    error(0,D_002F8640,NULL);memset(file,0,sizeof(*file));Bt4Cri_Unlock();return NULL;
}
int32_t dvCiReqRd(Bt4DvdCiWork *file,int32_t sectors,void *destination) {
    int32_t count;
    Bt4CiRequest *r=&D_002D3320;
    if (!file) { error(1,D_002F6008,NULL);return 0; }
    if (sectors<0) { error(1,D_002F6020,file);return 0; }
    if (!destination) { error(1,D_002F6040,file);return 0; }
    Bt4Cri_Lock();
    if (file->status==2) goto busy;
    if (!sectors) { file->status=1;goto busy; }
    if (!claim()) { file->request=0;goto busy; }
    count=sub(file->sectors,file->position);if (sectors<count) count=sectors;
    if (count<0) { release();goto busy; }
    file->request=request_word();file->requested_sectors=count;file->destination=Bt4Android_EncodePointer(destination);
    memcpy((unsigned char *)file+0x44,D_002D4CE0,3);
    r->type=1;r->state=1;r->lsn=file->lsn+(uint32_t)file->position;r->sectors=(uint32_t)count;r->dvd_destination=file->destination;
    memcpy(&r->mode,(unsigned char *)file+0x44,4);file->status=2;
    if (D_002D4CE4==1) {
        execute();dvd_complete(file);
        if (file->status==1) file->completed_sectors=sectors;
        else count=0; /* native read errors must not become fabricated success */
    }
    Bt4Cri_Unlock();return count;
busy:
    Bt4Cri_Unlock();return 0;
}
int32_t htCiReqRd(Bt4HostCiWork *file,int32_t sectors,void *destination) {
    int32_t count;
    uint64_t span,offset,valid;
    unsigned i;
    Bt4CiRequest *r=&D_002D3320;
    if (!file) { error(0,D_002F8680,NULL);return 0; }
    if (sectors<0) { error(0,D_002F8698,file);return 0; }
    if (!destination) { error(0,D_002F86B8,file);return 0; }
    Bt4Cri_Lock();
    for (i=0;i<13;++i) if (D_002E6FD0[i].used==1 && D_002E6FD0[i].status==2) goto busy;
    if (file->status==2) goto busy;
    if (!sectors) { file->status=1;goto busy; }
    if (!claim()) { file->request=0;goto busy; }
    count=sub(file->sectors,file->position);if (sectors<count) count=sectors;
    if (count<0 || file->position<0 || file->file_size<0) { release();goto busy; }
    span=(uint64_t)(uint32_t)count*2048;offset=(uint64_t)(uint32_t)file->position*2048;
    valid=offset<(uint64_t)file->file_size ? (uint64_t)file->file_size-offset : 0;if (span<valid) valid=span;
    if (span>SIZE_MAX || span>UINT64_C(0x100000000)-Bt4Android_EncodePointer(destination)) { release();goto busy; }
    file->request=request_word();file->reserved20=Bt4Android_EncodePointer(destination);file->reserved24=(uint32_t)count;
    /* Preserve the final padded sector while fixing the original read count
       that could exceed the remaining file or the destination span. */
    memset((unsigned char *)destination+(size_t)valid,0,(size_t)(span-valid));
    r->type=2;r->state=1;r->host_destination=file->reserved20;r->position=file->position;
    r->descriptor=file->descriptor;r->bytes=(int64_t)valid;
    file->completed_sectors=0;file->reserved14=0;file->status=2;
    if (D_002D4E88==1) { execute();host_complete(file);if (file->status!=1) count=0; }
    Bt4Cri_Unlock();return count;
busy:
    Bt4Cri_Unlock();return 0;
}
void func_0027C3C0(Bt4DvdCiWork *file) {
    if (!file) { error(1,D_002F6008,NULL);return; }
    Bt4Cri_Lock();
    if ((uint8_t)file->status>=2) {
        if (file->request && D_002D3320.used && D_002D3320.type==1) { D_002D3320.abort_requested=1;D_002D3320.state=0;dvd_complete(file); }
        file->completed_sectors=0;file->status=0;
    }
    Bt4Cri_Unlock();
}
void func_0028C028(Bt4HostCiWork *file) {
    if (!file) { error(0,D_002F86E0,NULL);return; }
    Bt4Cri_Lock();
    if ((uint8_t)file->status>=2) {
        if (file->request && D_002D3320.used && D_002D3320.type==2) { D_002D3320.abort_requested=1;D_002D3320.state=0;host_complete(file); }
        file->completed_sectors=0;file->status=0;
    }
    Bt4Cri_Unlock();
}
void func_0027C0F0(Bt4DvdCiWork *file) {
    if (!file) return;
    Bt4Cri_Lock();if ((uint8_t)file->status>=2) func_0027C3C0(file);memset(file,0,sizeof(*file));Bt4Cri_Unlock();
}
void func_0028BD28(Bt4HostCiWork *file) {
    if (!file) return;
    Bt4Cri_Lock();if ((uint8_t)file->status>=2) func_0028C028(file);memset(file,0,sizeof(*file));Bt4Cri_Unlock();
}
