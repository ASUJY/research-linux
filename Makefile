include config.mk

#
# if you want the ram-disk device, define this to be the
# size in blocks.
#
RAMDISK = #-DRAMDISK=512

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

${BUILD}/kernel.bin: ${BUILD}/boot/head.o $(BUILD)/init/main.o $(BUILD)/kernel/kernel.o $(BUILD)/kernel/chr_drv/chr_drv.a\
	$(BUILD)/lib/lib.a $(BUILD)/kernel/blk_drv/blk_drv.a
	@ld $(LDFLAGS) --start-group $^ --end-group -o $@ -Ttext 0x00000000

$(BUILD)/init/main.o: init/main.c
	@$(MAKE) -C init

Boot:
	@$(MAKE) -C boot

$(BUILD)/kernel/kernel.o:
	@$(MAKE) -C kernel

$(BUILD)/kernel/chr_drv/chr_drv.a:
	@$(MAKE) -C kernel/chr_drv

$(BUILD)/kernel/blk_drv/blk_drv.a:
	@$(MAKE) -C kernel/blk_drv

$(BUILD)/lib/lib.a:
	@$(MAKE) -C lib


clean:
	@rm -rf ${BUILD} bx_enh_dbg.ini
	@$(MAKE) -C boot clean
	@$(MAKE) -C init clean
	@$(MAKE) -C kernel clean
	@$(MAKE) -C kernel/chr_drv clean
	@$(MAKE) -C kernel/blk_drv clean
	@$(MAKE) -C lib clean

bochs:
	bochs -q -f bochsrc

qemug: clean all
	qemu-system-i386 \
		-m 32M \
		-boot c \
		-hda ./build/hd.img \
		-s -S

qemu: clean all
	qemu-system-i386 \
	-m 32M \
	-boot c \
	-hda ./build/hd.img