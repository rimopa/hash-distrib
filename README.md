# hash-distrib
#### Video Demo:  https://youtu.be/pBFAOuB8oR8
#### Description:

`hash-distrib` is a tool for beginners to easily analyse the distribution of keys of a hash function across a dataset.

In very short words, the program communicates with the provided hash function, hashes the files and stores the returned keys, and analyses how repeated the keys were.

A short description of the function of each file, before a deep insight into the program's sequence:

- `main.c` is the orchestrator that guides all of what is done, and where `main()` resides.
- `hash_distribution.c` and `hash_distribution.h` have the functions related to managing the keys and analyzing the final data.
- `readers.c` and `readers.h` have the functions related to reading the data from the inputted files.
- `hash_api.h` is a header file that specifies how a `hash_api` must be set up, and so is the file that allows `hash-distrib` and the provided hash function to communicate at runtime.

The program's sequence in a much more deep insight:

1. Receiving information from arguments.
   
    The first thing `hash-distrib` does is receiving information from the passed arguments in a syntax like this:

    ```sh
    hash-distrib <libhash.so> [options] <file>
    ```

    where:

    - `libhash.so` refers to a shared library containing the corresponding hash API to the provided hash function.

    - `file` refers to the file or files to hash with libhash.so.
    
    And the available options are:
    - `-v`: tell `hash-distrib` to be extremely verbose and print what it is doing at each step.
    - `-d`: force the program to print a detailed list of each key that was returned and how many times.
    - `-t` force the program to print a table showing, for each return count, how many distinct keys were returned that many times. Further details in step 7.
    - `-m binary` or `-m bin` or `-m b` is the default binary hash mode, in which every file is hashed separately.
    - `-m line` or `-m l` toggles on the line hash mode (instead of binary), where each line of each file is hashed separately.

2. Get `hash_api`
   This is one of the most interesting steps, because it's when the `hash-distrib` actually communicates at runtime with `libhash.so`.

    In this step, `hash-distrib` opens a handle to `libhash.so` with `dlopen()`, then searches for a function called `hash_api()` with `dlsym()`, and finally executes it to get the provided hash function's `HashAPI` struct (defined in `hash_api.h`), which contains all the needed information about the provided hash function.

    The `HashAPI` must include all this information:
    
    - `const char *name`: name of the hash function or algorithm.
    
    - `size_t ctx_size`: bytes needed for the functioning of the hash algorithm to preserve state between `update` calls.
    
    - `size_t out_size`: size of the returned key in bytes. Must be > 0 and < 256. Explained in final note.
    
    - `void (*init)(void *ctx)`: function to clean and set `ctx`'s to its initial state, ready for `update` to operate over it.
    
    - `void (*update)(void *ctx, const unsigned char *data, size_t len)`: function that reads `len` bytes from `data`, does what the function needs to hash those bytes and stores the state on `ctx`.
      
    - `void (*final)(void *ctx, unsigned char *out)`: function which reads from `ctx`, (produces if necessary) and writes the final key (`out_size` bytes long) at `out`.

3. Set up `keys_db`
    Before hashing any file, there must be somewhere to put the returned keys. This is exactly `key_db`'s role here.

    Here `create_keys_db()` (found in `hash_distribution.c`) is called, and evaluates whether it should use a hash table for storing the keys, or, if there are not many files, use a simpler unbucketed array instead.

