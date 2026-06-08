#define _XOPEN_SOURCE 700
#include <dlfcn.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <unistd.h>

#include "hash_distribution.h"
#include "hash_api.h"

#define HASH_API_FUNC_NAME "hash_api"
#define MAX 1024

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s <libhash.so> [-r reader.bin] <file>...\n"
            "\n"
            "  libhash.so           Executable as hash function to evaluate\n"
            "  -r reader.bin        Executable as reader function\n"
            "  file                 Files to hash with libhash.so. Read as binary\n"
            "\n",
            prog);
}

HashAPI get_hash_api(void *handle, const char *hashpath)
{
    char *error;
    const HashAPI (*hash_api_func)() = dlsym(handle, HASH_API_FUNC_NAME);
    if ((error = dlerror()) != NULL)
    {
        fputs(error, stderr);
        exit(4);
    }
    return hash_api_func();
}

void *open_handle(const char *hashpath)
{
    // Using shared library to call void*ev_hash(void*data) from hash.so
    // Used: https://stackoverflow.com/questions/63306734/when-to-actually-use-dlopen-does-dlopen-means-dynamic-loading
    void *handle = dlopen(hashpath, RTLD_LAZY);
    if (!handle)
    {
        fputs(dlerror(), stderr);
        exit(3);
    }
    return handle;
}

void read_args(int argc, char *argv[], char **hashpath, char **readerpath, int *nfiles)
{
    if (argc < 2)
    {
        usage(argv[0]);
        exit(1);
    }

    *hashpath = argv[1];
    *readerpath = NULL;

    // Read options, I learnt the syntax from Claude Sonnet 4.6:
    optind = 2;
    int opt;
    while ((opt = getopt(argc, argv, "r:d:h")) != -1)
    {
        switch (opt)
        {
        case 'r':
            *readerpath = optarg;
            break;
        case 'h':
            usage(argv[0]);
            exit(2);
        default:
            break;
        }
    }

    *nfiles = argc - optind;
}

void read_filepaths(int argc, char *argv[], int nfiles, const char *filepaths[])
{
    for (uint32_t i = 0; i < nfiles; i++)
    {
        filepaths[i] = argv[optind + i];
    }
}

unsigned int get_file_size(FILE *file_pointer)
{ // Get file size and rewind file pointer
    fseek(file_pointer, 0, SEEK_END);
    uint64_t file_size = ftell(file_pointer);
    rewind(file_pointer);
    return file_size;
}

void binary_hash(HashAPI hash_api, void *ctx, Node **keys_table, FILE *file_pointer, unsigned char *hash_key)
{
    uint64_t file_size = get_file_size(file_pointer);
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
        unsigned int final_read_size = file_size - HASH_CHUNK_SIZE * full_chunk_count;
        unsigned char buffer[final_read_size];
        fread(buffer, 1, final_read_size, file_pointer);
        hash_api.update(ctx, buffer, final_read_size);
    }
    hash_api.final(ctx, hash_key);
}

bool process_file(HashAPI hash_api, void *ctx, Node **keys_table, unsigned int keys_table_size, const char *path)
{
    unsigned char *hash_key = malloc(hash_api.out_size);
    FILE *file_pointer = fopen(path, "rb");

    if (!file_pointer)
    {
        perror(path);
        fprintf(stderr, "Could not open %s\n", path);
        printf("Skipping...\n");
        return false;
    }
    binary_hash(hash_api, ctx, keys_table, file_pointer, hash_key);
    keys_table_add(keys_table, keys_table_size, hash_api.out_size, hash_key);
    fclose(file_pointer);
    return true;
}

void process_files(HashAPI hash_api, Node **keys_table, unsigned int keys_table_size, const char *filepaths[], unsigned int nfiles, unsigned int *hash_count)
{
    void *ctx = malloc(hash_api.ctx_size);

    for (unsigned int i = 0; i < nfiles; i++)
    {
        if (process_file(hash_api, ctx, keys_table, keys_table_size, filepaths[i]))
            (*hash_count)++;
    }
    free(ctx);
}

int main(int argc, char *argv[])
{
    char *hashpath;
    char *readerpath;
    unsigned int nfiles;

    read_args(argc, argv, &hashpath, &readerpath, &nfiles);

    const char *filepaths[nfiles];

    read_filepaths(argc, argv, nfiles, filepaths);

    void *handle = open_handle(hashpath);

    const HashAPI hash_api = get_hash_api(handle, hashpath);

    if (hash_api.out_size < 2 || hash_api.out_size > 256)
    {
        printf("Invalid out_size: must be 2 <= out_size <=256, got %zu.\n", hash_api.out_size);
        dlclose(handle);
        return 5;
    }

    uint32_t hash_count = 0;

    const uint32_t keys_table_size = hash_api.out_size * hash_api.out_size;
    Node **keys_table = create_keys_table(keys_table_size);

    process_files(hash_api, keys_table, keys_table_size, filepaths, nfiles, &hash_count);

    unsigned int most_digits = 0;
    uint32_t node_count = 0;

    CountEntry *count_of_counts = create_count_of_counts(keys_table, keys_table_size, &node_count, &most_digits);

    detroy_keys_table(keys_table, keys_table_size);

    analyse(hash_api, &count_of_counts, node_count, hash_count, nfiles, most_digits);

    destroy_count_of_counts(count_of_counts);

    dlclose(handle);

    return 0;
}