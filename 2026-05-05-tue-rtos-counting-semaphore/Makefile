CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -Wpedantic -O0 -g
SRCS    = src/sem.c tests.c
HEADERS = src/sem.h
TARGET  = run_tests

# Disable Make's built-in implicit rules so `make tests` (typo) can't
# bypass the build by compiling tests.c alone.
MAKEFLAGS += --no-builtin-rules
.SUFFIXES:

.PHONY: test tests clean

test tests: $(TARGET)
	@./$(TARGET)

$(TARGET): $(SRCS) $(HEADERS)
	$(CC) $(CFLAGS) -I. -o $@ $(SRCS)

clean:
	rm -f $(TARGET) tests
