CXX = g++
AS = nasm
LD = ld
IMAGE = mqos.img
OVMF = /usr/share/ovmf/OVMF.fd

ASFLAGS = -f elf64

CXXFLAGS = -Wall -Wextra -ffreestanding -fno-stack-protector -fno-stack-check \
           -fno-lto -fno-PIE -m64 -march=x86-64 -mno-red-zone \
           -fno-exceptions -fno-rtti \
           -mcmodel=kernel

LDFLAGS = -T linker.ld -nostdlib -z max-page-size=0x1000 -static

OBJS = kernel.o interrupts.o font.o wallpaper.o screen.o

all: $(IMAGE)

screen.o: screen.cpp screen.hpp
	$(CXX) $(CXXFLAGS) -c screen.cpp -o screen.o

kernel.o: kernel.cpp
	$(CXX) $(CXXFLAGS) -c kernel.cpp -o kernel.o

interrupts.o: interrupts.asm
	$(AS) $(ASFLAGS) interrupts.asm -o interrupts.o

kernel.elf: kernel.o interrupts.o screen.o
	objcopy -I binary -O elf64-x86-64 -B i386 font.psf font.o
	objcopy -I binary -O elf64-x86-64 -B i386 wallpaper.bin wallpaper.o
	$(LD) $(LDFLAGS) $(OBJS) -o kernel.elf

$(IMAGE): kernel.elf BOOTX64.EFI
	mkdir -p sys
	cp kernel.elf sys/core
	# cp images/wallpaper.
	find sys | cpio -H newc -o > INITRD
	rm -rf sys
	dd if=/dev/zero of=$(IMAGE) bs=1M count=64
	parted $(IMAGE) -s mklabel gpt mkpart primary fat32 2048s 100% set 1 esp on
	mformat -i $(IMAGE)@@1048576 -F ::
	mmd -i $(IMAGE)@@1048576 ::/EFI
	mmd -i $(IMAGE)@@1048576 ::/EFI/BOOT
	mmd -i $(IMAGE)@@1048576 ::/BOOTBOOT
	mcopy -i $(IMAGE)@@1048576 BOOTX64.EFI ::/EFI/BOOT/BOOTX64.EFI
	mcopy -i $(IMAGE)@@1048576 INITRD ::/BOOTBOOT/INITRD
	echo "screen=1024x768" > CONFIG
	mcopy -i $(IMAGE)@@1048576 CONFIG ::/BOOTBOOT/CONFIG
	rm CONFIG INITRD

vdi: $(IMAGE)
	rm -f mqos.vdi
	VBoxManage convertfromraw $(IMAGE) mqos.vdi --format VDI


run: $(IMAGE)
	qemu-system-x86_64 -drive if=pflash,format=raw,readonly=on,file=$(OVMF) \
                   -drive file=$(IMAGE),format=raw \
                   -net none -serial stdio

clean:
	rm -f *.o kernel.elf $(IMAGE)

