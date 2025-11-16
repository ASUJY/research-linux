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

#define IS_SEEKABLE(x) ((x)>=1 && (x)<=3)

#define READ 0
#define WRITE 1
#define READA 2		/* read-ahead - don't pause */
#define WRITEA 3	/* "write-ahead" - silly, but somewhat useful */

void buffer_init(long buffer_end);

#define MAJOR(a) (((unsigned)(a))>>8)
#define MINOR(a) ((a)&0xff)

#define NAME_LEN 14
#define ROOT_INO 1  // 文件系统根inode所在位置

#define I_MAP_SLOTS 8
#define Z_MAP_SLOTS 8
#define SUPER_MAGIC 0x137F

#define NR_OPEN 20
#define NR_INODE 32
#define NR_FILE 64
#define NR_SUPER 8
#define NR_HASH 307
#define NR_BUFFERS nr_buffers
#define BLOCK_SIZE 1024
#define BLOCK_SIZE_BITS 10

#ifndef NULL
#define NULL ((void *)0)
#endif

#define INODES_PER_BLOCK ((BLOCK_SIZE)/(sizeof (struct d_inode)))
#define DIR_ENTRIES_PER_BLOCK ((BLOCK_SIZE)/(sizeof (struct dir_entry)))

#define PIPE_HEAD(inode) ((inode).i_zone[0])
#define PIPE_TAIL(inode) ((inode).i_zone[1])
#define PIPE_SIZE(inode) ((PIPE_HEAD(inode)-PIPE_TAIL(inode))&(PAGE_SIZE-1))

/*
 * 缓冲区头（buffer_head）是内核用于管理磁盘块缓存的核心数据结构，
 * 它描述了缓存块的状态、数据以及它在各种链表中的位置。
 */
struct buffer_head {
    char * b_data;		       /* 指向当前缓冲区(存放数据)的实际位置 */
    unsigned long b_blocknr;	       /* 块号(在磁盘中的位置) */
    unsigned short b_dev;	       /* 设备号 (0 = free) */
    unsigned char b_uptodate;	       /* 数据是否最新，1-最新 */
    unsigned char b_dirt;	       /* 脏标志,0-clean,1-dirty */
    unsigned char b_count;	       /* 引用计数，记录有多少进程正在使用该缓冲区 */
    unsigned char b_lock;	       /* 锁定标志, 0 - ok, 1 -locked */
    struct task_struct * b_wait;       /* 等待当前缓冲区的进程队列 */
    struct buffer_head * b_prev;       /* 用于哈希表链表，快速查找缓冲区 */
    struct buffer_head * b_next;
    struct buffer_head * b_prev_free;  /* 用于空闲链表，管理空闲缓冲区 */
    struct buffer_head * b_next_free;
};

/*
 * 磁盘中的inode结构
 */
struct d_inode {
  unsigned short i_mode;    // 文件类型和权限位
  unsigned short i_uid;     // 文件所有者用户ID
  unsigned long i_size;     // 文件大小（字节）
  unsigned long i_time;     // 文件最后修改时间
  unsigned char i_gid;      // 文件所属组ID
  unsigned char i_nlinks;   // 硬链接计数
  unsigned short i_zone[9]; // 数据块指针数组
};

/*
 * 内存中的inode结构
 */
struct m_inode {
    unsigned short i_mode;        // 文件类型和权限位
    unsigned short i_uid;         // 文件所有者用户ID
    unsigned long i_size;         // 文件大小（字节）
    unsigned long i_mtime;        // 文件最后修改时间
    unsigned char i_gid;          // 文件所属组ID
    unsigned char i_nlinks;       // 硬链接计数
    unsigned short i_zone[9];     // 数据块指针数组
    /* these are in memory also */
    struct task_struct *i_wait;   // 等待当前inode的进程队列
    unsigned long i_atime;        // 文件最后访问时间（内存维护）
    unsigned long i_ctime;        // 文件状态改变时间（内存维护）
    unsigned short i_dev;         // 设备号
    unsigned short i_num;         // inode编号
    unsigned short i_count;       // 引用计数，记录有多少进程正在使用当前inode
    unsigned char i_lock;         // 锁定标志，防止并发修改
    unsigned char i_dirt;         // 脏标志，1表示需要写回磁盘
    unsigned char i_pipe;         // 管道标志，1表示这是管道inode
    unsigned char i_mount;        // 挂载点标志，1表示这是挂载点
    unsigned char i_seek;         // seek操作标志
    unsigned char i_update;       // 文件更新时间标志
};

