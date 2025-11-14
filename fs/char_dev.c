//
// Created by asujy on 2025/11/6.
//

#include <errno.h>
#include <sys/types.h>

#include <linux/sched.h>

extern int tty_read(unsigned minor,char * buf,int count);
extern int tty_write(unsigned minor,char * buf,int count);

typedef (*crw_ptr)(int rw,unsigned minor,char * buf,int count,off_t * pos);

/* 对指定的终端设备进行读写操作 */
static int rw_ttyx(int rw, unsigned minor, char * buf, int count, off_t * pos) {
    return ((rw == READ) ? tty_read(minor, buf, count):
            tty_write(minor, buf, count));
}

/* 对当前进程的控制终端进行读写操作 */
static int rw_tty(int rw, unsigned minor, char * buf, int count, off_t * pos)
{
    if (current->tty < 0) {
        return -EPERM;
    }
    return rw_ttyx(rw, current->tty, buf, count, pos);
}

static int rw_memory(int rw, unsigned minor, char * buf, int count, off_t * pos)
{
    switch(minor) {
    // case 0:
    //     return rw_ram(rw,buf,count,pos);
    // case 1:
    //     return rw_mem(rw,buf,count,pos);
    // case 2:
    //     return rw_kmem(rw,buf,count,pos);
    // case 3:
    //     return (rw==READ)?0:count;	/* rw_null */
    // case 4:
    //     return rw_port(rw,buf,count,pos);
    default:
        return -EIO;
    }
}

#define NRDEVS ((sizeof (crw_table))/(sizeof (crw_ptr)))

static crw_ptr crw_table[]={
    NULL,		/* nodev */
    rw_memory,	        /* /dev/mem etc */
    NULL,		/* /dev/fd */
    NULL,		/* /dev/hd */
    rw_ttyx,	        /* /dev/ttyx */
    rw_tty,		/* /dev/tty */
    NULL,		/* /dev/lp */
    NULL                /* unnamed pipes */
};

/*
 * 字符设备读写操作函数
 * args:
 *  - rw: 读写命令
 *  - dev: 设备号
 *  - buf: 数据缓冲区
 *  - count: 要读写的字节数
 *  - pos: 文件位置指针
 */
int rw_char(int rw, int dev, char *buf, int count, off_t *pos) {
    crw_ptr call_addr;

    if (MAJOR(dev) >= NRDEVS) {
        return -ENODEV;
    }
    if (!(call_addr = crw_table[MAJOR(dev)])) { // 根据主设备号在设备表中查找对应的驱动函数
        return -ENODEV;
    }
    return call_addr(rw, MINOR(dev), buf, count, pos);
}