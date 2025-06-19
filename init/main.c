#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/sched.h>
#include <asm/system.h>

void main(void)		/* This really IS void, no error here. */
{
    sched_init();
    tty_init();
    printk("Hi OneOS!\n");
    sti();
}