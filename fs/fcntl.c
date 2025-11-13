//
// Created by asujy on 2025/11/6.
//

#include <errno.h>
#include <linux/sched.h>

#include <fcntl.h>

extern int sys_close(int fd);

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

int sys_dup2(unsigned int oldfd, unsigned int newfd) {
    sys_close(newfd);
    return dupfd(oldfd,newfd);
}

/*
 * 复制文件描述符
 */
int sys_dup(unsigned int fildes) {
    return dupfd(fildes, 0);
}

int sys_fcntl(unsigned int fd, unsigned int cmd, unsigned long arg)
{
    struct file * filp;

    if (fd >= NR_OPEN || !(filp = current->filp[fd])) {
        return -EBADF;
    }
    switch (cmd) {
        case F_DUPFD:
            return dupfd(fd,arg);
        case F_GETFD:
            return (current->close_on_exec>>fd)&1;
        case F_SETFD:
            if (arg&1)
                current->close_on_exec |= (1<<fd);
            else
                current->close_on_exec &= ~(1<<fd);
            return 0;
        case F_GETFL:
            return filp->f_flags;
        case F_SETFL:
            filp->f_flags &= ~(O_APPEND | O_NONBLOCK);
            filp->f_flags |= arg & (O_APPEND | O_NONBLOCK);
            return 0;
        case F_GETLK:	case F_SETLK:	case F_SETLKW:
                return -1;
        default:
            return -1;
    }
}