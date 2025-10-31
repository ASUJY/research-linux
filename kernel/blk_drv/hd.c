//
// Created by asujy on 2025/7/29.
//

#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/hdreg.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>

#define MAJOR_NR 3
#include "blk.h"

#define CMOS_READ(addr) ({ \
outb_p(0x80|addr,0x70); \
inb_p(0x71); \
})

/* Max read/write errors/sector */
#define MAX_ERRORS	7
#define MAX_HD		2

static void recal_intr(void);

static int recalibrate = 1; // 硬盘重新校准标志
static int reset = 1;       // 硬盘控制器重置标志

/* 硬盘的基本信息 */
struct hd_i_struct {
    int head;   // 磁头数
    int sect;   // 每磁道扇区数
    int cyl;    // 柱面数
    int wpcom;  // 写前预补偿柱面号
    int lzone;  // 磁头着陆区柱面号
    int ctl;    // 控制字节
};
#ifdef HD_TYPE
struct hd_i_struct hd_info[] = { HD_TYPE };
#define NR_HD ((sizeof (hd_info))/(sizeof (struct hd_i_struct)))
#else
// 全局硬盘信息数组
struct hd_i_struct hd_info[] = { {0,0,0,0,0,0},{0,0,0,0,0,0} };
static int NR_HD = 0;
#endif

/* 硬盘的分区信息 */
static struct hd_struct {
    long start_sect;
    long nr_sects;
} hd[5*MAX_HD]={{0,0},};

#define port_read(port,buf,nr) \
__asm__("cld;rep;insw"::"d" (port),"D" (buf),"c" (nr))

#define port_write(port,buf,nr) \
__asm__("cld;rep;outsw"::"d" (port),"S" (buf),"c" (nr))

extern void hd_interrupt(void);

/* 负责读取硬盘参数信息、读取分区表信息并初始化硬盘分区结构hd，加载根文件系统 */
int sys_setup(void * BIOS)
{
    static int callable = 1;
    int i;
    int drive;
    unsigned char cmos_disks;
    struct partition *p;
    struct buffer_head *bh;

    if (!callable) {
        return -1;
    }
    callable = 0;

    /* 如果没有预定义硬盘参数，就从0x90080处读入 */
#ifndef HD_TYPE
    /* 从 BIOS 数据区读取两个硬盘的参数 */
    for (drive = 0; drive < 2; drive++) {
        hd_info[drive].cyl = *(unsigned short *) BIOS;
        hd_info[drive].head = *(unsigned char *) (2+BIOS);
        hd_info[drive].wpcom = *(unsigned short *) (5+BIOS);
        hd_info[drive].ctl = *(unsigned char *) (8+BIOS);
        hd_info[drive].lzone = *(unsigned short *) (12+BIOS);
        hd_info[drive].sect = *(unsigned char *) (14+BIOS);
        BIOS += 16;
    }
    if (hd_info[1].cyl) {   // 根据第二个硬盘的柱面数判断是否存在第二个硬盘
        NR_HD = 2;          // 设置硬盘数量
    } else {
        NR_HD = 1;
    }
#endif
    /* 初始化主分区表（每个硬盘有5个分区项；0: 整个磁盘, 1-4: 主分区），
     * 设置硬盘的起始扇区以及总扇区数
     */
    for (i = 0; i < NR_HD; i++) {
        hd[i*5].start_sect = 0;
        hd[i*5].nr_sects = hd_info[i].head * hd_info[i].sect * hd_info[i].cyl;
    }

    /* 根据 CMOS 信息重新设置硬盘数量 */
    if ((cmos_disks = CMOS_READ(0x12)) & 0xf0) {
        if (cmos_disks & 0x0f) {
            NR_HD = 2;
        } else {
            NR_HD = 1;
        }
    } else {
        NR_HD = 0;
    }

    for (i = NR_HD; i < 2; i++) {
        hd[i*5].start_sect = 0;
        hd[i*5].nr_sects = 0;
    }

    for (drive = 0; drive < NR_HD; drive++) {
        /* 对每个存在的硬盘，读取其主引导记录（MBR）；0x300是硬盘的主设备号*/
        if (!(bh = bread(0x300 + drive * 5, 0))) {
            printk("Unable to read partition table of drive %d\n\r", drive);
            panic("");
        }
        /* 检查 MBR 结束标志 0x55A */
        if (bh->b_data[510] != 0x55 || (unsigned char) bh->b_data[511] != 0xAA) {
            printk("Bad partition table on drive %d\n\r", drive);
            panic("");
        }
        /* 解析分区表，分区表位于 MBR 偏移 0x1BE 处，遍历4个主分区表项 */
        p = 0x1BE + (void*)bh->b_data;
        for (i = 1; i < 5; i++, p++) {
            hd[i + 5 * drive].start_sect = p->start_sect;
            hd[i + 5 * drive].nr_sects = p->nr_sects;
        }
        brelse(bh);
    }
    if (NR_HD) {
        printk("Partition table%s ok.\n\r", (NR_HD > 1) ? "s" : "");
    }
    // rd_load();
    mount_root();   // 挂载根文件系统
    return 0;
}

