//
// Created by asujy on 2025/10/29.
//

#include <string.h>
#include <sys/stat.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>

struct m_inode inode_table[NR_INODE]={{0,},};   // 内存中的inode表

static void read_inode(struct m_inode * inode);
static void write_inode(struct m_inode * inode);

static inline void wait_on_inode(struct m_inode * inode)
{
    cli();
    while (inode->i_lock) {
        sleep_on(&inode->i_wait);
    }
    sti();
}

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

/*
 * 释放内存中的inode表 中的 指定设备的所有inode节点。
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
 * 正规文件中的数据是放在磁盘块的数据区中的，而一个文件名则通过对应的inode节点与这些数据磁盘块相联系，
 * 这些盘块的号码就存放在inode节点的数据块数组中。
 *
 * 文件数据块映射到硬盘中的数据块。
 * 文件块映射函数,实现了Unix文件系统的经典多级索引结构。
 * 将文件内的数据块号映射到磁盘上的物理块号，支持直接块、一级间接块和二级间接块。
 * 主要是对inode的数据块数组i_zone[]进行处理，并根据i_zone[]中所设置的数据块号(盘块号)
 * 来设置数据块位图的占用情况。
 * args:
 *  - inode: 描述文件的inode
 *  - block: 文件内的数据块号
 *  - create: 是否创建新块的标志
 * return: 返回文件中block数据块对应在设备上的数据块号
 */
static int _bmap(struct m_inode * inode, int block, int create)
{
    struct buffer_head * bh;
    int i;
    /*
     * 检查块号是否小于0以及是否超过最大支持范围
     * （7个直接块 + 512个一级间接块 + 512×512个二级间接块）
     */
    if (block < 0) {
        panic("_bmap: block<0");
    }
    if (block >= 7 + 512 + 512 * 512) {
        panic("_bmap: block>big");
    }
    /* 处理 直接块 (前7个块使用直接映射)*/
    if (block < 7) {
        if (create && !inode->i_zone[block]) {  // 如果需要创建且块未分配，分配新块
            if (inode->i_zone[block] = new_block(inode->i_dev)) {
                inode->i_ctime = CURRENT_TIME;
                inode->i_dirt = 1;      // 更新inode的修改时间和脏标志
            }
        }
        return inode->i_zone[block];
    }
    /* 一级间接块处理 (block 7-518)
     * 一级间接块映射：
     *  块号7指向一个间接块，包含512个块指针
     *  如果需要创建且间接块不存在，分配新的间接块
     *  读取间接块内容到对应的缓冲区中
     *  在间接块中查找block对应的数据块，如果需要创建且数据块不存在，分配新数据块
     */
    block -= 7;
    if (block < 512) {
        if (create && !inode->i_zone[7]) {
            if (inode->i_zone[7] = new_block(inode->i_dev)) {
                inode->i_dirt = 1;
                inode->i_ctime = CURRENT_TIME;
            }
        }
        if (!inode->i_zone[7]) {
            return 0;
        }
        if (!(bh = bread(inode->i_dev, inode->i_zone[7]))) {
            return 0;
        }
        i = ((unsigned short *) (bh->b_data))[block];
        if (create && !i) {
            if (i = new_block(inode->i_dev)) {
                ((unsigned short *) (bh->b_data))[block] = i;
                bh->b_dirt = 1;
            }
        }
        brelse(bh);
        return i;
    }
    /*
     * 二级间接块处理 (block >= 519)，与一级间接块类似
     * 二级间接块映射：
     *  块号8指向二级间接块
     *  第一级：block >> 9（除以512）得到一级索引
     *  第二级：block & 511（模512）得到二级索引
     *  两级分配都需要检查并创建必要的间接块
     */
    block -= 512;
    if (create && !inode->i_zone[8]) {
        if (inode->i_zone[8] = new_block(inode->i_dev)) {
            inode->i_dirt = 1;
            inode->i_ctime = CURRENT_TIME;
        }
    }
    if (!inode->i_zone[8]) {
        return 0;
    }
    if (!(bh = bread(inode->i_dev, inode->i_zone[8]))) {
        return 0;
    }
    i = ((unsigned short *)bh->b_data)[block >> 9];
    if (create && !i) {
        if (i = new_block(inode->i_dev)) {
            ((unsigned short *) (bh->b_data))[block >> 9] = i;
            bh->b_dirt = 1;
        }
    }
    brelse(bh);
    if (!i) {
        return 0;
    }
    if (!(bh = bread(inode->i_dev, i))) {
        return 0;
    }
    i = ((unsigned short *)bh->b_data)[block & 511];
    if (create && !i) {
        if (i = new_block(inode->i_dev)) {
            ((unsigned short *) (bh->b_data))[block & 511] = i;
            bh->b_dirt = 1;
        }
    }
    brelse(bh);
    return i;
}

