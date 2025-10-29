//
// Created by asujy on 2025/6/13.
//

#ifndef RESEARCH_LINUX_IO_H
#define RESEARCH_LINUX_IO_H

#define outb(value,port) \
__asm__ ("outb %%al,%%dx"::"a" (value),"d" (port))

#define inb(port) ({ \
unsigned char _v; \
__asm__ volatile ("inb %%dx,%%al":"=a" (_v):"d" (port)); \
_v; \
})

#define outb_p(value, port) \
__asm__ ("outb %%al, %%dx\n"\
         "\tjmp 1f\n"       \
         "1:\tjmp 1f\n"     \
         "1:"::"a" (value), "d" (port))

#define inb_p(port) ({ \
unsigned char _v; \
__asm__ volatile ("inb %%dx,%%al\n" \
	"\tjmp 1f\n" \
	"1:\tjmp 1f\n" \
	"1:":"=a" (_v):"d" (port)); \
_v; \
})

#endif //RESEARCH_LINUX_IO_H
