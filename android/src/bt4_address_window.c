#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "bt4_address_window.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

#if !defined(__linux__)
#error "Bt4Addr_MapWritable requires Android/Linux mmap semantics"
#endif

/* The flag is available on modern Android/Linux headers. Defining its
 * Linux UAPI value keeps a build with older NDK headers compilable.
 * Old kernels may ignore the unknown flag: verify the returned address.
 */
#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

int Bt4Addr_Init(Bt4AddressWindow *window, uintptr_t base) {
    if (window == NULL || base == 0 || base > UINTPTR_MAX - UINT32_MAX) {
        return BT4_ADDR_INVALID;
    }
    window->base = base;
    return BT4_ADDR_OK;
}

int Bt4Addr_ToHost(const Bt4AddressWindow *window, uint32_t guest,
                   void **host_out) {
    if (host_out == NULL) return BT4_ADDR_INVALID;
    *host_out = NULL;
    if (window == NULL || window->base == 0 ||
        window->base > UINTPTR_MAX - UINT32_MAX) return BT4_ADDR_INVALID;
    if (guest != 0) {
        *host_out = (void *)(window->base + (uintptr_t)guest);
    }
    return BT4_ADDR_OK;
}

int Bt4Addr_ToGuest(const Bt4AddressWindow *window, const void *host,
                    uint32_t *guest_out) {
    uintptr_t address;
    if (guest_out == NULL) return BT4_ADDR_INVALID;
    *guest_out = 0;
    if (window == NULL || window->base == 0 ||
        window->base > UINTPTR_MAX - UINT32_MAX) return BT4_ADDR_INVALID;
    if (host == NULL) return BT4_ADDR_OK;
    address = (uintptr_t)host;
    if (address <= window->base ||
        address - window->base > UINT32_MAX) {
        return BT4_ADDR_OUTSIDE_WINDOW;
    }
    *guest_out = (uint32_t)(address - window->base);
    return BT4_ADDR_OK;
}

int Bt4Addr_MapWritable(const Bt4AddressWindow *window, uint32_t guest,
                        size_t bytes, void **mapped_out) {
    void *target = NULL;
    void *actual;
    uintptr_t start;
    uintptr_t aligned;
    size_t prefix, rounded;
    long page_size;
    int result;
    if (mapped_out == NULL) return BT4_ADDR_INVALID;
    *mapped_out = NULL;
    if (bytes == 0 || guest == 0 ||
        bytes > (uint64_t)UINT32_MAX + 1u - guest) {
        return BT4_ADDR_INVALID;
    }
    result = Bt4Addr_ToHost(window, guest, &target);
    if (result != BT4_ADDR_OK) return result;
    page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0 || ((unsigned long)page_size & ((unsigned long)page_size - 1u))) {
        return BT4_ADDR_OS_FAILURE;
    }
    start = (uintptr_t)target;
    aligned = start & ~((uintptr_t)page_size - 1u);
    if (aligned < window->base) return BT4_ADDR_OUTSIDE_WINDOW;
    prefix = (size_t)(start - aligned);
    if (bytes > SIZE_MAX - prefix) return BT4_ADDR_INVALID;
    rounded = prefix + bytes;
    if (rounded > SIZE_MAX - ((size_t)page_size - 1u)) {
        return BT4_ADDR_INVALID;
    }
    rounded = (rounded + (size_t)page_size - 1u) &
              ~((size_t)page_size - 1u);
    if (rounded > UINTPTR_MAX - aligned) return BT4_ADDR_OUTSIDE_WINDOW;
    /* Rounding must not consume bytes beyond the guest 4 GiB window. */
    if (aligned + rounded > window->base + (uintptr_t)UINT32_MAX + 1u) {
        return BT4_ADDR_OUTSIDE_WINDOW;
    }
    actual = mmap((void *)aligned, rounded, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
                  -1, 0);
    if (actual == MAP_FAILED) return BT4_ADDR_OS_FAILURE;
    if ((uintptr_t)actual != aligned) {
        /* Older kernels can silently interpret NOREPLACE as a hint. */
        munmap(actual, rounded);
        errno = EEXIST;
        return BT4_ADDR_OS_FAILURE;
    }
    *mapped_out = target;
    return BT4_ADDR_OK;
}
