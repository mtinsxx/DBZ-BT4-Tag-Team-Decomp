#ifndef BT4_SDK_CI_QUERY_H
#define BT4_SDK_CI_QUERY_H
#include <stdint.h>
#include <stddef.h>
#include "bt4_iso9660.h"
/* Shared provider boundary for the recovered Ci open/read methods. */
int Bt4Ci_DvdLocate(const char *name, Bt4IsoFile *file, int *overridden);
int Bt4Ci_HostOverride(const char *name, uint32_t pair[2]);
int Bt4Ci_HostRelativePath(const char *name, char *out, size_t capacity);
int64_t Bt4Ci_HostSize(const char *name, int report);
int32_t dvCiGetFileSize(const char *name);
int32_t func_0027BDD8(const char *name);
int32_t func_0027C680(const void *target, int32_t command);
int32_t func_0028B850(const char *name);
int32_t func_0028B8F0(const char *name);
int32_t func_0028C288(const void *target, int32_t command);
#endif
