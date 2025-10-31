//
// Created by asujy on 2025/10/29.
//

#include <sys/stat.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>

struct m_inode inode_table[NR_INODE]={{0,},};

static void read_inode(struct m_inode * inode);
static void write_inode(struct m_inode * inode);

static inline void lock_inode(struct m_inode * inode)
{
    cli();
    while (inode->i_lock) {
        sleep_on(&inode->i_wait);
    }
    inode->i_lock = 1;
    sti();
}

static inline void unlock_inode(struct m_inode * inode)
{
    inode->i_lock = 0;
    wake_up(&inode->i_wait);
}

static inline void wait_on_inode(struct m_inode * inode)
{
    cli();
    while (inode->i_lock) {
        sleep_on(&inode->i_wait);
    }
    sti();
}

/*
 * 释放指定设备的所有inode节点。
 * 通常在设备移除或文件系统卸载时调用。
 */
void invalidate_inodes(int dev)
{
    int i;
    struct m_inode * inode;

    inode = 0 + inode_table;
    for (i = 0; i < NR_INODE; i++, inode++) {
        wait_on_inode(inode);
        if (inode->i_dev == dev) {
            if (inode->i_count) {
                printk("inode in use on removed disk\n\r");
            }
            inode->i_dev = inode->i_dirt = 0;   // 释放inode节点(将 inode 的设备号和脏位都清零)
        }
    }
}

/*
 * 同步所有内存中脏 inode 到磁盘，确保文件系统数据的一致性。
 */
void sync_inodes(void)
{
    int i;
    struct m_inode * inode;

    inode = 0 + inode_table;
    for (i = 0; i < NR_INODE; i++, inode++) {
        wait_on_inode(inode);
        if (inode->i_dirt && !inode->i_pipe) {
            write_inode(inode);
        }
    }
}

/*
 * 释放指定的inode节点(数据写入磁盘)
 */
void iput(struct m_inode * inode) {
    if (!inode) {
        return;
    }
    wait_on_inode(inode);   // 等待 inode 解锁，确保没有其他进程正在使用该 inode
    if (!inode->i_count) {
        panic("iput: trying to free free inode");
    }
    if (inode->i_pipe) {    // 处理管道类型的inode节点
        wake_up(&inode->i_wait);    // 唤醒等待该管道的进程
        if (--inode->i_count) {
            return;
        }
        free_page(inode->i_size);   // 释放管道占用的内存页
        inode->i_count = 0;
        inode->i_dirt = 0;
        inode->i_pipe = 0;
        return;
    }
    if (!inode->i_dev) {
        inode->i_count--;   // 无效设备处理: 如果 inode 没有关联的设备，只减少引用计数并返回
        return;             // 处理已释放或未初始化的 inode
    }
    if (S_ISBLK(inode->i_mode)) {
        sync_dev(inode->i_zone[0]); // 如果是块设备文件的inode节点，同步对应的设备(i_zone[0] 存储块设备的主设备号)
        wait_on_inode(inode);
    }
repeat:
    if (inode->i_count > 1) {
        inode->i_count--;           // 如果引用计数大于1，只需减少计数并返回
        return;
    }
    if (!inode->i_nlinks) {         // 如果链接计数为0（文件已被删除）,则释放该inode节点的所有逻辑块
        truncate(inode);            // 调用 truncate() 释放文件数据块
        free_inode(inode);          // 调用 free_inode() 释放 inode 本身
        return;
    }
    if (inode->i_dirt) {            // 如果 inode 是脏的（需要写回磁盘）
        write_inode(inode);	    // 调用 write_inode() 写回磁盘
        wait_on_inode(inode);
        goto repeat;                // 跳转到 repeat 重新检查状态（因为写操作期间状态可能改变）
    }
    inode->i_count--;
    return;
}

/*
 * 从 inode表 中获取一个空闲的inode节点项
 */
struct m_inode * get_empty_inode(void) {
    struct m_inode * inode;
    static struct m_inode * last_inode = inode_table;
    int i;

    do {
        inode = NULL;
        for (i = NR_INODE; i; i--) {
            if (++last_inode >= inode_table + NR_INODE) {
                last_inode = inode_table;
            }
            if (!last_inode->i_count) {
                inode = last_inode;
                if (!inode->i_dirt && !inode->i_lock) {
                    break;
                }
            }
        }
        if (!inode) {
            for (i = 0; i < NR_INODE; i++) {
                printk("%04x: %6d\t",inode_table[i].i_dev,
                        inode_table[i].i_num);
            }
            panic("No free inodes in mem");
        }
        wait_on_inode(inode);       // 等待找到的 inode 解锁
        while (inode->i_dirt) {
            write_inode(inode);     // 如果 inode 是脏的，将其写回磁盘
            wait_on_inode(inode);
        }
    } while (inode->i_count);
    memset(inode, 0, sizeof(*inode));   // 初始化inode，将 inode 结构体清零
    inode->i_count = 1;                 // 设置引用计数，表示已被使用
    return inode;
}

