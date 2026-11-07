#ifndef BT4_ISO_SOURCE_H
#define BT4_ISO_SOURCE_H
#include <stdint.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Duplicates a seekable descriptor. No copy, no borrowed descriptor lifetime.
   start/length delimit an AssetFileDescriptor slice. length=-1 uses fstat. */
int Bt4Iso_MountFd(int fd,int64_t start,int64_t length);
void Bt4Iso_Unmount(void);
int Bt4Iso_IsMounted(void);
FILE *Bt4Iso_Open(void);
/* Relative native-port paths: disc/BIN/..., disc/DATA/..., pzs4usN/00042.bin.
   Returns an independently seekable, read-only bounded view (including AFS).
   No extraction/cache on disk and no reads beyond a member's actual extent. */
FILE *Bt4Iso_OpenFile(const char *relative);
/* Thread-local last open diagnostic, copied before the next OpenFile call. */
const char *Bt4Iso_LastError(void);
#ifdef __cplusplus
}
#endif
#endif
