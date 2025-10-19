# ========================================================================= #
#         Makefile for LxcidOS (with Persistent Disk)                     #
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
USER_ASM_SOURCES = $(shell find $(USERDIR) -maxdepth 2 -name '*.asm' ! -name 'runner.asm')
USER_LIBS_C = $(shell find $(USERDIR)/lib -name '*.c')
RUNNER_ASM = $(USERDIR)/runner.asm

# --- Generate Object and Target File Lists ---
KERNEL_OBJS = $(patsubst %.c,$(BUILDDIR)/%.o,$(KERNEL_C_SOURCES)) \
              $(patsubst %.asm,$(BUILDDIR)/%.o,$(KERNEL_ASM_SOURCES))

RUNNER_OBJ = $(patsubst %.asm,$(BUILDDIR)/%.o,$(RUNNER_ASM))
USER_LIB_O_FILES = $(patsubst %.c,$(BUILDDIR)/%.o,$(USER_LIBS_C))

# --- FIX: Change targets from .bin to .elf for C programs ---
USER_ELFS_C = $(patsubst $(USERDIR)/%.c,$(BUILDDIR)/$(USERDIR)/%.elf,$(USER_C_SOURCES))
USER_BINS_ASM = $(patsubst $(USERDIR)/%.asm,$(BUILDDIR)/$(USERDIR)/%.bin,$(USER_ASM_SOURCES))
# --- FIX: Update the list of all user program targets ---
USER_PROGRAMS = $(USER_ELFS_C) $(USER_BINS_ASM)

# --- Flags and Configuration ---
CFLAGS = -ffreestanding -fno-pie -nostdlib -Wall -Wextra -I.
ASFLAGS = -f elf32
LDFLAGS_KERNEL = -T linker.ld -ffreestanding -nostdlib -lgcc
LDFLAGS_USER = -T user.ld

# ========================================================================= #
#                               BUILD RULES                                 #
# ========================================================================= #

.PHONY: all kernel userapps populate-disk run clean clean-disk

all: kernel userapps

kernel: $(BUILDDIR)/kernel.bin

userapps: $(USER_PROGRAMS)

$(BUILDDIR)/kernel.bin: $(KERNEL_OBJS) linker.ld
	@mkdir -p $(@D)
	@echo "LD (kernel) -> $@"
	$(CC) $(LDFLAGS_KERNEL) -o $@ $(KERNEL_OBJS)
	@echo "✅ Kernel linked successfully."

# --- Generic Compilation/Assembly Rules ---
$(BUILDDIR)/%.o: %.c
	@mkdir -p $(@D)
	@echo "CC $<"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/%.o: %.asm
	@mkdir -p $(@D)
	@echo "AS $<"
	$(AS) $(ASFLAGS) $< -o $@

# --- FIX: REVISED Build Rule for User C Programs ---
# This rule now produces a final .elf file and stops. It no longer uses objcopy.
$(BUILDDIR)/$(USERDIR)/%.elf: $(USERDIR)/%.c $(RUNNER_OBJ) $(USER_LIB_O_FILES) user.ld
	@mkdir -p $(@D)
	@echo "--- Building C Program (ELF): $(notdir $@) ---"
	$(CC) $(CFLAGS) -c $< -o $(BUILDDIR)/$(USERDIR)/$(*F).o
	$(LD) $(LDFLAGS_USER) -o $@ $(BUILDDIR)/$(USERDIR)/$(*F).o $(RUNNER_OBJ) $(USER_LIB_O_FILES)

# --- Build Rule for Standalone User ASM Programs (remains the same) ---
$(BUILDDIR)/$(USERDIR)/%.bin: $(USERDIR)/%.asm
	@mkdir -p $(@D)
	@echo "--- Building ASM Program (direct): $(notdir $@) ---"
	$(AS) -f bin $< -o $@

# ========================================================================= #
#                               UTILITY RULES                               #
# ========================================================================= #

populate-disk: all
	@echo "Populating disk image via mount..."
	@mkdir -p ./mnt
	@sudo mount fat_disk.img ./mnt
	# --- FIX: Copy the new .elf targets ---
	@echo "Copying $(words $(USER_PROGRAMS)) file(s)..."
	@sudo cp $(USER_PROGRAMS) ./mnt/
	@sudo umount ./mnt
	@rmdir ./mnt
	@echo "✅ Disk image populated."

run: kernel
	@echo "Starting QEMU..."
	qemu-system-i386 -kernel $(BUILDDIR)/kernel.bin -hda fat_disk.img

clean:
	@echo "Cleaning build directory..."
	$(RM) $(BUILDDIR)

clean-disk:
	@dd if=/dev/zero of=fat_disk.img bs=1M count=10
	@mkfs.fat -F 32 fat_disk.img
	@echo "✅ New disk image created and formatted."

