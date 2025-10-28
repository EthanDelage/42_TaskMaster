# Directories
TEST_SRC_DIR := test/src
TEST_BIN_DIR := test/bin

# Compiler and flags
CC := gcc

# Find test sources and their output binaries
TEST_SRCS := $(wildcard $(TEST_SRC_DIR)/*.c)
TEST_BINS := $(patsubst $(TEST_SRC_DIR)/%.c,$(TEST_BIN_DIR)/%,$(TEST_SRCS))

# Default target
test: $(TEST_BINS)

# Build rule: test/src/foo.c → test/bin/foo
$(TEST_BIN_DIR)/%: $(TEST_SRC_DIR)/%.c | $(TEST_BIN_DIR)
	$(CC) $< -o $@

# Ensure bin dir exists
$(TEST_BIN_DIR):
	mkdir -p $(TEST_BIN_DIR)

# Clean all test binaries
clean_tests:
	rm -rf $(TEST_BIN_DIR)

# Rebuild all tests (clean + build)
re_test: clean_tests test

.PHONY: test clean_tests re_test
