//
// Created by asujy on 2025/6/12.
//

/**
 * kernel.h 包含一些常用的函数原型。
 * */

#ifndef RESEARCH_LINUX_KERNEL_H
#define RESEARCH_LINUX_KERNEL_H

volatile void panic(const char * str);
int printk(const char * fmt, ...);

#endif //RESEARCH_LINUX_KERNEL_H
