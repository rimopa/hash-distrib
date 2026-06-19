#ifndef HASH_DISTRIB_READERS
#define HASH_DISTRIB_READERS

int binary_hash(HashAPI hash_api, void *ctx, FILE *file_pointer, unsigned char *hash_key_pointer);
void string_hash(HashAPI hash_api, void *ctx, unsigned char *hash_key_pointer, char *string);

#endif
