#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hash_api.h"

#define HASH_CHUNK_SIZE 4096

int binary_hash(HashAPI hash_api, void *ctx, FILE *file_pointer, unsigned char *hash_key_pointer)
{
    unsigned char buffer[HASH_CHUNK_SIZE];
    size_t elements_read;
    hash_api.init(ctx);

    while ((elements_read = fread(buffer, 1, HASH_CHUNK_SIZE, file_pointer)) > 0)
        hash_api.update(ctx, buffer, elements_read);

    if (ferror(file_pointer))
        return 1;

    hash_api.final(ctx, hash_key_pointer);

    return 0;
}

void string_hash(HashAPI hash_api, void *ctx, unsigned char *hash_key_pointer, char *string)
{
    hash_api.init(ctx);
    hash_api.update(ctx, (unsigned char *)string, strlen(string));
    hash_api.final(ctx, hash_key_pointer);
}
