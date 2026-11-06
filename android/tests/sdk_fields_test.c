#include "bt4_sdk_fields.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

/* Test storage for the genuine imported global fields, no game data. */
uint32_t D_002D3FE4, D_002D3FE8, D_002D97F4, D_002D97F8, D_002E6F5C, D_002E6F60;

int main(void) {
    uint32_t object[18], expected[18];
    func_0027BD48(0x80001234u, 0xFFFFFFFFu);
    assert(D_002D3FE4 == 0x80001234u && D_002D3FE8 == 0xFFFFFFFFu);
    func_0027F7F8(0x11223344u, 0x87654321u);
    assert(D_002D97F4 == 0x11223344u && D_002D97F8 == 0x87654321u);
    func_0028B838(0xFEDCBA98u, 0);
    assert(D_002E6F5C == 0xFEDCBA98u && D_002E6F60 == 0);
    func_0027BD48(0, 0);
    assert(D_002D3FE4 == 0 && D_002D3FE8 == 0);
    assert(func_0027C4E0() == 2048 && func_0028C158() == 2048);
    memset(object, 0xA5, sizeof(object));
    memcpy(expected, object, sizeof(object));
    object[2] = expected[2] = 0xFFEEDDCCu;
    assert(func_00283330(object) == 0xFFEEDDCCu);
    func_00283338(object, 0xF0000001u, 0xFFFFFFFFu);
    expected[14] = 0xF0000001u;
    expected[15] = 0xFFFFFFFFu;
    assert(memcmp(object, expected, sizeof(object)) == 0);
    func_00283338(object, 0, 0);
    assert(object[14] == 0 && object[15] == 0);
    return 0;
}
