//
// Created by asujy on 2025/9/25.
//

#include <linux/sched.h>
#include <linux/fs.h>
#include <asm/system.h>

extern int end;
struct buffer_head * start_buffer = (struct buffer_head *) &end;    // 全局缓冲区数组的起始地址
struct buffer_head* hash_table[NR_HASH];
static struct buffer_head * free_list = NULL;
static struct task_struct * buffer_wait = NULL;
int NR_BUFFERS = 0;    // 系统缓冲区总数

/*
 * 等待缓冲区bh解锁
 * 用于同步等待一个被锁定的缓冲区变为可用状态，是内核中处理I/O操作同步的关键函数
 */
static inline void wait_on_buffer(struct buffer_head * bh)
{
    cli();
    while (bh->b_lock) {
        sleep_on(&bh->b_wait);	// 将当前进程放入缓冲区的等待队列(即当前进程休眠)
    }
    sti();
}

/*
 *  对指定设备进行高速缓冲区数据与设备上数据的同步操作
 *  即将高速缓冲区中的数据写入到磁盘中
 * 将指定设备的所有脏缓冲区同步写入磁盘，确保数据持久化，是文件系统一致性的关键函数
 */
int sync_dev(int dev) {
    int i;
    struct buffer_head *bh;

    bh = start_buffer;
    // 遍历所有缓冲区
    for (i = 0; i < NR_BUFFERS; i++, bh++) {
        if (bh->b_dev != dev) {
            continue;
        }
        wait_on_buffer(bh);
        if (bh->b_dev == dev && bh->b_dirt) {    // 如果缓冲区是脏的（b_dirt != 0），调用底层I/O写入磁盘
            ll_rw_block(WRITE, bh);              // 发起块设备写请求
        }
    }

    sync_inodes();    // 同步 inode 信息，将i节点数据写入高速缓冲区

    // 重复第一遍的过程，确保在 sync_inodes() 过程中变脏的缓冲区也能被写回磁盘
    bh = start_buffer;
    for (i = 0; i < NR_BUFFERS; i++, bh++) {
        if (bh->b_dev != dev) {
            continue;
        }
        wait_on_buffer(bh);
        if (bh->b_dev == dev && bh->b_dirt) {
            ll_rw_block(WRITE, bh);
        }
    }

    return 0;
}

/*
 * 让指定设备在高速缓冲区中的数据无效
 */
static void inline invalidate_buffers(int dev)
{
    int i;
    struct buffer_head * bh;

    bh = start_buffer;
    for (i = 0; i < NR_BUFFERS; i++,bh++) {
        if (bh->b_dev != dev) {
            continue;
        }
        wait_on_buffer(bh);
        if (bh->b_dev == dev) {
            bh->b_uptodate = bh->b_dirt = 0;
        }
    }
}

/*
 * 检查软盘是否已经被更换，主要处理可移动介质（如软盘）的更换情况。
 */
void check_disk_change(int dev) {
    int i;

    /* 是软盘设备吗？不是则退出 */
    if (MAJOR(dev) != 2) {
        return;
    }
    // 目前没有使用软盘，所以注释掉软盘部分代码
    // if (!floppy_change(dev & 0x03)) {
    //     return;
    // }
    for (i = 0; i < NR_SUPER; i++) {
        if (super_block[i].s_dev == dev) {
            put_super(super_block[i].s_dev);
        }
    }
    invalidate_inodes(dev);
    invalidate_buffers(dev);
}

// 计算hash值，访问hash_table中对应的链表头
#define _hashfn(dev,block) (((unsigned)(dev^block))%NR_HASH)
#define hash(dev,block) hash_table[_hashfn(dev,block)]

/*
 * 将缓冲区从内核的两个重要管理队列中移除：哈希队列和空闲缓冲区队列
 */
static inline void remove_from_queues(struct buffer_head * bh)
{
    /* remove from hash-queue */
    if (bh->b_next) {
        bh->b_next->b_prev = bh->b_prev;
    }
    if (bh->b_prev) {
        bh->b_prev->b_next = bh->b_next;
    }
    if (hash(bh->b_dev, bh->b_blocknr) == bh) {
        hash(bh->b_dev, bh->b_blocknr) = bh->b_next;
    }

    /* remove from free list */
    if (!(bh->b_prev_free) || !(bh->b_next_free)) {
        panic("Free block list corrupted");
    }
    bh->b_prev_free->b_next_free = bh->b_next_free;
    bh->b_next_free->b_prev_free = bh->b_prev_free;
    if (free_list == bh) {
        free_list = bh->b_next_free;
    }
}

/*
 * 将缓冲区插入到内核的两个关键管理队列中：哈希队列和空闲缓冲区队列
 */
