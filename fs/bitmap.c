//
// Created by asujy on 2025/10/29.
//

#include <linux/sched.h>
#include <linux/kernel.h>

/* 将指定地址(addr)处的一块内存清零(清除 addr 地址处的第 nr 位) */
#define clear_bit(nr,addr) ({\
register int res __asm__("ax"); \
__asm__ __volatile__("btrl %2,%3\n\tsetnb %%al": \
"=a" (res):"0" (0),"r" (nr),"m" (*(addr))); \
res;})

/*
 * 释放设备dev上的逻辑块block
 * 复位指定逻辑块block的逻辑块位图比特位
 */
void free_block(int dev, int block)
{
    struct super_block * sb;
    struct buffer_head * bh;

    if (!(sb = get_super(dev))) {   //  根据设备号获取超级块
        panic("trying to free block on nonexistent device");
    }
    if (block < sb->s_firstdatazone || block >= sb->s_nzones) { // 块号范围检查
        panic("trying to free block not in datazone");
    }
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
    block -= sb->s_firstdatazone - 1 ;
    if (clear_bit(block&8191, sb->s_zmap[block/8192]->b_data)) {
        printk("block (%04x:%d) ", dev, block + sb->s_firstdatazone - 1);
        panic("free_block: bit already cleared");
    }
    sb->s_zmap[block/8192]->b_dirt = 1; // 标记位图脏: 表示位图已被修改，需要写回磁盘
}

/*
 * 负责清理 inode 结构并更新 inode 位图，是文件系统 inode 管理的组成部分。
 * 释放指定的inode节点，复位对应的inode节点位图中对应的位
 */
void free_inode(struct m_inode * inode)
{
    struct super_block * sb;
    struct buffer_head * bh;

    if (!inode) {
        return;
    }
    if (!inode->i_dev) {    // 如果 inode 没有关联的设备，将 inode 结构体清零并返回
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
    if (!(sb = get_super(inode->i_dev))) {  // 根据设备号获取超级块
        panic("trying to free inode on nonexistent device");
    }
    if (inode->i_num < 1 || inode->i_num > sb->s_ninodes) { // 检查 inode 编号是否在有效范围内
        panic("trying to free inode 0 or nonexistant inode");
    }
    if (!(bh = sb->s_imap[inode->i_num>>13])) { // 获取 inode 位图
        panic("nonexistent imap in superblock");
    }
    if (clear_bit(inode->i_num&8191, bh->b_data)) { // 清除位图位
        printk("free_inode: bit already cleared.\n\r");
    }
    bh->b_dirt = 1; // 表示 inode 位图已被修改，需要写回磁盘
    memset(inode, 0, sizeof(*inode));   // 将 inode 结构体完全清零，准备重用
}