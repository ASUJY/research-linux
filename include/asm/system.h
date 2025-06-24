//
// Created by asujy on 2025/6/13.
//

#ifndef RESEARCH_LINUX_SYSTEM_H
#define RESEARCH_LINUX_SYSTEM_H

#define sti() __asm__ ("sti"::)
#define cli() __asm__ ("cli"::)

// 设置描述符
#define _set_gate(gate_addr,type,dpl,addr) \
__asm__ ("movw %%dx,%%ax\n\t" \
"movw %0,%%dx\n\t" \
"movl %%eax,%1\n\t" \
"movl %%edx,%2" \
: \
: "i" ((short) (0x8000+(dpl<<13)+(type<<8))), \
"o" (*((char *) (gate_addr))), \
"o" (*(4+(char *) (gate_addr))), \
"d" ((char *) (addr)),"a" (0x00080000))

// 设置中断门描述符
#define set_intr_gate(n,addr) \
	_set_gate(&_idt[n],14,0,addr)

// 设置陷阱门描述符
#define set_trap_gate(n,addr) \
	_set_gate(&_idt[n],15,0,addr)

#endif //RESEARCH_LINUX_SYSTEM_H
