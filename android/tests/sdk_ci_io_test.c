#define _POSIX_C_SOURCE 200809L
#include "bt4_sdk_ci_io.h"
#include "bt4_sdk_ci_query.h"
#include "bt4_sdk_fields.h"
#include "bt4_guest_abi.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

Bt4CiRequest D_002D3320;
Bt4DvdCiWork D_002D4060[40];
Bt4HostCiWork D_002E6FD0[13];
uint32_t D_002D3380,D_002D3384,D_002D3FF0,D_002E6F64,D_00321838,D_002D4CE4,D_002D4E88;
unsigned char D_002D4CE0[4];
const char D_002F5F30[]="dvd-open-null",D_002F5F58[]="dvd-attributes",D_002F5F80[]="dvd-pool-full",D_002F5FD8[]="dvd-open-missing";
const char D_002F6020[]="dvd-negative",D_002F6040[]="dvd-destination";
const char D_002F8598[]="host-open-null",D_002F85C0[]="host-attributes",D_002F85E8[]="host-pool-full",D_002F8640[]="host-binding";
const char D_002F8698[]="host-negative",D_002F86B8[]="host-destination",D_002F86E0[]="host-stop-null";
extern const char D_002F6008[],D_002F8680[];
extern const char D_002F6160[];
extern uint32_t D_002D4E78[4];
extern void Test_CiSetRoot(const char *);
static unsigned char buffer[4098],sector[2048];
static uint32_t overrides[32];
static unsigned errors,guard_calls;
static const char *expected;
static void *expected_handle;
static void error_callback(uint32_t c,const char *text,void *h) {
    assert(c==0xF1234567u && text==expected && h==expected_handle);++errors;
}
static int guard(uint32_t address) { assert(Bt4Android_DecodePointer(address)==&D_002D3FF0);++guard_calls;return 0; }
static void both(unsigned char *p,uint32_t value) {
    unsigned i;for(i=0;i<4;++i){p[i]=(unsigned char)(value>>(8*i));p[i+4]=(unsigned char)(value>>(8*(3-i)));}
}
static void record(unsigned char *p,uint32_t lsn,uint32_t size,int directory,const char *name) {
    unsigned n=(unsigned)strlen(name),length=33+n+(n%2==0);
    memset(p,0,length);p[0]=(unsigned char)length;both(p+2,lsn);both(p+10,size);p[25]=directory?2:0;p[28]=p[31]=1;p[32]=(unsigned char)n;memcpy(p+33,name,n);
}
static void write_sector(FILE *f,unsigned lsn,const unsigned char *bytes) {
    assert(fseeko(f,(off_t)lsn*2048,SEEK_SET)==0 && fwrite(bytes,1,2048,f)==2048);
}
void test_ci_io(void) {
    char root[]="/tmp/bt4-ci-io-XXXXXX",iso_path[256],path[256];
    unsigned char pvd[2048]={0},directory[2048]={0},zero[2048]={0};
    FILE *f;
    Bt4DvdCiWork *dvd,*other;
    Bt4HostCiWork *host,saved;
    unsigned i;
    static const Bt4AbiEntry entries[]={{(void *)error_callback,"vipp"},{(void *)guard,"ii"}};
    static Bt4AbiModule module={NULL,entries,2};
    Bt4Android_RegisterSignatures(&module);
    assert(mkdtemp(root));Test_CiSetRoot(root);
    snprintf(path,sizeof(path),"%s/DATA",root);assert(mkdir(path,0700)==0);
    snprintf(path,sizeof(path),"%s/DATA/io-host.bin",root);f=fopen(path,"wb");assert(f);
    for(i=0;i<2048;++i)sector[i]=i<53?(unsigned char)(i+1):0xEF;
    assert(fwrite(sector,1,53,f)==53 && fclose(f)==0);
    snprintf(path,sizeof(path),"%s/DATA/io-large.bin",root);f=fopen(path,"wb");assert(f);
    assert(fseeko(f,(off_t)INT64_C(0x180000000),SEEK_SET)==0 && fwrite(sector,1,53,f)==53 && fclose(f)==0);
    snprintf(iso_path,sizeof(iso_path),"%s/test.iso",root);f=fopen(iso_path,"w+b");assert(f);
    write_sector(f,29,zero);pvd[0]=1;memcpy(pvd+1,"CD001",5);pvd[6]=1;record(pvd+156,20,2048,1,"x");
    record(directory,23,53,0,"READ.BIN;1");write_sector(f,16,pvd);write_sector(f,20,directory);write_sector(f,23,sector);assert(fclose(f)==0);
    assert(setenv("BT4_ISO",iso_path,1)==0);
    dvd=dvCiOpen("read.bin",0,0);other=dvCiOpen("read.bin",0,0);host=htCiOpen("io-host.bin",0,0);
    assert(dvd==D_002D4060+39 && other==D_002D4060+38 && host==D_002E6FD0+12);
    assert(dvd->reserved4==53 && dvd->lsn==23 && dvd->sectors==1 && host->file_size==53 && host->sectors==1);
    assert(!strcmp((char *)dvd+0x28,"READ.BIN;1"));
    memset(buffer,0xA5,sizeof(buffer));
    assert(dvCiReqRd(dvd,1,buffer+1)==1 && dvd->status==2 && D_002D3320.used && D_002D3320.state==1);
    assert(buffer[1]==0xA5 && htCiReqRd(host,1,buffer+1)==0 && host->status==0);
    assert(dvCiReqRd(other,1,buffer+1)==0); /* shared one-slot allocator */
    func_0028B7C0();assert(dvd->status==2 && D_002D3320.used);
    D_00321838=Bt4Android_EncodePointer((void *)guard);func_0027BCD0();assert(guard_calls==1 && dvd->status==2);D_00321838=0;
    D_002D3FF0=1;func_0027BCD0();assert(dvd->status==2);D_002D3FF0=0;
    func_0027BCD0();assert(dvd->status==1 && dvd->position==1 && dvd->completed_sectors==1 && !dvd->request && !D_002D3320.used);
    assert(!memcmp(buffer+1,sector,2048) && buffer[0]==0xA5 && buffer[2049]==0xA5); /* real ISO padding, not host zeros */
    assert(Bt4Ci_LastIoError()==0);
    func_0027C3C0(dvd);assert(dvd->status==1 && dvd->completed_sectors==1);
    assert(func_0027C148(dvd,0,0)==0);memset(buffer,0xA5,sizeof(buffer));
    assert(dvCiReqRd(dvd,1,buffer+1)==1);func_0027C3C0(dvd);assert(dvd->status==0 && dvd->position==0 && !D_002D3320.used && buffer[1]==0xA5);
    D_002D4CE4=1;assert(dvCiReqRd(dvd,3,buffer+1)==1 && dvd->status==1 && dvd->completed_sectors==3 && dvd->position==1);
    D_002D4CE4=0;assert(func_0027C148(dvd,0,0)==0 && dvCiReqRd(dvd,1,buffer+1)==1);func_0027C0F0(dvd);assert(!dvd->used && !D_002D3320.used);

    memset(buffer,0xA5,sizeof(buffer));assert(htCiReqRd(host,3,buffer+1)==1 && host->status==2 && D_002D3320.bytes==53);
    assert(buffer[1]==0xA5 && buffer[54]==0 && buffer[2049]==0xA5);
    func_0027BCD0();assert(host->status==2);func_0028B7C0();assert(host->status==1 && host->position==1 && host->completed_sectors==1 && !D_002D3320.used);
    assert(!memcmp(buffer+1,sector,53));for(i=54;i<=2048;++i)assert(buffer[i]==0);
    assert(func_0028BDC8(host,0,0)==0);D_002D4E88=1;
    assert(htCiReqRd(host,2,buffer+1)==1 && host->status==1 && host->position==1);
    assert(htCiReqRd(host,1,buffer+1)==0 && host->status==1 && host->completed_sectors==0);
    D_002D4E88=0;assert(func_0028BDC8(host,0,0)==0 && htCiReqRd(host,1,buffer+1)==1);func_0028C028(host);assert(host->status==0 && !D_002D3320.used);
    saved=*host;func_0028BD28(host);*host=saved; /* restored logical id still resolves to the same immutable native path */
    D_002D4E88=1;assert(htCiReqRd(host,1,buffer+1)==1 && !memcmp(buffer+1,sector,53));func_0028BD28(host);
    host=htCiOpen("io-large.bin",0,0);assert(host && host->file_size==INT64_C(0x180000035));
    assert(func_0028BDC8(host,0x300000,0)==0x300000);
    assert(htCiReqRd(host,1,buffer+1)==1 && !memcmp(buffer+1,sector,53));
    for(i=54;i<=2048;++i)assert(buffer[i]==0);
    func_0028BD28(host);
    host=htCiOpen("io-host.bin",0,0);assert(host);D_002D4E88=0;
    assert(htCiReqRd(host,1,buffer+1)==1);func_0028BD28(host);assert(!host->used && !D_002D3320.used);D_002D4E88=1;

    func_0027BD48(Bt4Android_EncodePointer((void *)error_callback),0xF1234567u);
    func_0028B838(Bt4Android_EncodePointer((void *)error_callback),0xF1234567u);
    expected_handle=NULL;expected=D_002F5F30;assert(!dvCiOpen(NULL,0,0));expected=D_002F5F58;assert(!dvCiOpen("read.bin",0,1));
    expected=D_002F8598;assert(!htCiOpen(NULL,0,0));expected=D_002F85C0;assert(!htCiOpen("io-host.bin",0,1));
    overrides[0]=55;overrides[1]=53;strcpy((char *)overrides+8,"virtual.bin");D_002D4E78[0]=Bt4Android_EncodePointer(overrides);D_002D4E78[1]=D_002D4E78[2]=1;D_002D4E78[3]=31;
    expected=D_002F8640;assert(!htCiOpen("virtual.bin",0,0)); /* an unbound PS2 id is never used as an OS fd */
    assert(Bt4Ci_BindHostDescriptor(55,"DATA/io-host.bin")==0 && Bt4Ci_BindHostDescriptor(55,"DATA/alias.bin")==-1);
    host=htCiOpen("virtual.bin",0,0);assert(host && host->kind==1 && host->descriptor==55);
    assert(htCiReqRd(host,1,buffer+1)==1 && !memcmp(buffer+1,sector,53));func_0028BD28(host);memset(D_002D4E78,0,sizeof(D_002D4E78));
    expected=D_002F6008;assert(dvCiReqRd(NULL,1,buffer)==0);expected=D_002F8680;assert(htCiReqRd(NULL,1,buffer)==0);
    expected_handle=other;expected=D_002F6020;assert(dvCiReqRd(other,-1,buffer)==0);expected=D_002F6040;assert(dvCiReqRd(other,1,NULL)==0);
    expected_handle=NULL;
    assert(setenv("BT4_ISO","/does-not-exist-ci-test.iso",1)==0);D_002D4CE4=1;
    assert(dvCiReqRd(other,1,buffer+1)==0 && other->status==3 && other->position==0 && !D_002D3320.used && Bt4Ci_LastIoError()==-ENOENT);func_0027C3C0(other);
    assert(setenv("BT4_ISO",iso_path,1)==0);D_002D4CE0[2]=1;assert(dvCiReqRd(other,1,buffer+1)==0 && other->status==3 && Bt4Ci_LastIoError()==-ENOTSUP);D_002D4CE0[2]=0;func_0027C3C0(other);
    func_0027C0F0(other);func_0027BD48(0,0);func_0028B838(0,0);
    dvd=dvCiOpen(D_002F6160,0,0);assert(dvd && dvd->sectors==INT32_MAX && func_0027C148(dvd,23,0)==23);
    assert(dvCiReqRd(dvd,1,buffer+1)==1 && !memcmp(buffer+1,sector,2048));func_0027C0F0(dvd);
    /* A declared extent longer than the actual immutable backing must fail,
       rather than use the padding to conceal an incomplete read. */
    overrides[1]=153;strcpy((char *)overrides+8,"oversized.bin");D_002D4E78[0]=Bt4Android_EncodePointer(overrides);D_002D4E78[1]=D_002D4E78[2]=1;D_002D4E78[3]=31;
    host=htCiOpen("oversized.bin",0,0);assert(host);
    assert(htCiReqRd(host,1,buffer+1)==0 && host->status==3 && host->position==0 && Bt4Ci_LastIoError()==-EIO);func_0028BD28(host);
    memset(D_002D4E78,0,sizeof(D_002D4E78));
    for(i=0;i<40;++i)assert(dvCiOpen("read.bin",0,0)==D_002D4060+39-i);
    assert(!dvCiOpen("read.bin",0,0));for(i=0;i<40;++i)func_0027C0F0(D_002D4060+i);
    for(i=0;i<13;++i)assert(htCiOpen("io-host.bin",0,0)==D_002E6FD0+12-i);
    assert(!htCiOpen("io-host.bin",0,0));for(i=0;i<13;++i)func_0028BD28(D_002E6FD0+i);
    assert(D_002D3380==D_002D3384 && !D_002D3320.used && errors==9);
    unsetenv("BT4_ISO");unlink(iso_path);snprintf(path,sizeof(path),"%s/DATA/io-host.bin",root);unlink(path);snprintf(path,sizeof(path),"%s/DATA/io-large.bin",root);unlink(path);snprintf(path,sizeof(path),"%s/DATA",root);rmdir(path);rmdir(root);
}
