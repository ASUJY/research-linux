//
// Created by asujy on 2025/7/2.
//
#include <linux/kernel.h>

volatile void panic(const char * s)
{
    printk("Kernel panic: %s\n\r",s);
    for(;;);
}