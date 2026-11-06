#ifndef BT4_ANDROID_BOOT_H
#define BT4_ANDROID_BOOT_H
#include <stddef.h>
#include <stdint.h>
typedef struct Bt4GameDataRun {
    unsigned char *destination;
    uint32_t source, offset, size;
} Bt4GameDataRun;
/* Validate all source ranges and checksum BEFORE modifying any destination.
   approved_profile means the caller has independently verified the complete
   supported program-file profile, not just the supplied main ELF. */
int Bt4Android_ImportPrograms(const void *elf, size_t elf_size,
    const void *overlay, size_t overlay_size, const Bt4GameDataRun *runs,
    size_t count, uint64_t checksum, int approved_profile);
extern const Bt4GameDataRun bt4_game_data_runs[];
extern const size_t bt4_game_data_count;
extern const uint64_t bt4_game_data_checksum;
#ifdef __ANDROID__
int Port_AndroidMemoryPrepare(void);
int Port_AndroidRunGame(void);
#endif
#endif
