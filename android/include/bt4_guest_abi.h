#ifndef BT4_GUEST_ABI_H
#define BT4_GUEST_ABI_H
#include <stddef.h>
#include <stdint.h>
extern void *Port_Window;

/* ABI shared with the LLVM ELF pass. No guest pointers occur in these records. */
typedef struct Bt4AbiEntry {
    const void *function;
    const char *kinds; /* optional native-FP '!', result/parameters p/i/x/v, '.' for varargs */
} Bt4AbiEntry;
typedef struct Bt4AbiModule {
    struct Bt4AbiModule *next;
    const Bt4AbiEntry *entries;
    uint64_t count;
} Bt4AbiModule;
typedef struct Bt4DataRelocation {
    uint32_t *slot;
    const void *target;
} Bt4DataRelocation;

int Bt4Android_TryEncodePointer(const void *pointer, uint32_t *address);
uint32_t Bt4Android_EncodePointer(const void *pointer);
void Bt4Android_RegisterSignatures(Bt4AbiModule *module);
int Bt4Android_CallKind(const void *function, uint32_t slot);
void Bt4Android_RequireSoftFloatCallback(const void *function);
void Bt4Android_ApplyRelocations(const Bt4DataRelocation *table, size_t count);
#endif
