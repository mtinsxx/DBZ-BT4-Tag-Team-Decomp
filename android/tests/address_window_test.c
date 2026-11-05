#include "bt4_address_window.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

static void test_guest_host_conversions(void) {
    Bt4AddressWindow w;
    void *p = (void *)1;
    uint32_t v = 17;
    uintptr_t base = (uintptr_t)0x10000000000ULL;
    assert(Bt4Addr_Init(&w, base) == BT4_ADDR_OK);
    assert(Bt4Addr_ToHost(&w, 0, &p) == BT4_ADDR_OK && p == NULL);
    assert(Bt4Addr_ToHost(&w, 0x12345678, &p) == BT4_ADDR_OK);
    assert((uintptr_t)p == base + 0x12345678);
    assert(Bt4Addr_ToGuest(&w, p, &v) == BT4_ADDR_OK && v == 0x12345678);
    assert(Bt4Addr_ToGuest(&w, NULL, &v) == BT4_ADDR_OK && v == 0);
    assert(Bt4Addr_ToGuest(&w, (void *)(base - 1u), &v) ==
           BT4_ADDR_OUTSIDE_WINDOW && v == 0);
    assert(Bt4Addr_ToGuest(&w, (void *)(base + (uintptr_t)UINT32_MAX + 1u), &v) ==
           BT4_ADDR_OUTSIDE_WINDOW && v == 0);
    assert(Bt4Addr_Init(&w, UINTPTR_MAX - 7u) == BT4_ADDR_INVALID);
    assert(Bt4Addr_Init(NULL, base) == BT4_ADDR_INVALID);
    assert(Bt4Addr_ToHost(&w, 42, NULL) == BT4_ADDR_INVALID);
}

static void test_non_destructive_mapping(void) {
    const uint32_t guest = 0x20000000u;
    Bt4AddressWindow w;
    void *reserved, *mapped;
    long page = sysconf(_SC_PAGESIZE);
    int result;
    assert(page > 0);
    reserved = mmap(NULL, (size_t)page * 2u, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(reserved != MAP_FAILED);
    assert((uintptr_t)reserved > guest);
    assert(munmap(reserved, (size_t)page * 2u) == 0);
    assert(Bt4Addr_Init(&w, (uintptr_t)reserved - guest) == BT4_ADDR_OK);
    result = Bt4Addr_MapWritable(&w, guest, (size_t)page * 2u, &mapped);
    assert(result == BT4_ADDR_OK && mapped == reserved);
    ((volatile unsigned char *)mapped)[0] = 0x5A;
    assert(Bt4Addr_MapWritable(&w, guest, (size_t)page, &reserved) ==
           BT4_ADDR_OS_FAILURE);
    assert(reserved == NULL);
    assert(((volatile unsigned char *)mapped)[0] == 0x5A);
    assert(Bt4Addr_MapWritable(&w, UINT32_MAX - 2u, 4u, &reserved) ==
           BT4_ADDR_INVALID);
    assert(munmap(mapped, (size_t)page * 2u) == 0);
}

int main(void) {
    test_guest_host_conversions();
    test_non_destructive_mapping();
    puts("Address window: conversions and non-destructive mmap passed.");
    return 0;
}
