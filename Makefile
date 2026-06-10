CC = gcc
CFLAGS = -Wall -Wextra -Werror
HASHCFLAGS = -g -shared -fPIC $(CFLAGS)

TARGET = hash-distrib
ADDITIONAL = hash_distribution.c
SRC = main.c

HASHTARGET = libhash.so
HASHSRC = example-hash.c

TESTFLAGS = ./$(HASHTARGET) $(TESTDATA)
TESTDATA = testdata/*

PROGRESSBAR_DIR = progressbar
PROGRESSBAR_INC = -I$(PROGRESSBAR_DIR)/include/progressbar

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) $(ADDITIONAL) -o $(TARGET)

test: $(HASHTARGET) $(TARGET)
	./$(TARGET) $(TESTFLAGS)

$(HASHTARGET): $(HASHSRC)
	$(CC) $(HASHCFLAGS) $(HASHSRC) -o $(HASHTARGET)

progressbar.o: $(PROGRESSBAR_DIR)/lib/progressbar.c
	$(CC) -c $(PROGRESSBAR_INC) $< -o $@

clean:
	rm -f $(TARGET) $(HASHTARGET) progressbar.o