#define _XOPEN_SOURCE 700
#include <dlfcn.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>
#include "progressbar/include/progressbar/progressbar.h"

#include "hash_distribution.h"
#include "readers.h"
#include "hash_api.h"

#define HASH_API_FUNC_NAME "hash_api"

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s <libhash.so> [options] <file>...\n"
            "\n"
            "  libhash.so       Executable as hash function to evaluate\n"
            "  file             Files to hash with libhash.so\n"
            "\n"
            "  Options:\n"
            "  -v               Verbose\n"
            "  -m binary        Default. Hash whole files as binary data\n"
            "  -m line          Open files as text and hash each line (text separated by \\n)\n",
            prog);
}

HashAPI get_hash_api(void *handle)
{
    char *error;
    HashAPI (*hash_api_func)(void) = dlsym(handle, HASH_API_FUNC_NAME);
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

int choose_mode(char *mode_string)
{
    if (strcmp(mode_string, "b") == 0 || strcmp(mode_string, "bin") == 0 || strcmp(mode_string, "binary") == 0)
        return 0;
    else if (strcmp(mode_string, "l") == 0 || strcmp(mode_string, "line") == 0)
        return 1;
    else
        return -1;
}

void read_args(int argc, char *argv[], char **hashpath, unsigned int *nfiles, bool *verbose, int *mode_pointer)
{
    if (argc < 2)
    {
        usage(argv[0]);
        exit(1);
    }

    *hashpath = argv[1];

    // Read options. I learnt the syntax from Claude Sonnet 4.6:
    optind = 2;
    int opt;
    char *mode_string = NULL;
    while ((opt = getopt(argc, argv, "vhm:")) != -1)
    {
        switch (opt)
        {
        case 'm':
            mode_string = optarg;
            break;
        case 'v':
            *verbose = true;
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
    if (mode_string == NULL)
        *mode_pointer = 0;
    else
    {
        *mode_pointer = choose_mode(mode_string);
        if (*mode_pointer < 0)
        {
            fprintf(stderr, "Unrecognized mode %s\n", mode_string);
            usage(argv[0]);
            exit(6);
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

void print_key_bytes(unsigned char *pointer, size_t bytes)
{
    printf("Returned key bytes: ");
    for (unsigned int i = 0; i < bytes - 1; i++)
        printf("%i, ", pointer[i]);
    printf("%i\n", pointer[bytes - 1]);
}

unsigned int process_lines(HashAPI hash_api, void *ctx, Node **keys_table, unsigned int keys_table_size, bool verbose, unsigned long long *distinct_keys_count_pointer, FILE *file_pointer)
{
    unsigned int local_hash_count = 0;
    unsigned char *hash_key_pointer;
    char *line = NULL;
    size_t len = 0;

    while (getline(&line, &len, file_pointer) != -1)
    {
        line[strcspn(line, "\n")] = '\0';

        hash_key_pointer = malloc(hash_api.out_size);
        string_hash(hash_api, ctx, hash_key_pointer, line);
        if (verbose)
            print_key_bytes(hash_key_pointer, hash_api.out_size);
        keys_table_add(keys_table, keys_table_size, hash_api.out_size, hash_key_pointer, verbose, distinct_keys_count_pointer);
        local_hash_count++;
    }
    free(line);
    return local_hash_count;
}

// Return valid hashes from file
unsigned int process_file(HashAPI hash_api, void *ctx, Node **keys_table, unsigned int keys_table_size, const char *path, bool verbose, unsigned long long *distinct_keys_count_pointer, int mode)
{
    FILE *file_pointer = NULL;
    unsigned int returnval = 0;

    if (mode == 0)
        file_pointer = fopen(path, "rb");
    else if (mode == 1)
        file_pointer = fopen(path, "r");

    if (!file_pointer)
    {
        perror(path);
        fprintf(stderr, "Could not open %s\nSkipping…\n", path);
        return 0;
    }

    if (mode == 0)
    {
        unsigned char *hash_key_pointer = malloc(hash_api.out_size);
        binary_hash(hash_api, ctx, file_pointer, hash_key_pointer);
        if (verbose)
            print_key_bytes(hash_key_pointer, hash_api.out_size);
        keys_table_add(keys_table, keys_table_size, hash_api.out_size, hash_key_pointer, verbose, distinct_keys_count_pointer);
        returnval = 1;
    }
    else if (mode == 1)
        returnval = process_lines(hash_api, ctx, keys_table, keys_table_size, verbose, distinct_keys_count_pointer, file_pointer);

    fclose(file_pointer);
    return returnval;
}

void process_files(HashAPI hash_api, Node **keys_table, unsigned int keys_table_size, const char *filepaths[], unsigned int nfiles, unsigned int *valid_hashes_count_pointer, bool verbose, unsigned long long *distinct_keys_count, int mode)
{
    void *ctx = malloc(hash_api.ctx_size);
    progressbar *progress;
    if (!verbose)
        progress = progressbar_new("Hashing data", nfiles);

    for (unsigned int i = 0; i < nfiles; i++)
    {
        if (verbose)
            printf("Hashing data from file: %s\n", filepaths[i]);
        else
            progressbar_inc(progress);
        *valid_hashes_count_pointer += process_file(hash_api, ctx, keys_table, keys_table_size, filepaths[i], verbose, distinct_keys_count, mode);
    }
    if (!verbose)
        progressbar_finish(progress);
    free(ctx);
}

int main(int argc, char *argv[])
{
    char *hashpath;
    int mode;
    bool verbose;
    unsigned int nfiles;

    read_args(argc, argv, &hashpath, &nfiles, &verbose, &mode);

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

    unsigned int valid_hashes_count = 0;
    unsigned long long distinct_keys_count = 0;

    const unsigned int keys_table_size = hash_api.out_size * hash_api.out_size;
    Node **keys_table = create_keys_table(keys_table_size);

    process_files(hash_api, keys_table, keys_table_size, filepaths, nfiles, &valid_hashes_count, verbose, &distinct_keys_count, mode);

    analyse(keys_table, keys_table_size, hash_api, distinct_keys_count, valid_hashes_count, nfiles);

    destroy_keys_table(keys_table, keys_table_size);

    dlclose(handle);

    return 0;
}