//
// Created by asujy on 2025/7/9.
//
#include <linux/kernel.h>
#include <linux/mm.h>
#include <asm/system.h>

/* 桶描述符 */
struct bucket_desc {    /* 16 bytes */
    void*                  page;           // 指向管理的物理内存页
    struct bucket_desc*    next;           // 指向同尺寸的在一个桶描述符
    void*                  freeptr;        // 指向管理的物理页中下一个空闲内存块
    unsigned short         refcnt;         // 本物理页中已分配的内存块数量
    unsigned short         bucket_size;    // 本桶的尺寸(每个内存块的大小)
};

/* 桶描述符目录 */
struct _bucket_dir {    /* 8 bytes */
    int size;                     // 桶大小（内存块大小）
    struct bucket_desc* chain;    // 指向该尺寸的桶描述符链表
};

/*
 * The following is the where we store a pointer to the first bucket
 * descriptor for a given size.
 *
 * If it turns out that the Linux kernel allocates a lot of objects of a
 * specific size, then we may want to add that specific size to this list,
 * since that will allow the memory to be allocated more efficiently.
 * However, since an entire page must be dedicated to each specific size
 * on this list, some amount of temperance must be exercised here.
 *
 * Note that this list *must* be kept in order.
 *
 * 存储桶目录列表
 */
struct _bucket_dir bucket_dir[] = {
    { 16,	(struct bucket_desc *) 0},    // 16字节尺寸的桶（16字节内存块的链表头）
    { 32,	(struct bucket_desc *) 0},    // 32字节尺寸的桶
    { 64,	(struct bucket_desc *) 0},
    { 128,	(struct bucket_desc *) 0},
    { 256,	(struct bucket_desc *) 0},
    { 512,	(struct bucket_desc *) 0},
    { 1024,	(struct bucket_desc *) 0},
    { 2048, (struct bucket_desc *) 0},
    { 4096, (struct bucket_desc *) 0},
    { 0,    (struct bucket_desc *) 0}};    // 结束标记

/*
 * This contains a linked list of free bucket descriptor blocks
 */
struct bucket_desc *free_bucket_desc = (struct bucket_desc *) 0;

/*
 * This routine initializes a bucket description page.
 * 建立空闲桶描述符链表，让free_bucket_desc指向第一个空闲桶描述符
 */
 static inline void init_bucket_desc() {
     struct bucket_desc *bdesc, *first;
     int i;

     first = bdesc = (struct bucket_desc*) get_free_page();    // 申请一页物理内存存放桶描述符
     if (!bdesc) {
         panic("Out of memory in init_bucket_desc()");
     }
     for (i = PAGE_SIZE/sizeof(struct bucket_desc); i > 1; i--) {
         bdesc->next = bdesc + 1;
         bdesc++;
     }
     /*
     * This is done last, to avoid race conditions in case
     * get_free_page() sleeps and this routine gets called again....
     */
     bdesc->next = free_bucket_desc;
     free_bucket_desc = first;
 }

/*
* 内核专用的动态分配内存函数
* len：请求的内存块长度
* return：被分配的内存块地址
*
* Linux0.11中的函数名为malloc，现改名为kmalloc，用于区分现在的malloc
* **/
void* kmalloc(unsigned int len) {
    struct _bucket_dir *bdir;     // 桶目录指针，用于查找合适大小的桶
    struct bucket_desc *bdesc;    // 桶描述符指针，管理具体的内存页
    void* retval;                 // 分配的内存块地址

     // 步骤1：在桶目录中寻找最小满足需求的桶尺寸
    for (bdir = bucket_dir; bdir->size; bdir++) {
        if (bdir->size >= len) {
            break;
        }
    }

    if (!bdir->size) {
        printk("malloc called with impossibly large argument (%d)\n", len);
        panic("malloc: bad arg");
    }

    /*
     * Now we search for a bucket descriptor which has free space
     */
    cli();	/* Avoid race conditions */
     // 步骤2：遍历该尺寸的桶链表，寻找有空闲块的桶
    for (bdesc = bdir->chain; bdesc; bdesc = bdesc->next) {
        if (bdesc->freeptr) {
            break;
        }
    }

     // 步骤3：如果没有可用桶，创建新桶
     if (!bdesc) {
         char* cp;    // 指向新分配页面的指针
         int i;

         // 检查空闲桶描述符池
         if (!free_bucket_desc) {
             init_bucket_desc();    // 初始化新的桶描述符页
         }
         // 从空闲链表中取出桶描述符
         bdesc = free_bucket_desc;
         free_bucket_desc = bdesc->next;

         // 初始化桶描述符
         bdesc->refcnt = 0;
         bdesc->bucket_size = bdir->size;
         bdesc->page = bdesc->freeptr = (void*) (cp = (char*)get_free_page());
         if (!cp) {
             panic("Out of memory in kernel kmalloc()");
         }

         // 步骤4：设置空闲块链表
         for (i = PAGE_SIZE / bdir->size; i > 1; i--) {
             *((char**) cp) = cp + bdir->size;
             cp += bdir->size;
         }
         *((char **) cp) = 0;

         // 步骤5：将新桶插入桶目录链表头部
         bdesc->next = bdir->chain;
         bdir->chain = bdesc;
     }

     // 步骤6：分配内存块
     retval = (void*) bdesc->freeptr;
     bdesc->freeptr = *((void**) retval);
     bdesc->refcnt++;
     sti();
     return retval;
}

void kfree_s(void* obj, int size) {
    void* page;
    struct _bucket_dir* bdir;
    struct bucket_desc *bdesc, *prev;
    // 计算对象(obj)所在页的起始地址
    page = (void*) ((unsigned long) obj & 0xfffff000);
    // 搜索桶目录中对应尺寸的桶链表
    for (bdir = bucket_dir; bdir->size; bdir++) {
        prev = 0;
        if (bdir->size < size) {
            continue;
        }
        // 遍历桶链表寻找包含该页的描述符
        for (bdesc = bdir->chain; bdesc; bdesc = bdesc->next) {
            if (bdesc->page == page) {
                goto found;    // 找到匹配的桶描述符
            }
            prev = bdesc;
        }
    }
    panic("Bad address passed to kernel kfree_s()");

found:
    cli();    // 禁用中断（临界区开始）
    // 将对象（内存块）加入到桶描述符中的空闲块链表
    *((void**) obj) = bdesc->freeptr;
    bdesc->freeptr = obj;
    bdesc->refcnt--;
    // 检查是否需要释放整个物理页
    if (bdesc->refcnt == 0) {
        /*
         * We need to make sure that prev is still accurate.  It
         * may not be, if someone rudely interrupted us....
         * 如果prev已经不是搜索到的桶描述符的前一个描述符，则重新搜索当前桶描述符的前一个描述符
         */
         if ((prev && (prev->next != bdesc)) ||
             (!prev && (bdir->chain != bdesc))) {
             for (prev = bdir->chain; prev; prev = prev->next) {
                 if (prev->next == bdesc) {
                     break;
                 }
             }
         }

         // 从桶链表中移除描述符
         if (prev) {
             prev->next = bdesc->next;
         } else {
             if (bdir->chain != bdesc) {
                 panic("malloc bucket chains corrupted");
             }
             bdir->chain = bdesc->next;
         }
         free_page((unsigned long) bdesc->page);    // 释放物理页
         bdesc->next = free_bucket_desc;            // 将桶描述符插入到空闲描述符链表头，即放入空闲描述符池，加快内存分配速度
         free_bucket_desc = bdesc;
    }
    sti();
    return;
}