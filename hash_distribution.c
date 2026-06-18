#include "hash_distribution.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "uthash/src/uthash.h"

#include "hash_api.h"

#define MAX_DETAILS_KEYS 25

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

unsigned int get_digit_count(long long n) // Inspired by https://www.programiz.com/c-programming/examples/digits-count
{
    unsigned int c = 0;
    do
    {
        n /= 10;
        c++;
    } while (n != 0);
    return c;
}

// Hash Keys Table

KeyDB create_key_db(HashAPI hash_api, int mode, unsigned int nfiles, bool verbose)
{
    unsigned int size = hash_api.out_size * hash_api.out_size;
    int method = KEYDB_METHOD_BUCKETS;

    if (mode == HASHING_MODE_BINARY && size >= nfiles)
    {
        size = nfiles;
        method = KEYDB_METHOD_ARRAY;
        if (verbose)
            printf("Using unbucket key storing method.\n");
    }
    else if (verbose)
        printf("Using bucketed key storing method.\n");

    KeyDB key_db = {
        .table = calloc(size, sizeof(Node *)),
        .size = size,
        .method = method,
        .node_count_ptr = calloc(1, sizeof(unsigned long))};
    return key_db;
}

Node *new_node(KeyDB key_db, unsigned char *hash_key_pointer)
{
    Node *new = malloc(sizeof(Node));
    if (new == nullptr)
    {
        fprintf(stderr, "Failed to allocate memory for new node.\n");
        free(hash_key_pointer);
        return nullptr;
    }

    new->key_pointer = hash_key_pointer;
    new->count = 1;
    new->next = nullptr;
    (*key_db.node_count_ptr)++;
    return new;
}

unsigned int keys_table_index(unsigned char *hash_key_pointer, size_t key_size, KeyDB key_db)
{
    if (key_size == 1)
        return 0;

    // uint16_t (two bytes) limit is 65535, which is also the max index for 256 bytes keys_tables, as 256*256 = 65536, so it could overflow and be set to 0.
    uint8_t xor_array[2] = {0, 0};

    for (unsigned int i = 0; i < key_size; i++)
    {
        xor_array[i % 2] ^= hash_key_pointer[i];
    }

    uint16_t val;
    memcpy(&val, xor_array, sizeof(uint16_t));

    return val % key_db.size;
}

bool key_db_buckets_add(KeyDB key_db, size_t key_size, unsigned char *hash_key_pointer, bool verbose)
{
    unsigned int index = keys_table_index(hash_key_pointer, key_size, key_db);

    if (verbose)
        printf("Using bucket %d/%d.\n", index, key_db.size);

    if (key_db.table[index] == nullptr)
    {
        key_db.table[index] = new_node(key_db, hash_key_pointer);
        if (key_db.table[index] == nullptr)
            return false;
        if (verbose)
            printf("Key added as new node at head of bucket.\n");
        return true;
    }
    Node *current = key_db.table[index];
    while (true)
    {
        if (memcmp(current->key_pointer, hash_key_pointer, key_size) == 0)
        {
            current->count++;
            free(hash_key_pointer);
            if (verbose)
                printf("Key already found in keys table, count of node++ and memory freed.\n");
            return true;
        }
        else if (current->next == nullptr)
        {
            current->next = new_node(key_db, hash_key_pointer);
            if (current->next == nullptr)
                return false;
            if (verbose)
                printf("Key not found, added as tail of bucket.\n");
            return true;
        }
        else
            current = current->next;
    }
    return true;
}

bool key_db_arr_add(KeyDB key_db, size_t key_size, unsigned char *hash_key_pointer, bool verbose)
{
    Node *current = nullptr;

    for (unsigned long i = 0; i < *(key_db.node_count_ptr); i++)
    {
        current = key_db.table[i];

        if (memcmp(current->key_pointer, hash_key_pointer, key_size) == 0)
        {
            current->count++;
            free(hash_key_pointer);
            if (verbose)
                printf("Key already found in keys table, count of node++ and memory freed.\n");
            return true;
        }
    }
    unsigned long index = *(key_db.node_count_ptr);

    key_db.table[index] = new_node(key_db, hash_key_pointer);
    if (key_db.table[index] == nullptr)
        return false;
    if (verbose)
        printf("Key not found, added to table at index %lu.\n", index);
    return true;
}

bool key_db_add(KeyDB key_db, size_t key_size, unsigned char *hash_key_pointer, bool verbose)
{
    if (key_db.method == KEYDB_METHOD_BUCKETS)
        return key_db_buckets_add(key_db, key_size, hash_key_pointer, verbose);
    else
        return key_db_arr_add(key_db, key_size, hash_key_pointer, verbose);
}

void destroy_key_db(KeyDB key_db)
{
    if (key_db.table != nullptr)
    {
        for (unsigned int i = 0; i < key_db.size; i++)
        {
            Node *cur = key_db.table[i];
            while (cur)
            {
                Node *next = cur->next;
                free(cur->key_pointer);
                free(cur);
                cur = next;
            }
        }
        free(key_db.table);
    }

    if (key_db.node_count_ptr != nullptr)
        free(key_db.node_count_ptr);
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
    if (a->num_values < b->num_values)
        return -1;
    if (a->num_values > b->num_values)
        return 1;
    return 0;
}

