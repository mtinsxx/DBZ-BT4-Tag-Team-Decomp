#include "bt4_sdk_pixels.h"
#include "bt4_guest_abi.h"
#include <assert.h>
#include <string.h>

void *Port_Window;
_Alignas(16) static uint8_t left[2048], right[2048];
_Alignas(16) static uint16_t destination[512], expected[512];
typedef void (*Kernel)(const Bt4MmiBlock *);
static Kernel kernels[8][2] = {
    {func_0029F2F8,func_0029F370}, {func_0029F408,func_0029F4C0},
    {func_0029F590,func_0029F640}, {func_0029F6F8,func_0029F7F0},
    {func_0029F8F0,func_0029F990}, {func_0029FA40,func_0029FB20},
    {func_0029FC08,func_0029FCE0}, {func_0029FDB0,func_0029FED0}
};

/* Pixel-coordinate oracle independent of the implementation's packed-row
   buffers. The phase seam retains the preceding row for vertical prediction. */
static unsigned pixel(unsigned plane, unsigned offset, unsigned x, int chroma) {
    unsigned width = chroma ? 8 : 16;
    unsigned position = chroma ? x % 16 : x;
    return position < width ? left[plane * 64 + offset + position] :
                              right[plane * 64 + offset + position - width];
}
static void reference(const Bt4MmiBlock *block, unsigned mode, unsigned chroma) {
    unsigned plane, filter = mode % 4, blend = mode / 4, first_result = 1;
    unsigned width = chroma ? 8 : 16, vertical = filter == 1 || filter == 3;
    unsigned horizontal = filter == 2 || filter == 3;
    unsigned h1 = block->first_height > 0 ? (unsigned)block->first_height :
        (block->first_height || !vertical ? 1 : 0);
    unsigned h2 = block->second_height > 0 ? (unsigned)block->second_height :
        (block->second_height ? 1 : 0);
    if (chroma && vertical && (((uint32_t)block->second_height & 0xFFFF) == 0)) h2 = 0;
    for (plane = 0; plane < (chroma ? 2u : 1u); ++plane) {
        unsigned row, previous_offset = 0;
        for (row = 0; row < h1 + h2; ++row) {
            unsigned phase = row >= h1;
            unsigned offset = (row + vertical) * (unsigned)block->stride + (phase ? (chroma ? 320 : 128) : 0);
            unsigned column;
            for (column = 0; column < width; ++column) {
                unsigned x = (block->x_shift & 15) + column;
                unsigned sum = pixel(plane,offset,x,chroma);
                unsigned predicted, index = plane * 64 + row * (unsigned)block->stride + column;
                if (horizontal) sum += pixel(plane,offset,x+1,chroma);
                if (vertical) {
                    unsigned bias = filter == 3 ? 2 : 1;
                    sum += pixel(plane,previous_offset,x,chroma);
                    if (horizontal) sum += pixel(plane,previous_offset,x+1,chroma);
                    if (mode == 7 && !chroma && !first_result) bias = 1;
                    predicted = (sum + bias) / (filter == 3 ? 4 : 2);
                } else predicted = horizontal ? (sum + 1) / 2 : sum;
                if (blend) {
                    unsigned wrapped = (predicted + expected[index]) % 65536;
                    unsigned bias = chroma || ((filter == 1 || filter == 2) && column >= 8) ?
                        (wrapped > 0 && wrapped < 32768) : 1;
                    predicted = ((wrapped + bias) % 65536) / 2;
                }
                expected[index] = (uint16_t)predicted;
            }
            previous_offset = offset;
            first_result = 0;
        }
    }
}

int main(void) {
    Bt4MmiBlock block;
    unsigned mode, chroma, x, h, i;
    uint32_t random = 0x31415926u;
    Port_Window = (void *)((uintptr_t)left - 0x20000000u);
    block.destination = Bt4Android_EncodePointer(destination);
    block.source_a = Bt4Android_EncodePointer(left);
    block.source_b = Bt4Android_EncodePointer(right);
    for (i = 0; i < sizeof(left); ++i) {
        random = random * 1664525u + 1013904223u; left[i] = (uint8_t)(random >> 24);
        random = random * 1664525u + 1013904223u; right[i] = (uint8_t)(random >> 24);
    }
    for (mode = 0; mode < 8; ++mode) for (chroma = 0; chroma < 2; ++chroma)
    for (x = 0; x < 16; ++x) for (h = 0; h < 6; ++h) {
        block.x_shift = x | 0xABCD0000u;
        block.first_height = (int32_t)(h % 3);
        block.second_height = (int32_t)(h / 3);
        block.stride = chroma ? 8 : 16;
        if (h == 5) block.stride *= 2;
        for (i = 0; i < 512; ++i) {
            random = random * 1664525u + 1013904223u;
            destination[i] = expected[i] = (uint16_t)(random >> 16);
        }
        reference(&block,mode,chroma);
        kernels[mode][chroma](&block);
        assert(memcmp(destination,expected,sizeof(destination)) == 0);
    }
    /* Known EE quirks: the luma blended bilinear kernel changes its rounding
       constant after its first output. Chroma retains two throughout. */
    for (i = 0; i < sizeof(left); ++i) left[i] = right[i] = (uint8_t)(i & 1);
    memset(destination,0,sizeof(destination));
    block.first_height = 2; block.second_height = 0; block.stride = 16; block.x_shift = 0;
    func_0029FDB0(&block);
    assert(destination[0] == 1 && destination[16] == 0);
    memset(destination,0,sizeof(destination));
    block.stride = 8;
    func_0029FED0(&block);
    assert(destination[0] == 1 && destination[8] == 1 && destination[64] == 1);
    /* Wrapped signed-positive blend rounding differs between luma halves. */
    memset(left,1,sizeof(left)); memset(right,1,sizeof(right));
    for (i = 0; i < 512; ++i) destination[i] = 0xFFFEu;
    block.first_height = 1; block.stride = 16;
    func_0029FA40(&block);
    assert(destination[0] == 0 && destination[8] == 32767);
    for (i = 0; i < 512; ++i) destination[i] = 0xFFFEu;
    block.stride = 8;
    func_0029F990(&block);
    assert(destination[0] == 32767);
    /* PSRAH's chroma phase gate sees only the low 16 height bits. */
    for (i = 0; i < 512; ++i) destination[i] = expected[i] = 0xA5A5;
    block.first_height = 1; block.second_height = 65536;
    reference(&block,1,1);
    func_0029F4C0(&block);
    assert(memcmp(destination,expected,sizeof(destination)) == 0);
    return 0;
}
