//
// Created by asujy on 2025/6/24.
//
#include <linux/head.h>
#include <linux/kernel.h>
#include <asm/system.h>

// 从段选择子seg和偏移地址addr读取 1 个字节
#define get_seg_byte(seg, addr) ({ \
    register char __res; \
    __asm__("push %%fs; mov %%ax,%%fs; movb %%fs:%2,%%al; pop %%fs" \
    :"=a" (__res):"0" (seg), "m" (*(addr))); \
    __res;})

// 从段选择子seg和偏移地址addr读取 4 个字节
#define get_seg_long(seg, addr)({ \
    register unsigned long __res; \
    __asm__("push %%fs; mov %%ax,%%fs; movl %%fs:%2,%%eax; pop %%fs" \
    :"=a" (__res):"0" (seg), "m" (*(addr))); \
    __res;})

// 获取当前 FS 寄存器的值
#define _fs() ({\
    register unsigned short __res;\
    __asm__("mov %%fs, %%ax":"=a" (__res):);\
    __res;})

int do_exit(long code);

void divide_error(void);
void reserved(void);

static void die(char* str, long esp_ptr, long nr) {
    long* esp = (long*)esp_ptr;
    int i;

    printk("%s: %04x\n\r", str, nr & 0xffff);       // 打印错误字符串和低16位错误码
    printk("EIP:\t%04x:%p\nEFLAGS:\t%p\nESP:\t%04x:%p\n",
        esp[1], esp[0], esp[2], esp[4], esp[3]);        // 打印保存在栈中的寄存器EIP、EFLAGS、ESP
    printk("fs: %04x\n", _fs());                    // 打印 FS 寄存器值
    //printk("base: %p, limit: %p\n", get_base(current->ldt[1]), get_limit(0x17));
    if (esp[4] == 0x17) {
        printk("Stack: ");
        for (i = 0; i < 4; i++) {
            printk("%p ", get_seg_long(0x17, i + (long *)esp[3]));
        }
        printk("\n");
    }
    //str(i);
    //printk("Pid: %d, process nr: %d\n\r", current->pid, 0xffff & i);
    for(i = 0; i < 10; i++) {   // 打印当前指令机器码
        printk("%02x ", 0xff & get_seg_byte(esp[1], (i + (char *)esp[0])));
    }
    printk("\n\r");
    do_exit(11);
}

void do_divide_error(long esp, long error_code) {
    die("divide error", esp, error_code);
}

void do_reserved(long esp, long error_code) {
    die("reserved (15,17-47) error", esp, error_code);
}

void trap_init(void) {
    int i;

    set_trap_gate(0, &divide_error);
    for (i = 17; i < 48; i++) {
        set_trap_gate(i, &reserved);
    }
}