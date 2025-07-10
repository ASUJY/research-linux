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
void* kmalloc(unsigned int size);
void kfree_s(void * obj, int size);

#define kfree(x) kfree_s((x), 0)

#endif //RESEARCH_LINUX_KERNEL_H
