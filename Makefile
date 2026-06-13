CC = gcc
CFLAGS = -Wall -Wextra -Werror
HASHCFLAGS = -g -shared -fPIC $(CFLAGS)

TARGET = hash-distrib
ADDITIONAL = hash_distribution.c
SRC = main.c

HASHTARGET = libhash.so
HASHSRC = hash_example.c

TESTFLAGS = ./$(HASHTARGET) $(TESTDATA)
TESTDATA = testdata/*

PROGRESBARTARGET = progressbar.o
PROGRESSBAR_DIR = progressbar
PROGRESSBAR_INC = -I$(PROGRESSBAR_DIR)/include/progressbar

$(TARGET): $(SRC) $(PROGRESBARTARGET)
	$(CC) $(CFLAGS) $(PROGRESSBAR_INC) $(SRC) $(ADDITIONAL) $(PROGRESBARTARGET) -o $(TARGET) -lncurses

test: $(HASHTARGET) $(TARGET)
	./$(TARGET) $(TESTFLAGS)

$(HASHTARGET): $(HASHSRC)
	$(CC) $(HASHCFLAGS) $(HASHSRC) -o $(HASHTARGET)

$(PROGRESBARTARGET): $(PROGRESSBAR_DIR)/lib/progressbar.c
	$(CC) -c $(PROGRESSBAR_INC) $< -o $@

clean:
	rm -f $(TARGET) $(HASHTARGET) $(PROGRESBARTARGET)
