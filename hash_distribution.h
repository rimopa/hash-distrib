#ifndef HASH_DISTRIB_HASH_DISTRIB
#define HASH_DISTRIB_HASH_DISTRIB

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "uthash/src/uthash.h"

#include "hash_api.h"

// Hashing modes

#define HASHING_MODE_BINARY 0
#define HASHING_MODE_LINES 1

// Hash Keys Table
typedef struct Node
{
    unsigned char *key_pointer;
    unsigned long count;
    struct Node *next;
} Node;

#define KEYDB_METHOD_BUCKETS 0
#define KEYDB_METHOD_ARRAY 1

typedef struct
{
    Node **table;
    const unsigned int size;
    const int method; // 0 = hash table with buckets, 1 = array
    unsigned long *node_count_ptr;
} KeyDB;

KeyDB create_key_db(HashAPI hash_api, int mode, unsigned int nfiles, bool verbose);
bool key_db_add(KeyDB key_db, size_t key_size, unsigned char *hash_key_pointer, bool verbose);
void destroy_key_db(KeyDB key_db);

// Analysis
void print_bytes_hex(unsigned char *pointer, size_t bytes, bool description);
void analyse(KeyDB key_db, HashAPI hash_api, unsigned long valid_hashes_count, unsigned int nfiles, bool table, bool details);

#endif