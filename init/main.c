#define __LIBRARY__

#include <unistd.h>

static inline fork(void) __attribute__((always_inline));
static inline pause(void) __attribute__((always_inline));
static inline _syscall0(int,fork)
static inline _syscall0(int,pause)
static inline _syscall1(int,setup,void *,BIOS)

#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/sched.h>
#include <asm/system.h>

#include <stdarg.h>
#include <fcntl.h>

#include <linux/fs.h>

static char printbuf[1024];

extern int vsprintf();
extern void init(void);
extern void blk_dev_init(void);
extern void hd_init(void);
extern void mem_init(long start, long end);
extern long rd_init(long mem_start, int length);

// 以下这些数据在setup.asm中已经保存到对应的内存地址中
#define EXT_MEM_K (*(unsigned short *)0x90002)      // 1M以后的扩展内存大小
#define DRIVE_INFO (*(struct drive_info *)0x90080)  // 硬盘参数
#define ORIG_ROOT_DEV (*(unsigned short *)0x901FC)  // 根文件系统所在设备号

static long memory_end          = 0;    // 系统物理内存的结束地址（字节）
static long buffer_memory_end   = 0;    // 高速缓冲区(磁盘缓冲区)的结束地址
static long main_memory_start   = 0;    // 主内存的起始地址

struct drive_info
{
    char dummy[32];
} drive_info;   // 存放硬盘参数

void main(void)		/* This really IS void, no error here. */
{
    ROOT_DEV = ORIG_ROOT_DEV;
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

    mem_init(main_memory_start, memory_end);
    trap_init();
    blk_dev_init();
    tty_init();
    sched_init();
    buffer_init(buffer_memory_end);
    hd_init();
    printk("Hi OneOS!\n");

    sti();
    move_to_user_mode();
    //int pid = fork();
    if (!fork()) {		/* we count on this going ok */
        init();
    }
    for(;;) {
        pause();
    }
}

static int printf(const char *fmt, ...) {
    va_list args;
    int i;

    va_start(args, fmt);
    write(1, printbuf, i = vsprintf(printbuf, fmt, args));
    va_end(args);
    return i;
}

void init(void) {
    int pid;
    setup((void *) &drive_info);
    (void) open("/dev/tty0", O_RDWR, 0);
    (void) dup(0);
    (void) dup(0);
    printf("%d buffers = %d bytes buffer space\n\r",NR_BUFFERS,
                NR_BUFFERS*BLOCK_SIZE);
    printf("Free mem: %d bytes\n\r",memory_end-main_memory_start);
    if (!(pid = fork())) {
        close(0);
    }
}