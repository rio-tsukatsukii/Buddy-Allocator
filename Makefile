BUILD_DIR := ./build
SRCS_DIR := ./src
TESTS_DIR := ./tests

TEST_SRC := $(TESTS_DIR)/main.c
LIB_SRCS := $(shell find $(SRCS_DIR) -name '*.c')
ASM_SRCS := $(shell find $(SRCS_DIR) -name '*.S')

TEST_OBJ := $(BUILD_DIR)/main.o
LIB_OBJS := $(patsubst $(SRCS_DIR)/%.c,$(BUILD_DIR)/%.o,$(LIB_SRCS))
ASM_OBJS := $(patsubst $(SRCS_DIR)/%.S,$(BUILD_DIR)/%.o,$(ASM_SRCS))

LIB := libbuddy_alloc

CC := gcc
AS := gcc
AR := ar
LD := ld

CFLAGS_BASE := -Wall -Wextra -Werror -std=c99 -march=native -fPIC -Iinclude
CFLAGS_OPT := -O2
CFLAGS_DEBUG := -O0 -ggdb3
CFLAGS := $(CFLAGS_BASE) $(CFLAGS_OPT)

NOLIBC_FLAGS := -nostdlib -ffreestanding

LDFLAGS := -Wl,-rpath,$(BUILD_DIR)

shared_lib: $(BUILD_DIR)/$(LIB).so
static_lib: $(BUILD_DIR)/$(LIB).a

test: $(BUILD_DIR)/main

debug: CFLAGS := $(CFLAGS_BASE) $(CFLAGS_DEBUG) -DDEBUG
debug: clean test

verbose_debug: CFLAGS := $(CFLAGS_BASE) $(CFLAGS_DEBUG) -DVERBOSE_DEBUG
verbose_debug: clean test

$(BUILD_DIR)/main: $(TEST_OBJ) $(BUILD_DIR)/$(LIB).so | $(BUILD_DIR)
	@echo "[CC]	$@"
	@$(CC) $(TEST_OBJ) -L$(BUILD_DIR) -l$(LIB:lib%=%) $(LDFLAGS) -o $@

$(TEST_OBJ): $(TEST_SRC) | $(BUILD_DIR)
	@echo "[CC]	$@"
	@$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: $(SRCS_DIR)/%.c | $(BUILD_DIR)
	@echo "[CC]	$@"
	@$(CC) $(CFLAGS) $(NOLIBC_FLAGS) -c -o $@ $<

$(BUILD_DIR)/%.o: $(SRCS_DIR)/%.S | $(BUILD_DIR)
	@echo "[AS]	$@"
	@$(AS) -c -o $@ $<

$(BUILD_DIR)/$(LIB).a: $(LIB_OBJS) $(ASM_OBJS) | $(BUILD_DIR)
	@echo "[AR]	$@"
	@$(AR) rcs $@ $^

$(BUILD_DIR)/$(LIB).so: $(LIB_OBJS) $(ASM_OBJS) | $(BUILD_DIR)
	@echo "[LD]	$@"
	@$(LD) -shared -o $@ $^

$(BUILD_DIR):
	@echo "Creating $(BUILD_DIR)"
	@mkdir -p $(BUILD_DIR)

.PHONY: clean static_lib shared_lib test debug verbose_debug
clean:
	@echo "Cleaning $(BUILD_DIR)"
	@rm -rf $(BUILD_DIR)
