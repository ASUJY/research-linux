OBJS := math_emulate.o
MATH_BUILD_DIR := $(BUILD)/kernel/math
CFLAGS := $(CFLAGS) -I./ -I../../include

.c.s:
	@$(CC) $(CFLAGS) $(DEBUG) -S -o $*.s $<
.s.o:
	$(AS) $(ASFLAGS) -o $*.o $<
.c.o:
	@$(CC) $(CFLAGS) $(DEBUG) -c -o $*.o $<

$(MATH_BUILD_DIR)/math.a: dep CREATE_MATH_BUILD_DIR $(OBJS)
	@$(AR) rcs ${MATH_BUILD_DIR}/math.a $(OBJS)
	@sync

CREATE_MATH_BUILD_DIR:
	@mkdir -p ${MATH_BUILD_DIR}

clean:
	@rm -f core *.o *.a *.d
	@for i in *.c;do rm -f `basename $$i .c`.s;done

dep:
	@(for i in *.c;do echo -n `echo $$i | sed 's,\.c,\.s,'`" "; \
		$(CPP) $(CFLAGS) -M $$i;done) >> blk_drv.d

-include chr_drv.d