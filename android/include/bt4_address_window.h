/* Android ARM64 guest address-window primitives.
 *
 * The original game stores PS2-sized (uint32_t) pointers. This API
 * translates address values without truncating 64-bit host pointers.
 *
 * This is ONE prerequisite, not the complete LLVM pointer-lowering pass:
 * every use of a 32-bit pointer inside translated game code must also
 * obey the same conversion rule. See android/README.md.
 */
#ifndef BT4_ADDRESS_WINDOW_H
#define BT4_ADDRESS_WINDOW_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Bt4AddressWindow {
    uintptr_t base;
} Bt4AddressWindow;

typedef struct Bt4MapPlan {
    uintptr_t window, requested, aligned;
    uint32_t guest;
    size_t bytes, page_size, mapped_bytes;
} Bt4MapPlan;
typedef struct Bt4MapTrace {
    Bt4MapPlan plan;
    uintptr_t returned;
    int attempted, protection, flags, result, error, noreplace_error, used_hint, cleanup_error;
} Bt4MapTrace;

/* 0 = success, negatives denote a rejected operation. */
enum {
    BT4_ADDR_OK = 0,
    BT4_ADDR_INVALID = -1,
    BT4_ADDR_OUTSIDE_WINDOW = -2,
    BT4_ADDR_OS_FAILURE = -3
};

/* 'base' is an ASLR-selected origin: guest 0 means NULL, guest N
 * means (base + N). Initialization rejects host arithmetic overflow.
 * The caller must supply a base compatible with the game's placement.
 */
int Bt4Addr_Init(Bt4AddressWindow *window, uintptr_t base);

/* Convert a guest address. guest == 0 round-trips to NULL. */
int Bt4Addr_ToHost(const Bt4AddressWindow *window, uint32_t guest,
                   void **host_out);
int Bt4Addr_ToGuest(const Bt4AddressWindow *window, const void *host,
                    uint32_t *guest_out);

/* Attempt to map a writable range at exactly base + guest. Never overwrite
 * an existing process mapping. Handles 4 KB and 16 KB host page sizes.
 * NULL output on failure; see return code / errno for diagnosis.
 * Linux / Android only. The mapped pages belong to the caller.
 */
int Bt4Addr_MapWritable(const Bt4AddressWindow *window, uint32_t guest,
                        size_t bytes, void **mapped_out);
/* Pure arithmetic, also tested with simulated 16/64 KiB page sizes. */
int Bt4Addr_Plan(const Bt4AddressWindow *window,uint32_t guest,size_t bytes,
                 size_t page_size,Bt4MapPlan *plan);
/* Only NONE or READ|WRITE; never executable. Exact-address hint fallback is
   allowed only when the kernel rejects NOREPLACE as unsupported. All returned
   addresses are checked; a different mapping is immediately released. */
int Bt4Addr_Map(const Bt4AddressWindow *window,uint32_t guest,size_t bytes,
                int protection,void **mapped_out,Bt4MapTrace *trace);

#ifdef __cplusplus
}
#endif
#endif
