//
// Created by asujy on 2025/7/25.
//

#include <string.h>
#include <errno.h>

#include <linux/sched.h>
#include <asm/system.h>

extern void write_verify(unsigned long address);

long last_pid=0;  // 最新的进程号

void verify_area(void * addr,int size)
{
    unsigned long start;

    start = (unsigned long) addr;
    size += start & 0xfff;
    start &= 0xfffff000;
    start += get_base(current->ldt[2]);
    while (size>0) {
        size -= 4096;
        write_verify(start);
        start += 4096;
    }
}

/*
* 为新进程设置线性地址空间并复制父进程的页表。
* 设置新进程的代码段和数据段的基地址，限长并复制页表。
* 主要工作：
* 1. 继承父进程的内存布局
*   - 获取当前进程（父进程）的代码段/数据段基地址和限长
*   - 要求父进程满足 代码段与数据段共用同一基地址（不支持分离的指令与数据空间）
* 2. 设置新进程的地址空间
* 3. 复制父进程的页表到新进程中
* nr：task数组下标
* p：新进程的PCB
* */
int copy_mem(int nr, struct task_struct * p)
{
  unsigned long old_data_base;    // 当前进程（父进程）数据段基地址
  unsigned long new_data_base;    // 新进程（子进程）数据段基地址
  unsigned long data_limit;       // 数据段长度限制（段界限）
  unsigned long old_code_base;
  unsigned long new_code_base;
  unsigned long code_limit;

  /* 获取当前进程（父进程）的LDT表中代码段和数据段的段大小(段限长) */
  code_limit = get_limit(0x0f);
  data_limit = get_limit(0x17);
  /* 获取当前进程（父进程）的LDT表中代码段和数据段在线性空间中的基地址 */
  old_code_base = get_base(current->ldt[1]);
  old_data_base = get_base(current->ldt[2]);
  if (old_data_base != old_code_base) {
    panic("We don't support separate I&D");
  }
  if (data_limit < code_limit) {
    panic("Bad data_limit");
  }

  /* 新进程在线性地址空间中的基地址等于64MB * 任务号，即每个进程获得独立的64MB线性地址空间 */
  new_data_base = new_code_base = nr * 0x4000000;
  p->start_code = new_code_base;
  set_base(p->ldt[1], new_code_base);  // 设置新进程LDT表中的代码段的基地址
  set_base(p->ldt[2], new_data_base);  // 设置新进程LDT表中的数据段的基地址

  /*
  * 设置新进程的页目录项和页表项。
  * 将父进程页表复制到新进程的地址空间（写时复制机制的基础）
  * 失败时清理已分配页表并返回内存不足错误
  */
  if (copy_page_tables(old_data_base, new_data_base, data_limit)) {
    free_page_tables(new_data_base, data_limit);
    return -ENOMEM;
  }
  return 0;
}

/*
 * 这是主要的fork子程序。负责创建一个新的进程。
 * 主要完成以下工作：
 * 1. 分配并初始化新进程的任务结构体task_struct(即PCB)
 * 2. 复制父进程上下文到子进程中
 * 3. 设置新进程的内存空间(通过copy_mem)
 * 4. 管理文件描述符和文件系统引用
 * 5. 设置任务状态段(TSS)和LDT表
 *
 *  nr：调用find_empty_process()分配的任务数组下标号
 *  none：system_call.s中调用sys_call_table时压入堆栈的返回地址
 */
