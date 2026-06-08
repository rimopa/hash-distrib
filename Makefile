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

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) $(ADDITIONAL) -o $(TARGET)

test: $(HASHTARGET) $(TARGET)
	./$(TARGET) $(TESTFLAGS)

$(HASHTARGET): $(HASHSRC)
	$(CC) $(HASHCFLAGS) $(HASHSRC) -o $(HASHTARGET)

clean:
	rm $(TARGET) $(HASHTARGET)