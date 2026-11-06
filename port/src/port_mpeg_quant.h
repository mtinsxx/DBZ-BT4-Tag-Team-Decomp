#ifndef PORT_MPEG_QUANT_H
#define PORT_MPEG_QUANT_H
#include <stdint.h>
/* Standard MPEG-2 tables shared with the existing native movie decoder.
   Defaults/load input use transmitted zigzag order; raster output is for DCT. */
void Port_MpegQuantDefaults(uint8_t intra[64], uint8_t non_intra[64]);
void Port_MpegQuantRaster(const uint8_t wire[64], uint8_t raster[64]);
#endif