bool count_of_counts_add(CountEntry **count_of_counts, unsigned long key)
{
    CountEntry *s;
    HASH_FIND(hh, *count_of_counts, &key, sizeof(s->id), s);
    if (s != nullptr)
    {
        s->num_values++;
        return true;
    }
    else
    {
        s = malloc(sizeof *s);

        if (s == nullptr)
        {
            fprintf(stderr, "Failed to allocate memory for new node for count of counts table.\n");
            return false;
        }

        s->id = key;
        s->num_values = 1;
        HASH_ADD(hh, *count_of_counts, id, sizeof s->id, s);
    }
    return true;
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

CountEntry *create_count_of_counts(KeyDB key_db)
{
    CountEntry *count_of_counts = nullptr; // empty map
    for (unsigned int i = 0; i < key_db.size; i++)
    {
        if (key_db.table[i] == nullptr)
            continue;

        Node *n = key_db.table[i];

        while (true)
        {
            if (!count_of_counts_add(&count_of_counts, n->count))
            {
                destroy_count_of_counts(count_of_counts);
                return nullptr;
            }

            if (n->next == nullptr)
                break;
            n = n->next;
        }
    }
    return count_of_counts;
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
        printf(".\n");
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

void print_mean(unsigned long sum, unsigned long valid_hashes_count)
{
    printf("\nMean of counts: %.3f\n", (float)sum / valid_hashes_count);
    printf("The closer the mean is to 1, the more the keys that were returned only once.\n");
    printf("The bigger it is, the more repeated the keys your hash function returned.\n");
}

void analysis_table(KeyDB key_db)
{
    CountEntry *count_of_counts = create_count_of_counts(key_db);

    if (count_of_counts == nullptr)
    {
        printf("Cannot display table without valid count of counts. Cancelling operation.\n");
        return;
    }

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

        printf("|%lu", count_entry->num_values);
        for (unsigned int i = 0; i < most_digits - nv_digits; i++)
            printf(" ");
        printf("|%lu", count_entry->id);
        for (unsigned int i = 0; i < most_digits - id_digits; i++)
            printf(" ");
        printf("|\n");
    }
    printbar(width);

    destroy_count_of_counts(count_of_counts);
}

int write_to_file(KeyDB key_db, char *path, size_t key_size)
{
    FILE *file_pointer;
    file_pointer = fopen(path, "w");

    if (file_pointer == nullptr)
    {
        perror(path);
        fprintf(stderr, "Could not open %s.\n", path);
        return 1;
    }

    Node *current_node = nullptr;
    size_t elements_written;

    for (unsigned long i = 0; i < key_db.size; i++)
    {
        if (key_db.table[i] == nullptr)
            continue;
        current_node = key_db.table[i];
        while (true)
        {
            elements_written = fwrite(current_node->key_pointer, key_size, 1, file_pointer);
            if (elements_written != 1)
            {
                fprintf(stderr, "Error writing key to %s.\n", path);
                fclose(file_pointer);
                return 2;
            }

            fprintf(file_pointer, " %lu", current_node->count);

            if (current_node->next == nullptr)
                break;
            current_node = current_node->next;
        }
    }

    fclose(file_pointer);

    return 0;
}

void analysis_details(HashAPI hash_api, KeyDB key_db)
{
    printf("\n");
    Node *current_node = nullptr;
    for (unsigned long i = 0; i < key_db.size; i++)
    {
        if (key_db.table[i] == nullptr)
            continue;
        current_node = key_db.table[i];
        while (true)
        {
            printf("The key ");
            print_bytes_hex(current_node->key_pointer, hash_api.out_size, false);
            printf(" was returned %lu times.\n", current_node->count);

            if (current_node->next == nullptr)
                break;
            current_node = current_node->next;
        }
    }
}

unsigned long get_counts_sum(KeyDB key_db)
{
    unsigned long counts_sum = 0;
    Node *current_node = nullptr;
    for (unsigned long i = 0; i < key_db.size; i++)
    {
        if (key_db.table[i] == nullptr)
            continue;
        current_node = key_db.table[i];
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

void analyse(KeyDB key_db, HashAPI hash_api, unsigned long valid_hashes_count, unsigned int nfiles, bool table, bool details)
{
    const unsigned long distinct_keys_count = *(key_db.node_count_ptr);

    if (valid_hashes_count < 1)
    {
        printf("\nNo valid hashes were returned, no possible analysis.\n");
        return;
    }

    printf("\n");
    printbar(79);
    printf(
        "Distribution analysis of '%s' hash function:\n\n"
        "%lu hashes in %u files were returned.\n"
        "Out of 2^%zu possible, %lu distinct keys were returned.\n",
        hash_api.name, valid_hashes_count, nfiles, hash_api.out_size * 8, distinct_keys_count);

    if (valid_hashes_count == 1 && !details && !table)
    {
        details = true;
    }
    else if (distinct_keys_count == 1 && valid_hashes_count > 1)
    {
        printf("\nWow, you got exactly the same key every time!\n"
               "If you plan to use a hash table for this kind of dataset and hash function,\n"
               "all the data is going to fall into the same bucket, which would defeat the purpose of the hash table.\n");
    }
    else if (distinct_keys_count == valid_hashes_count)
    {
        printf("\nWow, your returned keys were all different!\n");
    }
    else if (!table && !details)
    {
        if (distinct_keys_count <= MAX_DETAILS_KEYS)
            details = true;
        else
            table = true;
    }

    if (details)
        analysis_details(hash_api, key_db);

    if (table)
        analysis_table(key_db);

    if ((table || details) && distinct_keys_count != valid_hashes_count)
        print_mean(get_counts_sum(key_db), distinct_keys_count);
}