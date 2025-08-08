# Define the commands to run for the cross-compiler
AS = i686-elf-as
CC = i686-elf-gcc
LD = i686-elf-ld

# Define source and build directories
SRCDIR = src
DRVDIR = drivers
BUILDDIR = build

# Use VPATH to tell make where to find source files
VPATH = $(SRCDIR):$(DRVDIR)

# List source file BASENAMES, not full paths
C_SOURCES = kernel.c terminal.c
ASM_SOURCES = boot.asm

# Create lists of object files in the build directory
C_OBJS = $(patsubst %.c, $(BUILDDIR)/%.o, $(C_SOURCES))
ASM_OBJS = $(patsubst %.asm, $(BUILDDIR)/%.o, $(ASM_SOURCES))
OBJS = $(C_OBJS) $(ASM_OBJS)

# Flags for the C compiler
# Add include paths for both src and drivers directories
CFLAGS = -ffreestanding -fno-pie -nostdlib -Wall -Wextra -I$(SRCDIR) -I$(DRVDIR)

# --- Primary Build Rules ---

all: $(BUILDDIR)/kernel.bin

# Rule to link all object files into the final kernel binary
# FIX: Added $(CFLAGS) to the linker command to pass -nostdlib
$(BUILDDIR)/kernel.bin: $(OBJS)
	$(CC) -T linker.ld -o $@ $^ $(CFLAGS) -lgcc

# --- Compilation Rules ---

# A generic rule to compile any .c file into a .o file
# Because of VPATH, make will find the source file in the correct directory
$(BUILDDIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) -c $< -o $@ $(CFLAGS)

# A generic rule to assemble any .asm file into a .o file
$(BUILDDIR)/%.o: %.asm
	@mkdir -p $(@D)
	nasm $< -f elf32 -o $@

# --- Utility Rules ---

run: all
	qemu-system-i386 -kernel $(BUILDDIR)/kernel.bin

clean:
	rm -rf $(BUILDDIR)
