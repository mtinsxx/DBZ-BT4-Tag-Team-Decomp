#ifndef BT4_BASE_PROFILE_H
#define BT4_BASE_PROFILE_H
#include <stdint.h>
#include <stdio.h>
typedef struct Bt4FileFingerprint { uint64_t size; uint32_t crc; int error; } Bt4FileFingerprint;
int Bt4Base_Fingerprint(FILE *file,Bt4FileFingerprint *out);
/* Original catalogue: 0 Spanish, 1 English, -1 unsupported. No marker fallback. */
int Bt4Base_MainLanguage(const Bt4FileFingerprint *value);
int Bt4Base_Matches(unsigned index,int language,const Bt4FileFingerprint *value);
#endif
