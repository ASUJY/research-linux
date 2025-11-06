//
// Created by asujy on 2025/10/29.
//

#include <linux/sched.h>
#include <linux/kernel.h>

/* 将指定地址(addr)处的一块内存清零 */
#define clear_block(addr) \
__asm__("cld\n\t" \
        "rep\n\t" \
        "stosl" \
        ::"a" (0),"c" (BLOCK_SIZE/4),"D" ((long) (addr)):)

/* 置位指定地址开始的第nr个位偏移处的比特位，返回原比特位 */
#define set_bit(nr,addr) ({\
register int res __asm__("ax"); \
__asm__ __volatile__("btsl %2,%3\n\tsetb %%al": \
"=a" (res):"0" (0),"r" (nr),"m" (*(addr))); \
res;})

/* 复位指定地址开始的第nr位偏移处的比特位。返回原比特位 */
#define clear_bit(nr,addr) ({\
register int res __asm__("ax"); \
__asm__ __volatile__("btrl %2,%3\n\tsetnb %%al": \
"=a" (res):"0" (0),"r" (nr),"m" (*(addr))); \
res;})

/* 从addr开始查找第一个空闲位（第一个0值比特位） */
#define find_first_zero(addr) ({ \
int __res; \
__asm__("cld\n" \
        "1:\tlodsl\n\t" \
        "notl %%eax\n\t" \
        "bsfl %%eax,%%edx\n\t" \
        "je 2f\n\t" \
        "addl %%edx,%%ecx\n\t" \
        "jmp 3f\n" \
        "2:\taddl $32,%%ecx\n\t" \
        "cmpl $8192,%%ecx\n\t" \
        "jl 1b\n" \
        "3:" \
        :"=c" (__res):"c" (0),"S" (addr):); \
__res;})

/*
 * 释放设备dev上的数据块block（复位指定数据块block对应数据块位图中的比特位）
 */
void free_block(int dev, int block)
{
    struct super_block * sb;
    struct buffer_head * bh;

    /* 获取指定设备dev的超级块，并根据超级块上给出的设备数据块的范围来判断数据块号block的有效性 */
    if (!(sb = get_super(dev))) {
        panic("trying to free block on nonexistent device");
    }
    if (block < sb->s_firstdatazone || block >= sb->s_nzones) {
        panic("trying to free block not in datazone");
    }
    /* 在高速缓冲区中进行查找，看看指定的数据块是否在高速缓冲区中，
     * 如果在高速缓冲区中，则将对应的缓冲块释放掉
     */
    bh = get_hash_table(dev, block);
    if (bh) {
        if (bh->b_count != 1) {
            printk("trying to free block (%04x:%d), count=%d\n",
                    dev,block,bh->b_count);
            return;
        }
        bh->b_dirt = 0;         // 清除脏位（不需要写回硬盘）
        bh->b_uptodate = 0;     // 清除更新标志（数据已无效）
        brelse(bh);             // 释放缓冲区
    }
    /* 计算block的数据块号（以s_firstdatazone为第一个块算起），
     * 并对数据块位图进行操作，复位对应的比特位*/
    block -= sb->s_firstdatazone - 1 ;
    if (clear_bit(block&8191, sb->s_zmap[block/8192]->b_data)) {
        printk("block (%04x:%d) ", dev, block + sb->s_firstdatazone - 1);
        panic("free_block: bit already cleared");
    }
    /* 根据数据块号设置相应数据块位图在缓冲区中对应的缓冲块的已修改标志 */
    sb->s_zmap[block/8192]->b_dirt = 1; // 标记位图脏: 表示位图已被修改，需要写回磁盘
}

/*
 * 向设备dev申请一个数据块，返回数据块号；并置位指定数据块block对应的数据块位图比特位。
 * 用于在文件系统中分配新的数据块。
 */
