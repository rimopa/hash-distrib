#define _XOPEN_SOURCE 700
#include <dlfcn.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "progressbar/include/progressbar/progressbar.h"

#include "hash_distribution.h"
#include "readers.h"
#include "hash_api.h"

#define HASH_API_FUNC_NAME "hash_api"
#define OUT_SIZE_LOWER_BOUND 1
#define OUT_SIZE_UPPER_BOUND 255

static void
usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s <libhash.so> [options] <file>...\n"
            "\n"
            "  libhash.so       Executable as hash function to evaluate\n"
            "  file             Files to hash with libhash.so\n"
            "\n"
            "  Options:\n"
            "  -d               Print details of how many times each key was returned\n"
            "  -t               Print a table reflecting how common each number of times a key was returned is\n"
            "  -v               Verbose\n"
            "  -m binary        Default. Hash whole files as binary data\n"
            "  -m line          Open files as text and hash each line (text separated by \\n)\n",
            prog);
}

HashAPI get_hash_api(void *handle)
{
    char *error;
    HashAPI (*hash_api_func)(void) = dlsym(handle, HASH_API_FUNC_NAME);
    if ((error = dlerror()) != nullptr)
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
        return HASHING_MODE_BINARY;
    else if (strcmp(mode_string, "l") == 0 || strcmp(mode_string, "line") == 0)
        return HASHING_MODE_LINES;
    else
        return -1;
}

