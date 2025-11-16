//
// Created by asujy on 2025/7/2.
//
#include <signal.h>

#include <linux/sched.h>
#include <linux/kernel.h>

volatile void do_exit(long code);

/*
 * 内存不足处理函数：打印消息，终止当前进程并发送SIGSEGV信号
 */
static inline volatile void oom(void) {
    printk("out of memory\n\r");
    do_exit(SIGSEGV);
}

// 刷新TLB，确保新页表生效
#define invalidate() \
__asm__("movl %%eax,%%cr3"::"a" (0))

#define LOW_MEM 0x100000    // 物理内存的分界点（1MB 地址）。低于此地址的内存用于内核代码和静态数据，高于此地址的内存用于动态分配。
#define PAGING_MEMORY (15*1024*1024)        // 可用于分页管理的物理内存大小（15MB）。这是内核可动态管理的最大内存（1MB~16MB）
#define PAGING_PAGES (PAGING_MEMORY>>12)    // 物理页数量
#define MAP_NR(addr) (((addr)-LOW_MEM)>>12)    // 计算物理地址 addr 对应的物理页号索引（在 mem_map 数组中的位置）
#define USED 100    // 表示页面已被占用（不可分配）

/*  判断给定地址addr是否位于当前进程的代码段中 */
#define CODE_SPACE(addr) ((((addr)+4095)&~4095) < \
current->start_code + current->end_code)

static long HIGH_MEMORY = 0;    // 系统实际可用的物理内存结束位置

/* 复制1页内存页（使用汇编优化） */
#define copy_page(from,to) \
__asm__("cld ; rep ; movsl"::"S" (from),"D" (to),"c" (1024):)

/*
* 物理页管理数组，每个元素代表一个4KB的物理页
* 0 = 空闲页
* USED (100) = 永久占用页（内核或硬件保留）
* ≥1 = 页的引用计算（动态分配内存时使用）
* */
static unsigned char mem_map [ PAGING_PAGES ] = {0,};

/*
* 申请一个空闲物理页，从数组 mem_map 中查找空闲页，标记为已用，清空页面内容。
*
* return：返回一个空闲物理页面的起始地址
* */
unsigned long get_free_page(void) {
    register unsigned long __res asm("ax") = 0;
    __asm__("std; repne; scasb\n\t"     // 从mem_map数组的最后一项往前查找，看看能否找到为0的元素（表示某个物理页面空闲）
    "jne 1f\n\t"                        // 如果未找到空闲页，跳转到标签 1（直接返回0）
    "movb $1, 1(%%edi)\n\t"             // 找到空闲页后，将匹配的 mem_map 项设为1；1(%%edi) = EDI + 1（回退到匹配位置，因为EDI指向的是匹配项的下一个位置）
    "sall $12, %%ecx\n\t"               // ECX 现在包含的是物理页数量，将 ECX 中的值左移12位（乘以4096），得到物理页的起始地址（不包括低端物理内存）
    "addl %2, %%ecx\n\t"                // 加上低端内存地址（LOW_MEM = 0x100000），ECX 现在包含实际的物理页的起始地址
    "movl %%ecx, %%edx\n\t"             // 将物理页地址保存到 EDX
    "movl $1024, %%ecx\n\t"             // 设置计数器 ECX = 1024（用于清零4KB页面）
    "leal 4092(%%edx), %%edi\n\t"       // 计算物理页的末端地址：EDX + 4092（页的最后4字节），EDI 指向要清零区域的末尾
    "rep; stosl\n\t"                    // 将edi所致内存清零（将整个4KB物理页面清零）
    "movl %%edx, %%eax\n"               // 将物理页地址从 EDX 移到 EAX（作为返回值）
    "1:"
    :"=a" (__res)                       // 输出：结果通过 EAX 存入 __res
    :"0" (0), "i" (LOW_MEM), "c" (PAGING_PAGES),    // 输入：EAX 初始值 = 0（用于清零），LOW_MEM 作为立即数，ECX = 可分页总数，EDI 指向 mem_map 数组末尾
    "D" (mem_map + PAGING_PAGES - 1)
    :"dx");
    return __res;
}

