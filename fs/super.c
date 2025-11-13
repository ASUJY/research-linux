//
// Created by asujy on 2025/10/29.
//

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>

void wait_for_keypress(void);

/* 测试内存中addr处的比特位的值，并返回该比特位值 */
#define set_bit(bitnr,addr) ({ \
register int __res __asm__("ax"); \
__asm__("bt %2,%3;setb %%al":"=a" (__res):"a" (0),"r" (bitnr),"m" (*(addr))); \
__res; })

struct super_block super_block[NR_SUPER];
int ROOT_DEV = 0;   // 根文件系统的设备号

static void lock_super(struct super_block * sb)
{
    cli();
    while (sb->s_lock) {
        sleep_on(&(sb->s_wait));
    }
    sb->s_lock = 1;
    sti();
}

static void free_super(struct super_block * sb)
{
    cli();
    sb->s_lock = 0;
    wake_up(&(sb->s_wait));
    sti();
}

static void wait_on_super(struct super_block * sb)
{
    cli();
    while (sb->s_lock) {
        sleep_on(&(sb->s_wait));
    }
    sti();
}

/*
 * 根据设备号，在super_block中获取一个超级块
 */
struct super_block * get_super(int dev) {
    struct super_block * s;

    if (!dev) {
        return NULL;
    }
    s = 0 + super_block;
    while (s < NR_SUPER + super_block) {
        if (s->s_dev == dev) {
            wait_on_super(s);
            if (s->s_dev == dev) {
                return s;
            }
            s = 0 + super_block;
        } else {
            s++;
        }
    }
    return NULL;

}

/*
 * 释放和清理指定设备的超级块
 * 释放设备所使用的超级块数组项（置s_dev=0），并释放该设备i节点位图和逻辑块位图所占用的高速缓冲块。
 * 如果超级块对应的文件系统是根文件系统，或者其i节点上己经安装有其它的文件系统，则不能释放该超级块。
 */
void put_super(int dev) {
    struct super_block * sb;
    struct m_inode * inode;
    int i;

    if (dev == ROOT_DEV) {  // 如果尝试释放根设备，打印警告信息并直接返回
        printk("root diskette changed: prepare for armageddon\n\r");
        return;
    }
    if (!(sb = get_super(dev))) {
        return;
    }
    if (sb->s_imount) {     // 检查超级块是否被挂载
        printk("Mounted disk changed - tssk, tssk\n\r");
        return;
    }
    lock_super(sb);
    sb->s_dev = 0;
    for (i = 0; i < I_MAP_SLOTS; i++) {
        brelse(sb->s_imap[i]);  // 释放inode位图缓冲区
    }
    for (i = 0; i < Z_MAP_SLOTS; i++) {
        brelse(sb->s_zmap[i]);  // 释放块位图缓冲区
    }
    free_super(sb);
    return;
}

/*
 * 从指定设备上读取并初始化超级块信息，是文件系统挂载过程中的核心组成部分
 */
static struct super_block * read_super(int dev) {
    struct super_block * s;
    struct buffer_head * bh;
    int i;
    int block;

    if (!dev) {
        return NULL;
    }
    check_disk_change(dev);
    if (s = get_super(dev)) {   // 如果该设备的超级块已经在高速缓冲区中，则直接返回超级块指针
        return s;
    }
    for (s = 0 + super_block; ; s++) {  // 在超级块数组中找出一个空闲槽位
        if (s >= NR_SUPER + super_block) {
            return NULL;
        }
        if (!s->s_dev) {
            break;
        }
    }
    /* 初始化超级块字段 */
    s->s_dev = dev;
    s->s_isup = NULL;
    s->s_imount = NULL;
    s->s_time = 0;
    s->s_rd_only = 0;
    s->s_dirt = 0;
    lock_super(s);
    /* 读取超级块数据 */
    if (!(bh = bread(dev, 1))) {
        s->s_dev = 0;
        free_super(s);
        return NULL;
    }
    // 将设备上读取的超级块信息复制到超级块数组相应的结构中
    *((struct d_super_block *) s) = *((struct d_super_block *) bh->b_data);
    brelse(bh); // 释放存放超级块信息的高速缓冲块