void read_args(int argc, char *argv[], char **hashpath, unsigned int *nfiles, int *mode_pointer, bool *verbose_pointer, bool *table_pointer, bool *details_pointer)
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
    char *mode_string = nullptr;
    while ((opt = getopt(argc, argv, "hdtvm:")) != -1)
    {
        switch (opt)
        {
        case 'h':
            usage(argv[0]);
            exit(0);
        case 'd':
            *details_pointer = true;
            break;
            break;
        case 't':
            *table_pointer = true;
            break;
        case 'v':
            *verbose_pointer = true;
            break;
        case 'm':
            mode_string = optarg;
            break;
        case '?':
            usage(argv[0]);
            exit(2);
        default:
            abort();
        }
    }
    if (mode_string == nullptr)
        *mode_pointer = HASHING_MODE_BINARY;
    else
    {
        *mode_pointer = choose_mode(mode_string);
        if (*mode_pointer < 0)
        {
            fprintf(stderr, "Unrecognized mode %s.\n", mode_string);
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

int process_lines(HashAPI hash_api, void *ctx, KeyDB key_db, bool verbose, FILE *file_pointer)
{
    unsigned int local_hash_count = 0;
    unsigned char *hash_key_pointer;
    char *line = nullptr;
    size_t len = 0;

    while (getline(&line, &len, file_pointer) != -1)
    {
        line[strcspn(line, "\n")] = '\0';

        hash_key_pointer = malloc(hash_api.out_size);

        if (hash_key_pointer == nullptr)
        {
            fprintf(stderr, "Failed to allocate bytes for hash key for a line.\n");
            free(line);
            return -1;
        }
        string_hash(hash_api, ctx, hash_key_pointer, line);
        if (verbose)
            print_bytes_hex(hash_key_pointer, hash_api.out_size, true);
        if (key_db_add(key_db, hash_api.out_size, hash_key_pointer, verbose))
            local_hash_count++;
        else
        {
            free(line);
            return -1;
        }
    }
    free(line);
    return local_hash_count;
}

int binary_process_file(HashAPI hash_api, void *ctx, KeyDB key_db, bool verbose, const char *path, FILE *file_ptr)
{
    unsigned char *hash_key_pointer = malloc(hash_api.out_size);

    if (hash_key_pointer == nullptr)
    {
        fprintf(stderr, "Failed to allocate bytes for hash key for a file.\n");
        return -1;
    }

    if (binary_hash(hash_api, ctx, file_ptr, hash_key_pointer) != 0)
    {
        perror(path);
        fprintf(stderr, "Error: Could not hash %s.\n", path);
        free(hash_key_pointer);
        return 0;
    }
    else
    {
        if (verbose)
            print_bytes_hex(hash_key_pointer, hash_api.out_size, true);
        if (key_db_add(key_db, hash_api.out_size, hash_key_pointer, verbose))
            return 1;
        else
            return -1;
    }
}

bool safe_fopen(const char *path, int mode, FILE **file_ptr_ptr)
{
    *file_ptr_ptr = nullptr;

    if (mode == HASHING_MODE_BINARY)
        *file_ptr_ptr = fopen(path, "rb");
    else if (mode == HASHING_MODE_LINES)
        *file_ptr_ptr = fopen(path, "r");

    if (*file_ptr_ptr == nullptr)
    {
        perror(path);
        fprintf(stderr, "Could not open %s\nSkipping.\n", path);
        return false;
    }
    else
        return true;
}

// Return valid hashes from file
unsigned int process_file(HashAPI hash_api, void *ctx, KeyDB key_db, const char *path, bool verbose, int mode)
{
    FILE *file_ptr;
    unsigned int returnval = 0;

    if (!safe_fopen(path, mode, &file_ptr))
        return 0;

    if (mode == HASHING_MODE_BINARY)
        returnval = binary_process_file(hash_api, ctx, key_db, verbose, path, file_ptr);
    else if (mode == HASHING_MODE_LINES)
        returnval = process_lines(hash_api, ctx, key_db, verbose, file_ptr);

    fclose(file_ptr);
    return returnval;
}

int process_files(HashAPI hash_api, KeyDB key_db, const char *filepaths[], unsigned int nfiles, unsigned long *valid_hashes_count_pointer, bool verbose, int mode)
{
    void *ctx = malloc(hash_api.ctx_size);

    int process_file_returnval = 0;

    if (ctx == nullptr)
    {
        fprintf(stderr, "Failed to allocate bytes for ctx.\n");
        return -1;
    }

    progressbar *progress;
    if (!verbose)
        progress = progressbar_new("Hashing data", nfiles);

    for (unsigned int i = 0; i < nfiles; i++)
    {
        if (verbose)
            printf("Hashing data from file: %s.\n", filepaths[i]);
        else
            progressbar_inc(progress);

        if ((process_file_returnval = process_file(hash_api, ctx, key_db, filepaths[i], verbose, mode)) < 0)
        {
            if (!verbose)
                progressbar_finish(progress);
            free(ctx);
            return -2;
        }
        else
            *valid_hashes_count_pointer += process_file_returnval;
    }
    if (!verbose)
        progressbar_finish(progress);
    free(ctx);

    return 0;
}

bool is_valid_out_size(size_t out_size)
{
    if (out_size < OUT_SIZE_LOWER_BOUND || out_size > OUT_SIZE_UPPER_BOUND)
    {
        printf("Invalid out_size: must be %d <= out_size <=%d, got %zu.\n", OUT_SIZE_LOWER_BOUND, OUT_SIZE_UPPER_BOUND, out_size);
        return false;
    }
    return true;
}

static void close_handle(void **h)
{
    if (*h)
        dlclose(*h);
}

static void free_key_db(KeyDB *kdb) { destroy_key_db(*kdb); }

int main(int argc, char *argv[])
{
    char *hashpath;
    int mode;
    bool verbose, table, details;
    verbose = table = details = false;
    unsigned int nfiles;

    read_args(argc, argv, &hashpath, &nfiles, &mode, &verbose, &table, &details);

    if (nfiles < 1)
    {
        usage(argv[0]);
        printf("No files to hash provided.\n");
        return 6;
    }

    const char *filepaths[nfiles];

    read_filepaths(argv, nfiles, filepaths);

    [[gnu::cleanup(close_handle)]] void *handle = open_handle(hashpath);
    const HashAPI hash_api = get_hash_api(handle);

    if (!is_valid_out_size(hash_api.out_size))
        return 5;

    unsigned long valid_hashes_count = 0;

    [[gnu::cleanup(free_key_db)]] KeyDB key_db = create_key_db(hash_api, mode, nfiles, verbose);
    if (key_db.table == nullptr || key_db.node_count_ptr == nullptr)
    {
        fprintf(stderr, "Failed to allocate memory for Key DB.\n");
        return 7;
    }

    if (process_files(hash_api, key_db, filepaths, nfiles, &valid_hashes_count, verbose, mode) < 0)
        return 8;

    analyse(key_db, hash_api, valid_hashes_count, nfiles, table, details);

    return 0;
}