/*
* 释放一个物理页面
* addr：要释放的物理页面的起始物理地址
* */
void free_page(unsigned long addr) {
    if (addr < LOW_MEM) {    // 低于 1MB 的内存是内核的代码和数据区域，不是动态分配的，因此直接返回（不允许释放）
        return;
    }
    if (addr >= HIGH_MEMORY) {    // 地址超过内核管理的最高物理地址（HIGH_MEMORY）,说明尝试释放一个不存在的页面，触发内核 panic（严重错误处理）
        panic("trying to free nonexistent page");
    }
    addr -= LOW_MEM;    // 将地址转换为相对于 1MB 的偏移量（因为 mem_map 管理的是从 1MB 开始的页面）
    addr >>= 12;        // 得到物理地址在mem_map数组中的索引
    if (mem_map[addr]--) {    // 如果数组元素的值大于0，则减少该页面的引用计数，直接返回
        //printk("[%s]return: 0x%X\n", __FUNCTION__, addr);
        return;
    }
    mem_map[addr] = 0;
    panic("trying to free free page");    // 试图释放空闲页面，触发内核panic错误
}

/*
 * 释放页表和相关物理页。
 *
 * 释放从虚拟地址 from 开始、大小为 size 的连续内存区域，包括:
 * 1. 物理内存页：解除映射并释放物理页
 * 2. 页表结构：释放页表占用的物理页
 * 3. 页目录项：清除页目录条目
 */
int free_page_tables(unsigned long from, unsigned long size)
{
    unsigned long *pg_table;
    unsigned long *dir;
    unsigned long nr;

    if (from & 0x3fffff) {
        panic("free_page_tables called with wrong alignment");
    }

    /* 禁止释放内核空间，from=0 对应内核页表 */
    if (!from) {
        panic("Trying to free up swapper memory space");
    }

    size = (size + 0x3fffff) >> 22;                 // 计算需释放的页目录项数量
    dir = (unsigned long *) ((from >> 20) & 0xffc); // 获取起始页目录项指针
    for (; size-- > 0; dir++) {
        if (!(1 & *dir)) {  // 跳过未使用的页目录项
            continue;
        }

        pg_table = (unsigned long *) (0xfffff000 & *dir);   // 获取页表物理地址
        for (nr = 0; nr < 1024; nr++) {
            if (1 & *pg_table) {                            // 检查页表项是否存在
                free_page(0xfffff000 & *pg_table);      // 释放物理页
            }
            *pg_table = 0;
            pg_table++;
        }
        free_page(0xfffff000 & *dir);   // 释放页表占用的物理页
        *dir = 0;                           // 清空页目录项
    }
    invalidate();
    return 0;
}

/*
 * 复制页表，copy_page_tables 并不复制实际物理内存，只复制页表项，并将父子进程的页表项设为只读。
 * 当任一进程尝试写入时触发缺页异常，再分配新物理页。
 *
 * 将源地址范围(from)的页表复制到目标地址(to)，覆盖指定大小(size)的内存区域
 */
int copy_page_tables(unsigned long from, unsigned long to, long size) {
    unsigned long * from_page_table;    // 源（父进程）页表指针
    unsigned long * to_page_table;      // 目标（新进程）页表指针
    unsigned long this_page;            // 当前处理的页表项
    unsigned long * from_dir;           // 源（父进程）页目录项指针
    unsigned long * to_dir;             // 目标（新进程）页目录项指针
    unsigned long nr;                   // 页表项数量

    /* 要求源地址和目标地址按 4MB 对齐 */
    if ((from & 0x3fffff) || (to & 0x3fffff)) {
        panic("copy_page_tables called with wrong alignment");
    }

    /*
     * 计算页目录表的起始页目录项地址。
     * 对应的目录项号 = from >> 22，因为每项占4字节，因此实际的目录项指针 = 目录项号 << 2，也即(from>>20)。
     * 与上0xffc确保目录项指针范围有效（4字节对齐）
     */
    from_dir = (unsigned long *) ((from>>20) & 0xffc); /* _pg_dir = 0 */
    to_dir = (unsigned long *) ((to>>20) & 0xffc);      // 新进程的页目录项（页目录表的地址）

    /* 计算需复制的页目录项数量 */
    size = ((unsigned) (size + 0x3fffff)) >> 22;
    /* 遍历页目录项 */
    for (; size-- > 0; from_dir++, to_dir++) {
        /* 检查目标（新进程）页目录项是否已被占用，以及跳过未使用的源（父进程）页目录项 */
        if (1 & *to_dir) {
            panic("copy_page_tables: already exist");
        }
        if (!(1 & *from_dir)) {
            continue;
        }

        from_page_table = (unsigned long *) (0xfffff000 & *from_dir);   // 获取源页目录表中目录项对应的页表的地址
        if (!(to_page_table = (unsigned long *) get_free_page())) {     // 为目标页表分配物理页
            return -1;    /* Out of memory, see freeing */
        }
        *to_dir = ((unsigned long) to_page_table) | 7;                  // 设置目标页目录项，页表物理地址 + 权限位（7=111b 表示用户可读写）

        // 复制页表项
        nr = (from == 0) ? 0xA0 : 1024;     // 内核空间复制160项(640KB)，用户空间复制1024项(4MB)
        for (; nr-- > 0; from_page_table++, to_page_table++) {
            this_page = *from_page_table;   // 获取源页表项
            if (!(1 & this_page)) {         // 跳过未使用的页
                continue;
            }
            this_page &= ~2;                // 清除写标志(R/W=0)，即设置为只读
            *to_page_table = this_page;     // 复制页表项(只读)

            // 更新内存引用计数
            if (this_page > LOW_MEM) {          // LOW_MEM=1MB，仅跟踪1MB以上内存
                *from_page_table = this_page;   // 更新源页表项为只读

                this_page -= LOW_MEM;
                this_page >>= 12;               // 计算物理页号
                mem_map[this_page]++;           // 增加物理页引用计数
            }
        }
    }
    invalidate();   // 刷新TLB，确保新页表生效
    return 0;
}

