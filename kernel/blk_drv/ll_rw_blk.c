//
// Created by asujy on 2025/9/4.
//

#include <linux/sched.h>
#include <asm/system.h>

#include "blk.h"

struct request request[NR_REQUEST];
struct task_struct * wait_for_request = NULL;

struct blk_dev_struct blk_dev[NR_BLK_DEV] = {
  { NULL, NULL },		/* no_dev */
  { NULL, NULL },		/* dev mem */
  { NULL, NULL },		/* dev fd */
  { NULL, NULL },		/* dev hd */
  { NULL, NULL },		/* dev ttyx */
  { NULL, NULL },		/* dev tty */
  { NULL, NULL }		/* dev lp */
};

/*
 * 实现了缓冲区的互斥访问机制，确保同一时间只有一个进程可以操作特定的缓冲区
 *
 * lock_buffer和unlock_buffer是Linux内核中用于缓冲区同步的基础函数，
 * 通过禁用中断和等待队列实现简单的自旋锁机制，确保对缓冲区的原子访问。
 * */
static inline void lock_buffer(struct buffer_head * bh)
{
    cli();
    while (bh->b_lock) {
        sleep_on(&bh->b_wait);    // 将当前进程加入等待队列并睡眠
    }
    bh->b_lock = 1;    // 设置缓冲区锁定标志,现在当前进程独占访问该缓冲区
    sti();
}

static inline void unlock_buffer(struct buffer_head * bh)
{
    if (!bh->b_lock) {
        printk("ll_rw_block.c: buffer not locked\n\r");
    }
    bh->b_lock = 0;          // 清除缓冲区锁定标志,允许其他进程获取该缓冲区
    wake_up(&bh->b_wait);    // 唤醒所有在该缓冲区等待队列上睡眠的进程
}

/*
 * add-request adds a request to the linked list.
 * It disables interrupts so that it can muck with the
 * request-lists in peace.
 * 将新的I/O请求添加到指定设备的请求队列，并实现类似电梯算法的请求排序优化
 * args:
 *     - dev: 块设备结构指针
 *     - req：要添加的请求结构指针
 */
static void add_request(struct blk_dev_struct * dev, struct request * req)
{
    struct request * tmp;

    req->next = NULL;
    cli();
    if (req->bh) {
        // 如果请求关联了缓冲区，清除其脏标志
        req->bh->b_dirt = 0;    // 因为请求即将被处理，数据将写入磁盘，缓冲区将变干净
    }
    if (!(tmp = dev->current_request)) {    // 如果设备当前没有处理中的请求
        dev->current_request = req;            // 直接将新请求设为当前请求
        sti();
        (dev->request_fn)();        // 立即调用设备请求处理函数开始I/O操作
        return;
    }
    for ( ; tmp->next ; tmp=tmp->next)    //在请求队列中寻找合适的插入位置
        if ((IN_ORDER(tmp,req) ||
            !IN_ORDER(tmp,tmp->next)) &&
            IN_ORDER(req,tmp->next))
            break;
    req->next = tmp->next;    // 将新请求插入到找到的位置
    tmp->next = req;
    sti();
}

/*
 * 块设备I/O请求创建的核心函数，负责将缓冲区操作转换为设备请求，并管理有限的请求队列资源
 * args:
 *     - major：主设备号
 *     - rw：操作类型
 *     - bh：缓冲区头
 */
static void make_request(int major,int rw, struct buffer_head * bh)
{
    struct request * req;
    int rw_ahead;    // 标记是否为预读/预写操作

    /* WRITEA/READA is special case - it is not really needed, so if the */
    /* buffer is locked, we just forget about it, else it's a normal read */
    if (rw_ahead = (rw == READA || rw == WRITEA)) {
        if (bh->b_lock) {
            // 如果缓冲区已锁定，直接放弃预读/预写（非必需操作）
            return;
        }
        if (rw == READA) {
            rw = READ;
        } else {
            rw = WRITE;
        }
    }
    if (rw!=READ && rw!=WRITE) {
        panic("Bad block dev command, must be R/W/RA/WA");
    }
    lock_buffer(bh);    // 锁定缓冲区，防止其他进程操作
    if ((rw == WRITE && !bh->b_dirt) || (rw == READ && bh->b_uptodate)) {
        unlock_buffer(bh);
        return;
    }
repeat:
    /* we don't allow the write-requests to fill up the queue completely:
     * we want some room for reads: they take precedence. The last third
     * of the requests are only for reads.
     */
    if (rw == READ) {
        req = request+NR_REQUEST;
    } else {
        req = request+((NR_REQUEST*2)/3);    // 不允许写请求完全占满队列，保留1/3空间给读请求
    }

    /* 查找一个空闲请求(从后向前搜索请求数组) */
    while (--req >= request) {
        if (req->dev<0) {
            break;
        }
    }

    /*
     * 如果没找到空闲请求槽（req < request）
     * 对于预读/预写：直接放弃，解锁缓冲区返回
     * 对于普通操作：睡眠等待空闲请求，被唤醒后重试
     */
    if (req < request) {
        if (rw_ahead) {
            unlock_buffer(bh);
            return;
        }
        sleep_on(&wait_for_request);
        goto repeat;
    }
    /* 填充request请求信息 */
    req->dev = bh->b_dev;
    req->cmd = rw;
    req->errors=0;
    req->sector = bh->b_blocknr<<1;
    req->nr_sectors = 2;
    req->buffer = bh->b_data;
    req->waiting = NULL;
    req->bh = bh;
    req->next = NULL;
    add_request(major+blk_dev,req);    // 将请求添加到指定设备的请求队列
}

/*
 * 负责将缓冲区读写请求提交给相应的块设备驱动程序
 * args:
 *     - rw：读写操作类型
 *     - bh：缓冲区头指针,包含要读写的块信息
 */
void ll_rw_block(int rw, struct buffer_head * bh)
{
    unsigned int major;

    if ((major=MAJOR(bh->b_dev)) >= NR_BLK_DEV ||
    !(blk_dev[major].request_fn)) {    // 检查主设备号是否超出范围以及是否注册了请求处理函数request_fn
        printk("Trying to read nonexistent block-device\n\r");
        return;
    }
    make_request(major,rw,bh);    // 创建并提交I/O请求
}

/*
 * 块设备请求队列的初始化函数
 */
void blk_dev_init(void)
{
    int i;

    for (i = 0; i < NR_REQUEST; i++) {
        request[i].dev = -1;
        request[i].next = NULL;
    }
}