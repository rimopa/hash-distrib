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

// Use https://github.com/troydhanson/uthash for count of counts hash table, Claude Opus 4.6's recommendation
typedef struct
{
    unsigned long int id;         // Number of times a hash appears
    unsigned long int num_values; // Number of hashes that appear that many times
    unsigned int id_digits;
    unsigned int num_values_digits;
    UT_hash_handle hh;
} CountEntry;

// Hash Keys Table
Node **create_keys_table(unsigned int keys_table_size);
void keys_table_add(Node **keys_table, unsigned int keys_table_size, size_t key_size, unsigned char *hash_key_pointer, bool verbose);
void destroy_keys_table(Node **keys_table, unsigned int keys_table_size);

// Count of counts table
void count_of_counts_add(CountEntry **count_of_counts, unsigned long long key, unsigned int *most_digits);
CountEntry *create_count_of_counts(Node **keys_table, unsigned int keys_table_size, unsigned long long *node_count, unsigned int *most_digits);
void destroy_count_of_counts(CountEntry *count_of_counts);

// Analysis
void analyse(HashAPI hash_api, CountEntry **count_of_counts, unsigned long long node_count, unsigned int hash_count, unsigned int nfiles, unsigned int most_digits);

#endif