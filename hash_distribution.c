#include "hash_distribution.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "uthash/src/uthash.h"

#include "hash_api.h"

#define MAX_DETAILS_KEYS 50
#define DATA_PATH "/tmp/hash-distrib-data.txt"

// Use https://github.com/troydhanson/uthash for count of counts hash table, Claude Opus 4.6's recommendation
typedef struct
{
    unsigned long id;         // Number of times a hash appears
    unsigned long num_values; // Number of hashes that appear that many times
    UT_hash_handle hh;
} CountEntry;

unsigned int maxuint(unsigned int a, unsigned int b)
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

Node *new_node(unsigned char *hash_key_pointer, unsigned long long *distinct_keys_count_pointer)
{
    Node *new = malloc(sizeof(Node));
    new->key_pointer = hash_key_pointer;
    new->count = 1;
    new->next = nullptr;
    (*distinct_keys_count_pointer)++;
    return new;
}

unsigned int keys_table_index(unsigned char *hash_key_pointer, size_t key_size, unsigned int keys_table_size)
{
    // uint16_t (two bytes) limit is 65535, which is also the max index for 256 bytes keys_tables, as 256*256 = 65536.
    unsigned char *xor_array = calloc(1, sizeof(uint16_t));
    for (unsigned int i = 0; i < key_size; i++)
    {
        xor_array[i % 2] ^= hash_key_pointer[i];
    }

    uint16_t val;
    memcpy(&val, xor_array, sizeof(uint16_t));

    free(xor_array);

    return val % keys_table_size;
}

void keys_table_add(Node **keys_table, unsigned int keys_table_size, size_t key_size, unsigned char *hash_key_pointer, bool verbose, unsigned long long *distinct_keys_count_pointer)
{
    unsigned int index = keys_table_index(hash_key_pointer, key_size, keys_table_size);
    if (verbose)
        printf("Using bucket %d/%d\n", index, keys_table_size);

    if (keys_table[index] == nullptr)
    {
        keys_table[index] = new_node(hash_key_pointer, distinct_keys_count_pointer);
        if (verbose)
            printf("Added as new node at head of bucket\n");
        return;
    }
    Node *current = keys_table[index];
    while (true)
    {
        if (memcmp(current->key_pointer, hash_key_pointer, key_size) == 0)
        {
            current->count++;
            free(hash_key_pointer);
            if (verbose)
                printf("Already found in keys table, count of node++ and memory freed\n");
            break;
        }
        else if (current->next == nullptr)
        {
            current->next = new_node(hash_key_pointer, distinct_keys_count_pointer);
            if (verbose)
                printf("Not found, added as tail of bucket\n");
            break;
        }
        else
            current = current->next;
    }
}

void destroy_keys_table(Node **keys_table, unsigned int keys_table_size)
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

int sort_by_id(const CountEntry *a, const CountEntry *b)
{
    if (a->id < b->id)
        return -1;
    if (a->id > b->id)
        return 1;
    return 0;
}
int sort_by_num_values(const CountEntry *a, const CountEntry *b)
{
    return (a->num_values - b->num_values);
}

void count_of_counts_add(CountEntry **count_of_counts, unsigned long long key)
{
    CountEntry *s;
    HASH_FIND_INT(*count_of_counts, &key, s);
    if (s != nullptr)
    {
        s->num_values++;
        return;
    }
    else
    {
        s = malloc(sizeof(CountEntry));
        s->id = key;
        s->num_values = 1;
        HASH_ADD_INT(*count_of_counts, id, s);
    }
}

