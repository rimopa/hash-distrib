CC       := clang
CFLAGS   := -Wall -Wextra -Werror -std=c23 -pedantic
CPPFLAGS := -Isrc/progressbar/include/progressbar
LDFLAGS  := -lncurses

DEBUGGER := gdb

TARGET   := hash-distrib
NAMEDTARGET := hash-distrib-varnamed
SRC      := src/main.c
ADDITIONAL := src/hash_distribution.c src/readers.c

HASHLIB  := libhash.so
HASHSRC  ?= hash-algorithms/sha256.c

ARGS ?= -m binary
TESTDATA ?= testdata/*

PROGRESSBAR_DIR := src/progressbar
PROGRESSBAR_OBJ := progressbar.o

.PHONY: all test testv testl testvl debug debugl clean

all: $(TARGET)

$(TARGET): $(SRC) $(PROGRESSBAR_OBJ) $(ADDITIONAL)
	$(CC) $(CFLAGS) $(CPPFLAGS) $^ -o $@ $(LDFLAGS)

$(NAMEDTARGET): $(SRC) $(PROGRESSBAR_OBJ) $(ADDITIONAL)
	$(CC) -g $(CFLAGS) $(CPPFLAGS) $^ -o $@ $(LDFLAGS)

$(HASHLIB): $(HASHSRC)
	$(CC) -shared -fPIC -x c $< -o $@

$(PROGRESSBAR_OBJ): $(PROGRESSBAR_DIR)/lib/progressbar.c
	$(CC) $(CFLAGS) -c $(CPPFLAGS) $< -o $@

test: $(HASHLIB) $(TARGET)
	./$(TARGET) ./$(HASHLIB) $(ARGS) $(TESTDATA)


debug: $(HASHLIB) $(NAMEDTARGET)
	$(DEBUGGER) --args ./$(NAMEDTARGET) ./$(HASHLIB) $(ARGS) $(TESTDATA)

clean:
	rm -f $(TARGET) $(NAMEDTARGET) $(HASHLIB) $(PROGRESSBAR_OBJ)
	clear