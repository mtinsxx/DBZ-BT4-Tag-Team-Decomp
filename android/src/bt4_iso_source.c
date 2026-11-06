#define _GNU_SOURCE
#include "bt4_iso_source.h"
#include "bt4_iso9660.h"
#include "port_host.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct Slice { int fd;int64_t base,length,position; } Slice;
PORT_HOST static int mounted_fd=-1;
PORT_HOST static int64_t mounted_start,mounted_length;
PORT_HOST static pthread_mutex_t mount_lock=PTHREAD_MUTEX_INITIALIZER;
static int duplicate(int fd) { return fcntl(fd,F_DUPFD_CLOEXEC,0); }
int Bt4Iso_MountFd(int fd,int64_t start,int64_t length) {
    struct stat st;int copy;off_t cursor;
    if(fd<0 || start<0 || length< -1) { errno=EINVAL;return -1; }
    if(fstat(fd,&st) || (cursor=lseek(fd,0,SEEK_CUR))<0)return -1;
    if(length==-1) {
        int64_t end=st.st_size;
        if(end<=start) {
            end=lseek(fd,0,SEEK_END);
            if(lseek(fd,cursor,SEEK_SET)<0 || end<0)return -1;
        }
        if(end>=start)length=end-start;
    }
    if(length<=0 || start>INT64_MAX-length || (st.st_size>0 && start+length>st.st_size)) { errno=EINVAL;return -1; }
    { char byte;ssize_t got;do { got=pread(fd,&byte,1,start); } while(got<0&&errno==EINTR);
      if(got!=1) { if(!got)errno=EIO;return -1; } }
    copy=duplicate(fd);if(copy<0)return -1;
    pthread_mutex_lock(&mount_lock);
    if(mounted_fd>=0)close(mounted_fd);
    mounted_fd=copy;mounted_start=start;mounted_length=length;
    pthread_mutex_unlock(&mount_lock);return 0;
}
void Bt4Iso_Unmount(void) {
    pthread_mutex_lock(&mount_lock);if(mounted_fd>=0)close(mounted_fd);mounted_fd=-1;
    pthread_mutex_unlock(&mount_lock);
}
int Bt4Iso_IsMounted(void) {
    int result;pthread_mutex_lock(&mount_lock);result=mounted_fd>=0;pthread_mutex_unlock(&mount_lock);return result;
}
static ssize_t slice_read(void *cookie,char *out,size_t bytes) {
    Slice *s=cookie;ssize_t n;
    if(s->position>=s->length)return 0;
    if(bytes>(uint64_t)(s->length-s->position))bytes=(size_t)(s->length-s->position);
    do { n=pread(s->fd,out,bytes,s->base+s->position); } while(n<0 && errno==EINTR);
    if(n>0)s->position+=n;
    else if(n==0 && bytes) { errno=EIO;return -1; }
    return n;
}
static int64_t slice_seek(void *cookie,int64_t delta,int whence) {
    Slice *s=cookie;int64_t origin=whence==SEEK_SET?0:whence==SEEK_CUR?s->position:whence==SEEK_END?s->length:-1;
    if(origin<0 || (delta<0 && delta< -origin) || (delta>0 && origin>INT64_MAX-delta)) { errno=EINVAL;return -1; }
    s->position=origin+delta;return s->position;
}
static int slice_close(void *cookie) { Slice *s=cookie;int result=close(s->fd);free(s);return result; }
#ifdef __ANDROID__
static int android_read(void *cookie,char *out,int bytes) { return (int)slice_read(cookie,out,(size_t)bytes); }
static fpos_t android_seek(void *cookie,fpos_t delta,int whence) { return (fpos_t)slice_seek(cookie,delta,whence); }
#else
static int host_seek(void *cookie,off64_t *offset,int whence) {
    int64_t result=slice_seek(cookie,*offset,whence);if(result<0)return -1;*offset=result;return 0;
}
#endif
static FILE *open_range(uint64_t offset,uint64_t length) {
    Slice *s=calloc(1,sizeof(*s));FILE *file=NULL;
    if(!s)return NULL;
    s->fd=-1;
    pthread_mutex_lock(&mount_lock);
    if(mounted_fd>=0) {
        if(offset>(uint64_t)mounted_length || length>(uint64_t)mounted_length-offset)errno=EINVAL;
        else { s->fd=duplicate(mounted_fd);s->base=mounted_start+(int64_t)offset;s->length=(int64_t)length; }
    } else { s->fd=-1;errno=ENODEV; }
    pthread_mutex_unlock(&mount_lock);
    if(s->fd<0) { free(s);return NULL; }
#ifdef __ANDROID__
    file=funopen(s,android_read,NULL,android_seek,slice_close);
#else
    cookie_io_functions_t functions={.read=slice_read,.seek=host_seek,.close=slice_close};
    file=fopencookie(s,"r",functions);
#endif
    if(!file)slice_close(s);
    return file;
}
FILE *Bt4Iso_Open(void) {
    uint64_t size;int active;const char *path;
    pthread_mutex_lock(&mount_lock);active=mounted_fd>=0;size=(uint64_t)mounted_length;pthread_mutex_unlock(&mount_lock);
    if(active)return open_range(0,size);
    path=getenv("BT4_ISO");return path&&*path?fopen(path,"rb"):NULL;
}
static uint32_t le32(const unsigned char *p) { return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
FILE *Bt4Iso_OpenFile(const char *relative) {
    Bt4IsoFile entry;FILE *iso;char path[256];uint64_t offset,size,index=0;
    if(!relative || !Bt4Iso_IsMounted()) { errno=ENODEV;return NULL; }
    iso=Bt4Iso_Open();if(!iso)return NULL;
    if(!strncmp(relative,"disc/",5)) {
        int found=Bt4Iso_Find(iso,relative+5,&entry);
        if(found<0)goto malformed;
        if(!found)goto missing;
        offset=(uint64_t)entry.sector*2048;size=entry.size;
    } else {
        /* AFS folders are the port's archive-ID namespace, not host paths. */
        const char *slash=strchr(relative,'/');char archive[32];size_t length=slash?(size_t)(slash-relative):0;
        unsigned char header[8],record[8];uint32_t count,member_offset,member_size;
        if(length<4 || length>=sizeof(archive) || strncmp(relative,"pzs",3))goto missing;
        for(size_t i=0;i<length;++i) {
            char c=relative[i];if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')))goto missing;
            archive[i]=(c>='a'&&c<='z')?(char)(c-'a'+'A'):c;
        }
        archive[length]=0;
        const char *digit=slash+1;
        if(*digit<'0'||*digit>'9')goto missing;
        for(;*digit>='0'&&*digit<='9';++digit) {
            index=index*10+(unsigned)(*digit-'0');if(index>UINT32_MAX)goto missing;
        }
        if(strcmp(digit,".bin"))goto missing;
        snprintf(path,sizeof(path),"DATA/%s.AFS",archive);
        int found=Bt4Iso_Find(iso,path,&entry);
        if(found<0)goto malformed;
        if(!found)goto missing;
        if(entry.size<8)goto malformed;
        offset=(uint64_t)entry.sector*2048;
        if(fseeko(iso,(off_t)offset,SEEK_SET) || fread(header,1,8,iso)!=8 || memcmp(header,"AFS\0",4))goto malformed;
        count=le32(header+4);
        if(count>(entry.size-8)/8)goto malformed;
        if(index>=count)goto missing;
        if(fseeko(iso,(off_t)(offset+8+(uint64_t)index*8),SEEK_SET) || fread(record,1,8,iso)!=8)goto malformed;
        member_offset=le32(record);member_size=le32(record+4);
        if((member_size && member_offset<8+(uint64_t)count*8) || member_offset>entry.size || member_size>entry.size-member_offset)goto malformed;
        offset+=member_offset;size=member_size;
    }
    fclose(iso);return open_range(offset,size);
malformed: errno=EIO;fclose(iso);return NULL;
missing: errno=ENOENT;fclose(iso);return NULL;
}
