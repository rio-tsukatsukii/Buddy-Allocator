BUILD_DIR := ./build
SRCS_DIR := ./src
TESTS_DIR := ./tests

TEST_SRC := $(TESTS_DIR)/main.c
LIB_SRCS := $(shell find $(SRCS_DIR) -name '*.c')
ASM_SRCS := $(shell find $(SRCS_DIR) -name '*.S')

TEST_OBJ := $(BUILD_DIR)/main.o
LIB_OBJS := $(patsubst $(SRCS_DIR)/%.c,$(BUILD_DIR)/%.o,$(LIB_SRCS))
ASM_OBJS := $(patsubst $(SRCS_DIR)/%.S,$(BUILD_DIR)/%.o,$(ASM_SRCS))

STATIC_LIB := $(BUILD_DIR)/libbuddy_alloc.a
SHARED_LIB := $(BUILD_DIR)/libbuddy_alloc.so

CC := gcc
AS := gcc
AR := ar

CFLAGS_BASE := -Wall -Wextra -Werror -std=c99 -march=native -fPIC -Iinclude
CFLAGS_OPT := -O2
CFLAGS_DEBUG := -O0 -ggdb3
CFLAGS := $(CFLAGS_BASE) $(CFLAGS_OPT)

NOLIBC_FLAGS := -nostdlib -ffreestanding

shared_lib: $(SHARED_LIB)
static_lib: $(STATIC_LIB)

test: $(BUILD_DIR)/main

debug: CFLAGS := $(CFLAGS_BASE) $(CFLAGS_DEBUG) -DDEBUG
debug: clean test

verbose_debug: CFLAGS := $(CFLAGS_BASE) $(CFLAGS_DEBUG) -DVERBOSE_DEBUG
verbose_debug: clean test

$(BUILD_DIR)/main: $(TEST_OBJ) $(SHARED_LIB) | $(BUILD_DIR)
	@echo "[LD]	$@"
	@$(CC) $(TEST_OBJ) $(SHARED_LIB) -o $@

$(TEST_OBJ): $(TEST_SRC) | $(BUILD_DIR)
	@echo "[CC]	$@"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: $(SRCS_DIR)/%.c | $(BUILD_DIR)
	@echo "[CC]	$@"
	@$(CC) $(CFLAGS) $(NOLIBC_FLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: $(SRCS_DIR)/%.S | $(BUILD_DIR)
	@echo "[AS]	$@"
	@$(AS) -c -o $@ $<

$(STATIC_LIB): $(LIB_OBJS) $(ASM_OBJS) | $(BUILD_DIR)
	@echo "[AR]	$@"
	@$(AR) rcs $@ $^

$(SHARED_LIB): $(LIB_OBJS) $(ASM_OBJS) | $(BUILD_DIR)
	@echo "[LD]	$@"
	@$(CC) -shared -Wl,-soname,libbuddy_alloc.so.1 -o $@ $^

$(BUILD_DIR):
	@echo "Creating $(BUILD_DIR)"
	@mkdir -p $(BUILD_DIR)

.PHONY: clean static_lib shared_lib test debug verbose_debug
clean:
	@echo "Cleaning $(BUILD_DIR)"
	@rm -rf $(BUILD_DIR)
