#include <assert.h>
#include <stdint.h>
extern uint32_t __floatundisf(uint64_t);
int main(void) {
    assert(__floatundisf(0) == 0);
    assert(__floatundisf(1) == 0x3f800000u);
    assert(__floatundisf(UINT64_C(0x100000001)) == 0x4f800000u);
    assert(__floatundisf(UINT64_MAX) == 0x5f800000u);
    assert(__floatundisf(16777217) == 0x4b800000u);
    return 0;
}
