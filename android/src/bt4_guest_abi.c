#include "bt4_guest_abi.h"
#include "port_addr.h"
#include "port_host.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Registered by .init_array before guest constructors execute, then immutable.
 * A module must remain loaded while guest code may call its function pointers. */
PORT_HOST static Bt4AbiModule *modules;
PORT_HOST static uint64_t registration_generation;
typedef struct AbiCache { const void *function; const char *kinds; } AbiCache;
static _Thread_local AbiCache cache[128];
static _Thread_local uint64_t cache_generation;

int Bt4Android_TryEncodePointer(const void *pointer, uint32_t *address)
{
    uintptr_t value = (uintptr_t)pointer, base = (uintptr_t)Port_Window;
    if (address == NULL) return -1;
    if (pointer == NULL) { *address = 0; return 0; }
    /* Offset zero belongs to NULL, including a non-null pointer at W. */
    if (base == 0 || value <= base || value - base > UINT32_MAX) return -1;
    *address = (uint32_t)(value - base);
    return 0;
}

uint32_t Bt4Android_EncodePointer(const void *pointer)
{
    uint32_t address;
    if (Bt4Android_TryEncodePointer(pointer, &address) != 0) {
        fprintf(stderr, "bt4: pointer %p is outside guest window %p + [1, 0xffffffff]\n", pointer, Port_Window);
        abort();
    }
    return address;
}

void Bt4Android_ApplyRelocations(const Bt4DataRelocation *table, size_t count)
{
    size_t i;
    for (i = 0; i < count; ++i) {
        uint32_t address = Bt4Android_EncodePointer(table[i].target);
        memcpy(table[i].slot, &address, sizeof(address));
    }
}

void Bt4Android_RegisterSignatures(Bt4AbiModule *module)
{
    module->next = modules;
    modules = module;
    ++registration_generation;
}

static const char *signature(const void *function)
{
    const Bt4AbiModule *module;
    const char *k = NULL;
    uintptr_t hash = ((uintptr_t)function >> 4) ^ ((uintptr_t)function >> 12);
    AbiCache *entry = &cache[hash & 127];
    if (cache_generation != registration_generation) {
        memset(cache, 0, sizeof(cache));
        cache_generation = registration_generation;
    }
    if (entry->function == function) k = entry->kinds;
    else {
        for (module = modules; module != NULL; module = module->next) {
            uint64_t i;
            for (i = 0; i < module->count; ++i) {
                const Bt4AbiEntry *candidate = &module->entries[i];
                if (candidate->function == function) {
                    k = candidate->kinds;
                    break;
                }
            }
            if (k != NULL) break;
        }
        entry->function = function;
        entry->kinds = k;
    }
    return k;
}

int Bt4Android_CallKind(const void *function, uint32_t slot)
{
    const char *k = signature(function);
    uint32_t j;
    if (k == NULL) return 0; /* Foreign callback: caller's signature. */
    if (*k == '!') ++k;
    for (j = 0; j < slot && k[j] != '\0' && k[j] != '.'; ++j) {}
    return j == slot && k[j] != '.' ? (unsigned char)k[j] : 0;
}

void Bt4Android_RequireSoftFloatCallback(const void *function)
{
    const char *k = signature(function);
    if (k == NULL || *k == '!' || strchr(k, '.') != NULL) {
        fprintf(stderr, "bt4: native/unknown floating callback %p requires an ABI adapter\n", function);
        abort();
    }
}
