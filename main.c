#define _XOPEN_SOURCE 700
#include <dlfcn.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <unistd.h>
#include "progressbar/include/progressbar/progressbar.h"

#include "hash_distribution.h"
#include "hash_api.h"

#define HASH_API_FUNC_NAME "hash_api"
#define HASH_CHUNK_SIZE 4096

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s <libhash.so> [options] [-r reader.bin] <file>...\n"
            "\n"
            "  libhash.so           Executable as hash function to evaluate\n"
            "  -r reader.bin        Executable as reader function\n"
            "  file                 Files to hash with libhash.so. Read as binary\n"
            ""
            "  Options:\n"
            "  -v                   Verbose\n",
            prog);
}

HashAPI get_hash_api(void *handle)
{
    char *error;
    HashAPI (*hash_api_func)() = dlsym(handle, HASH_API_FUNC_NAME);
    if ((error = dlerror()) != NULL)
    {
        fputs(error, stderr);
        exit(4);
    }
    return hash_api_func();
}

void *open_handle(const char *hashpath)
{
    // Inspired by https://stackoverflow.com/questions/63306734/when-to-actually-use-dlopen-does-dlopen-means-dynamic-loading
    void *handle = dlopen(hashpath, RTLD_LAZY);
    if (!handle)
    {
        fputs(dlerror(), stderr);
        exit(3);
    }
    return handle;
}

void read_args(int argc, char *argv[], char **hashpath, char **readerpath, unsigned int *nfiles, bool *verbose)
{
    if (argc < 2)
    {
        usage(argv[0]);
        exit(1);
    }

    *hashpath = argv[1];
    *readerpath = NULL;

    // Read options. I learnt the syntax from Claude Sonnet 4.6:
    optind = 2;
    int opt;
    while ((opt = getopt(argc, argv, "vr:h")) != -1)
    {
        switch (opt)
        {
        case 'v':
            *verbose = true;
            break;
        case 'r':
            *readerpath = optarg;
            break;
        case 'h':
            usage(argv[0]);
            exit(0);
        case '?':
            usage(argv[0]);
            exit(2);
        default:
            abort();
        }
    }

    *nfiles = argc - optind;
}

void read_filepaths(char *argv[], int nfiles, const char *filepaths[])
{
    for (int i = 0; i < nfiles; i++)
    {
        filepaths[i] = argv[optind + i];
    }
}

// Get file size and rewind file pointer
long get_file_size(FILE *file_pointer)
{
    fseek(file_pointer, 0, SEEK_END);
    long file_size = ftell(file_pointer);
    rewind(file_pointer);
    return file_size;
}

int binary_hash(HashAPI hash_api, void *ctx, FILE *file_pointer, unsigned char *hash_key)
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
        unsigned int final_read_size = file_size - HASH_CHUNK_SIZE * full_chunk_count;
        unsigned char buffer[final_read_size];
        fread(buffer, 1, final_read_size, file_pointer);
        hash_api.update(ctx, buffer, final_read_size);
    }
    hash_api.final(ctx, hash_key);

    return 0;
}

void printbytes(unsigned char *pointer, size_t bytes)
{
    for (unsigned int i = 0; i < bytes - 1; i++)
        printf("%i, ", pointer[i]);
    printf("%i\n", pointer[bytes - 1]);
}

bool process_file(HashAPI hash_api, void *ctx, Node **keys_table, unsigned int keys_table_size, const char *path, bool verbose, unsigned long long * node_count)
{
    unsigned char *hash_key_pointer = malloc(hash_api.out_size);
    FILE *file_pointer = fopen(path, "rb");

    if (!file_pointer)
    {
        perror(path);
        fprintf(stderr, "Could not open %s\nSkipping…\n", path);
        free(hash_key_pointer);
        return false;
    }

    binary_hash(hash_api, ctx, file_pointer, hash_key_pointer);
    if (verbose)
    {
        printf("Returned key bytes: ");
        printbytes(hash_key_pointer, hash_api.out_size);
    }
    keys_table_add(keys_table, keys_table_size, hash_api.out_size, hash_key_pointer, verbose, node_count);
    return true;
}

void process_files(HashAPI hash_api, Node **keys_table, unsigned int keys_table_size, const char *filepaths[], unsigned int nfiles, unsigned int *hash_count, bool verbose, unsigned long long *node_count)
{
    void *ctx = malloc(hash_api.ctx_size);
    progressbar *progress;
    if (!verbose)
        progress = progressbar_new("Hashing files", nfiles);

    for (unsigned int i = 0; i < nfiles; i++)
    {
        if (verbose)
            printf("Hashing %s\n", filepaths[i]);
        else
            progressbar_inc(progress);
        if (process_file(hash_api, ctx, keys_table, keys_table_size, filepaths[i], verbose, node_count))
            (*hash_count)++;
    }
    if (!verbose)
        progressbar_finish(progress);
    free(ctx);
}

int main(int argc, char *argv[])
{
    char *hashpath;
    char *readerpath;
    bool verbose;
    unsigned int nfiles;

    read_args(argc, argv, &hashpath, &readerpath, &nfiles, &verbose);

    const char *filepaths[nfiles];

    read_filepaths(argv, nfiles, filepaths);

    void *handle = open_handle(hashpath);

    const HashAPI hash_api = get_hash_api(handle);

    if (hash_api.out_size < 2 || hash_api.out_size > 256)
    {
        printf("Invalid out_size: must be 2 <= out_size <=256, got %zu.\n", hash_api.out_size);
        dlclose(handle);
        return 5;
    }

    unsigned int hash_count = 0;
    unsigned long long node_count = 0;

    const unsigned int keys_table_size = hash_api.out_size * hash_api.out_size;
    Node **keys_table = create_keys_table(keys_table_size);

    process_files(hash_api, keys_table, keys_table_size, filepaths, nfiles, &hash_count, verbose, &node_count);

    analyse(keys_table, keys_table_size, hash_api, node_count, hash_count, nfiles);

    destroy_keys_table(keys_table, keys_table_size);

    dlclose(handle);

    return 0;
}