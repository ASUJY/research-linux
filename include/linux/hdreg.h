//
// Created by asujy on 2025/10/5.
//

#ifndef HDREG_H
#define HDREG_H

/* Hd controller regs. Ref: IBM AT Bios-listing */
#define HD_DATA		0x1f0	/* 硬盘数据寄存器，_CTL when writing */
#define HD_ERROR	0x1f1	/* 硬盘错误寄存器，see err-bits */
#define HD_STATUS	0x1f7	/* 硬盘状态寄存器，see status-bits */

#define HD_CMD		0x3f6   /* 硬盘控制寄存器 */

/* Bits of HD_STATUS */
#define ERR_STAT	0x01  /* 硬盘发生错误 */
#define DRQ_STAT	0x08  /* 硬盘数据请求就绪 */
#define SEEK_STAT	0x10  /* 硬盘寻道完成 */
#define WRERR_STAT	0x20  /* 硬盘写错误 */
#define READY_STAT	0x40  /* 硬盘就绪 */
#define BUSY_STAT	0x80  /* 硬盘控制器忙碌 */

/* Values for HD_COMMAND */
#define WIN_RESTORE		0x10  /* 重新校准硬盘 */
#define WIN_READ		0x20  /* 读取硬盘 */
#define WIN_WRITE		0x30  /* 写入硬盘 */
#define WIN_SPECIFY		0x91  /* 设置硬盘驱动器参数的命令 */

#endif //HDREG_H
