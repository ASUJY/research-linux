OBJS := ramdisk.o
BLK_DRV_BUILD_DIR := $(BUILD)/kernel/blk_drv
CFLAGS := $(CFLAGS) -I./

.c.s:
	@$(CC) $(CFLAGS) $(DEBUG) -S -o $*.s $<
.s.o:
	$(AS) $(ASFLAGS) -o $*.o $<
.c.o:
	@$(CC) $(CFLAGS) $(DEBUG) -c -o $*.o $<

$(BLK_DRV_BUILD_DIR)/blk_drv.a: dep CREATE_BLK_DRV_BUILD_DIR $(OBJS)
	@$(AR) rcs ${BLK_DRV_BUILD_DIR}/blk_drv.a $(OBJS)
	@sync

CREATE_BLK_DRV_BUILD_DIR:
	@mkdir -p ${BLK_DRV_BUILD_DIR}

clean:
	@rm -f core *.o *.a *.d
	@for i in *.c;do rm -f `basename $$i .c`.s;done

dep:
	@(for i in *.c;do echo -n `echo $$i | sed 's,\.c,\.s,'`" "; \
		$(CPP) $(CFLAGS) -M $$i;done) >> blk_drv.d

-include chr_drv.d