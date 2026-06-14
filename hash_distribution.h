#ifndef HASH_DISTRIB_HASH_DISTRIB
#define HASH_DISTRIB_HASH_DISTRIB

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "uthash/src/uthash.h"

#include "hash_api.h"

typedef struct Node
{
    unsigned char *key_pointer;
    unsigned long long count;
    struct Node *next;
} Node;

// Hash Keys Table
Node **create_keys_table(unsigned int keys_table_size);
void keys_table_add(Node **keys_table, unsigned int keys_table_size, size_t key_size, unsigned char *hash_key_pointer, bool verbose, unsigned long long* node_count_pointer);
void destroy_keys_table(Node **keys_table, unsigned int keys_table_size);

// Analysis
void analyse(Node **keys_table, unsigned int keys_table_size, HashAPI hash_api, unsigned long long node_count, unsigned int hash_count, unsigned int nfiles);

#endif