/*
 * 检测硬盘控制器是否已就绪并可以接受新命令
 * return：>0控制器就绪，否则表示超时失败
 */
static int controller_ready(void)
{
    int retries = 10000;

    while (--retries && (inb_p(HD_STATUS)&0xc0) != 0x40);

    return (retries);
}

/*
 * 检查硬盘控制器命令执行的结果状态，通过读取状态寄存器来判断上一个命令是否成功完成
 */
static int win_result(void)
{
    int i = inb_p(HD_STATUS);   // 读取状态寄存器：从硬盘状态寄存器获取当前状态

    if ((i & (BUSY_STAT | READY_STAT | WRERR_STAT | SEEK_STAT | ERR_STAT))  // 就绪 + 寻道完成，且没有错误、忙碌或写错误
            == (READY_STAT | SEEK_STAT)) {
        return(0); /* ok */
    }
    if (i&1) {
        i = inb(HD_ERROR);
    }
    return (1);
}

/*
 * 用于向硬盘控制器发送完整的I/O命令序列
 * args:
 *  - drive：驱动器号
 *  - nsect：扇区数
 *  - sect：起始扇区号
 *  - head：磁头号
 *  - cyl：柱面号
 *  - cmd：命令码(读、写等)
 *  - intr_addr：中断处理函数
 */
static void hd_out(unsigned int drive, unsigned int nsect, unsigned int sect,
                   unsigned int head, unsigned int cyl, unsigned int cmd,
                   void (*intr_addr)(void)) {
    register int port asm("dx");    // 将port变量绑定到DX寄存器，提高端口I/O操作的效率

    if (drive > 1 || head > 15) {
        panic("Trying to write bad sector");
    }
    if (!controller_ready()) {
        panic("HD controller not ready");
    }

    do_hd = intr_addr;                          // 注册中断处理程序，硬盘中断发生时调用
    outb_p(hd_info[drive].ctl, HD_CMD);         // 设置控制寄存器：写入驱动器特定的控制字节
    port = HD_DATA;                             // 设置端口为数据寄存器地址
    outb_p(hd_info[drive].wpcom>>2, ++port);    // 写入写预补偿柱面(右移2位)
    outb_p(nsect, ++port);                      // 写入要读写的扇区数量
    outb_p(sect, ++port);                       // 写入起始扇区号
    outb_p(cyl, ++port);                        // 写入柱面号的低8位
    outb_p(cyl>>8, ++port);                     // 写入柱面号的高8位
    outb_p(0xA0|(drive<<4)|head, ++port);       // 驱动器号和磁头号
    outb(cmd,++port);                           // 发送命令，操作硬盘
}

/*
 * 硬盘控制器状态检测函数
 *
 * 检测硬盘控制器是否处于忙碌状态，通过轮询硬盘状态寄存器来等待控制器就绪。
 * 如果控制器在超时时间内未就绪，则报告超时错误。
 *
 * return：0表示控制器就绪，1表示超时
 */
