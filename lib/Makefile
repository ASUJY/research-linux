OBJS := ctype.o
LIB_BUILD_DIR := $(BUILD)/lib
CFLAGS := $(CFLAGS) -I../include/

.c.s:
	@$(CC) $(CFLAGS) $(DEBUG) -S -o $*.s $<
.s.o:
	@$(AS) -o $*.o $<
.c.o:
	@$(CC) $(CFLAGS) $(DEBUG) -c -o $*.o $<

$(LIB_BUILD_DIR)/lib.a: dep CREATE_LIB_BUILD_DIR $(OBJS)
	@$(AR) rcs ${LIB_BUILD_DIR}/lib.a $(OBJS)
	@sync

CREATE_LIB_BUILD_DIR:
	mkdir -p ${LIB_BUILD_DIR}

clean:
	@rm -f core *.o *.a *.d
	@for i in *.c;do rm -f `basename $$i .c`.s;done

dep:
	@(for i in *.c;do echo -n `echo $$i | sed 's,\.c,\.s,'`" "; \
		$(CPP) $(CFLAGS) -M $$i;done) >> lib.d

-include lib.d
