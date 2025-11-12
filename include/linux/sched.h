//
// Created by asujy on 2025/6/13.
//

#ifndef RESEARCH_LINUX_SCHED_H
#define RESEARCH_LINUX_SCHED_H

#define NR_TASKS 64     // 系统中同时存在的最大进程数量
#define HZ 100          // 时钟滴答频率（100赫兹表示每个滴答10ms）

#define FIRST_TASK task[0]
#define LAST_TASK task[NR_TASKS-1]

#include <linux/head.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <signal.h>

// 进程(任务)运行状态
#define TASK_RUNNING		0
#define TASK_INTERRUPTIBLE	1
#define TASK_UNINTERRUPTIBLE	2
#define TASK_ZOMBIE		3
#define TASK_STOPPED		4

extern int copy_page_tables(unsigned long from, unsigned long to, long size);
extern int free_page_tables(unsigned long from, unsigned long size);

extern void sched_init(void);
extern void schedule(void);
extern void trap_init(void);

typedef int (*fn_ptr)();

/**
 * 数学协处理器使用的结构，用于保存进程切换时i387的执行状态信息
 */
struct i387_struct {
    long	cwd;
    long	swd;
    long	twd;
    long	fip;
    long	fcs;
    long	foo;
    long	fos;
    long	st_space[20];	/* 8*10 bytes for each FP-reg = 80 bytes */
};

/**
 * 任务状态段TSS的数据结构
 */
struct tss_struct {
    long	back_link;	/* 16 high bits zero */
    long	esp0;
    long	ss0;		/* 16 high bits zero */
    long	esp1;
    long	ss1;		/* 16 high bits zero */
    long	esp2;
    long	ss2;		/* 16 high bits zero */
    long	cr3;
    long	eip;
    long	eflags;
    long	eax,ecx,edx,ebx;
    long	esp;
    long	ebp;
    long	esi;
    long	edi;
    long	es;             /* 16 high bits zero */
    long	cs;		        /* 16 high bits zero */
    long	ss;		        /* 16 high bits zero */
    long	ds;		        /* 16 high bits zero */
    long	fs;		        /* 16 high bits zero */
    long	gs;		        /* 16 high bits zero */
    long	ldt;		    /* 16 high bits zero */
    long	trace_bitmap;   /* bits: trace 0, bitmap 16-31 */
    struct i387_struct i387;
};

/**
 * 进程(任务)描述符(PCB)
 */
struct task_struct {
    /* these are hardcoded - don't touch */
    long state;     /* 任务的运行状态，-1不可运行，0可运行(就绪)，>0已停止 */
    long counter;   /* 任务运行的时间片，1片表示10ms */
    long priority;  /* 任务运行优先级，任务开始运行时conter=priority */
    long signal;
    struct sigaction sigaction[32];
    long blocked;   /* bitmap of masked signals */
    /* various fields */
    int exit_code;  /* 任务执行停止的退出码，父进程会用到 */
    unsigned long start_code;   /* 代码段地址，任务要执行的指令地址 */
    unsigned long end_code;     /* 代码长度(字节数) */
    unsigned long end_data;     /* 代码长度 + 数据长度(字节数) */
    unsigned long brk;          /* 总长度(字节数) */
    unsigned long start_stack;  /* 内核栈地址 */
    long pid;       /* 进程号 */
    long father;    /* 父进程号 */
    long pgrp;      /* 父进程组号 */
    long session;   /* 会话号 */
    long leader;
    unsigned short uid;     /* 用户id */
    unsigned short euid;    /* 有效用户id */
    unsigned short suid;    /* 保存的用户id */
    unsigned short gid;     /* 组标识号 */
    unsigned short egid;    /* 有效组id */
    unsigned short sgid;    /* 保存的组id */
    long alarm;
    long utime;     /* 用户态运行时间(滴答数) */
    long stime;     /* 内核态运行时间(滴答数) */
    long cutime;    /* 子进程用户态运行时间 */
    long cstime;    /* 子进程内核态运行时间 */
    long start_time;    /* 进程开始运行时刻 */
    unsigned short used_math;   /* 是否使用了协处理器 */
    /* file system info */
    int tty;    /* 进程使用tty的子设备号，-1表示没有使用 */
    unsigned short umask;
    struct m_inode *pwd;
    struct m_inode *root;
    struct m_inode *executable;
    unsigned long close_on_exec;
    struct file *filp[NR_OPEN];
    /* ldt for this task 0 - zero 1 - cs 2 - ds&ss */
    struct desc_struct ldt[3];  /* 本进程使用的LDT表描述符，0-空 1-代码段cs，2-数据段和栈段ds&ss */
    /* tss for this task */
    struct tss_struct tss;      /* 本进程的任务状态段TSS */
};

