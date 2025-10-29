//
// Created by asujy on 2025/6/13.
//

#ifndef RESEARCH_LINUX_SEGMENT_H
#define RESEARCH_LINUX_SEGMENT_H

/**
 * 从fs段寄存器指向的段中获取字符
 * @param addr 内存地址
 * @return 字符
 */
static inline unsigned char get_fs_byte(const char * addr)
{
    unsigned register char _v;

    __asm__ ("movb %%fs:%1,%0":"=r" (_v):"m" (*addr));
    return _v;
}

#endif //RESEARCH_LINUX_SEGMENT_H