/*
 * 获取inode(增加引用计数)
 * 从 inode 缓存中获取指定设备号和 inode 编号的 inode 节点；
 * 实现了 inode 缓存查找、挂载点处理和缓存未命中时的磁盘读取。
 * args:
 *  - dev: 设备号
 *  - nr: inode编号
 */
struct m_inode * iget(int dev, int nr) {
    struct m_inode * inode;
    struct m_inode * empty;

    if (!dev) {
        panic("iget with dev==0");
    }
    empty = get_empty_inode();  // 获取一个空闲 inode 槽位
    inode = inode_table;
    /*
     * 遍历 inode 表，查找设备号和 inode 编号都匹配的 inode节点
     */
    while (inode < NR_INODE + inode_table) {
        if (inode->i_dev != dev || inode->i_num != nr) {
            inode++;
            continue;
        }
        wait_on_inode(inode);
        if (inode->i_dev != dev || inode->i_num != nr) {
            inode = inode_table;
            continue;
        }
        inode->i_count++;
        if (inode->i_mount) { // 如果该 inode 是挂载点（其它文件系统的安装点）
            int i;
            /* 遍历超级块数组，找到挂载在该 inode 上的文件系统 */
            for (i = 0; i < NR_SUPER; i++) {
                if (super_block[i].s_imount == inode){
                    break;
                }
            }
            if (i >= NR_SUPER) {    // 挂载点错误,找不到对应的超级块
                printk("Mounted inode hasn't got sb\n");
                if (empty) {
                    iput(empty);    // 释放预分配的空闲 inode
                }
                return inode;
            }
            /* 挂载点处理 */
            iput(inode);    // 释放当前 inode 的引用
            /* 切换到被挂载文件系统的设备号和根 inode 编号 */
            dev = super_block[i].s_dev;
            nr = ROOT_INO;
            inode = inode_table;
            continue;
        }
        if (empty) {    // 已经找到相应的inode节点，所以放弃临时申请的inode空闲槽位
            iput(empty);
        }
        return inode;
    }
    if (!empty) {
        return (NULL);
    }
    /* 如果在inode表中没有找到指定的inode节点，则利用前面申请的空闲inode节点
     * 在inode表中建立该节点，并从相应的设备上湖区该inode节点的信息
     */
    inode = empty;
    inode->i_dev = dev;
    inode->i_num = nr;
    read_inode(inode);

    return inode;   // 返回新读取的 inode
}

/*
 * 从设备(硬盘)上读取指定inode节点的信息到内存中(高速缓冲区)
 * 是文件系统 inode 缓存机制的组成部分
 */
static void read_inode(struct m_inode * inode)
{
    struct super_block * sb;
    struct buffer_head * bh;
    int block;

    lock_inode(inode);  // 对 inode 加锁，防止并发读取造成数据不一致
    if (!(sb = get_super(inode->i_dev))) {
        panic("trying to read inode without dev");
    }
    block = 2 + sb->s_imap_blocks + sb->s_zmap_blocks +
            (inode->i_num-1)/INODES_PER_BLOCK;  // 计算 inode节点 在磁盘中的位置
    if (!(bh = bread(inode->i_dev, block))) {   // 从磁盘读取包含目标 inode 的块
        panic("unable to read i-node block");
    }
    *(struct d_inode *)inode =
            ((struct d_inode *)bh->b_data)
                    [(inode->i_num-1)%INODES_PER_BLOCK];    // 将磁盘上的 inode 数据复制到内存 inode 结构
    brelse(bh);
    unlock_inode(inode);
}

/*
 * 将内存中的 inode 写回磁盘的函数，负责 inode 的持久化操作
 * (写入缓冲区相应的缓冲块中，待缓冲区刷新时会写入磁盘中)
 */
static void write_inode(struct m_inode * inode)
{
    struct super_block * sb;
    struct buffer_head * bh;
    int block;

    lock_inode(inode);
    if (!inode->i_dirt || !inode->i_dev) {  // 检查 inode 是否脏（需要写回）以及 是否有有效的设备号
        unlock_inode(inode);
        return;
    }
    if (!(sb = get_super(inode->i_dev))) {  // 根据设备号获取对应的超级块
        panic("trying to write inode without device");
    }
    block = 2 + sb->s_imap_blocks + sb->s_zmap_blocks +
            (inode->i_num - 1) / INODES_PER_BLOCK;  // 计算 inode 位置
    if (!(bh = bread(inode->i_dev, block))) {       // 从磁盘读取包含目标 inode 的块
        panic("unable to read i-node block");
    }
    ((struct d_inode *)bh->b_data)[(inode->i_num - 1) % INODES_PER_BLOCK] =
                    *(struct d_inode *)inode;       // 将内存中的 inode 数据复制到缓冲区的正确位置
    bh->b_dirt = 1;     // 标记缓冲区脏: 表示缓冲区内容已修改，需要写回磁盘
    inode->i_dirt = 0;  //  inode 数据已同步到缓冲区，清除脏标记
    brelse(bh);
    unlock_inode(inode);
}