struct file {
    unsigned short f_mode;        // 文件访问模式（读、写、读写）
    unsigned short f_flags;       // 文件打开标志（O_RDONLY, O_WRONLY, O_CREAT等）
    unsigned short f_count;       // 引用计数，多个文件描述符可能指向同一个file结构
    struct m_inode *f_inode;      // 指向对应的内存inode
    off_t f_pos;                  // 当前文件读写位置
};

/*
 * 内存中的超级块结构
 */
struct super_block {
  unsigned short s_ninodes;        // inode总数
  unsigned short s_nzones;         // 数据块总数
  unsigned short s_imap_blocks;    // inode位图占用的块数
  unsigned short s_zmap_blocks;    // 块位图占用的块数
  unsigned short s_firstdatazone;  // 第一个数据块(逻辑块)号
  unsigned short s_log_zone_size;  // Log2^(数据块数/逻辑块)（用于计算实际块大小）
  unsigned long s_max_size;        // 最大文件长度
  unsigned short s_magic;          // 文件系统魔数，标识文件系统类型
  /* These are only in memory */
  struct buffer_head * s_imap[8];   // inode位图缓冲区指针数组（每个位图块管理8192个inode）
  struct buffer_head * s_zmap[8];   // 块位图缓冲区指针数组（每个位图块管理8192个块）
  unsigned short s_dev;             // 设备号
  struct m_inode * s_isup;          // 指向被挂载文件系统的根inode
  struct m_inode * s_imount;        // 指向挂载点inode
  unsigned long s_time;             // 超级块最后修改时间
  struct task_struct * s_wait;      // 等待当前超级块的进程队列
  unsigned char s_lock;             // 锁定标志
  unsigned char s_rd_only;          // 只读标志
  unsigned char s_dirt;             // 脏标志
};

/*
 * 磁盘中的超级块结构
 */
struct d_super_block {
  unsigned short s_ninodes;        // inode总数
  unsigned short s_nzones;         // 数据块总数
  unsigned short s_imap_blocks;    // inode位图占用的块数
  unsigned short s_zmap_blocks;    // 块位图占用的块数
  unsigned short s_firstdatazone;  // 第一个数据块(逻辑块)号
  unsigned short s_log_zone_size;  // Log2^(数据块数/逻辑块)（用于计算实际块大小）
  unsigned long s_max_size;        // 最大文件长度
  unsigned short s_magic;          // 文件系统魔数，标识文件系统类型
};

struct dir_entry {
 unsigned short inode;  // 关联的inode编号
 char name[NAME_LEN];   // 文件名
};

extern struct m_inode inode_table[NR_INODE];
extern struct file file_table[NR_FILE];
extern struct super_block super_block[NR_SUPER];
extern struct buffer_head * start_buffer;
extern int nr_buffers;

extern void check_disk_change(int dev);
extern int floppy_change(unsigned int nr);
extern int ticks_to_floppy_on(unsigned int dev);
extern void floppy_on(unsigned int dev);
extern void floppy_off(unsigned int dev);
extern void truncate(struct m_inode * inode);
extern void sync_inodes(void);
extern int bmap(struct m_inode * inode,int block);
extern int create_block(struct m_inode * inode,int block);
extern struct m_inode * namei(const char * pathname);
extern int open_namei(const char * pathname, int flag, int mode,
 struct m_inode ** res_inode);
extern void iput(struct m_inode * inode);
extern struct m_inode * iget(int dev,int nr);
extern struct m_inode * get_empty_inode(void);
extern struct m_inode * get_pipe_inode(void);
extern struct buffer_head * get_hash_table(int dev, int block);
extern struct buffer_head * getblk(int dev, int block);
extern void ll_rw_block(int rw, struct buffer_head * bh);
extern void brelse(struct buffer_head * buf);
extern struct buffer_head * bread(int dev,int block);
extern void bread_page(unsigned long addr,int dev,int b[4]);
extern struct buffer_head * breada(int dev,int block,...);
extern int new_block(int dev);
extern void free_block(int dev, int block);
extern struct m_inode * new_inode(int dev);
extern void free_inode(struct m_inode * inode);
extern int sync_dev(int dev);
extern struct super_block * get_super(int dev);
extern void put_super(int);
extern void invalidate_inodes(int);
extern int ROOT_DEV;

extern void mount_root(void);

#endif //FS_H
