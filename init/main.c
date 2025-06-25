#include <linux/kernel.h>
#include <linux/tty.h>
#include <linux/sched.h>
#include <asm/system.h>

void main(void)		/* This really IS void, no error here. */
{
    trap_init();
    tty_init();
    sched_init();
    printk("Hi OneOS!\n");
    sti();
    int a = 5 / 0;
}