static inline void insert_into_queues(struct buffer_head * bh)
{
    /* put at end of free list */
    bh->b_next_free = free_list;
    bh->b_prev_free = free_list->b_prev_free;
    free_list->b_prev_free->b_next_free = bh;
    free_list->b_prev_free = bh;
    /* put the buffer in new hash-queue if it has a device */
    bh->b_prev = NULL;
    bh->b_next = NULL;
    if (!bh->b_dev) {
            return;
    }
    bh->b_next = hash(bh->b_dev,bh->b_blocknr);
    hash(bh->b_dev,bh->b_blocknr) = bh;
    bh->b_next->b_prev = bh;
}

/*
 * 在高速缓冲中寻找给定设备和指定块的缓冲区块(在缓冲区哈希表中快速查找指定设备和块号对应的缓冲区)
 * 如果找到则返回缓冲区块的指针，否则返回NULL
 */
static struct buffer_head* find_buffer(int dev, int block) {
    struct buffer_head* tmp;

	// 遍历哈希桶：从哈希表对应桶的头节点开始遍历
    for (tmp = hash(dev, block); tmp != NULL; tmp = tmp->b_next) {
        if (tmp->b_dev == dev && tmp->b_blocknr == block) {
            return tmp;
        }
    }

    return NULL;
}

/*
 * 在哈希表中查找指定设备和块号的缓冲区
 */
struct buffer_head* get_hash_table(int dev, int block) {
    struct buffer_head* bh;
    for (;;) {
        if (!(bh = find_buffer(dev, block))) {
            return NULL;        // 在高速缓冲区中寻找给定设备和指定块的缓冲区块，没有则返回NULL
        }
        bh->b_count++;         // 对该缓冲区增加引用计数，防止其他进程释放该缓冲区
        wait_on_buffer(bh);    // 阻塞当前进程，等待该缓冲区解锁(如果已被上锁)

        // 重新验证缓冲区是否仍匹配（可能在等待期间被替换）
        if (bh->b_dev == dev && bh->b_blocknr == block) {
            return bh;
        }
        // 如果该缓冲区所属的设备号和块号在进程睡眠时发生了改变，则撤销对它的引用计数，重新寻找
        bh->b_count--;			// 发现缓冲区无效时释放占用
    }
}

// 评估一个缓冲区是否适合被回收和重用。
// 优先级：干净未锁定 > 脏未锁定 > 干净锁定 > 脏锁定
#define BADNESS(bh) (((bh)->b_dirt<<1)+(bh)->b_lock)

/*
 * 获取指定的设备（dev）号和块号（block）的缓冲区
 * 这个函数的目的是找到一个可用的 buffer_head 来承载指定的（dev, block）数据。
 */
struct buffer_head * getblk(int dev,int block) {
    struct buffer_head *tmp;
    struct buffer_head *bh;
repeat:
	// 通过设备号和块号组成的键去哈希表中查找。
	// 如果找到了，说明这个块已经被缓存了，直接返回对应的 buffer_head (bh)。
    if (bh = get_hash_table(dev, block)) {
        return bh;
    }

    // 如果缓存未命中，就需要从空闲链表 (free_list) 中找一个空闲缓冲区来重用。
    tmp = free_list;
    do {
        if (tmp->b_count) {    //  跳过引用计数不为0的缓冲区（说明它正在被使用，不是真正的空闲）。
            continue;
        }
        // 寻找一个引用计数为0且“badness”值最低（即最干净，最不需要进行额外I/O操作）的缓冲区
        if (!bh || BADNESS(tmp) < BADNESS(bh)) {
            bh = tmp;
            if (!BADNESS(tmp)) {
                break;
            }
        }
    } while ((tmp = tmp->b_next_free) != free_list);

    /* 如果遍历完整个空闲链表都没有找到合适的缓冲区 (!bh)，说明系统内存压力很大，所有缓冲区都在使用中。
     * 当前进程就会在 buffer_wait 等待队列上睡眠 (sleep_on)，等待其他进程释放缓冲区。
     */
    if (!bh) {
        sleep_on(&buffer_wait);
        goto repeat;
    }
    wait_on_buffer(bh);
    if (bh->b_count) {
        goto repeat;
    }
    // 如果选中的缓冲区是“脏”的（b_dirt != 0），它的数据比磁盘上的新，必须被写回磁盘后才能被重用。
    while (bh->b_dirt) {
        sync_dev(bh->b_dev);	// 同步写入设备，确保数据持久化
        wait_on_buffer(bh);     // 阻塞当前进程，等待写操作完成并释放锁
        if (bh->b_count)
            goto repeat;
    }
    if (find_buffer(dev,block)) {
        goto repeat;
    }

