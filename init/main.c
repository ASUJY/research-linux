#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/sched.h>
#include <asm/system.h>

// 以下这些数据在setup.asm中已经保存到对应的内存地址中
#define EXT_MEM_K (*(unsigned short *)0x90002)      // 1M以后的扩展内存大小
#define DRIVE_INFO (*(struct drive_info *)0x90080)  // 硬盘参数

static long memory_end          = 0;    // 系统物理内存的结束地址（字节）
static long buffer_memory_end   = 0;    // 高速缓冲区(磁盘缓冲区)的结束地址
static long main_memory_start   = 0;    // 主内存的起始地址

struct drive_info
{
    char dummy[32];
} drive_info;   // 存放硬盘参数

void main(void)		/* This really IS void, no error here. */
{
    drive_info = DRIVE_INFO;                    // 保存磁盘参数
    memory_end = (1 << 20) + (EXT_MEM_K << 10); // 总物理内存大小 = 1MB + 扩展内存
    memory_end &= 0xfffff000;                   // 忽略不到4KB的内存数(按4KB对齐)
    if (memory_end > 16 * 1024 * 1024) {
        memory_end = 16 * 1024 * 1024;
    }
    // 设置高速缓冲区大小
    if (memory_end > 12 * 1024 * 1024) {
        buffer_memory_end = 4 * 1024 * 1024;
    } else if (memory_end > 6 * 1024 * 1024) {
        buffer_memory_end = 2 * 1024 * 1024;
    } else {
        buffer_memory_end = 1 * 1024 * 1024;
    }
    main_memory_start = buffer_memory_end;

// 如果定义了内存虚拟盘，则初始化虚拟盘
#ifdef RAMDISK
    main_memory_start += rd_init(main_memory_start, RAMDISK * 1024);    // 从main_memory_start位置开始，分配RAMDISK*1024字节的内存空间
#endif

    trap_init();
    tty_init();
    sched_init();
    printk("Hi OneOS!\n");
    sti();
}