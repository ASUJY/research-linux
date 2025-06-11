include config.mk

BUILD:=$(CURDIR)/build
export BUILD
HD_IMG_NAME:="hd.img"

all: Boot ${BUILD}/system.bin
	@bximage -q -hd=16 -func=create -sectsize=512 -imgmode=flat $(BUILD)/$(HD_IMG_NAME)
	@dd if=${BUILD}/boot/bootsect.o of=$(BUILD)/$(HD_IMG_NAME) bs=512 seek=0 count=1 conv=notrunc
	@dd if=${BUILD}/boot/setup.o of=$(BUILD)/$(HD_IMG_NAME) bs=512 seek=1 count=4 conv=notrunc
	@dd if=${BUILD}/system.bin of=$(BUILD)/$(HD_IMG_NAME) bs=512 seek=5 count=240 conv=notrunc

${BUILD}/system.bin: ${BUILD}/kernel.bin
	@objcopy -O binary ${BUILD}/kernel.bin ${BUILD}/system.bin
	@nm ${BUILD}/kernel.bin | sort > ${BUILD}/system.map

${BUILD}/kernel.bin: ${BUILD}/boot/head.o $(BUILD)/init/main.o
	@ld -m elf_i386 $^ -o $@ -Ttext 0x00000000

$(BUILD)/init/main.o: init/main.c
	@$(MAKE) -C init

Boot:
	@$(MAKE) -C boot

clean:
	@rm -rf ${BUILD} bx_enh_dbg.ini
	@$(MAKE) -C boot clean
	@$(MAKE) -C init clean


bochs:
	bochs -q -f bochsrc