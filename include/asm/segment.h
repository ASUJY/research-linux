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

static inline unsigned long get_fs_long(const unsigned long *addr)
{
  unsigned long _v;

  __asm__ ("movl %%fs:%1,%0":"=r" (_v):"m" (*addr)); \
  return _v;
}

static inline void put_fs_byte(char val,char *addr)
{
  __asm__ ("movb %0,%%fs:%1"::"r" (val),"m" (*addr));
}

static inline void put_fs_long(unsigned long val,unsigned long * addr)
{
  __asm__ ("movl %0,%%fs:%1"::"r" (val),"m" (*addr));
}

static inline unsigned long get_fs()
{
  unsigned short _v;
  __asm__("mov %%fs,%%ax":"=a" (_v):);
  return _v;
}

static inline unsigned long get_ds()
{
  unsigned short _v;
  __asm__("mov %%ds,%%ax":"=a" (_v):);
  return _v;
}

static inline void set_fs(unsigned long val)
{
  __asm__("mov %0,%%fs"::"a" ((unsigned short) val));
}

#endif //RESEARCH_LINUX_SEGMENT_H