/*
 *  INIT_TASK用于设置第一个进程的进程描述符(0号进程)
 *  基址Base=0, 段长limit=0x9ffff (=640kB)
 *  对应上面PCB结构的第1个进程的信息
 */
#define INIT_TASK \
/* state etc */	{ 0,15,15, \
/* signals */	0,{{},},0, \
/* ec,brk... */	0,0,0,0,0,0, \
/* pid etc.. */	0,-1,0,0,0, \
/* uid etc */	0,0,0,0,0,0, \
/* alarm */	0,0,0,0,0,0, \
/* math */	0, \
/* fs info */	-1,0022,NULL,NULL,NULL,0, \
/* filp */	{NULL,}, \
	{ \
		{0,0}, \
/* ldt */	{0x9f,0xc0fa00}, \
		{0x9f,0xc0f200}, \
	}, \
/*tss*/	{0,PAGE_SIZE+(long)&init_task,0x10,0,0,0,0,(long)&_pg_dir,\
	 0,0,0,0,0,0,0,0, \
	 0,0,0x17,0x17,0x17,0x17,0x17,0x17, \
	 _LDT(0),0x80000000, \
		{} \
	}, \
}

extern struct task_struct *task[NR_TASKS];
extern struct task_struct *last_task_used_math;
extern struct task_struct *current;
extern long volatile jiffies;

extern long startup_time;
#define CURRENT_TIME (startup_time+jiffies/HZ)

extern void sleep_on(struct task_struct ** p);
extern void wake_up(struct task_struct ** p);
extern void interruptible_sleep_on(struct task_struct ** p);

#define FIRST_TSS_ENTRY 4
#define FIRST_LDT_ENTRY (FIRST_TSS_ENTRY+1)
// 计算在GDT表中第n个任务(进程)的TSS描述符的索引号
#define _TSS(n) ((((unsigned long) n)<<4)+(FIRST_TSS_ENTRY<<3))
#define _LDT(n) ((((unsigned long) n)<<4)+(FIRST_LDT_ENTRY<<3))
// 加载第n个任务(进程)的任务寄存器tr
#define ltr(n) __asm__("ltr %%ax"::"a" (_TSS(n)))
#define lldt(n) __asm__("lldt %%ax"::"a" (_LDT(n)))

/*
 *	switch_to(n) 将切换当前任务到任务n。首先检测任务n是不是当前任务，
 *	如果是则直接返回。如果切换到的任务最近使用过数学协处理器，则需要复位
 *	控制寄存器cr0中的TS标志位。
 */
#define switch_to(n) {\
struct {long a,b;} __tmp; \
__asm__("cmpl %%ecx, current\n\t" \
"je 1f\n\t" \
"movw %%dx, %1\n\t" \
"xchgl %%ecx, current\n\t" \
"ljmp %0\n\t" \
"cmpl %%ecx, last_task_used_math\n\t" \
"jne 1f\n\t" \
"clts\n" \
"1:" \
::"m" (*&__tmp.a),"m" (*&__tmp.b), \
"d" (_TSS(n)),"c" ((long) task[n])); \
}

#define _set_base(addr,base) \
__asm__("movw %%dx,%0\n\t" \
"rorl $16,%%edx\n\t" \
"movb %%dl,%1\n\t" \
"movb %%dh,%2" \
::"m" (*((addr)+2)), \
"m" (*((addr)+4)), \
"m" (*((addr)+7)), \
"d" (base) \
:)

#define _set_limit(addr,limit) \
__asm__("movw %%dx,%0\n\t" \
    "rorl $16,%%edx\n\t" \
    "movb %1,%%dh\n\t" \
    "andb $0xf0,%%dh\n\t" \
    "orb %%dh,%%dl\n\t" \
    "movb %%dl,%1" \
    ::"m" (*(addr)), \
    "m" (*((addr)+6)), \
    "d" (limit) \
    :)

#define set_base(ldt,base) _set_base( ((char *)&(ldt)) , base )
#define set_limit(ldt,limit) _set_limit( ((char *)&(ldt)) , (limit-1)>>12 )

#define _get_base(addr) ({\
unsigned long __base; \
__asm__("movb %3,%%dh\n\t" \
"movb %2,%%dl\n\t" \
"shll $16,%%edx\n\t" \
"movw %1,%%dx" \
:"=&d" (__base) \
:"m" (*((addr)+2)), \
"m" (*((addr)+4)), \
"m" (*((addr)+7))); \
__base;})

#define get_base(ldt) _get_base( ((char *)&(ldt)) )

#define get_limit(segment) ({ \
unsigned long __limit; \
__asm__("lsll %1,%0\n\tincl %0":"=r" (__limit):"r" (segment)); \
__limit;})

#endif //RESEARCH_LINUX_SCHED_H
