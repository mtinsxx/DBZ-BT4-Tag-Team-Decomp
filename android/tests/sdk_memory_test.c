#include "bt4_sdk_memory.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

typedef int32_t (*Copy)(void *, const void *, uint32_t);

int main(void) {
    Copy word_copies[] = {func_002BA2D0, func_002BA518, func_002BA938, func_002BC748};
    uint32_t source[] = {0x80000000u, 0xFFFFFFFFu, 0x12345678u, 0x01020304u};
    size_t routine;
    uint32_t count;
    for (routine = 0; routine < sizeof(word_copies) / sizeof(word_copies[0]); ++routine) {
        Copy copy = word_copies[routine];
        assert(copy(NULL, NULL, 0) == 0);
        assert(copy(NULL, NULL, 3) == 0);
        for (count = 0; count <= sizeof(source); ++count) {
            uint32_t destination[6];
            uint32_t expected[6];
            memset(destination, 0xA5, sizeof(destination));
            memset(expected, 0xA5, sizeof(expected));
            memcpy((unsigned char *)expected + 4, source, count & ~3u);
            assert(copy(destination + 1, source, count) == 0);
            assert(memcmp(destination, expected, sizeof(expected)) == 0);
        }
        {
            uint32_t overlap[] = {1, 2, 3, 4, 5};
            uint32_t forward[] = {1, 1, 1, 1, 1};
            assert(copy(overlap + 1, overlap, 16) == 0);
            assert(memcmp(overlap, forward, sizeof(overlap)) == 0);
        }
    }
    assert(func_002BA688(NULL, NULL, 0) == 0);
    for (count = 0; count <= sizeof(source); ++count) {
        unsigned char destination[20], expected[20];
        memset(destination, 0xCC, sizeof(destination));
        memset(expected, 0xCC, sizeof(expected));
        memcpy(expected + 1, source, count);
        assert(func_002BA688(destination + 1, source, count) == 0);
        assert(memcmp(destination, expected, sizeof(expected)) == 0);
    }
    {
        unsigned char overlap[] = {1, 2, 3, 4};
        assert(func_002BA688(overlap + 1, overlap, 3) == 0);
        assert(overlap[0] == 1 && overlap[1] == 1 && overlap[2] == 1 && overlap[3] == 1);
    }
    assert(func_002BA308(source, source + 3, 0x80000000u) == source);
    assert(func_002BA308(source, source + 3, 0x12345678u) == source + 2);
    assert(func_002BA308(source, source + 3, 0x01020304u) == NULL);
    assert(func_002BA308(source, source + 3, 42) == NULL);
    assert(func_002BA308(source, source, source[0]) == NULL);
    assert(func_002BA308(source + 1, source, source[1]) == NULL);
    return 0;
}
