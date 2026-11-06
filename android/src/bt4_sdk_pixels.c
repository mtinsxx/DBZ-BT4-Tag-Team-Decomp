/* Portable scalar equivalents of the original R5900 MMI sampling kernels.
   This is native C compiled ahead of time, not an instruction interpreter.
   Register-level evidence and rounding quirks: SDK_RECONSTRUCTION.md. */
#include "bt4_sdk_pixels.h"
#include "bt4_guest_abi.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(Bt4MmiBlock) == 0x1C, "MMI argument-record layout");
enum Filter { COPY, VERTICAL, HORIZONTAL, BILINEAR };

static void sample(uint16_t *out, uint32_t a, uint32_t b, unsigned shift,
                   unsigned width, int horizontal) {
    uint8_t packed[32];
    unsigned i;
    memcpy(packed, Bt4Android_DecodePointer(a), width);
    memcpy(packed + width, Bt4Android_DecodePointer(b), width);
    if (width == 8) memcpy(packed + 16, packed, 16); /* QFSRV with identical halves rotates */
    for (i = 0; i < width; ++i)
        out[i] = packed[shift + i] + (horizontal ? packed[shift + i + 1] : 0);
}

static int32_t rows(int32_t count, int first, int vertical) {
    if (count == INT32_MIN) abort(); /* original ADDI decrement overflow */
    if (count > 0) return count;
    if (count != 0 || (first && !vertical)) return 1;
    return 0;
}

static void run(const Bt4MmiBlock *block, int chroma, enum Filter filter, int blend) {
    unsigned plane, width = chroma ? 8 : 16;
    int vertical = filter == VERTICAL || filter == BILINEAR;
    int horizontal = filter == HORIZONTAL || filter == BILINEAR;
    int first_output = 1;
    uint32_t step = (uint32_t)block->stride;
    for (plane = 0; plane < (chroma ? 2u : 1u); ++plane) {
        uint32_t a = block->source_a + plane * 64;
        uint32_t b = block->source_b + plane * 64;
        uint32_t destination = block->destination + plane * 128;
        uint16_t previous[16], current[16], old[16], result[16];
        unsigned phase, shift = block->x_shift & 15;
        if (vertical) sample(previous, a, b, shift, width, horizontal);
        for (phase = 0; phase < 2; ++phase) {
            int32_t row, count, height = phase ? block->second_height : block->first_height;
            if (phase) { a += chroma ? 0x140 : 0x80; b += chroma ? 0x140 : 0x80; }
            /* Chroma vertical kernels derive the phase mask with PSRAH on
               their 0x8000 control bit: only the low 16 height bits gate it. */
            count = phase && chroma && vertical && (((uint32_t)height & 0xFFFF) == 0) ?
                0 : rows(height, !phase, vertical);
            for (row = 0; row < count; ++row) {
                unsigned column;
                if (vertical) { a += step; b += step; }
                sample(current, a, b, shift, width, horizontal);
                if (blend) memcpy(old, Bt4Android_DecodePointer(destination), width * 2);
                for (column = 0; column < width; ++column) {
                    uint16_t value;
                    if (vertical) {
                        unsigned bias = filter == BILINEAR ? 2 : 1;
                        /* 0x29FDB0 changes its interpolation constant from two
                           to one after its first output. Keep this observed EE
                           behavior instead of replacing it with ideal MPEG math. */
                        if (filter == BILINEAR && blend && !chroma && !first_output) bias = 1;
                        value = (uint16_t)((previous[column] + current[column] + bias) >>
                            (filter == BILINEAR ? 2 : 1));
                    } else value = horizontal ? (uint16_t)((current[column] + 1) >> 1) : current[column];
                    if (blend) {
                        uint16_t sum = (uint16_t)(value + old[column]); /* PADDH wraps */
                        unsigned conditional = chroma || ((filter == VERTICAL || filter == HORIZONTAL) && column >= 8);
                        unsigned bias = conditional ? (sum != 0 && sum < 0x8000) : 1;
                        value = (uint16_t)((uint16_t)(sum + bias) >> 1); /* PSRLH, not arithmetic */
                    }
                    result[column] = value;
                }
                memcpy(Bt4Android_DecodePointer(destination), result, width * 2);
                if (vertical) memcpy(previous, current, width * 2);
                else { a += step; b += step; }
                destination += step * 2;
                first_output = 0;
            }
        }
    }
}

void func_0029F2F8(const Bt4MmiBlock *block) { run(block, 0, COPY, 0); }
void func_0029F370(const Bt4MmiBlock *block) { run(block, 1, COPY, 0); }
void func_0029F408(const Bt4MmiBlock *block) { run(block, 0, VERTICAL, 0); }
void func_0029F4C0(const Bt4MmiBlock *block) { run(block, 1, VERTICAL, 0); }
void func_0029F590(const Bt4MmiBlock *block) { run(block, 0, HORIZONTAL, 0); }
void func_0029F640(const Bt4MmiBlock *block) { run(block, 1, HORIZONTAL, 0); }
void func_0029F6F8(const Bt4MmiBlock *block) { run(block, 0, BILINEAR, 0); }
void func_0029F7F0(const Bt4MmiBlock *block) { run(block, 1, BILINEAR, 0); }
void func_0029F8F0(const Bt4MmiBlock *block) { run(block, 0, COPY, 1); }
void func_0029F990(const Bt4MmiBlock *block) { run(block, 1, COPY, 1); }
void func_0029FA40(const Bt4MmiBlock *block) { run(block, 0, VERTICAL, 1); }
void func_0029FB20(const Bt4MmiBlock *block) { run(block, 1, VERTICAL, 1); }
void func_0029FC08(const Bt4MmiBlock *block) { run(block, 0, HORIZONTAL, 1); }
void func_0029FCE0(const Bt4MmiBlock *block) { run(block, 1, HORIZONTAL, 1); }
void func_0029FDB0(const Bt4MmiBlock *block) { run(block, 0, BILINEAR, 1); }
void func_0029FED0(const Bt4MmiBlock *block) { run(block, 1, BILINEAR, 1); }
