//
// Created by asujy on 2025/7/16.
//

#ifndef FS_H
#define FS_H

#include <sys/types.h>

/* devices are as follows: (same as minix, so we can use the minix
 * file system. These are major numbers.)
 *
 * 0 - unused (nodev)
 * 1 - /dev/mem
 * 2 - /dev/fd
 * 3 - /dev/hd
 * 4 - /dev/ttyx
 * 5 - /dev/tty
 * 6 - /dev/lp
 * 7 - unnamed pipes
 */

#define READ 0
#define WRITE 1
#define READA 2		/* read-ahead - don't pause */
#define WRITEA 3	/* "write-ahead" - silly, but somewhat useful */

void buffer_init(long buffer_end);

#define MAJOR(a) (((unsigned)(a))>>8)
#define MINOR(a) ((a)&0xff)

#define NR_OPEN 20
#define NR_HASH 307
#define BLOCK_SIZE 1024

#ifndef NULL
#define NULL ((void *)0)
#endif

/*
 * 缓冲区头（buffer_head）是内核用于管理磁盘块缓存的核心数据结构，
 * 它描述了缓存块的状态、数据以及它在各种链表中的位置。
 */
struct buffer_head {
    char * b_data;				/* pointer to data block (1024 bytes) */
    unsigned long b_blocknr;	/* 块号 */
    unsigned short b_dev;		/* 设备号 (0 = free) */
    unsigned char b_uptodate;	/* 数据是否最新 */
    unsigned char b_dirt;		/* 脏标志,0-clean,1-dirty */
    unsigned char b_count;		/* users using this block */
    unsigned char b_lock;		/* 锁定标志, 0 - ok, 1 -locked */
    struct task_struct * b_wait;
    struct buffer_head * b_prev;
    struct buffer_head * b_next;
    struct buffer_head * b_prev_free;
    struct buffer_head * b_next_free;
};


struct m_inode {
    unsigned short i_mode;
    unsigned short i_uid;
    unsigned long i_size;
    unsigned long i_mtime;
    unsigned char i_gid;
    unsigned char i_nlinks;
    unsigned short i_zone[9];
    /* these are in memory also */
    struct task_struct *i_wait;
    unsigned long i_atime;
    unsigned long i_ctime;
    unsigned short i_dev;
    unsigned short i_num;
    unsigned short i_count;
    unsigned char i_lock;
    unsigned char i_dirt;
    unsigned char i_pipe;
    unsigned char i_mount;
    unsigned char i_seek;
    unsigned char i_update;
};

struct file {
    unsigned short f_mode;
    unsigned short f_flags;
    unsigned short f_count;
    struct m_inode *f_inode;
    off_t f_pos;
};

extern struct buffer_head * get_hash_table(int dev, int block);
extern struct buffer_head * getblk(int dev, int block);
extern void ll_rw_block(int rw, struct buffer_head * bh);
extern void brelse(struct buffer_head * buf);
extern struct buffer_head * bread(int dev,int block);
extern int sync_dev(int dev);

#endif //FS_H
