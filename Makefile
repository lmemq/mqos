CXX = g++
AS = nasm        # Добавили ассемблер
LD = ld
IMAGE = mqos.img
OVMF = /usr/share/ovmf/OVMF.fd

# Флаги для ассемблера (64-битный эльф)
ASFLAGS = -f elf64

CXXFLAGS = -Wall -Wextra -ffreestanding -fno-stack-protector -fno-stack-check \
           -fno-lto -fno-PIE -m64 -march=x86-64 -mno-red-zone \
           -fno-exceptions -fno-rtti \
           -mcmodel=kernel

LDFLAGS = -T linker.ld -nostdlib -z max-page-size=0x1000 -static

# Добавляем interrupts.o в список объектов
OBJS = kernel.o interrupts.o font.o wallpaper.o

all: $(IMAGE)

# Правило для компиляции C++
kernel.o: kernel.cpp
	$(CXX) $(CXXFLAGS) -c kernel.cpp -o kernel.o

# НОВОЕ: Правило для компиляции Ассемблера
interrupts.o: interrupts.asm
	$(AS) $(ASFLAGS) interrupts.asm -o interrupts.o

kernel.elf: kernel.o interrupts.o
	# Превращаем бинарники в объектные файлы
	objcopy -I binary -O elf64-x86-64 -B i386 font.psf font.o
	objcopy -I binary -O elf64-x86-64 -B i386 wallpaper.bin wallpaper.o
	# Линкуем всё вместе
	$(LD) $(LDFLAGS) $(OBJS) -o kernel.elf

ISO_IMAGE = disk.iso

$(IMAGE): kernel.elf BOOTX64.EFI
	# ... твой существующий код создания disk.img ...
	mkdir -p sys
	cp kernel.elf sys/core
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
	# echo "screen=1024x768" > CONFIG
	echo "screen=1024x768" > CONFIG
	mcopy -i $(IMAGE)@@1048576 CONFIG ::/BOOTBOOT/CONFIG
	rm CONFIG INITRD

# НОВАЯ ЧАСТЬ: Создание ISO
iso: $(IMAGE)
	# 1. Извлекаем FAT32 раздел
	dd if=$(IMAGE) of=efiboot.img bs=1M skip=1
	
	# 2. Готовим структуру ISO
	mkdir -p iso_root/BOOTBOOT
	mkdir -p iso_root/EFI/BOOT
	cp BOOTX64.EFI iso_root/EFI/BOOT/BOOTX64.EFI
	
	mkdir -p sys && cp kernel.elf sys/core
	find sys | cpio -H newc -o > iso_root/BOOTBOOT/INITRD
	rm -rf sys
	echo "screen=1024x768" > iso_root/BOOTBOOT/CONFIG
	
	# ПЕРЕНОСИМ образ внутрь, чтобы xorriso его увидел
	mv efiboot.img iso_root/efiboot.img
	
	# 3. Сборка (путь -e теперь относительно корня ISO)
	xorriso -as mkisofs -R -f -e efiboot.img -no-emul-boot \
		-o mqos.iso \
		iso_root
	
	# Чистка
	rm -rf iso_root



run: $(IMAGE)
	qemu-system-x86_64 -drive if=pflash,format=raw,readonly=on,file=$(OVMF) \
                   -drive file=$(IMAGE),format=raw \
                   -net none -serial stdio

clean:
	rm -f *.o kernel.elf $(IMAGE)

