#ifndef BLK_H
#define BLK_H

#include <linux/fs.h>

#define NR_BLK_DEV	7
/*
 * NR_REQUEST is the number of entries in the request-queue.
 * NOTE that writes may use only the low 2/3 of these: reads
 * take precedence.
 *
 * 32 seems to be a reasonable number: enough to get some benefit
 * from the elevator-mechanism, but not so much as to lock a lot of
 * buffers when they are in the queue. 64 seems to be too many (easily
 * long pauses in reading when heavy writing/syncing is going on)
 */
#define NR_REQUEST	32

struct request {
    int dev;		             /* 设备号 */
    int cmd;		             /* 读写命令，READ or WRITE */
    int errors;                      /* 该请求的错误次数 */
    unsigned long sector;            /* 起始扇区 */
    unsigned long nr_sectors;        /* 扇区数量 */
    char * buffer;                   /* 数据缓冲区 */
    struct task_struct * waiting;    /* 等待此请求的进程 */
    struct buffer_head * bh;         /* 数据缓冲区，硬盘的数据先写入到此内存中 */
    struct request * next;           /* 下一个请求 */
};

/*
 * This is used in the elevator algorithm: Note that
 * reads always go before writes. This is natural: reads
 * are much more time-critical than writes.
 */
#define IN_ORDER(s1,s2) \
((s1)->cmd<(s2)->cmd || (s1)->cmd==(s2)->cmd && \
((s1)->dev < (s2)->dev || ((s1)->dev == (s2)->dev && \
(s1)->sector < (s2)->sector)))

struct blk_dev_struct {
    void (*request_fn)(void);            // 请求处理函数
    struct request * current_request;    // 当前请求
};

extern struct blk_dev_struct blk_dev[NR_BLK_DEV];    // 内核的块设备数组
extern struct request request[NR_REQUEST];           // 全局IO请求数组
extern struct task_struct * wait_for_request;

#ifdef MAJOR_NR

/*
 * Add entries as needed. Currently the only block devices
 * supported are hard-disks and floppies.
 */

#if (MAJOR_NR == 1)
/* ram disk */
#define DEVICE_NAME "ramdisk"
#define DEVICE_REQUEST do_rd_request

#elif (MAJOR_NR == 2)
/* floppy */
#define DEVICE_NAME "floppy"

#elif (MAJOR_NR == 3)
/* harddisk */
#define DEVICE_NAME "harddisk"
#define DEVICE_INTR do_hd
#define DEVICE_REQUEST do_hd_request
#define DEVICE_NR(device) (MINOR(device)/5)
#define DEVICE_OFF(device)

#elif
/* unknown blk device */
#error "unknown blk device"

#endif    // (MAJOR_NR == 1)

// 指向当前正在处理的请求
#define CURRENT (blk_dev[MAJOR_NR].current_request)
#define CURRENT_DEV DEVICE_NR(CURRENT->dev)

#ifdef DEVICE_INTR
// 硬盘中断处理函数指针
void (*DEVICE_INTR)(void) = NULL;
#endif    // DEVICE_INTR

static inline void unlock_buffer(struct buffer_head * bh)
{
    if (!bh->b_lock) {
        printk(DEVICE_NAME ": free buffer being unlocked\n");
    }
    bh->b_lock=0;
    wake_up(&bh->b_wait);
}

/*
 * end_request函数用于完成一个块设备I/O请求的处理，
 * 它负责更新缓冲区状态、唤醒等待进程，并推进请求队列。
 * args:
 *  - uptodate：表示I/O操作是否成功；1 成功；0 失败
 */
static inline void end_request(int uptodate)
{
    DEVICE_OFF(CURRENT->dev);   // 关闭设备，硬盘不需要特殊的关闭操作
    if (CURRENT->bh) {
        CURRENT->bh->b_uptodate = uptodate; // 更新缓冲区的数据有效性标志
        unlock_buffer(CURRENT->bh);         // 唤醒等待该缓冲区的所有进程
    }
    if (!uptodate) {
        printk(DEVICE_NAME " I/O error\n\r");
        printk("dev %04x, block %d\n\r",CURRENT->dev,
                CURRENT->bh->b_blocknr);
    }
    wake_up(&CURRENT->waiting);     // 唤醒正在等待这个特定请求完成的进程
    wake_up(&wait_for_request);     // 唤醒等待任何请求可用的进程
    CURRENT->dev = -1;              // 将设备号设为-1，表示该请求槽位可用
    CURRENT = CURRENT->next;        // 指向下一个请求
}

/*
 *在块设备驱动中初始化并验证当前请求，确保请求的有效性和数据一致性。
 * - 检查空指针
 * - 验证设备归属
 * - 确保数据一致性
 */
#define INIT_REQUEST \
repeat: \
    if (!CURRENT) \
        return; \
    if (MAJOR(CURRENT->dev) != MAJOR_NR) \
        panic(DEVICE_NAME ": request list destroyed"); \
    if (CURRENT->bh) { \
    if (!CURRENT->bh->b_lock) \
        panic(DEVICE_NAME ": block not locked"); \
    }

#endif    // MAJOR_NR

#endif    // BLK_H