/*
 * 专门用于查询文件数据块到物理块的映射关系，但不会创建新块。
 * 根据inode的信息获取文件数据块block在设备上对应的数据块号
 */
int bmap(struct m_inode * inode, int block)
{
    return _bmap(inode, block, 0);
}

/*
 * 用于为文件的指定数据块分配物理磁盘块
 * 创建文件数据块block在设备上对应的数据块，并返回设备上对应的数据块号
 */
int create_block(struct m_inode * inode, int block)
{
    return _bmap(inode, block, 1);
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
    /* 处理管道类型的inode节点 */
    if (inode->i_pipe) {
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
        return;
    }
    /* 处理块设备类型的inode */
    if (S_ISBLK(inode->i_mode)) {
        sync_dev(inode->i_zone[0]); // 如果是块设备文件的inode节点，同步数据到对应的设备(i_zone[0] 存储块设备的主设备号)
        wait_on_inode(inode);
    }
repeat:
    if (inode->i_count > 1) {
        inode->i_count--;           // 如果引用计数大于1，减少计数并返回
        return;
    }
    if (!inode->i_nlinks) {         // 如果inode的链接计数为0（文件已被删除）,则释放该inode节点的所有数据块
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
 * 从 inode表(inode_table) 中获取一个空闲的inode节点项
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
    memset(inode, 0, sizeof(*inode));   // 已找到空闲inode，初始化inode，将 inode 清零
    inode->i_count = 1;                 // 设置引用计数，表示已被使用
    return inode;
}

struct m_inode * get_pipe_inode(void)
{
    struct m_inode * inode;

    if (!(inode = get_empty_inode())) {
        return NULL;
    }
    if (!(inode->i_size = get_free_page())) {
        inode->i_count = 0;
        return NULL;
    }
    inode->i_count = 2;	/* sum of readers/writers */
    PIPE_HEAD(*inode) = PIPE_TAIL(*inode) = 0;
    inode->i_pipe = 1;
    return inode;
}

/*
 * 从设备dev上读取指定节点号nr的inode节点（从 inode 缓存中获取指定设备号
 * 和 inode 编号的 inode 节点）
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
    empty = get_empty_inode();  // 从inode表(inode_table)中获取一个空闲 inode 槽位
    inode = inode_table;
    while (inode < NR_INODE + inode_table) {
        /*
         * 遍历 inode 表，查找设备号和 inode 编号都匹配的 inode节点(指定节点号nr的inode节点)；
         * 并递增该inode节点的引用次数。
         */
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
        /*
         * 如果该 inode 是挂载点（其它文件系统的安装点），则在超级块表中
         * 查找安装在此inode节点的超级块。
         * 如果没找到则释放函数刚开始获取的空闲inode节点。
         * 如果找到了，则将该inode节点写入硬盘中。再从安装在此inode的文件系统的超级块上中
         * 获取对应的设备号，重新扫描整个inode表，以获取该被安装文件系统的根节点(inode)。
         */
        if (inode->i_mount) {
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
            iput(inode);
            /* 切换到被挂载文件系统的设备号和根 inode 编号 */
            dev = super_block[i].s_dev;
            nr = ROOT_INO;
            inode = inode_table;
            continue;
        }
        /* 如果该 inode 不是挂载点（其它文件系统的安装点），
         * 则说明已经找到了对应的inode，所以放弃临时申请的inode空闲槽位 */
        if (empty) {
            iput(empty);
        }
        return inode;
    }
    if (!empty) {
        return (NULL);
    }
    /* 如果在inode表中没有找到指定的inode节点，则利用前面申请的空闲inode节点
     * 在inode表中建立该节点，并从相应的设备上获取该inode节点的信息
     */
    inode = empty;
    inode->i_dev = dev;
    inode->i_num = nr;
    read_inode(inode);

    return inode;   // 返回新读取的 inode
}

/*
 * 从设备(硬盘)上读取指定inode节点的信息到内存中对应的缓冲区
 */
static void read_inode(struct m_inode * inode)
{
    struct super_block * sb;
    struct buffer_head * bh;
    int block;

    lock_inode(inode);  // 对 inode 加锁，防止并发读取造成数据不一致
    /*  */
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
    /* 获取指定设备dev的超级块，并计算出inode所在的数据块在硬盘中的实际块号，
     * 读取inode所在块的数据到缓冲区中
     * 将内存中的 inode 数据复制到缓冲区的对应位置，等待把数据同步到硬盘中*/
    if (!(sb = get_super(inode->i_dev))) {
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