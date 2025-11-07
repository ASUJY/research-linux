//
// Created by asujy on 2025/11/6.
//

#include <errno.h>

#include <linux/sched.h>

/*
 * 复制文件描述符
 * args:
 *  - fd: 要复制的文件描述符
 *  - arg: 指定新文件描述符的最小值
 */
static int dupfd(unsigned int fd, unsigned int arg) {
    if (fd >= NR_OPEN || !current->filp[fd]) {
        return -EBADF;
    }
    if (arg >= NR_OPEN) {
        return -EINVAL;
    }
    /*
     * 在当前进程的文件描述符数组中寻找索引号大于等于arg但是还没有使用的项
     */
    while (arg < NR_OPEN) {
        if (current->filp[arg]) {
            arg++;
        } else {
            break;
        }
    }
    if (arg >= NR_OPEN) {
        return -EMFILE;
    }
    /*
    * close_on_exec是一个位图，表示在exec时关闭的文件描述符
    * 清除对应位，表示新文件描述符在exec时不自动关闭
     */
    current->close_on_exec &= ~(1 << arg);
    /*
    * 将目标文件描述符指向源文件描述符的同一个file结构
    * 增加file结构的引用计数(f_count)
    * 这意味着两个文件描述符共享同一个file结构
     */
    (current->filp[arg] = current->filp[fd])->f_count++;
    return arg;
}

/*
 * 复制文件描述符
 */
int sys_dup(unsigned int fildes) {
    return dupfd(fildes, 0);
}