/* Base of the 32-bit guest-address window in an ASLR-loaded Android .so.
 *
 * The ELF shared object is equivalent to the desktop image positioned at
 * guest +0x20000000. dladdr obtains the actual relocated load bias instead
 * of requesting unsafe fixed image addresses or disabling ASLR.
 *
 * This is a necessary but not sufficient part of porting the runtime:
 * the LLVM game-pointer pass and the Android linker must honor this base.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "../include/bt4_address_window.h"
#include "../../port/src/port_addr.h"
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __ANDROID__
void *Port_Window = NULL;

int Bt4Android_WindowConfigure(void) {
    Dl_info info;
    uintptr_t image, base;
    Bt4AddressWindow window;

    if (Port_Window != NULL) return 0;
    if (dladdr((const void *)&Port_Window, &info) == 0 ||
        info.dli_fbase == NULL) {
        fprintf(stderr, "bt4 android: cannot find the loaded ELF image base\n");
        return -1;
    }
    image = (uintptr_t)info.dli_fbase;
    if (image <= (uintptr_t)0x20000000u) {
        fprintf(stderr, "bt4 android: ELF image is below required guest image offset\n");
        return -1;
    }
    base = image - (uintptr_t)0x20000000u;
    if (Bt4Addr_Init(&window, base) != BT4_ADDR_OK) {
        fprintf(stderr, "bt4 android: ASLR load address cannot represent a 4 GiB guest window\n");
        return -1;
    }
    Port_Window = (void *)window.base;
    return 0;
}

/* Compact table/data constructors need the window before encoding addresses.
 * Memory mappings and user-data import are deferred until SDL_main.
 */
__attribute__((constructor(101))) static void Bt4Android_EarlyWindow(void) {
    (void)Bt4Android_WindowConfigure();
}
#endif