    // 占用并初始化缓冲区
    bh->b_count = 1;
    bh->b_dirt = 0;
    bh->b_uptodate = 0;
    remove_from_queues(bh);    // 将它从原有的哈希链和空闲链中移除。
    bh->b_dev = dev;
    bh->b_blocknr = block;
    insert_into_queues(bh);    //  将它根据新的设备号和块号插入到正确的哈希链中，并可能放回空闲链

    return bh;                // 调用者之后会读取磁盘数据到 bh 这个缓冲区中。
}

/*
 * 释放缓冲区
 * 递减引用计数
 */
void brelse(struct buffer_head * buf)
{
    if (!buf) {
        return;
    }
    wait_on_buffer(buf);    // 如果缓冲区正在被I/O操作锁定，阻塞当前进程
    if (!(buf->b_count--)) {
        panic("Trying to free free buffer");
    }
    wake_up(&buffer_wait);    // 唤醒所有在buffer_wait上睡眠的进程
}

/*
 * 从指定的块设备上读取指定的数据块到缓冲区中
 * arg:
 *     - dev: 设备号
 *     - block: 块号
 * return: 缓冲区头指针
 */
struct buffer_head * bread(int dev,int block) {
    struct buffer_head * bh;    // 声明缓冲区头指针，用于管理磁盘块在内存中的缓存

    // 在高速缓冲区中申请一个缓冲块(为指定设备块分配缓冲区)
    if (!(bh = getblk(dev, block))) {
        panic("bread: getblk returned NULL\n");
    }

	// 如果缓冲区已经是最新的，直接返回缓冲区指针，避免不必要的磁盘读取（缓存优化）
    if (bh->b_uptodate) {
        return bh;
    }

    ll_rw_block(READ,bh);    // 发起读请求：向底层块设备驱动程序提交读操作请求
    wait_on_buffer(bh);		 // 阻塞当前进程，等待I/O操作完成

    if (bh->b_uptodate) {
        return bh;
    }

    brelse(bh);				// 如果读取失败，释放缓冲区引用计数

    return NULL;
}

#define COPYBLK(from,to) \
__asm__("cld\n\t" \
    "rep\n\t" \
    "movsl\n\t" \
    ::"c" (BLOCK_SIZE/4),"S" (from),"D" (to) \
    :)

void bread_page(unsigned long address,int dev,int b[4])
{
    struct buffer_head * bh[4];
    int i;

    for (i = 0; i < 4; i++) {
        if (b[i]) {
            if (bh[i] = getblk(dev, b[i])) {
                if (!bh[i]->b_uptodate) {
                    ll_rw_block(READ, bh[i]);
                }
            }
        } else {
            bh[i] = NULL;
        }
    }
    for (i = 0; i < 4; i++, address += BLOCK_SIZE) {
        if (bh[i]) {
            wait_on_buffer(bh[i]);
            if (bh[i]->b_uptodate) {
                COPYBLK((unsigned long) bh[i]->b_data, address);
            }
            brelse(bh[i]);
        }
    }
}

/*
 * 内核中的缓冲区初始化函数
 *
 * 参数buffer_end是指定的缓冲区内存的末端。对于系统有16MB内存，则缓冲区末端设置为4MB。
 * 对于系统有8MB内存，缓冲区末端设置为2MB。
 */
void buffer_init(long buffer_end)
{
    struct buffer_head * h = start_buffer;
    void * b;
    int i;

    if (buffer_end == 1<<20) {
        b = (void *) (640*1024);    // 如果缓冲区末端为1MB,从640KB开始
    } else {
        b = (void *) buffer_end;
    }
    while ( (b -= BLOCK_SIZE) >= ((void *) (h + 1)) ) {
        h->b_dev = 0;       // 缓冲区未关联任何设备
        h->b_dirt = 0;      // 缓冲区内容未修改
        h->b_count = 0;     // 引用计数为零
        h->b_lock = 0;      // 缓冲区未上锁
        h->b_uptodate = 0;  // 缓冲区数据不是最新的
        h->b_wait = NULL;
        h->b_next = NULL;
        h->b_prev = NULL;
        h->b_data = (char *) b;
        h->b_prev_free = h - 1;
        h->b_next_free = h + 1;
        h++;
        NR_BUFFERS++;
        if (b == (void *) 0x100000) {
            b = (void *) 0xA0000;
        }
    }
    h--;
    free_list = start_buffer;
    free_list->b_prev_free = h;
    h->b_next_free = free_list;
    for (i = 0; i < NR_HASH; i++) {
        hash_table[i] = NULL;
    }
}
