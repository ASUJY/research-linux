//
// Created by asujy on 2025/7/29.
//

#include <linux/fs.h>
#include <linux/head.h>
#include <asm/system.h>
#include <asm/io.h>

#define MAJOR_NR 3
#include "blk.h"

extern void hd_interrupt(void);

void unexpected_hd_interrupt(void)
{
	printk("Unexpected HD interrupt\n\r");
}

void do_hd_request(void) {

}

void hd_init(void) {
	blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST;
	set_intr_gate(0x2E, &hd_interrupt);		// 设置硬盘中断处理例程
	// 开启硬盘中断
	outb_p(inb_p(0x21)&0xfb, 0x21);
	outb(inb_p(0xA1)&0xbf, 0xA1);
}