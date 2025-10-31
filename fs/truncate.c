//
// Created by asujy on 2025/10/29.
//

#include <linux/sched.h>

#include <sys/stat.h>

/*
 * 释放一级间接块及其指向的所有数据块，是文件系统空间回收机制的组成部分
 * args:
 *  - dev: 设备号
 *  - block: 块号
 */
static void free_ind(int dev, int block)
{
    struct buffer_head * bh;
    unsigned short * p;
    int i;

    if (!block) {
        return;
    }
    if (bh = bread(dev, block)) {   // 从磁盘读取间接块内容到缓冲区
        p = (unsigned short *) bh->b_data;
        for (i = 0; i < 512; i++,p++) {
            if (*p) {
                free_block(dev, *p);    // 调用 free_block() 释放对应的数据块
            }
        }
        brelse(bh);
    }
    free_block(dev,block);
}

/*
 * 释放二级间接块及其指向的所有一级间接块和数据块
 */
static void free_dind(int dev,int block)
{
    struct buffer_head * bh;
    unsigned short * p;
    int i;

    if (!block) {
        return;
    }
    if (bh = bread(dev, block)) {   // 从磁盘读取二级间接块内容到缓冲区
        p = (unsigned short *) bh->b_data;
        for (i = 0; i < 512; i++,p++) {
            if (*p) {
                free_ind(dev, *p);  // 释放一级间接块
            }
        }
        brelse(bh);
    }
    free_block(dev, block);
}

/*
 * 用于截断文件大小，释放文件占用的所有数据块，将文件大小设为0。
 * 将节点对应的文件长度截为0，并释放占用的设备空间。
 */
void truncate(struct m_inode * inode) {
    int i;

    /* 检查是否是普通文件或者目录，如果不是则直接返回 */
    if (!(S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode))) {
        return;
    }

    /* 释放inode节点的7个直接逻辑块 */
    for (i = 0; i < 7; i++) {
        if (inode->i_zone[i]) {
            free_block(inode->i_dev, inode->i_zone[i]);
            inode->i_zone[i] = 0;
        }
    }
    free_ind(inode->i_dev, inode->i_zone[7]);   // 释放一级间接块(i_zone[7] 是一级间接块的指针,释放该间接块及其指向的所有数据块)
    free_dind(inode->i_dev, inode->i_zone[8]);  // 释放二级间接块(i_zone[8] 是二级间接块的指针,释放该间接块及其所有子间接块和数据块)
    inode->i_zone[7] = inode->i_zone[8] = 0;              // 将一级和二级间接块指针清零
    inode->i_size = 0;  // 将文件大小设为0
    inode->i_dirt = 1;  // 表示 inode 已被修改，需要写回磁盘
    inode->i_mtime = inode->i_ctime = CURRENT_TIME;         // 更新时间戳
}