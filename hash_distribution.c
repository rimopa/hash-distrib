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

int maxint(int a, int b)
{
    if (a > b)
        return a;
    return b;
}

unsigned int get_digit_count(int n) // Inspired by https://www.programiz.com/c-programming/examples/digits-count
{
    unsigned int c = 0;
    do
    {
        n /= 10;
        ++c;
    } while (n != 0);
    return c;
}

// Hash Keys Table

Node **create_keys_table(unsigned int keys_table_size)
{
    return calloc(keys_table_size, sizeof(Node *));
}

Node *new_node(unsigned char *hash_key_pointer)
{
    Node *new = malloc(sizeof(Node));
    new->key_pointer = hash_key_pointer;
    new->count = 1;
    new->next = NULL;
    return new;
}

void keys_table_add(Node **keys_table, unsigned int keys_table_size, size_t key_size, unsigned char *hash_key_pointer)
{
    unsigned int val;
    memcpy(&val, hash_key_pointer, sizeof(val));
    size_t index = val % keys_table_size;

    if (keys_table[index] == NULL)
    {
        keys_table[index] = new_node(hash_key_pointer);
        return;
    }
    Node *current = keys_table[index];
    while (true)
    {
        if (memcmp(current->key_pointer, hash_key_pointer, key_size) == 0)
        {
            current->count++;
            free(hash_key_pointer);
            break;
        }
        else if (current->next == NULL)
        {
            current->next = new_node(hash_key_pointer);
            break;
        }
        else
            current = current->next;
    }
}

void detroy_keys_table(Node **keys_table, unsigned int keys_table_size)
{
    for (unsigned int i = 0; i < keys_table_size; i++)
    {
        Node *cur = keys_table[i];
        while (cur)
        {
            Node *next = cur->next;
            free(cur->key_pointer);
            free(cur);
            cur = next;
        }
    }
    free(keys_table);
}

// Count of counts table

unsigned int sort_by_id(const CountEntry *a, const CountEntry *b)
{
    return (a->id - b->id);
}
unsigned int sort_by_num_values(const CountEntry *a, const CountEntry *b)
{
    return (a->num_values - b->num_values);
}

void count_of_counts_add(CountEntry **count_of_counts, unsigned long long key, unsigned int *most_digits)
{
    CountEntry *s;
    HASH_FIND_INT(*count_of_counts, &key, s);
    if (s != NULL)
    {
        s->num_values++;
        s->num_values_digits = get_digit_count(s->num_values);
        *most_digits = maxint(*most_digits, s->num_values_digits);
        return;
    }
    else
    {
        s = malloc(sizeof(CountEntry));
        s->id = key;
        s->id_digits = get_digit_count(s->id);
        s->num_values = 1;
        s->num_values_digits = 1;
        HASH_ADD_INT(*count_of_counts, id, s);
        *most_digits = maxint(*most_digits, s->id_digits);
    }
}

CountEntry *create_count_of_counts(Node **keys_table, unsigned int keys_table_size, unsigned long long *node_count, unsigned int *most_digits)
{
    CountEntry *count_of_counts = NULL; // empty map
    for (unsigned int i = 0; i < keys_table_size; i++)
    {
        if (keys_table[i] == NULL)
            continue;

        Node *n = keys_table[i];

        while (true)
        {
            (*node_count)++;
            count_of_counts_add(&count_of_counts, n->count, most_digits);
            if (n->next == NULL)
                break;
            n = n->next;
        }
    }
    return count_of_counts;
}

void destroy_count_of_counts(CountEntry *count_of_counts)
{
    CountEntry *count_entry, *tmp; // Variables for iteraton

    HASH_ITER(hh, count_of_counts, count_entry, tmp)
    {
        HASH_DEL(count_of_counts, count_entry);
        free(count_entry);
    }
}

// Analysis

void printbar(unsigned int width)
{
    for (unsigned int i = 0; i < width; i++)
        printf("-");
    printf("\n");
}

void analyse(HashAPI hash_api, CountEntry **count_of_counts, unsigned long long node_count, unsigned int hash_count, unsigned int nfiles, unsigned int most_digits)
{
    CountEntry *count_entry, *tmp; // Variables for iteraton

    printf("----------------------\n");
    printf("Distribution analysis:\n");
    printf("\n");
    printf("%i/%i hashes returned.\n", hash_count, nfiles);
    printf("Out of 2^%zu possible, %lli distinct keys were returned.\n", hash_api.out_size * 8, node_count);
    printf("\n");
    if (node_count == 0)
    {
        return;
    }
    printf("The following table is to be read like this: ");
    printf("[Column 1] number of keys were returned [Column 2] number of times\n");

    HASH_SORT(*count_of_counts, sort_by_id);
    const unsigned int width = most_digits * 2 + 3;
    printbar(width);
    HASH_ITER(hh, *count_of_counts, count_entry, tmp)
    {
        printf("|%li", count_entry->num_values);
        for (unsigned int i = 0; i < most_digits - count_entry->num_values_digits; i++)
            printf(" ");
        printf("|%li", count_entry->id);
        for (unsigned int i = 0; i < most_digits - count_entry->id_digits; i++)
            printf(" ");
        printf("|\n");
    }
    printbar(width);
    printf("\n");
}

#endif