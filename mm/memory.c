//
// Created by asujy on 2025/7/2.
//
#include <linux/kernel.h>

#define LOW_MEM 0x100000    // 物理内存的分界点（1MB 地址）。低于此地址的内存用于内核代码和静态数据，高于此地址的内存用于动态分配。
#define PAGING_MEMORY (15*1024*1024)        // 可用于分页管理的物理内存大小（15MB）。这是内核可动态管理的最大内存（1MB~16MB）
#define PAGING_PAGES (PAGING_MEMORY>>12)    // 物理页数量
#define MAP_NR(addr) (((addr)-LOW_MEM)>>12)    // 计算物理地址 addr 对应的物理页号索引（在 mem_map 数组中的位置）
#define USED 100    // 表示页面已被占用（不可分配）

static long HIGH_MEMORY = 0;    // 系统实际可用的物理内存结束位置

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
        printk("[%s]return: 0x%X\n", __FUNCTION__, addr);
        return;
    }
    mem_map[addr] = 0;
    panic("trying to free free page");    // 试图释放空闲页面，触发内核panic错误
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