unsigned long put_page(unsigned long page, unsigned long address)
{
    unsigned long tmp;
    unsigned long *page_table;

    /* NOTE !!! This uses the fact that _pg_dir=0 */

    if (page < LOW_MEM || page >= HIGH_MEMORY) {
        printk("Trying to put page %p at %p\n", page, address);
    }
    if (mem_map[(page-LOW_MEM)>>12] != 1) {
        printk("mem_map disagrees with %p at %p\n", page, address);
    }
    page_table = (unsigned long *) ((address>>20) & 0xffc); // 找到页目录项
    if ((*page_table)&1) {
        page_table = (unsigned long *) (0xfffff000 & *page_table);
    } else {
        if (!(tmp = get_free_page())) {
            return 0;
        }
        *page_table = tmp|7;    // 写入页目录项
        page_table = (unsigned long *) tmp;
    }
    page_table[(address>>12) & 0x3ff] = page | 7;   // 写入页表项
    /* no need for invalidate */
    return page;
}

/*
 *  取消写保护页的函数，用于页异常中断过程中写保护异常的处理（写时复制核心实现）
 */
void un_wp_page(unsigned long* table_entry) {
    unsigned long old_page;     // 原物理页地址
    unsigned long new_page;     // 新物理页地址

    // 从页表项中提取物理页地址（清除低12位标志位）
    old_page = 0xfffff000 & *table_entry;

    // 如果原页面在低端内存之上且引用计数为1（只有当前进程使用）
    if (old_page >= LOW_MEM && mem_map[MAP_NR(old_page)] == 1) {
        *table_entry |= 2;  // 直接设置页表项为可写（设置R/W位）
        invalidate();
        return;
    }

    // 申请新的空闲页，失败则执行OOM
    if (!(new_page = get_free_page())) {
        oom();
    }

    // 如果原页面大于内存低端，意味着页面是共享的，则减少原页面的引用计数
    if (old_page >= LOW_MEM) {
        mem_map[MAP_NR(old_page)]--;
    }

    // 设置新页表项：新页面地址 + 权限标志（7=111b表示用户可读写、存在）
    *table_entry = new_page | 7;
    invalidate();

    // 复制原页面内容到新页面
    copy_page(old_page, new_page);
}

/*
 * 当用户试图往一个共享页面上写时，这个函数处理已存在的内存页面（写时复制）
 * 通过将页面复制到一个新地址上并递减原页面的共享页面计数值来实现
 */
void do_wp_page(unsigned long error_code, unsigned long address) {
#if 0
    if (CODE_SPACE(address)) {
        do_exit(SIGSEGV);
    }
#endif
    un_wp_page((unsigned long *)
                (((address>>10) & 0xffc) + (0xfffff000 &
                *((unsigned long *) ((address>>20) &0xffc)))));
}

void write_verify(unsigned long address)
{
    unsigned long page;

    if (!( (page = *((unsigned long *) ((address>>20) & 0xffc)) )&1)) {
        return;
    }
    page &= 0xfffff000;
    page += ((address>>10) & 0xffc);
    if ((3 & *(unsigned long *) page) == 1) {
        /* non-writeable, present */
        un_wp_page((unsigned long *) page);
    }
    return;
}

void get_empty_page(unsigned long address)
{
    unsigned long tmp;

    if (!(tmp = get_free_page()) || !put_page(tmp, address)) {
        free_page(tmp);		/* 0 is ok - ignored */
        oom();
    }
}

