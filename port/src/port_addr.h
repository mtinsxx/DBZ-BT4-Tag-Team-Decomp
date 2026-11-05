/* Game addresses in the port's own code.

   The game keeps addresses in 4 bytes (its pointers, the DMA chains, the hardware registers). On Linux and Windows a
   game address IS the host address: everything the game can reach lies below 4 GB. On macOS the low 4 GB cannot be
   mapped, and a game address is an offset in a 4 GB window that starts 0x20000000 below the program
   (port/tools/macwin/macwin.cpp, docs/port/macos_plan.md).

   Port_Ptr: a game address (a 4-byte value read from the game's memory, a PS2 address) -> a pointer to use here.
   Port_Addr: a pointer to memory the game can reach -> the game address to hand it or to store in its memory.
   Both keep 0 / NULL. On Linux and Windows they are plain casts. Pointers the game passes to a port function, and
   those a port function returns, need neither: they cross calls as ordinary pointers on every system. */
#ifndef PORT_ADDR_H
#define PORT_ADDR_H
#include <stdint.h>

#if defined(__ANDROID__)
/* Android's shared libraries load with PIE/ASLR. The game address remains
 * 32-bit, but the host address is W + offset, just like macOS's window
 * model. W must be configured from the actual loaded library before
 * Port_MapMemory is allowed to reserve guest memory.
 *
 * This only covers uses of Port_Ptr/Port_Addr in port sources. The game
 * sources additionally require LLVM lowering of every __ptr32 dereference,
 * conversion and boundary call. Do not treat this as complete by itself.
 */
extern void *Port_Window;
#define PORT_WINDOW ((uintptr_t)Port_Window)
static inline void *Port_Ptr(uint32_t addr) {
    return addr != 0 ? (void *)(PORT_WINDOW + (uintptr_t)addr) : (void *)0;
}
static inline uint32_t Port_Addr(const volatile void *p) {
    return p != 0 ? (uint32_t)((uintptr_t)p - PORT_WINDOW) : 0;
}
#elif defined(__APPLE__)
#include <mach-o/ldsyms.h> /* _mh_execute_header: the program's first byte */
#define PORT_WINDOW ((uintptr_t)&_mh_execute_header - 0x20000000u)
static inline void *Port_Ptr(uint32_t addr) {
    return addr != 0 ? (void *)(PORT_WINDOW + addr) : (void *)0;
}
static inline uint32_t Port_Addr(const volatile void *p) {
    return p != 0 ? (uint32_t)((uintptr_t)p - PORT_WINDOW) : 0;
}
#else
/* Linux and Windows: the casts themselves, as the code had them before these names existed (macros, not functions:
   the compiler sees what it saw then, whatever its options). */
#define Port_Ptr(addr) ((void *)(uintptr_t)(uint32_t)(addr))
#define Port_Addr(p) ((uint32_t)(uintptr_t)(p))
#endif
#endif
