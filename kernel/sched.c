//
// Created by asujy on 2025/6/13.
//

#include <linux/sched.h>
#include <linux/head.h>
#include <asm/system.h>
#include <asm/io.h>

void show_stat(void)
{
    int i;


}

// 定义每个时间片的滴答数
#define LATCH (1193180/HZ)

extern int timer_interrupt(void);
extern int system_call(void);

union task_union {
    struct task_struct task;
    char stack[PAGE_SIZE];
};

// 任务0
static union task_union init_task = {INIT_TASK,};

// 任务列表(进程列表，用来管理进程)
struct task_struct * task[NR_TASKS] = {&(init_task.task), };

long user_stack [ PAGE_SIZE>>2 ] ;  // 定义栈数组，4096字节大小

struct {
    long * a;   // 指向栈顶的指针，即esp
    short b;    // 段选择子
} stack_start = { & user_stack [PAGE_SIZE>>2] , 0x10 };

void sched_init(void) {
    int i;
    struct desc_struct *p;

    // 设置初始任务(任务0)的任务状态段描述符(TSS)和局部描述符表(LDT)，在GDT表中安装这两个描述符
    set_tss_desc(_gdt + FIRST_TSS_ENTRY, &(init_task.task.tss));
    set_ldt_desc(_gdt + FIRST_LDT_ENTRY, &(init_task.task.ldt));

    // 给GDT表中剩下的描述符初始化为0，任务列表初始化为NULL
    p = _gdt + 2 + FIRST_TSS_ENTRY;
    for (i = 1; i < NR_TASKS; i++) {
        task[i] = NULL;
        p->a = p->b = 0;
        p++;
        p->a = p->b = 0;
        p++;
    }

    __asm__("pushfl ; andl $0xffffbfff,(%esp) ; popfl");
    ltr(0);     // 加载TSS描述符到TR寄存器中
    lldt(0);    // 加载LDT表

    // 开启了定时器，之后这个定时器会持续的、以一定频率向 CPU 发出中断信号。
    outb_p(0x36,0x43);		        /* binary, mode 3, LSB/MSB, ch 0 */
    outb_p(LATCH & 0xff , 0x40);	/* LSB */
    outb(LATCH >> 8 , 0x40);	    /* MSB */

    set_intr_gate(0x20, &timer_interrupt);  // 设置中断向量0x20的中断描述符(时钟中断，每次定时器向 CPU 发出中断后，便会执行这个函数)
    outb(inb_p(0x21)&~0x01, 0x21);          // 允许时钟中断信号通过8259a芯片送往CPU
    //set_system_gate(0x80,&system_call);   // 设置中断向量0x80的中断描述符(系统调用system_call，用户态程序想要调用内核提供的方法，都需要基于这个系统调用来进行)
}