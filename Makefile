BUILD:=./build
HD_IMG_NAME:="hd.img"

all: BOOT_BUILD ${BUILD}/boot/bootsect.o ${BUILD}/boot/setup.o ${BUILD}/system.bin
	bximage -q -hd=16 -func=create -sectsize=512 -imgmode=flat $(BUILD)/$(HD_IMG_NAME)
	dd if=${BUILD}/boot/bootsect.o of=$(BUILD)/$(HD_IMG_NAME) bs=512 seek=0 count=1 conv=notrunc
	dd if=${BUILD}/boot/setup.o of=$(BUILD)/$(HD_IMG_NAME) bs=512 seek=1 count=4 conv=notrunc
	dd if=${BUILD}/system.bin of=$(BUILD)/$(HD_IMG_NAME) bs=512 seek=5 count=240 conv=notrunc

${BUILD}/system.bin: ${BUILD}/kernel.bin
	objcopy -O binary ${BUILD}/kernel.bin ${BUILD}/system.bin
	nm ${BUILD}/kernel.bin | sort > ${BUILD}/system.map

${BUILD}/kernel.bin: ${BUILD}/boot/head.o
	ld -m elf_i386 $^ -o $@ -Ttext 0x00000000

# 把汇编文件编译成目标文件
${BUILD}/boot/%.o: boot/%.asm
	nasm $< -o $@

${BUILD}/boot/head.o: boot/head.asm
	nasm -f elf32 -g $< -o $@

BOOT_BUILD:
	mkdir -p ${BUILD}/boot

clean:
	rm -rf ${BUILD} bx_enh_dbg.ini

bochs:
	bochs -q -f bochsrc