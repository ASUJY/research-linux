#include <linux/kernel.h>
#include <linux/tty.h>

void main(void)		/* This really IS void, no error here. */
{
    tty_init();
    printk("Hi OneOS!");
}