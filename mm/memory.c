//
// Created by asujy on 2025/7/2.
//

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