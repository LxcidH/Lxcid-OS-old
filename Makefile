# ========================================================================= #
#                         Makefile for LxcidOS                            #
#                (Simplified, Corrected, and Automated)                   #
# ========================================================================= #

# --- Commands ---
CC = i686-elf-gcc
AS = nasm
RM = rm -rf

# --- Directories ---
BUILDDIR = build

# --- Automatic Source File Discovery ---
# This finds all .c and .asm files in the project directories.
C_SOURCES = $(shell find src drivers idt io lib shell memory fs -name '*.c')
ASM_SOURCES = $(shell find src drivers idt io lib shell -name '*.asm')

# --- Object Files ---
# This automatically generates the .o paths in the build directory.
C_OBJS = $(patsubst %.c, $(BUILDDIR)/%.o, $(C_SOURCES))
ASM_OBJS = $(patsubst %.asm, $(BUILDDIR)/%.o, $(ASM_SOURCES))
OBJS = $(C_OBJS) $(ASM_OBJS)

# --- Flags ---
# Add -I for every directory that might contain a .h file.
# Also add -Ilib because you should create a string.h in there.
CFLAGS = -ffreestanding -fno-pie -nostdlib -Wall -Wextra -Isrc -Idrivers -Iidt -Iio -Ilib -Ishell -Imemory
ASFLAGS = -f elf32
LDFLAGS = -T linker.ld -lgcc

# ========================================================================= #
#                               BUILD RULES                                 #
# ========================================================================= #

# --- Primary Build Rule ---
all: $(BUILDDIR)/kernel.bin

# Rule to link all object files into the final kernel binary.
$(BUILDDIR)/kernel.bin: $(OBJS)
	@mkdir -p $(@D)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)
	@echo "✅ Kernel linked successfully: $@"

# --- Generic Compilation Rules ---
# This single rule handles ALL C files from any directory.
$(BUILDDIR)/%.o: %.c
	@mkdir -p $(@D)
	@echo "CC $<"
	$(CC) $(CFLAGS) -c $< -o $@

# This single rule handles ALL Assembly files from any directory.
$(BUILDDIR)/%.o: %.asm
	@mkdir -p $(@D)
	@echo "AS $<"
	$(AS) $(ASFLAGS) $< -o $@

# --- Utility Rules ---
run: all
	qemu-system-i386 -kernel $(BUILDDIR)/kernel.bin
clean:
	@echo "Cleaning build directory..."
	$(RM) $(BUILDDIR)

.PHONY: all run clean