int copy_process(int nr,long ebp,long edi,long esi,long gs,long none,
    long ebx,long ecx,long edx,
    long fs,long es,long ds,
    long eip,long cs,long eflags,long esp,long ss)
{
  struct task_struct *p;
  int i;
  struct file *f;

  p = (struct task_struct *) get_free_page();  // 为新进程的数据结构分配内存(存放PCB)
  if (!p) {
    return -EAGAIN;
  }
  task[nr] = p;      // 新进程的PCB放到 进程管理结构task[]中进行管理
  memcpy(p, current, sizeof(struct task_struct));
  //*p = *current;	    /* NOTE! this doesn't copy the supervisor stack */
  /* 初始化独属于新进程自己的数据 */
  p->state       = TASK_UNINTERRUPTIBLE;
  p->pid         = last_pid;
  p->father      = current->pid;
  p->counter     = p->priority;
  p->signal      = 0;
  p->alarm       = 0;
  p->leader      = 0;		          /* process leadership doesn't inherit */
  p->utime       = p->stime = 0;
  p->cutime      = p->cstime = 0;
  p->start_time  = jiffies;

  /* 设置任务状态段TSS，初始化独属于新进程自己的TSS的数据 */
  p->tss.back_link     = 0;
  p->tss.esp0          = PAGE_SIZE + (long) p;   // 设置新进程的内核栈顶(指向PCB所在内存页中的高地址处)，即ss0:esp0用于作为进程在内核态执行时的堆栈
  p->tss.ss0           = 0x10;
  p->tss.eip           = eip;                    // 指令指针（从父进程继承）
  p->tss.eflags        = eflags;
  p->tss.eax           = 0;                      // 当fork返回时，新进程会返回0的原因
  p->tss.ecx           = ecx;
  p->tss.edx           = edx;
  p->tss.ebx           = ebx;
  p->tss.esp           = esp;
  p->tss.ebp           = ebp;
  p->tss.esi           = esi;
  p->tss.edi           = edi;
  p->tss.es            = es & 0xffff;
  p->tss.cs            = cs & 0xffff;
  p->tss.ss            = ss & 0xffff;
  p->tss.ds            = ds & 0xffff;
  p->tss.fs            = fs & 0xffff;
  p->tss.gs            = gs & 0xffff;
  p->tss.ldt           = _LDT(nr);               // 设置新进程的LDT表
  p->tss.trace_bitmap  = 0x80000000;

  /* 如果父进程使用过数学协处理器，保存其状态到子进程的TSS */
  if (last_task_used_math == current) {
    __asm__("clts ; fnsave %0"::"m" (p->tss.i387));
  }

  /*
  * 设置新进程的内存空间。
  * 如果出错，则复位task数组中对应的项，并释放为该新进程分配的内存页。
  * */
  if (copy_mem(nr, p)) {
    task[nr] = NULL;
    free_page((long) p);
    return -EAGAIN;
  }

  /* 管理文件资源 */
  for (i = 0; i < NR_OPEN; i++) {
    if (f = p->filp[i]) {
      f->f_count++;        // 增加打开文件引用计数
    }
  }
  if (current->pwd) {
    current->pwd->i_count++;  // 增加工作目录引用
  }
  if (current->root) {
    current->root->i_count++;
  }
  if (current->executable) {
    current->executable->i_count++;
  }

  /* 在GDT表中设置新进程的LDT和TSS描述符 */
  set_tss_desc(_gdt + (nr << 1) + FIRST_TSS_ENTRY, &(p->tss));
  set_ldt_desc(_gdt + (nr << 1) + FIRST_LDT_ENTRY, &(p->ldt));

  p->state = TASK_RUNNING;	/* do this last, just in case */
  return last_pid;
}

/* 查找空闲进程号,作为新进程的PID */
int find_empty_process(void) {
  int i;

repeat:
  if ((++last_pid) < 0) {
    last_pid = 1;
  }
  /* 搜索task数组，last_pid是否已经被某进程占用了 */
  for (i = 0; i < NR_TASKS; i++) {
    if (task[i] && task[i]->pid == last_pid) {
      goto repeat;
    }
  }
  /* 在task数组中为新任务寻找一个空闲项 */
  for (i = 1; i < NR_TASKS; i++) {
    if (!task[i]) {
      return i;
    }
  }

  /* task数组中64个项都已经被全部占用，则返回出错码 */
  return -EAGAIN;
}