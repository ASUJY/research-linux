all: BOOT_BUILD_DIR $(BUILD)/boot/bootsect.o $(BUILD)/boot/setup.o $(BUILD)/boot/head.o

$(BUILD)/boot/head.o: head.asm
	@nasm -f elf32 -g $< -o $@

# 把汇编文件编译成目标文件
$(BUILD)/boot/%.o: ./%.asm
	@nasm -g $< -o $@

BOOT_BUILD_DIR:
	@mkdir -p ${BUILD}/boot/

clean:
