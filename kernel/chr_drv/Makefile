OBJS := console.o tty_io.o
CHR_DRV_BUILD_DIR := $(BUILD)/kernel/chr_drv
CFLAGS := $(CFLAGS) -I../../include/

.c.s:
	@$(CC) $(CFLAGS) $(DEBUG) -S -o $*.s $<
.s.o:
	@$(AS) -o $*.o $<
.c.o:
	@$(CC) $(CFLAGS) $(DEBUG) -c -o $*.o $<

$(CHR_DRV_BUILD_DIR)/chr_drv.a: dep CREATE_CHR_DRV_BUILD_DIR $(OBJS)
	@$(AR) rcs ${CHR_DRV_BUILD_DIR}/chr_drv.a $(OBJS)
	@sync

CREATE_CHR_DRV_BUILD_DIR:
	@mkdir -p ${CHR_DRV_BUILD_DIR}

clean:
	@rm -f core *.o *.a *.d
	@for i in *.c;do rm -f `basename $$i .c`.s;done

dep:
	@(for i in *.c;do echo -n `echo $$i | sed 's,\.c,\.s,'`" "; \
		$(CPP) $(CFLAGS) -M $$i;done) >> chr_drv.d

-include chr_drv.d