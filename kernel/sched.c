//
// Created by asujy on 2025/6/13.
//

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/sys.h>
#include <linux/fdreg.h>
#include <linux/head.h>

#include <asm/system.h>
#include <asm/io.h>

#define _S(nr) (1<<((nr)-1))
#define _BLOCKABLE (~(_S(SIGKILL) | _S(SIGSTOP)))

void show_task(int nr,struct task_struct * p)
{
    int i,j = 4096-sizeof(struct task_struct);

    printk("%d: pid=%d, state=%d, ",nr,p->pid,p->state);
    i=0;
    while (i<j && !((char *)(p+1))[i]) {
        i++;
    }
    printk("%d (of %d) chars free in kernel stack\n\r",i,j);
}

void show_stat(void)
{
    int i;

    for (i = 0; i < NR_TASKS; i++) {
        if (task[i]) {
            show_task(i, task[i]);
        }
    }
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
long startup_time=0;
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
 *  'math_state_restore()' saves the current math information in the
 * old math state array, and gets the new ones from the current task
 */
void math_state_restore()
{
    if (last_task_used_math == current) {
        return;
    }
    __asm__("fwait");
    if (last_task_used_math) {
        __asm__("fnsave %0"::"m" (last_task_used_math->tss.i387));
    }
    last_task_used_math=current;
    if (current->used_math) {
        __asm__("frstor %0"::"m" (current->tss.i387));
    } else {
        __asm__("fninit"::);
        current->used_math=1;
    }
}

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

    for(p = &LAST_TASK ; p > &FIRST_TASK ; --p) {
        if (*p) {
            if ((*p)->alarm && (*p)->alarm < jiffies) {
                (*p)->signal |= (1<<(SIGALRM-1));
                (*p)->alarm = 0;
            }
            if (((*p)->signal & ~(_BLOCKABLE & (*p)->blocked)) &&
            (*p)->state==TASK_INTERRUPTIBLE) {
                (*p)->state=TASK_RUNNING;
            }
        }
    }

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

int sys_pause(void)
{
    current->state = TASK_INTERRUPTIBLE;
    schedule();
    return 0;
}

/*
 * 实现进程睡眠等待，将当前进程设置为不可中断的等待状态，并让睡眠队列头 的指针 指向当前任务。
 * 这个函数提供了进程与中断处理程序之间的同步机制。
 * p: 指向等待队列头部的指针，用于管理等待特定资源的进程链表
 */
void sleep_on(struct task_struct **p) {
    struct task_struct *tmp;    // 用于保存前一个等待进程，实现链式唤醒

    if (!p) {
        return;
    }
    // 禁止进程0（idle进程）睡眠，否则触发内核恐慌（panic），因为idle进程是系统唯一不可休眠的进程。
    if (current == &(init_task.task)) {
        panic("task[0] trying to sleep");
    }

    tmp = *p;       // tmp保存当前等待队列头部（用于后续唤醒）
    *p = current;   // 将当前进程（current）插入队列头部
    current->state = TASK_UNINTERRUPTIBLE;  // 将当前进程设置为不可中断的等待状态
    schedule();     // 调用调度器切换进程，当前进程进入睡眠，直到被唤醒后重新运行

    if (tmp) {
        tmp->state=0;   // 当前进程被唤醒后，将之前保存的前一个队列头部进程（tmp）状态设为0（即TASK_RUNNING），尝试唤醒它。
    }
}

void interruptible_sleep_on(struct task_struct **p)
{
    struct task_struct *tmp;

    if (!p) {
        return;
    }
    if (current == &(init_task.task)) {
        panic("task[0] trying to sleep");
    }
    tmp = *p;
    *p = current;
repeat:
    current->state = TASK_INTERRUPTIBLE;
    schedule();
    if (*p && *p != current) {
        (**p).state = 0;
        goto repeat;
    }
    *p = NULL;
    if (tmp) {
        tmp->state = 0;
    }
}

/*
 * 唤醒等待队列上的睡眠进程
 * 实现了简单的进程状态转换和队列管理
 */
void wake_up(struct task_struct **p)
{
    if (p && *p) {
        (**p).state=0;    // 将进程状态设置为0，即TASK_RUNNING，让进程p可以被调度器选择运行
        *p=NULL;          // 将等待队列头设置为NULL，表示队列已空
    }
}

static struct task_struct * wait_motor[4] = {NULL,NULL,NULL,NULL};
static int  mon_timer[4]={0,0,0,0};
static int moff_timer[4]={0,0,0,0};
unsigned char current_DOR = 0x0C;

int ticks_to_floppy_on(unsigned int nr)
{
    extern unsigned char selected;
    unsigned char mask = 0x10 << nr;

    if (nr > 3) {
        panic("floppy_on: nr>3");
    }
    moff_timer[nr] = 10000;		/* 100 s = very big :-) */
    cli();				/* use floppy_off to turn it off */
    mask |= current_DOR;
    if (!selected) {
        mask &= 0xFC;
        mask |= nr;
    }
    if (mask != current_DOR) {
        outb(mask, FD_DOR);
        if ((mask ^ current_DOR) & 0xf0) {
            mon_timer[nr] = HZ/2;
        } else if (mon_timer[nr] < 2) {
            mon_timer[nr] = 2;
        }
        current_DOR = mask;
    }
    sti();
    return mon_timer[nr];
}

void floppy_on(unsigned int nr)
{
    cli();
    while (ticks_to_floppy_on(nr)) {
        sleep_on(nr+wait_motor);
    }
    sti();
}

void floppy_off(unsigned int nr)
{
    moff_timer[nr]=3*HZ;
}

void do_floppy_timer(void)
{
    int i;
    unsigned char mask = 0x10;

    for (i=0 ; i<4 ; i++,mask <<= 1) {
        if (!(mask & current_DOR))
            continue;
        if (mon_timer[i]) {
            if (!--mon_timer[i])
                wake_up(i+wait_motor);
        } else if (!moff_timer[i]) {
            current_DOR &= ~mask;
            outb(current_DOR,FD_DOR);
        } else
            moff_timer[i]--;
    }
}

#define TIME_REQUESTS 64

static struct timer_list {
    long jiffies;
    void (*fn)();
    struct timer_list * next;
} timer_list[TIME_REQUESTS], * next_timer = NULL;

void add_timer(long jiffies, void (*fn)(void))
{
    struct timer_list * p;

    if (!fn) {
        return;
    }
    cli();
    if (jiffies <= 0) {
        (fn)();
    } else {
        for (p = timer_list; p < timer_list + TIME_REQUESTS; p++) {
            if (!p->fn) {
                break;
            }
        }
        if (p >= timer_list + TIME_REQUESTS) {
            panic("No more time requests free");
        }
        p->fn = fn;
        p->jiffies = jiffies;
        p->next = next_timer;
        next_timer = p;
        while (p->next && p->next->jiffies < p->jiffies) {
            p->jiffies -= p->next->jiffies;
            fn = p->fn;
            p->fn = p->next->fn;
            p->next->fn = fn;
            jiffies = p->jiffies;
            p->jiffies = p->next->jiffies;
            p->next->jiffies = jiffies;
            p = p->next;
        }
    }
    sti();
}

void do_timer(long cpl){
    extern int beepcount;
    extern void sysbeepstop(void);

    if (beepcount) {
        if (!--beepcount) {
            sysbeepstop();
        }
    }

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

    if (current_DOR & 0xf0) {
        do_floppy_timer();
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

int sys_alarm(long seconds) {
    int old = current->alarm;

    if (old) {
        old = (old - jiffies) / HZ;
    }
    current->alarm = (seconds > 0) ? (jiffies + HZ * seconds) : 0;
    return (old);
}

int sys_getpid(void) {
    return current->pid;
}

int sys_getppid(void) {
    return current->father;
}

int sys_getuid(void) {
    return current->uid;
}

int sys_geteuid(void) {
    return current->euid;
}

int sys_getgid(void) {
    return current->gid;
}

int sys_getegid(void) {
    return current->egid;
}

int sys_nice(long increment) {
    if (current->priority - increment > 0) {
        current->priority -= increment;
    }
    return 0;
}

void sched_init(void) {
    int i;
    struct desc_struct *p;

    if (sizeof(struct sigaction) != 16) {
        panic("Struct sigaction MUST be 16 bytes");
    }

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