static int try_to_share(unsigned long address, struct task_struct * p)
{
    unsigned long from;
    unsigned long to;
    unsigned long from_page;
    unsigned long to_page;
    unsigned long phys_addr;

    from_page = to_page = ((address >> 20) & 0xffc);
    from_page += ((p->start_code>>20) & 0xffc);
    to_page += ((current->start_code>>20) & 0xffc);
    /* is there a page-directory at from? */
    from = *(unsigned long *) from_page;
    if (!(from & 1)) {
        return 0;
    }
    from &= 0xfffff000;
    from_page = from + ((address >> 10) & 0xffc);
    phys_addr = *(unsigned long *) from_page;
    /* is the page clean and present? */
    if ((phys_addr & 0x41) != 0x01) {
        return 0;
    }
    phys_addr &= 0xfffff000;
    if (phys_addr >= HIGH_MEMORY || phys_addr < LOW_MEM) {
        return 0;
    }
    to = *(unsigned long *) to_page;
    if (!(to & 1)) {
        if (to = get_free_page()) {
            *(unsigned long *) to_page = to | 7;
        } else {
            oom();
        }
    }
    to &= 0xfffff000;
    to_page = to + ((address>>10) & 0xffc);
    if (1 & *(unsigned long *) to_page) {
        panic("try_to_share: to_page already exists");
    }
    /* share them: write-protect */
    *(unsigned long *) from_page &= ~2;
    *(unsigned long *) to_page = *(unsigned long *) from_page;
    invalidate();
    phys_addr -= LOW_MEM;
    phys_addr >>= 12;
    mem_map[phys_addr]++;
    return 1;
}

static int share_page(unsigned long address)
{
    struct task_struct ** p;

    if (!current->executable) {
        return 0;
    }
    if (current->executable->i_count < 2) {
        return 0;
    }
    for (p = &LAST_TASK; p > &FIRST_TASK; --p) {
        if (!*p) {
            continue;
        }
        if (current == *p) {
            continue;
        }
        if ((*p)->executable != current->executable) {
            continue;
        }
        if (try_to_share(address, *p)) {
            return 1;
        }
    }
    return 0;
}

void do_no_page(unsigned long error_code, unsigned long address) {
    int nr[4];
    unsigned long tmp;
    unsigned long page;
    int block;
    int i;

    address &= 0xfffff000;
    tmp = address - current->start_code;
    if (!current->executable || tmp >= current->end_data) {
        get_empty_page(address);
        return;
    }
    if (share_page(tmp)) {
        return;
    }
    if (!(page = get_free_page())) {    // 申请一页空闲物理页
        oom();
    }
    /* remember that 1 block is used for header */
    block = 1 + tmp/BLOCK_SIZE;
    for (i = 0; i < 4; block++,i++) {   // 一个数据块 1024 字节，所以一页内存需要读 4 个数据块
        nr[i] = bmap(current->executable, block); // bmap 负责将相对于文件的数据块编号转换为相对于整个硬盘的数据块编号，比如这个文件的第 1 块数据，可能对应在整个硬盘的第 24 块的位置。
    }
    bread_page(page, current->executable->i_dev, nr);
    i = tmp + 4096 - current->end_data;
    tmp = page + 4096;
    while (i-- > 0) {
        tmp--;
        *(char *)tmp = 0;
    }
    if (put_page(page, address)) {  // 完成页表的映射
        return;
    }
    free_page(page);
    oom();
}

/*
* 物理内存初始化
* start_mem：可用物理内存起始地址(≥1MB)
* end_mem：可用物理内存结束地址
* */
void mem_init(long start_mem, long end_mem) {
    int i;
    HIGH_MEMORY = end_mem;
    // 初始化所有物理页为USED状态，先将整个 mem_map 数组标记为"已占用"，后续再释放可用区域
    for (i = 0; i < PAGING_PAGES; i++) {
        mem_map[i] = USED;
    }
    i = MAP_NR(start_mem);    // 计算可用于分配的物理页的页号
    end_mem -= start_mem;
    end_mem >>= 12;            // 计算可用于分配的物理页数量
    while (end_mem-- > 0) {    // 将可用于分配的物理页标记为空闲状态（可用于分配）
        mem_map[i++] = 0;
    }
}

void calc_mem(void)
{
    int i;
    int j;
    int k;
    int free=0;
    long * pg_tbl;

    for (i = 0; i < PAGING_PAGES; i++) {
        if (!mem_map[i]) {
            free++;
        }
    }
    printk("%d pages free (of %d)\n\r",free,PAGING_PAGES);
    for (i = 2; i < 1024; i++) {
        if (1 & _pg_dir[i]) {
            pg_tbl = (long *) (0xfffff000 & _pg_dir[i]);
            for (j = k = 0; j < 1024; j++) {
                if (pg_tbl[j]&1) {
                    k++;
                }
            }
            printk("Pg-dir[%d] uses %d pages\n",i,k);
        }
    }
}