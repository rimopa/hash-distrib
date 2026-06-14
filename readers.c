#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hash_api.h"

#define HASH_CHUNK_SIZE 4096

// Get file size and rewind file pointer
long get_file_size(FILE *file_pointer)
{
    fseek(file_pointer, 0, SEEK_END);
    long file_size = ftell(file_pointer);
    rewind(file_pointer);
    return file_size;
}

void binary_hash(HashAPI hash_api, void *ctx, FILE *file_pointer, unsigned char *hash_key_pointer)
{
    long file_size = get_file_size(file_pointer);

    unsigned int full_chunk_count = file_size / HASH_CHUNK_SIZE;

    hash_api.init(ctx);

    {
        unsigned char buffer[HASH_CHUNK_SIZE];
        for (unsigned int i = 0; i < full_chunk_count; i++)
        {
            fread(buffer, 1, HASH_CHUNK_SIZE, file_pointer);
            hash_api.update(ctx, buffer, HASH_CHUNK_SIZE);
        }
    }

    {
        size_t final_read_size = file_size - HASH_CHUNK_SIZE * full_chunk_count;
        unsigned char buffer[final_read_size];
        fread(buffer, 1, final_read_size, file_pointer);
        hash_api.update(ctx, buffer, final_read_size);
    }
    hash_api.final(ctx, hash_key_pointer);
}

void string_hash(HashAPI hash_api, void *ctx, unsigned char *hash_key_pointer, char *string)
{
    hash_api.init(ctx);
    hash_api.update(ctx, (unsigned char *)string, strlen(string));
    hash_api.final(ctx, hash_key_pointer);
}