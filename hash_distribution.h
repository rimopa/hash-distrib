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

typedef struct
{
    Node **keys_table;
    unsigned int size;
} KeyDB;

// Hash Keys Table
KeyDB create_key_db(unsigned int size);
void key_db_add(KeyDB key_db, size_t key_size, unsigned char *hash_key_pointer, bool verbose, unsigned long long *node_count_pointer);
void destroy_key_db(KeyDB key_db);

// Analysis
void print_bytes_hex(unsigned char *pointer, size_t bytes, bool description);
void analyse(KeyDB key_db, HashAPI hash_api, unsigned long long node_count, unsigned int valid_hashes_count, unsigned int nfiles, bool table, bool details);

#endif