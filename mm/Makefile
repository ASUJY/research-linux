OBJS := memory.o
MM_BUILD_DIR := $(BUILD)/mm

.c.s:
	@$(CC) $(CFLAGS) -S -o $*.s $<
.s.o:
	@$(AS) -o $*.o $<
.c.o:
	@$(CC) $(CFLAGS) $(DEBUG) -c -o $*.o $<

$(MM_BUILD_DIR)/mm.o: dep CREATE_MM_BUILD_DIR $(OBJS)
	$(LD) $(LDFLAGS) -r -o $(MM_BUILD_DIR)/mm.o $(OBJS)

CREATE_MM_BUILD_DIR:
	mkdir -p ${MM_BUILD_DIR}

clean:
	@rm -f core *.o *.a *.d
	@for i in *.c;do rm -f `basename $$i .c`.s;done

dep:
	@(for i in *.c;do echo -n `echo $$i | sed 's,\.c,\.s,'`" "; \
		$(CPP) -M $$i;done) >> mm.d

-include mm.d
