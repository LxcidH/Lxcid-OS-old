# ========================================================================= #
#                         Makefile for LxcidOS                            #
#                (Cleaned, Corrected, and Automated)                      #
# ========================================================================= #

# --- Commands ---
CC = i686-elf-gcc
AS = nasm
RM = rm -rf

# --- Directories ---
BUILDDIR = build

# --- Kernel Source Files ---
# Note: 'user' directory is now excluded from the kernel build.
C_SOURCES = $(shell find src drivers idt io lib shell memory fs -name '*.c')
ASM_SOURCES = $(shell find src drivers idt io lib shell -name '*.asm')

# --- User Program Source Files ---
USER_ASM_SOURCES = $(shell find user -name '*.asm')

# --- Object & Target Files ---
C_OBJS = $(patsubst %.c, $(BUILDDIR)/%.o, $(C_SOURCES))
ASM_OBJS = $(patsubst %.asm, $(BUILDDIR)/%.o, $(ASM_SOURCES))
KERNEL_OBJS = $(C_OBJS) $(ASM_OBJS)

# This automatically creates the list of .bin targets for user programs
USER_BINS = $(patsubst user/%.asm, $(BUILDDIR)/user/%.bin, $(USER_ASM_SOURCES))

# --- Flags ---
CFLAGS = -ffreestanding -fno-pie -nostdlib -Wall -Wextra -Isrc -Idrivers -Iidt -Iio -Ilib -Ishell -Imemory -Ifs
ASFLAGS = -f elf32
LDFLAGS = -T linker.ld -lgcc

# ========================================================================= #
#                               BUILD RULES                                 #
# ========================================================================= #

# --- Primary Build Rule ---
# This now depends on the kernel and all user programs found.
all: $(BUILDDIR)/kernel.bin $(USER_BINS)

# Rule to link the kernel.
$(BUILDDIR)/kernel.bin: $(KERNEL_OBJS)
	@mkdir -p $(@D)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)
	@echo "✅ Kernel linked successfully: $@"

# --- Generic Compilation Rules ---
# Rule for all kernel C files.
$(BUILDDIR)/%.o: %.c
	@mkdir -p $(@D)
	@echo "CC $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Rule for all kernel Assembly files.
$(BUILDDIR)/%.o: %.asm
	@mkdir -p $(@D)
	@echo "AS $<"
	$(AS) $(ASFLAGS) $< -o $@

# --- User Program Build Rule ---
# This is the corrected rule for building flat binaries.
$(BUILDDIR)/user/%.bin: user/%.asm
	@mkdir -p $(@D)
	@echo "AS (bin) $<"
	$(AS) -f bin $< -o $@

# --- Utility Rules ---
run: all
	@echo "Mounting disk image to copy program..."
	@mkdir -p ./tmp_mount
	@sudo mount fat_disk.img ./tmp_mount
	@sudo cp build/user/hello.bin ./tmp_mount/
	@sudo umount ./tmp_mount
	@rmdir ./tmp_mount
	@echo "Starting QEMU..."
	qemu-system-i386 -kernel $(BUILDDIR)/kernel.bin -hda fat_disk.img

clean:
	@echo "Cleaning build directory..."
	$(RM) $(BUILDDIR)

clean-drive:
	@echo "Cleaning HDD..."
	rm -rf fat_disk.img
	@echo "HDD img deleted!"
	dd if=/dev/zero of=fat_disk.img bs=1M count=10
	mkfs.fat -F 32 fat_disk.img

.PHONY: all run clean clean-drive
