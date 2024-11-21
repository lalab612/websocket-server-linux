
#ifndef _SHA1_H_
#define _SHA1_H_

#include "ws_core.h"

#define SHA_CIRCULAR_SHIFT(bits, word) ((((word) << (bits)) & 0xFFFFFFFF) | ((word) >> (32 - (bits))))

#define SHA_DIGEST_LENGTH 20

typedef struct sha1_context_t
{
    unsigned message_digest[5];
    unsigned length_low;
    unsigned length_high;
    unsigned char message_block[64];
    int message_block_index;
    int computed;
    int corrupted;
} sha1_context_t;

int ws_sha1(char *dst, const char *src);

#endif
