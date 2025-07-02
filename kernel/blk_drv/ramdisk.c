#define MAJOR_NR 1    // 定义主设备号，表示该RAM磁盘设备的主设备号

#include <blk.h>

char* rd_start;        // 指向RAM磁盘的起始内存地址
int	rd_length = 0;    // 记录RAM磁盘的长度（字节数）

void do_rd_request(void)
{
}

/*
* ram磁盘(ramdisk)初始化函数
* mem_start：物理内存起始地址
* length：请求的RAM磁盘大小
* 返回初始化的RAM磁盘大小。
* */
long rd_init(long mem_start, int length) {
    int i;
    char* cp;

    blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST;    // 注册设备，将ramdisk对应的请求处理函数(request_fn)设置为DEVICE_REQUEST
    rd_start = (char*)mem_start;
    rd_length = length;
    // ramdisk的内存区域清零
    cp = rd_start;
    for (i = 0; i < length; i++) {
        *cp++ = '\0';
    }
    return (length);
}