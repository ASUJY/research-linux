//
// Created by asujy on 2025/6/13.
//

#include <linux/sched.h>
#include <linux/head.h>
#include <asm/system.h>


void show_stat(void)
{
    int i;


}

extern int timer_interrupt(void);

long user_stack [ PAGE_SIZE>>2 ] ;  // 定义栈数组，4096字节大小

struct {
    long * a;   // 指向栈顶的指针，即esp
    short b;    // 段选择子
} stack_start = { & user_stack [PAGE_SIZE>>2] , 0x10 };

void sched_init(void) {
    set_intr_gate(0x20, &timer_interrupt);  // 设置中断向量0x20的中断描述符(时钟中断)
    //outb(inb_p(0x21)&~0x01, 0x21);          // 允许时钟中断信号通过8259a芯片送往CPU
}