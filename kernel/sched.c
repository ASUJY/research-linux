//
// Created by asujy on 2025/6/13.
//

#include <linux/sched.h>
#include <linux/sys.h>
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

long volatile jiffies=0;                            // (滴答数)每10毫秒加1
struct task_struct *current = &(init_task.task);    // 当前任务（初始化为0号任务）
struct task_struct *last_task_used_math = NULL;     // 使用过协处理器的任务

// 任务列表(进程列表，用来管理进程)
struct task_struct * task[NR_TASKS] = {&(init_task.task), };

long user_stack [ PAGE_SIZE>>2 ] ;  // 定义栈数组，4096字节大小

struct {
    long * a;   // 指向栈顶的指针，即esp
    short b;    // 段选择子
} stack_start = { & user_stack [PAGE_SIZE>>2] , 0x10 };

/*
 *  'schedule()' 是调度函数，它可以在所有的环境下工作（比如能够对IO-边界处理很好的响应等）
 *
 *  注意，任务0(0号进程)是个闲置(idle)任务，只有当没有其它任务可以运行时才调用它。它不能被杀死，
 *  也不能睡眠。任务0中的状态信息state是从来不用的！
 */
void schedule(void) {
    int i;
    int next;
    int c;
    struct task_struct **p;

    /* 以下是调度程序的主要部分 */
    while (1) {
        c = -1;
        next = 0;
        i = NR_TASKS;
        p = &task[NR_TASKS];

        /**
         * 从任务数组的最后一个任务开始循环处理，并跳过不含任务的数组槽。
         * 比较每个就绪状态(可以运行的)任务的counter值(任务运行的时间片)，哪个值大就运行哪个。
         */
        while (--i) {
            if (!*--p) {
                continue;
            }
            if ((*p)->state == TASK_RUNNING && (*p)->counter > c) {
                c = (*p)->counter;
                next = i;
            }
        }
        if (c) {
            break;  // 如果有counter值大于0的任务，则执行任务切换
        }

        /**
         * 根据每个任务的优先级，更新每一个任务的counter值(时间片)。
         * counter = counter / 2 + priority
         */
        for (p = &LAST_TASK; p > &FIRST_TASK; --p) {
            if (*p) {
                (*p)->counter = ((*p)->counter >> 1) + (*p)->priority;
            }
        }
    }

    switch_to(next);    // 切换到任务号为next的任务，并执行这个任务
}

#define TIME_REQUESTS 64

static struct timer_list {
    long jiffies;
    void (*fn)();
    struct timer_list * next;
} timer_list[TIME_REQUESTS], * next_timer = NULL;

void do_timer(long cpl){
    // 如果当前特权级(cpl)为0，则将当前进程的内核态运行时间stime加1，否则增加用户态运行时间utime
    if (cpl) {
        current->utime++;
    } else {
        current->stime++;
    }

    /**
    * 如果有用户的定时器存在，则将链表第1个定时器的值减1。如果已等于0，则调用相应的处理
    * 程序，并将该处理程序指针置为空。然后去掉该项定时器。
    * next_timer是定时器链表的头指针。
     */
    if (next_timer) {
        next_timer->jiffies--;
        while (next_timer && next_timer->jiffies <= 0) {
            void (*fn)(void);

            fn = next_timer->fn;
            next_timer->fn = NULL;
            next_timer = next_timer->next;
            (fn)();
        }
    }

    // 进程运行时间还没用完，则直接返回
    if ((--current->counter) > 0) {
        return;
    }
    current->counter = 0;
    if (!cpl) {
        return; // 对于内核态程序，不依赖counter值进行调度，直接返回。
    }

    schedule();
}

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
    set_system_gate(0x80, &system_call);   // 设置中断向量0x80的中断描述符(系统调用system_call，用户态程序想要调用内核提供的方法，都需要基于这个系统调用来进行)
}