static int drive_busy(void) {
    unsigned int i;

    for (i = 0; i < 10000; i++) {
        if (READY_STAT == (inb_p(HD_STATUS) & (BUSY_STAT | READY_STAT))) {
            break;
        }
    }

    i = inb(HD_STATUS);
    i &= BUSY_STAT | READY_STAT | SEEK_STAT;

    if (i == (READY_STAT |SEEK_STAT)) {
        return (0);
    }
    printk("HD controller times out\n\r");
    return(1);
}

/*
 * 重置硬盘控制器
 */
static void reset_controller(void) {
    int i;

    outb(4, HD_CMD);    // 发送重置命令, 向硬盘命令寄存器写入值4
    for (i = 0; i < 100; i++) {
        nop();
    }
    outb(hd_info[0].ctl & 0x0f, HD_CMD);

    if (drive_busy()) {
        printk("HD-controller still busy\n\r");
    }
    if ((i = inb(HD_ERROR)) != 1) {
        printk("HD-controller reset failed: %02x\n\r",i);
    }
}

/*
 * 硬盘重置函数
 * args:
 *  - nr：硬盘驱动器编号（0或1）
 */
static void reset_hd(int nr)
{
    reset_controller(); // 重置控制器
    hd_out(nr,hd_info[nr].sect,hd_info[nr].sect,hd_info[nr].head-1,
            hd_info[nr].cyl,WIN_SPECIFY,&recal_intr);   // 向硬盘发送WIN_SPECIFY命令
}

void unexpected_hd_interrupt(void)
{
    printk("Unexpected HD interrupt\n\r");
}

/*
 * 硬盘读写错误处理函数
 */
static void bad_rw_intr(void)
{
    if (++CURRENT->errors >= MAX_ERRORS) {
        end_request(0); // 如果错误次数达到或超过最大允许错误数，调用end_request(0)标记请求失败
    }
    if (CURRENT->errors > MAX_ERRORS/2) {
        reset = 1;          // 如果错误次数超过最大错误数的一半，尝试重置控制器恢复状态
    }
}

/*
 * 这是硬盘读操作完成时的中断处理函数
 */
static void read_intr(void)
{
    if (win_result()) {     // 调用win_result()检查读操作是否成功
        bad_rw_intr();
        do_hd_request();
        return;
    }
    port_read(HD_DATA,CURRENT->buffer,256); // 从硬盘数据寄存器读取256个字（512字节）
    CURRENT->errors = 0;                    // 成功读取后清除错误计数
    CURRENT->buffer += 512;                 // 将缓冲区指针向前移动512字节,准备接收下一个扇区的数据
    CURRENT->sector++;                      // 增加当前处理的逻辑扇区号，指向下一个要读取的扇区
    if (--CURRENT->nr_sectors) {            // 检查是否还有更多扇区要读
        do_hd = &read_intr;                 // 确保下一个读完成中断仍然调用这个函数
        return;                             // 等待下一个中断来读取下一个扇区
    }
    end_request(1);                 // 所有扇区读取完成，标记请求成功
    do_hd_request();                        // 继续处理请求队列中的下一个I/O请求
}

/*
 * 这是硬盘写操作完成时的中断处理函数
 */
static void write_intr(void)
{
    if (win_result()) {     // 调用win_result()检查写操作是否成功
        bad_rw_intr();      // 如果失败，调用bad_rw_intr()进行错误计数和处理
        do_hd_request();    // 继续处理，调用do_hd_request()继续处理请求队列
        return;             // 错误情况下不执行后续的数据处理逻辑
    }
    if (--CURRENT->nr_sectors) {    // 检查是否还有更多扇区要写
        CURRENT->sector++;          // 指向下一个要写入的扇区
        CURRENT->buffer += 512;     // 缓冲区指针向前移动512字节,准备写入下一个扇区的数据
        do_hd = &write_intr;        // 确保下一个写完成中断仍然调用这个函数
        port_write(HD_DATA,CURRENT->buffer, 256);    // 向硬盘数据寄存器写入256个字（512字节）
        return;                     // 等待下一个中断来处理下一个扇区的写入
    }
    end_request(1);         // 所有扇区写入完成，标记请求成功
    do_hd_request();                // 继续处理请求队列中的下一个I/O请求
}

