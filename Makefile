# ========================================================================= #
#            Makefile for LxcidOS (with Persistent Disk)                  #
# ========================================================================= #

# --- Toolchain and Commands ---
CC = i686-elf-gcc
AS = nasm
LD = i686-elf-ld
OBJCOPY = i686-elf-objcopy
RM = rm -rf

# --- Directories ---
BUILDDIR = build
USERDIR = user
KERNEL_SRC_DIRS = src drivers idt io lib shell memory fs

# --- Kernel Source Files ---
KERNEL_C_SOURCES = $(foreach dir,$(KERNEL_SRC_DIRS),$(shell find $(dir) -name '*.c'))
KERNEL_ASM_SOURCES = $(foreach dir,$(KERNEL_SRC_DIRS),$(shell find $(dir) -name '*.asm'))

# --- User Program Source Files ---
USER_C_SOURCES = $(shell find $(USERDIR) -maxdepth 2 -name '*.c' ! -path '*/lib/*')
USER_ASM_SOURCES = $(shell find $(USERDIR) -maxdepth 2 -name '*.asm')
USER_LIBS = $(shell find $(USERDIR)/lib -name '*.c')

# --- Generate Object and Target File Lists ---
KERNEL_OBJS = $(patsubst %.c,$(BUILDDIR)/%.o,$(KERNEL_C_SOURCES)) \
              $(patsubst %.asm,$(BUILDDIR)/%.o,$(KERNEL_ASM_SOURCES))

USER_BINS_C = $(patsubst $(USERDIR)/%.c,$(BUILDDIR)/$(USERDIR)/%.bin,$(USER_C_SOURCES))
USER_BINS_ASM = $(patsubst $(USERDIR)/%.asm,$(BUILDDIR)/$(USERDIR)/%.bin,$(USER_ASM_SOURCES))
USER_BINS = $(USER_BINS_C) $(USER_BINS_ASM)

# --- Flags and Configuration ---
CFLAGS = -ffreestanding -fno-pie -nostdlib -Wall -Wextra -I$(USERDIR)/lib $(foreach dir,$(KERNEL_SRC_DIRS),-I$(dir))
ASFLAGS = -f elf32
LDFLAGS_KERNEL = -T linker.ld -ffreestanding -nostdlib -lgcc
LDFLAGS_USER = -T user.ld

# ========================================================================= #
#                           BUILD RULES                                     #
# ========================================================================= #

.PHONY: all kernel populate-disk run clean clean-disk

# Default target: build the kernel and all user programs.
all: kernel $(USER_BINS)

# Rule to build the kernel binary.
kernel: $(BUILDDIR)/kernel.bin

$(BUILDDIR)/kernel.bin: $(KERNEL_OBJS) linker.ld
	@mkdir -p $(@D)
	@echo "LD (kernel) -> $@"
	$(CC) $(LDFLAGS_KERNEL) -o $@ $(KERNEL_OBJS)
	@echo "✅ Kernel linked successfully."

# --- Generic Compilation Rules (Kernel) ---
# This is the single, correct rule for compiling any kernel C file to an object file.
$(BUILDDIR)/%.o: %.c
	@mkdir -p $(@D)
	@echo "CC $<"
	$(CC) $(CFLAGS) -c $< -o $@

# This is the single, correct rule for assembling any kernel ASM file to an object file.
$(BUILDDIR)/%.o: %.asm
	@mkdir -p $(@D)
	@echo "AS $<"
	$(AS) $(ASFLAGS) $< -o $@

# --- Universal Build Rules (User Programs) ---
$(BUILDDIR)/$(USERDIR)/%.bin: $(USERDIR)/%.c $(USER_LIBS) user.ld
	@mkdir -p $(@D)
	@echo "--- Building C Program: $(notdir $@) ---"
	$(CC) $(CFLAGS) -c $< -o $(BUILDDIR)/user_main.o
	$(CC) $(CFLAGS) -c $(USER_LIBS) -o $(BUILDDIR)/user_lib.o
	$(LD) $(LDFLAGS_USER) -o $(@:.bin=.elf) $(BUILDDIR)/user_main.o $(BUILDDIR)/user_lib.o
	$(OBJCOPY) -O binary $(@:.bin=.elf) $@

$(BUILDDIR)/$(USERDIR)/%.bin: $(USERDIR)/%.asm
	@mkdir -p $(@D)
	@echo "--- Building ASM Program (direct): $(notdir $@) ---"
	$(AS) -f bin $< -o $@
# ========================================================================= #
#                           UTILITY RULES                                   #
# ========================================================================= #

# NEW: Explicitly copy all compiled binaries to the disk.
populate-disk: all
	@echo "Populating disk image via mount..."
	# 1. Create a temporary mount point
	@mkdir -p ./mnt
	# 2. Mount the disk image partition using a dynamically calculated offset
	@sudo mount fat_disk.img ./mnt
	# 3. Copy all user binaries
	@echo "Copying $(words $(USER_BINS)) file(s)..."
	@sudo cp $(USER_BINS) ./mnt/
	# 4. Unmount the disk image (CRITICAL STEP)
	@sudo umount ./mnt
	# 5. Clean up the temporary directory
	@rmdir ./mnt
	@echo "✅ Disk image populated."

# UPDATED: The run command no longer rebuilds the disk.
run: kernel
	@echo "Starting QEMU..."
	qemu-system-i386 -kernel $(BUILDDIR)/kernel.bin -hda fat_disk.img

clean:
	@echo "Cleaning build directory..."
	$(RM) $(BUILDDIR)

clean-disk:
	dd if=/dev/zero of=fat_disk.img bs=1M count=10
	mkfs.fat -F 32 fat_disk.img
	@echo "✅ New disk image created and formatted."
