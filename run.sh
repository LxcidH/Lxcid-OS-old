nasm main.s -f bin -o boot.bin
nasm kernel.s -f bin -o kernel.bin
cat boot.bin kernel.bin > os.bin
qemu-system-i386 -fda os.bin