    /*
     * 验证超级块魔数是否正确
     * 如果读取的超级块的文件系统魔数字段内容不对，则说明设备上的文件系统有问题
     * 目前只支持minix文件系统1.0版本，魔数为0x137f
     */
    if (s->s_magic != SUPER_MAGIC) {
        s->s_dev = 0;
        free_super(s);
        return NULL;
    }
    /* 将inode位图和块位图指针数组初始化为NULL */
    for (i = 0; i < I_MAP_SLOTS; i++) {
        s->s_imap[i] = NULL;
    }
    for (i = 0; i < Z_MAP_SLOTS; i++) {
        s->s_zmap[i] = NULL;
    }
    /* 读取inode位图,从块2开始读取inode位图 */
    block=2;
    for (i = 0; i < s->s_imap_blocks; i++) {
        if (s->s_imap[i] = bread(dev, block)) {
            block++;
        } else {
            break;
        }
    }
    /* 读取块位图 */
    for (i = 0; i < s->s_zmap_blocks; i++) {
        if (s->s_zmap[i] = bread(dev, block)) {
            block++;
        } else {
            break;
        }
    }
    /*
     * 验证读取的总块数是否正确
     * 如果不正确，释放所有已读取的位图缓冲区，清理超级块并返回NULL
     */
    if (block != 2 + s->s_imap_blocks + s->s_zmap_blocks) {
        for (i = 0; i < I_MAP_SLOTS; i++) {
            brelse(s->s_imap[i]);
        }
        for (i = 0; i < Z_MAP_SLOTS; i++) {
            brelse(s->s_zmap[i]);
        }
        s->s_dev = 0;
        free_super(s);
        return NULL;
    }
    /* 设置inode位图和块位图的第0位（通常保留不用） */
    s->s_imap[0]->b_data[0] |= 1;
    s->s_zmap[0]->b_data[0] |= 1;
    free_super(s);
    return s;
}

int sys_umount(char * dev_name) {

}

int sys_mount(char * dev_name, char * dir_name, int rw_flag) {

}

/*
 * 挂载根文件系统（安装根文件系统）
 * 在系统启动过程中初始化根文件系统，设置进程的工作目录和根目录。
 */
void mount_root(void)
{
    int i;
    int free;
    struct super_block * p;
    struct m_inode * mi;

    /*
     * 验证磁盘 inode 结构体大小是否为32字节
     */
    if (32 != sizeof(struct d_inode)) {
        panic("bad i-node size");
    }
    /* 初始化文件表，将所有文件表项的引用计数清零（系统同时只能打开64个文件） */
    for (i = 0; i < NR_FILE; i++) {
        file_table[i].f_count = 0;
    }

    if (MAJOR(ROOT_DEV) == 2) { /* 如果根设备是软盘（主设备号2），提示用户插入磁盘。 */
        printk("Insert root floppy and press ENTER");
        wait_for_keypress();
    }

    /* 清零所有超级块槽位，标记为未使用状态 */
    for (p = &super_block[0]; p < &super_block[NR_SUPER]; p++) {
        p->s_dev = 0;
        p->s_lock = 0;
        p->s_wait = NULL;
    }

    /* 读取根文件系统超级块，即从根设备读取文件系统超级块 */
    if (!(p = read_super(ROOT_DEV))) {
        panic("Unable to mount root");
    }
    /* 获取根目录的i-node */
    if (!(mi = iget(ROOT_DEV, ROOT_INO))) {
        panic("Unable to read root i-node");
    }

    /* 增加引用计数：根i-node被多个地方引用：
     * - 超级块的s_isup
     * - 超级块的s_imount
     * - 当前进程的工作目录
     * - 当前进程的根目录
     */
    mi->i_count += 3 ;	/* NOTE! it is logically used 4 times, not 1 */

    /* 建立挂载关系 */
    p->s_isup = p->s_imount = mi;   // 设置超级块的挂载i-node
    current->pwd = mi;              // 设置当前进程（1号进程）的工作目录和根目录为根 inode
    current->root = mi;
    /* 统计空闲数据块：遍历块位图，统计未使用的数据块数量。 统计空闲i-node：遍历i-node位图，统计未使用的i-node数量。*/
    free = 0;
    i = p->s_nzones;
    while (--i >= 0) {
        if (!set_bit(i&8191, p->s_zmap[i>>13]->b_data)) {
            free++;
        }
    }
    printk("%d/%d free blocks\n\r", free, p->s_nzones);
    free = 0;
    i = p->s_ninodes + 1;
    while (--i >= 0) {
        if (!set_bit(i&8191, p->s_imap[i>>13]->b_data)) {
            free++;
        }
    }
    printk("%d/%d free inodes\n\r", free, p->s_ninodes);
}