4. Preparing files or lines to be hashed
    
    A progressbar (from [this project](https://github.com/doches/progressbar)) that will follow the hashing is set up, so that users in a terminal can visually see the hashes being done with each file. The verbose flag disables this, although it gives much more detail instead.

    Each file is safely opened with `safe_fopen()`, which checks for opening errors.
    
    Additionaly, if the line mode was set in the arguments, `process_lines()` is called and `getline()` is used to read each line.

5. Hashing
   
   This part is managed mostly in `readers.c`.

   `ctx_size` is used to allocate `ctx`, for the provided hash functions' private usage between `hash_api.update()` calls.

   Aditionally, `out_size` is used to allocate `hash_key`, which will be used by the hash function to write its final hash.

   Then, the procedure explained above is followed for retrieving a valid hash key from the hash function:

   - First, `hash_api.init()` is called to clean up `ctx` and set everything to its initial value.

   - Then, `hash_api.update()` is called once or more, depending on the file/line's size.

   - Finally, `hash_api.final()` is called for the hash function to write its final key to `hash_key`.

6. Adding to `key_db`
   
   This part is managed in `hash_distributions.c`.

   After the file/line has been hashed, it needs to be stored in `key_db`. So, `key_db_add()` is called, which, depending on if the used method is the bucketed or the unbucketed array-like one, calls `key_db_buckets_add()` or `key_db_arr_add()`.

   - In case `key_db_buckets_add()` is selected:

        The key is XOR'd into only 2 bytes using `keys_table_index()`, those 2 bytes are taken as a `uint16_t`, then modulo'd with the table's size, and finally used as the index for the hash table.
   
        Then, it goes to that index, and starts comparing each node's `key_pointer` with `hash_key_pointer` using `memcmp()` (and reading `out_size` bytes).
    
        If the node points to the same key, then the node's count is incremented by one (`node->count++;`).
    
        If it's not found, then the last node (which was previously pointing to nullptr) is set to point towards a new node with this newfound key and a `count` of 1.
   
   - In case `key_db_arr_add()` is chosen instead, the sequence is much simpler. The array is checked for the same key until `node_count`. If found, the count is incremented by 1 in the key's node. Otherwise, a new node is added at index `node_count`, with the newfound key and `node_count` incremented by 1 for the next iteration to occur correctly.

7. Analysing
   
   This final part managed in `hash-distribution.c` is the most impressive for me, as to how much data can be obtained from this simple `key_db` structure.

   After a very general performance description, `analyse()` tries to signal curious things about the hash's distribution, for example, if all the keys were the same or all of them were different (no collisions).

   If neither `-d` nor `-t` were passed as options, `hash-distrib` may decide one to display depending on the distribution of the keys.

   Then, it goes through each step:

   - `-d` Details: a simple loop that cycles through `key_db` and prints each key next to its corresponding key in hexadecimal, which is the most commonly used representation of hash keys in strings.
   
   - `-t`, Table:
        
        First, `create_count_of_counts()` is called, which creates a new hash table called `count_of_counts` using [this project](https://github.com/troydhanson/uthash).

        `count_of_counts` does not sound very intuitive, but does exactly what its name says: counting counts.

        The function iterates all over `key_db` to count how many times each count appeared.
        For example, it may find one key that appeared three times (its `count` is 3), so it stores `1 3`, and then finds another that appeared three times, so the number of keys that had a return count of 3 is incremented to 2 (`2 3`).

        This is finally displayed in a table with that same structure. For example:

        | Count of `count` | `count` |
        | ---------------- | ------- |
        | 32               | 1       |
        | 5                | 2       |
        | 1                | 7       |

        Should be read like:
        > 32 keys were returned once each, 5 keys were returned twice each, and 1 key was returned 7 times.

        The perfect hash function with a dataset that doesn't repeat data would have a table where all the keys were returned only once each.

        Note: `count_of_counts` is freed in this same step.
    - Bonus mean:
  
        An additional mean value is calculated taking all the sum of the counts in `key_db` and dividing them by `distinct_keys_count` in `print_mean()`.

        The less the keys were repeated, the closer the mean is going to be 1. Conversely, the more they were repeated, the bigger and closer it is going to be to the number of hashes that were made.

8. Cleanup
   
   After all that has been done and hopefully `hash-distrib` encountered no problems, a final cleanup is done.

   - `key_db` is freed using `destroy_key_db()`, which iterates each node and frees it.
   - `libhash.so`'s handle is closed.


#### Some design choices I had to make

##### `out_size` maximum value
I set `out_size` a maximum of 255 (2^8 - 1) bytes.

First, the size of the hash table used in `key_db` is set up to have `out_size`^2 buckets.

Second, 2^16 - 1 is the maximum value that is guaranteed to be representable with an `unsigned int` (read [here](https://en.wikipedia.org/wiki/C_data_types)), the type which stores the size.

As a result, a key size of 256 bytes (or more) would generate a size of 2^16 or more, which is a value that'd need at least 17 bits to be represented, and then could overflow `key_db.size`, which is extremely dangerous and may cause disaster down the road, especially if it's set to exactly 0.

As an additional point, the hash function with the longest key in [this list](https://en.wikipedia.org/wiki/List_of_hash_functions) (and the longest I know of) is [FNV](https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function) with the 1024-bit variant.
And that's 128 bytes, only half of the 256 bytes my program allows.

In conclusion, adding support for >=256 hashes would require rewriting the logic of `key_db`, which I don't feel is necessary because I haven't seen hash keys nearly that long.

#### compiling + shared library vs. just compiling everything together

One of the first decisions I had to make was the way `hash-distrib` would communicate with the provided hash functions.

The main two options were:
- Compiling once and using shared libraries as plugins
  
  _or_

- Distributing my program as a single header file that should be `#include`d at the start of the user's file with the hash function, and some provided function called in their main(), then all compiled together.
  
And as this is a tool for beginners, making the right call was extremely important.

So I compared them with a table like this.

|                                    | compiled + shared library                                                          | one single compile                                                                                                          |
| ---------------------------------- | ---------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------------- |
| Opportunity to learn for me        | More, as I'd need to investigate how to manage shared libraries and how they work. | Less, including logic is already easy for me.                                                                               |
| Possible variable name overlapping | Impossible.                                                                        | Possible.                                                                                                                   |
| Simplicity for beginners           | Initially a little more, as I may need to explain compiler flags.                  | Initially a little bit less, but more later in the procedure of getting the program running.                                |
| Compatibility concerns             | More, as shared libraries are not managed the same way in every platform.          | Less, as it is completely local code compiled locally in the users' machine.                                                |
| Performance                        | Maybe worse because the compiler compiles as two separate things.                  | Theoretically may be better because the compiler sees all as one single code.                                                 |
| Modularity of the project          | Free.                                                                              | Pretty restricted, have to fit everything into one single `.h` file, and have as few `.c`s in the compile step as possible. |

In the end, what really convinced me to go for compiled distribution and shared libraries with a plugin architecture is the opportunity to learn to manage shared libraries and its logic.
Additionally, the fact that I'd have to distribute all of my project as a single `.h` didn't really convince me.

Either way, if the command procedure would become complicated, I might make the Makefile more friendly and tell them to just run that, which would surely simplify the procedure a lot.