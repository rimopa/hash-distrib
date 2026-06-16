Tool for beginners to quickly and graphically see the distribution of keys of a hash function across a dataset.

## Usage
How to use hash-distrib

`hash-distrib <libhash.so> [options] <file> ...`
| Name       | Meaning                                              |
|------------|------------------------------------------------------|
| libhash.so | Shared library containing the corresponding hash API |
| file       | Files to hash with libhash.so                        |
| -v         | Verbose flag                                         |
| -d         | Print details of how many times each key was returned|
| -t         | Print a table showing, for each return count, how many distinct keys were returned that many times (e.g. a row showing 9 1 means 9 keys were each returned once).|
| -m binary  | Default mode, hash whole files as binary data        |
| -m line    | Line mode, hash each line of a text file             |

Examples:
```sh
./hash-distrib libhash.so                          data.xyz and.yzx also.zxy even.xlsx
./hash-distrib lib_my_better_hash.so -m line -d    data.txt 
./hash-distrib librother.so          -v -m line -t hellosekai.c helloworld.h hellomundo.asm
./hash-distrib libhashman.so         -m binary -v  data.pdf data2.pdf data3.pdf
```

## Testing

A quick way to try out hash-distrib.

1. Clone the repo.
  ```sh
  git clone --recursive https://github.com/rimopa/hash-distrib
  ```

2. In the repo folder, build the provided algorithm of your choice as a shared library. For example, SHA256. This will let hash-distrib access its functions on the fly.
  ```sh
  cd hash-distrib
  gcc -shared -fPIC hash-algorithms/sha256.c -o libsha256.so
  ```

3. If you don't have it already, get the compiled hash-distrib.
  ```sh
  make hash-distrib
  ```

4. Run hash-distrib with your dataset!
  ```
  ./hash-distrib ./libsha256.so my_data_folder/data.dat other_data/2nd_data.png …
  ```
  or, if you'd prefer to hash lines of text instead of whole files:
  ```
  ./hash-distrib ./libsha256.so -m line dataset1/poem.txt even_more_data/datacenter.csv …
  ```

## Adapting your hash

### Understanding the Hash API

How to adapt your hash to the hash-distrib.

To adapt your hash, first of all, you should read the structure of the API that'll let you and hash-distrib communicate.

```c
typedef struct
{
    const char *name; // Name of your hash algorithm
    size_t ctx_size;  // Size you need for your context in bytes
    size_t out_size;  // Size of your returned key in bytes. Must be >= 2 and <= 256
    void (*init)(void *ctx);
    void (*update)(void *ctx, const unsigned char *data, size_t len);
    void (*final)(void *ctx, unsigned char *out);
} HashAPI;
```

In more detail:

- `name`: the name of your hash algorithm, it can be whatever you like.
- `ctx_size`: The size of your context in bytes. That is, size of the state you need to persist between one call and the next of the update function. More explanation about how to use `init`, `update` and `final` below.
- `out_size`: The size in bytes you need to store the hash key your hash outputs. Its minimum is 2 (2 bytes) and its maximum is 256 bytes.

When hash-distrib processes an entry, it always calls your functions in the same order: `init`, then `update` one or more times, then `final`. You can think of it like brewing a cup of tea: first you prepare the cup (`init`), then you pour in the hot water and let it steep (`update`, possibly in several pours if the data is large), and finally you set the cup down and the tea that's ready is your result (`final`).

`init(void *ctx)`
This is called at the start of each new entry, before any data arrives. Your job here is to set your context to its clean starting state, for instance, zero out counters, set seed values, get ready to receive data. Think of it as wiping the slate clean. The `ctx` pointer already points to a block of memory of exactly the size you declared in `ctx_size`; you don't allocate anything, you just fill it in.

`update(void *ctx, const unsigned char *data, size_t len)`
This is where the actual input arrives. hash-distrib may call `update` more than once per entry if the data is large. It feeds it to you in chunks of some n bytes at a time. Your function receives a pointer to the current chunk and its length. You should "absorb" that data into your context however your algorithm requires. Don't assume you'll see all the data in one call, because there may always be more updates incoming until `final` is called.
Sidenote: if you're wondering why separate and not fully hash everything in a single chunk, that's impossible (or extremely inefficient) for many complex algorithms or large sets of data. A simple way to imagine it: that would require you to load the entire files into memory, which is very heavy if you have a large one.

`final(void *ctx, unsigned char *out)`
Called once, after all the data has been fed through `update`. This is where you compute and emit the final hash value. Write exactly `out_size` bytes into the `out` buffer — that's the key hash-distrib will use to count distribution.

Some quick tips:
- The parameter ctx is always `void*` because hash-distrib doesn't know your context struct type. So, a good practice is to cast your own type at the start of each function (`init`, `update`, `final`), for example `my_ctx *c = (my_ctx *)ctx;`.
- In `final`, `out` is a raw byte buffer, so be careful when filling it. A good practice is to cast your key into variables and copy them using `memcpy`.

### Writing it down and compiling

Once you've adapted your functions:

1. Add the hash_api.h header

```c
#include "hash_api.h"
```

2. Write your hash data into an API like this to return:

```c
static const HashAPI api = {
    .name = "awesome_algorithm",

    .ctx_size = sizeof(awa_ctx),
    .out_size = AWESOME_OUT_SIZE,

    .init = awa_init,
    .update = awa_update,
    .final = awa_final};

HashAPI hash_api(void)
{
    return api;
}
```
Note: When you're executing the program, hash-distrib will do runtime access to `hash_api(void)` by its name. So, it is very important that the function which returns your `HashAPI` is called exactly `hash_api(void)`.

3. Compile your program as a shared library. This will let hash-distrib access `hash_api()` (and by extension, your API) on the fly.

```sh
gcc -shared -fPIC awesome_algo.c -o libawa.so
```

4. Get your dataset analysed!

```sh
./hash-distrib ./libawa.so testdata.xyz testpicture.png …
```

## Used resources

- [uthash](https://github.com/troydhanson/uthash)
- [progressbar](https://github.com/doches/progressbar)