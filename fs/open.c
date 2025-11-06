//
// Created by asujy on 2025/10/31.
//

#include <errno.h>
#include <sys/stat.h>

#include <linux/sched.h>
#include <linux/tty.h>

/*
 * 打开（或创建）文件系统调用函数。
 * 它负责分配文件描述符、文件结构，处理特殊设备文件，并调用底层路径解析函数。
 *  args:
 *  - filename: 文件名路径
 *  - flag: 打开标志(O_CREAT等)
 *  - mode: 创建文件时的权限模式
 */
int sys_open(const char * filename, int flag, int mode) {
    struct m_inode *inode;
    struct file *f;
    int i;
    int fd; // 文件描述符

    /* 权限模式处理: 用当前进程的umask过滤权限位，即将用户设置的模式与进程的模式屏蔽码相与，
     * 产生真正许可的文件模式 */
    mode &= 0777 & ~current->umask;

    /* 查找空闲文件描述符：遍历进程的文件描述符表，找到第一个空闲位置 */
    for (fd = 0; fd < NR_OPEN; fd++) {
        if (!current->filp[fd]) {
            break;
        }
    }
    if (fd >= NR_OPEN) {    // 文件描述符检查：如果找不到空闲文件描述符，返回无效参数错误
        return -EINVAL;
    }

    current->close_on_exec &= ~(1 << fd);   // 清除close_on_exec标志：默认情况下，新打开的文件在执行时不关闭

    /*
     * 查找空闲文件结构：
     * 获取系统文件表的起始地址
     * 遍历系统文件表，找到第一个引用计数为0的空闲文件结构项
     */
    f = 0 + file_table;
    for (i = 0; i < NR_FILE; i++,f++) {
        if (!f->f_count) {
            break;
        }
    }
    if (i >= NR_FILE) { // 如果找不到空闲文件结构，返回无效参数错误
        return -EINVAL;
    }

    (current->filp[fd] = f)->f_count++; // 将文件结构赋值给进程的文件描述符表，增加文件结构的引用计数
    if ((i = open_namei(filename, flag, mode, &inode)) < 0) {   // 调用open_namei执行打开操作
        current->filp[fd] = NULL;
        f->f_count = 0;
        return i;
    }


    /*
     * 字符设备特殊处理：
     * 如果是字符设备文件（终端设备）
     * - 主设备号4（ttyxx）：如果进程是会话首领且没有控制终端，设置控制终端
     * - 主设备号5（tty）：如果进程没有tty(控制终端)，不允许打开，返回权限错误
     */
    if (S_ISCHR(inode->i_mode)) {
        if (MAJOR(inode->i_zone[0]) == 4) {
            if (current->leader && current->tty < 0) {
                /* 设置当前进程的tty号为该inode的子设备号，
                 * 并设置当前进程tty对应的tty表项的父进程组号等于进程的父进程组号 */
                current->tty = MINOR(inode->i_zone[0]);
                tty_table[current->tty].pgrp = current->pgrp;
            }
        } else if (MAJOR(inode->i_zone[0]) == 5) {
            if (current->tty < 0) {
                iput(inode);
                current->filp[fd] = NULL;
                f->f_count = 0;
                return -EPERM;
            }
        }
    }
    /*
     * 块设备特殊处理：如果是块设备文件，检查磁盘是否更换（如软盘）
     * 若更换则需要让高速缓冲区中对应该设备的所有缓冲块失效
     */
    if (S_ISBLK(inode->i_mode)) {
        check_disk_change(inode->i_zone[0]);
    }
    /* 初始化文件结构：设置文件模式、标志、引用计数 */
    f->f_mode = inode->i_mode;
    f->f_flags = flag;
    f->f_count = 1;
    f->f_inode = inode;
    f->f_pos = 0;
    return (fd);
}