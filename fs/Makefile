OBJS := buffer.o super.o file_table.o inode.o truncate.o bitmap.o open.o namei.o fcntl.o read_write.o char_dev.o
FS_BUILD_DIR := $(BUILD)/fs
CFLAGS := $(CFLAGS) -I../include/ -I../include/linux
CC	= gcc
.c.s:
	$(CC) $(CFLAGS) $(DEBUG) -S -o $*.s $<
.s.o:
	$(AS) $(ASFLAGS) -o $*.o $<
.c.o:
	$(CC) $(CFLAGS) $(DEBUG) -c -o $*.o $<

$(FS_BUILD_DIR)/fs.o: dep CREATE_FS_BUILD_DIR $(OBJS)
	$(LD) $(LDFLAGS) -r -o $(FS_BUILD_DIR)/fs.o $(OBJS)

CREATE_FS_BUILD_DIR:
	mkdir -p ${FS_BUILD_DIR}

clean:
	@rm -f core *.o *.a *.d
	@for i in *.c;do rm -f `basename $$i .c`.s;done

dep:
	@(for i in *.c;do echo -n `echo $$i | sed 's,\.c,\.s,'`" "; \
		$(CPP) -M $(CFLAGS) $$i;done) >> fs.d

-include fs.d
