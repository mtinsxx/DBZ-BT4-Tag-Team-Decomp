#ifndef BT4_PORT_READ_FILE_H
#define BT4_PORT_READ_FILE_H
#include <stdio.h>
#if defined(__ANDROID__) || defined(BT4_ANDROID_ABI_TEST)
#include "../../android/include/bt4_iso_source.h"
#include <string.h>
#endif
static inline FILE *Port_ReadFile(const char *path) {
#if defined(__ANDROID__) || defined(BT4_ANDROID_ABI_TEST)
    if(path&&!strncmp(path,"iso://",6))return Bt4Iso_OpenFile(path+6);
#endif
    return fopen(path,"rb");
}
#endif
