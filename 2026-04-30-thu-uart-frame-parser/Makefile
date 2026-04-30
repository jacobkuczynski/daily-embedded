CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -Wpedantic -O0 -g
SRCS    = src/frame_parser.c tests.c
HEADERS = src/frame_parser.h
TARGET  = run_tests

# Disable Make's built-in implicit rules and suffix rules.
# Otherwise `make tests` falls back to "build tests from tests.c"
# and silently links without src/frame_parser.c, producing confusing
# undefined-symbol errors. We want our explicit rules, every time.
MAKEFLAGS += --no-builtin-rules
.SUFFIXES:

.PHONY: test tests clean

# Accept either `make test` or `make tests` — both run the same thing.
test tests: $(TARGET)
	@./$(TARGET)

$(TARGET): $(SRCS) $(HEADERS)
	$(CC) $(CFLAGS) -I. -o $@ $(SRCS)

clean:
	rm -f $(TARGET) tests
