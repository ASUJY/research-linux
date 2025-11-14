OBJS := printk.o vsprintf.o sched.o system_call.o traps.o asm.o exit.o panic.o fork.o signal.o sys.o mktime.o
KERNEL_BUILD_DIR := $(BUILD)/kernel
CFLAGS := $(CFLAGS) -I../include/ -I../include/linux
.c.s:
	@$(CC) $(CFLAGS) $(DEBUG) -S -o $*.s $<
.s.o:
	@$(AS) $(ASFLAGS) -o $*.o $<
.c.o:
	$(CC) $(CFLAGS) $(DEBUG) -c -o $*.o $<

$(KERNEL_BUILD_DIR)/kernel.o: dep CREATE_KERNEL_BUILD_DIR $(OBJS)
	$(LD) $(LDFLAGS) -r -o $(KERNEL_BUILD_DIR)/kernel.o $(OBJS)
	@sync

CREATE_KERNEL_BUILD_DIR:
	@mkdir -p ${KERNEL_BUILD_DIR}

clean:
	@rm -f core *.o *.a *.d
	@for i in *.c;do rm -f `basename $$i .c`.s;done

dep:
	@(for i in *.c;do echo -n `echo $$i | sed 's,\.c,\.s,'`" "; \
		$(CPP) $(CFLAGS) -M $$i;done) >> kernel.d

-include kernel.d