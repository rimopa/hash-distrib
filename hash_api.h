#ifndef HASH_DISTRIB_HASH_API_TYPE
#define HASH_DISTRIB_HASH_API_TYPE

#include <stddef.h>

typedef struct
{
    const char *name; // Name of your hash algorithm
    size_t ctx_size;  // Size you need for your context in bytes
    size_t out_size;  // Size of your returned key in bytes. Must be >= 1 and <= 255
    void (*init)(void *ctx);
    void (*update)(void *ctx, const unsigned char *data, size_t len);
    void (*final)(void *ctx, unsigned char *out);
} HashAPI;

#endif