CountEntry *create_count_of_counts(Node **keys_table, unsigned int keys_table_size)
{
    CountEntry *count_of_counts = nullptr; // empty map
    for (unsigned int i = 0; i < keys_table_size; i++)
    {
        if (keys_table[i] == nullptr)
            continue;

        Node *n = keys_table[i];

        while (true)
        {
            count_of_counts_add(&count_of_counts, n->count);
            if (n->next == nullptr)
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

void print_bytes_hex(unsigned char *pointer, size_t bytes, bool description)
{
    if (description)
        printf("Returned key: ");
    printf("0x");
    for (size_t i = 0; i < bytes; i++)
        printf("%02X", pointer[i]);
    if (description)
        printf("\n");
}

void printbar(unsigned int width)
{
    for (unsigned int i = 0; i < width; i++)
        printf("-");
    printf("\n");
}

unsigned int count_of_counts_most_digits(CountEntry *count_of_counts)
{
    CountEntry *count_entry, *tmp; // Variables for iteraton
    unsigned int most_digits = 1;
    HASH_ITER(hh, count_of_counts, count_entry, tmp)
    {
        most_digits = maxuint(most_digits, get_digit_count(count_entry->id));
        most_digits = maxuint(most_digits, get_digit_count(count_entry->num_values));
    }
    return most_digits;
}

void print_mean(unsigned long long sum, unsigned int valid_hashes_count)
{
    printf("\nMean of counts: %.3f\n", (float)sum / valid_hashes_count);
    printf("The closer the mean is to 1, the more the keys that were returned only once\n");
    printf("The bigger it is, the more repeated the keys your hash function returned\n");
}

void analysis_table(Node **keys_table, unsigned int keys_table_size)
{
    CountEntry *count_of_counts = create_count_of_counts(keys_table, keys_table_size);

    HASH_SORT(count_of_counts, sort_by_id);

    unsigned int most_digits = count_of_counts_most_digits(count_of_counts);
    const unsigned int width = most_digits * 2 + 3;

    printf("\nThe following table is to be read like this: ");
    printf("[Column 1] number of keys were returned [Column 2] number of times.\n");

    printbar(width);
    CountEntry *count_entry, *tmp; // Variables for iteraton
    HASH_ITER(hh, count_of_counts, count_entry, tmp)
    {
        unsigned int nv_digits = get_digit_count(count_entry->num_values);
        unsigned int id_digits = get_digit_count(count_entry->id);

        printf("|%li", count_entry->num_values);
        for (unsigned int i = 0; i < most_digits - nv_digits; i++)
            printf(" ");
        printf("|%li", count_entry->id);
        for (unsigned int i = 0; i < most_digits - id_digits; i++)
            printf(" ");
        printf("|\n");
    }
    printbar(width);

    destroy_count_of_counts(count_of_counts);
}

int write_to_file(Node **keys_table, unsigned int keys_table_size, char *path, size_t key_size)
{
    FILE *file_pointer;
    file_pointer = fopen(path, "w");

    if (file_pointer == nullptr)
    {
        perror(path);
        fprintf(stderr, "Could not open %s\n", path);
        return 1;
    }

    Node *current_node = nullptr;
    size_t elements_written;

    for (unsigned long i = 0; i < keys_table_size; i++)
    {
        if (keys_table[i] == nullptr)
            continue;
        current_node = keys_table[i];
        while (true)
        {
            elements_written = fwrite(current_node->key_pointer, key_size, 1, file_pointer);
            if (elements_written != 1)
            {
                fprintf(stderr, "Error writing key to %s", path);
                return 2;
            }

            fprintf(file_pointer, " %llu", current_node->count);

            if (current_node->next == nullptr)
                break;
            current_node = current_node->next;
        }
    }

    fclose(file_pointer);

    return 0;
}

void analysis_details(HashAPI hash_api, Node **keys_table, unsigned int keys_table_size)
{
    printf("\n");
    Node *current_node = nullptr;
    for (unsigned long i = 0; i < keys_table_size; i++)
    {
        if (keys_table[i] == nullptr)
            continue;
        current_node = keys_table[i];
        while (true)
        {
            printf("The key ");
            print_bytes_hex(current_node->key_pointer, hash_api.out_size, false);
            printf(" was returned %llu times\n", current_node->count);

            if (current_node->next == nullptr)
                break;
            current_node = current_node->next;
        }
    }
}

unsigned long long get_counts_sum(Node **keys_table, unsigned int keys_table_size)
{
    unsigned long long counts_sum = 0;
    Node *current_node = nullptr;
    for (unsigned long i = 0; i < keys_table_size; i++)
    {
        if (keys_table[i] == nullptr)
            continue;
        current_node = keys_table[i];
        while (true)
        {

            counts_sum += current_node->count;
            if (current_node->next == nullptr)
                break;
            current_node = current_node->next;
        }
    }
    return counts_sum;
}

void analyse(Node **keys_table, unsigned int keys_table_size, HashAPI hash_api, unsigned long long distinct_keys_count, unsigned int valid_hashes_count, unsigned int nfiles, bool table, bool details)
{
    if (valid_hashes_count < 1)
    {
        printf("\nNo valid hashes were returned, no possible analysis.\n");
        return;
    }

    printf("\n");
    printbar(79);
    printf(
        "Distribution analysis of '%s' hash function:\n\n"
        "%i hashes in %i files were returned\n"
        "Out of 2^%zu possible, %lli distinct keys were returned\n",
        hash_api.name, valid_hashes_count, nfiles, hash_api.out_size * 8, distinct_keys_count);

    if ((valid_hashes_count == 1) && !(details || table))
    {
        analysis_details(hash_api, keys_table, keys_table_size);
        return;
    }

    if (distinct_keys_count == 1 && valid_hashes_count > 1)
    {
        printf("\nWow, you got exactly the same key every time!\n"
               "If you plan to use a hash table for this kind of dataset and hash function,\n"
               "it's going to be a linked list with some extra steps and edge cases.\n");
    }
    else if (distinct_keys_count == valid_hashes_count)
    {
        printf("\nWow, your returned keys were all different!\n"
               "If you plan to use a hash table for this kind of dataset and hash function,\n"
               "it's going to be an array with some extra steps and edge cases.\n");
    }
    else if (!table && !details)
    {
        if (distinct_keys_count <= MAX_DETAILS_KEYS)
            details = true;
        else
            table = true;
    }

    if (details)
        analysis_details(hash_api, keys_table, keys_table_size);

    if (table)
        analysis_table(keys_table, keys_table_size);

    if (table || details)
        print_mean(get_counts_sum(keys_table, keys_table_size), valid_hashes_count);
}