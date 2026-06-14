CC = gcc
CFLAGS = -g -Wall -Wextra -Werror
HASHCFLAGS = -g -shared -fPIC $(CFLAGS)

DEBUGGER = gdb

TARGET = hash-distrib
ADDITIONAL = hash_distribution.c
SRC = main.c

HASHTARGET = libhash.so
HASHSRC = hash_example.c
TESTDATA = testdata/*

PROGRESBARTARGET = progressbar.o
PROGRESSBAR_DIR = progressbar
PROGRESSBAR_INC = -I$(PROGRESSBAR_DIR)/include/progressbar

$(TARGET): $(SRC) $(PROGRESBARTARGET)
	$(CC) $(SRC) $(CFLAGS) $(PROGRESSBAR_INC) $(ADDITIONAL) $(PROGRESBARTARGET) -o $(TARGET) -lncurses

test: $(HASHTARGET) $(TARGET)
	./$(TARGET) ./$(HASHTARGET) $(TESTDATA)

testv: $(HASHTARGET) $(TARGET)
	./$(TARGET) ./$(HASHTARGET) -v $(TESTDATA)


$(HASHTARGET): $(HASHSRC)
	$(CC) $(HASHCFLAGS) $(HASHSRC) -o $(HASHTARGET)

$(PROGRESBARTARGET): $(PROGRESSBAR_DIR)/lib/progressbar.c
	$(CC) -c $(PROGRESSBAR_INC) $< -o $@

debug: $(HASHTARGET) $(TARGET)
	$(DEBUGGER) --args ./$(TARGET) $(TESTFLAGS)

clean:
	rm -f $(TARGET) $(HASHTARGET) $(PROGRESBARTARGET)