int new_block(int dev) {
    struct buffer_head * bh;
    struct super_block * sb;
    int i;
    int j;

    /* 获取指定设备dev的超级块，并对整个数据块位图进行搜索，寻找首个是0的比特位，
     * 若没有找到，则说明块设备(硬盘)空间已用完；否则将该比特位置为1，表示占用对应的数据块 */
    if (!(sb = get_super(dev))) {
        panic("trying to get new block from nonexistant device");
    }
    j = 8192;
    for (i = 0; i < 8; i++) {       // 查找空闲块；遍历8个位图块（每个管理8192个块
        if (bh = sb->s_zmap[i]) {
            if ((j = find_first_zero(bh->b_data)) < 8192) {
                break;
            }
        }
    }
    if (i >= 8 || !bh || j >= 8192) {
        return 0;   // 没有找到空闲块，返回0
    }
    if (set_bit(j, bh->b_data)) {   // 标记块已分配
        panic("new_block: bit already set");
    }
    bh->b_dirt = 1; // 标记数据块对应的缓冲区为脏（需要写回磁盘）
    /* 计算数据块的块号，并在高速缓冲区中申请相应的缓冲块，并把该缓冲块清零 */
    j += i * 8192 + sb->s_firstdatazone - 1;    // 计算实际块号
    if (j >= sb->s_nzones) {
        return 0;
    }
    if (!(bh = getblk(dev, j))) {       // 获取数据块对应的缓冲区
        panic("new_block: cannot get block");
    }
    if (bh->b_count != 1) {
        panic("new block: count is != 1");
    }
    clear_block(bh->b_data);

    /* 设置缓冲块的已更新和已修改标志，最后释放该缓冲块
     * （这样就相当于在硬盘中申请了一个数据块，并且数据块初始化为0） */
    bh->b_uptodate = 1;
    bh->b_dirt = 1;
    brelse(bh);
    return j;   // 返回分配的块号
}

/*
 * 释放指定的inode，并复位对应的inode位图比特位
 * 负责清理 inode 结构并更新 inode 位图，是文件系统 inode 管理的组成部分。
 */
void free_inode(struct m_inode * inode)
{
    struct super_block * sb;
    struct buffer_head * bh;

    if (!inode) {
        return;
    }
    if (!inode->i_dev) {    // 如果 inode 没有关联的设备，清空 inode 所占内存区并返回
        memset(inode, 0, sizeof(*inode));
        return;
    }
    if (inode->i_count > 1) {
        printk("trying to free inode with count=%d\n", inode->i_count);
        panic("free_inode");
    }
    if (inode->i_nlinks) {  // 如果链接计数不为0（表示还有目录项引用）
        panic("trying to free inode with links");
    }
    /* 获取指定设备dev的超级块，并根据超级块上给出的inode总数量来判断inode编号的有效性 */
    if (!(sb = get_super(inode->i_dev))) {
        panic("trying to free inode on nonexistent device");
    }
    if (inode->i_num < 1 || inode->i_num > sb->s_ninodes) {
        panic("trying to free inode 0 or nonexistant inode");
    }
    if (!(bh = sb->s_imap[inode->i_num>>13])) { // 获取 inode 对应的缓冲区
        panic("nonexistent imap in superblock");
    }
    if (clear_bit(inode->i_num&8191, bh->b_data)) { // 复位inode对应的inode位图比特位
        printk("free_inode: bit already cleared.\n\r");
    }
    bh->b_dirt = 1;                     // 表示 inode 对应的缓冲区已被修改，需要写回磁盘
    memset(inode, 0, sizeof(*inode));   // 将 inode 结构体完全清零，以便重用
}

/*
 * 为设备dev建立一个新的inode，返回指向新inode的指针
 * 用于在指定设备上分配并初始化一个新的磁盘inode
 * 实现了inode位图管理、inode编号分配和基本属性初始化。
 * args:
 *  - dev: 设备号
 */
struct m_inode * new_inode(int dev)
{
    struct m_inode * inode;
    struct super_block * sb;
    struct buffer_head * bh;
    int i;
    int j;

    /* 在内存inode表(inode_table)中获取一个空闲的inode表项，获取指定设备dev的超级块，
     * 并从inode位图中找一个空闲的inode比特位 */
    if (!(inode = get_empty_inode())) {
        return NULL;
    }
    if (!(sb = get_super(dev))) {
        panic("new_inode with unknown device");
    }
    j = 8192;
    for (i = 0; i < 8; i++) {   // 获取inode对应的缓冲区，以及查找空闲inode比特位：遍历8个inode位图块（每个管理8192个inode）
        if (bh = sb->s_imap[i]) {
            if ((j = find_first_zero(bh->b_data)) < 8192) {
                break;  // 如果找到空闲位，跳出循环
            }
        }
    }
    /* 检查缓冲区的有效性以及根据超级块上给出的inode总数量来判断inode编号的有效性 */
    if (!bh || j >= 8192 || j + i * 8192 > sb->s_ninodes) {
        iput(inode);    // 释放inode
        return NULL;
    }
    if (set_bit(j, bh->b_data)) {   // 标记inode为已分配
        panic("new_inode: bit already set");
    }
    bh->b_dirt = 1;     // 标记位图缓冲区为脏（需要写回磁盘）
    /* 初始化inode基本属性 */
    inode->i_count = 1;
    inode->i_nlinks = 1;
    inode->i_dev = dev;
    inode->i_uid = current->euid;
    inode->i_gid = current->egid;
    inode->i_dirt = 1;
    inode->i_num = j + i * 8192;
    inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
    return inode;
}