/*
 * 这是硬盘重新校准(Recalibrate)命令完成时的中断服务例程
 *
 * 函数的核心目的：在校准完成后恢复正常的I/O处理
 */
static void recal_intr(void)
{
    if (win_result()) {     // 调用win_result()检查重新校准操作是否成功
        bad_rw_intr();
    }
    do_hd_request();        // 调用do_hd_request()继续处理硬盘请求队列
}

/*
 * 硬盘请求处理函数
 */
void do_hd_request(void) {
    int i;
    int r = 0;
    unsigned int block;
    unsigned int dev;
    unsigned int sec;
    unsigned int head;
    unsigned int cyl;
    unsigned int nsect;

    INIT_REQUEST;               // 检查当前请求的有效性
    // 从当前请求中提取设备号(次设备号/分区信息)和块号
    dev = MINOR(CURRENT->dev);
    block = CURRENT->sector;
    /* 检查设备号和块号是否在有效范围内 */
    if (dev >= 5 * NR_HD || block + 2 > hd[dev].nr_sects) {
        end_request(0);     // 如果参数无效，结束当前请求并标记失败
        goto repeat;
    }
    block += hd[dev].start_sect;
    dev /= 5;

    /*
     * 将线性块地址转换为柱面(Cylinder)、磁头(Head)、扇区(Sector)地址
     * 第一次除法：计算扇区号和在磁道内的偏移
     * 第二次除法：计算柱面号和磁头号
     */
    __asm__("divl %4":"=a" (block),"=d" (sec):"0" (block),"1" (0),
                "r" (hd_info[dev].sect));
    __asm__("divl %4":"=a" (cyl),"=d" (head):"0" (block),"1" (0),
            "r" (hd_info[dev].head));

    sec++;  // 扇区号调整：扇区号从1开始（不是0）
    nsect = CURRENT->nr_sectors;
    if (reset) {
        reset = 0;                 // 清除重置标志
        recalibrate = 1;           // 设置重新校准标志
        reset_hd(CURRENT_DEV);  // 如果重置标志被设置，调用reset_hd()重置控制器
        return;
    }

    if (recalibrate) {
        recalibrate = 0;
        hd_out(dev,hd_info[CURRENT_DEV].sect,0,0,0,
                WIN_RESTORE,&recal_intr);   // 如果重新校准标志被设置,发送WIN_RESTORE命令将磁头归零
        return;
    }

    if (CURRENT->cmd == WRITE) {
        //  在发送写命令时注册write_intr中断处理程序
        hd_out(dev,nsect,sec,head,cyl,WIN_WRITE,&write_intr);
        // 等待DRQ(数据请求)状态，表示控制器准备好接收数据
        for(i=0 ; i<3000 && !(r=inb_p(HD_STATUS)&DRQ_STAT) ; i++)
            /* nothing */ ;
        if (!r) {
            bad_rw_intr();
            goto repeat;
        }
        port_write(HD_DATA,CURRENT->buffer,256);    // 写入数据到硬盘
    } else if (CURRENT->cmd == READ) {
        // 在发送读命令时注册read_intr中断处理程序
        hd_out(dev,nsect,sec,head,cyl,WIN_READ,&read_intr);
    } else {
        panic("unknown hd-command");
    }
}

void hd_init(void) {
	blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST;
	set_intr_gate(0x2E, &hd_interrupt);		// 设置硬盘中断处理例程
	// 开启硬盘中断
	outb_p(inb_p(0x21)&0xfb, 0x21);
	outb(inb_p(0xA1)&0xbf, 0xA1);
}