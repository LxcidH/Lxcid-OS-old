# ========================================================================= #
#                         Makefile for LxcidOS                              #
#         (Corrected for the user's specific project structure)             #
# ========================================================================= #

# --- Commands ---
CC = i686-elf-gcc
AS = nasm

# --- Directories ---
BUILDDIR = build

# --- Source Files ---
# List the full, explicit path to every source file.
C_SOURCES = \
    src/kernel.c \
    drivers/terminal.c \
    drivers/pic.c \
    drivers/keyboard.c \
	shell/shell.c \
    idt/idt.c \
    io/io.c \

ASM_SOURCES = \
    src/boot.asm \
    idt/interrupts.asm \
    io/io.asm

# --- Object Files ---
# Automatically generate the list of object files in the build directory.
OBJS = \
    $(patsubst src/%.c, $(BUILDDIR)/%.o, $(filter src/%.c, $(C_SOURCES))) \
    $(patsubst drivers/%.c, $(BUILDDIR)/%.o, $(filter drivers/%.c, $(C_SOURCES))) \
    $(patsubst idt/%.c, $(BUILDDIR)/%.o, $(filter idt/%.c, $(C_SOURCES))) \
    $(patsubst io/%.c, $(BUILDDIR)/%.o, $(filter io/%.c, $(C_SOURCES))) \
    $(patsubst src/%.asm, $(BUILDDIR)/%.o, $(filter src/%.asm, $(ASM_SOURCES))) \
    $(patsubst idt/%.asm, $(BUILDDIR)/%.o, $(filter idt/%.asm, $(ASM_SOURCES))) \
    $(patsubst io/%.asm, $(BUILDDIR)/%.o, $(filter io/%.asm, $(ASM_SOURCES))) \
 $(patsubst shell/%.c, $(BUILDDIR)/%.o, $(filter shell/%.c, $(C_SOURCES))) \
# --- Flags ---
# Add an include path for every directory that contains header files.
CFLAGS = -ffreestanding -fno-pie -nostdlib -Wall -Wextra -Isrc -Idrivers -Iidt -Iio
ASFLAGS = -f elf32

# ========================================================================= #
#                              BUILD RULES                                  #
# ========================================================================= #

# --- Primary Build Rule ---
all: $(BUILDDIR)/kernel.bin

# Rule to link all object files into the final kernel binary.
$(BUILDDIR)/kernel.bin: $(OBJS)
	@mkdir -p $(@D)
	$(CC) -T linker.ld -o $@ $^ $(CFLAGS) -lgcc

# --- Compilation Rules ---
# We need a separate, explicit rule for each source directory.

$(BUILDDIR)/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/%.o: drivers/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/%.o: idt/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/%.o: io/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/%.o: shell/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/%.o: src/%.asm
	@mkdir -p $(@D)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILDDIR)/%.o: idt/%.asm
	@mkdir -p $(@D)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILDDIR)/%.o: io/%.asm
	@mkdir -p $(@D)
	$(AS) $(ASFLAGS) $< -o $@


# --- Utility Rules ---
run: all
	qemu-system-i386 -kernel $(BUILDDIR)/kernel.bin

clean:
	rm -rf $(BUILDDIR)
