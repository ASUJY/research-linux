//
// Created by asujy on 2025/6/13.
//

#ifndef RESEARCH_LINUX_STRING_H
#define RESEARCH_LINUX_STRING_H

static inline char * strcpy(char * dest,const char *src)
{
  __asm__("cld\n"
    "1:\tlodsb\n\t"
    "stosb\n\t"
    "testb %%al,%%al\n\t"
    "jne 1b"
    ::"S" (src),"D" (dest):"ax");
  return dest;
}

static inline char * strncpy(char * dest,const char *src,int count)
{
  __asm__("cld\n"
    "1:\tdecl %2\n\t"
    "js 2f\n\t"
    "lodsb\n\t"
    "stosb\n\t"
    "testb %%al,%%al\n\t"
    "jne 1b\n\t"
    "rep\n\t"
    "stosb\n"
    "2:"
    ::"S" (src),"D" (dest),"c" (count):"ax");
  return dest;
}

static inline int strcmp(const char * cs,const char * ct)
{
  register int __res __asm__("ax");
  __asm__("cld\n"
    "1:\tlodsb\n\t"
    "scasb\n\t"
    "jne 2f\n\t"
    "testb %%al,%%al\n\t"
    "jne 1b\n\t"
    "xorl %%eax,%%eax\n\t"
    "jmp 3f\n"
    "2:\tmovl $1,%%eax\n\t"
    "jl 3f\n\t"
    "negl %%eax\n"
    "3:"
    :"=a" (__res):"D" (cs),"S" (ct):);
  return __res;
}

static inline char * strchr(const char * s,char c)
{
  register char * __res __asm__("ax");
  __asm__("cld\n\t"
    "movb %%al,%%ah\n"
    "1:\tlodsb\n\t"
    "cmpb %%ah,%%al\n\t"
    "je 2f\n\t"
    "testb %%al,%%al\n\t"
    "jne 1b\n\t"
    "movl $1,%1\n"
    "2:\tmovl %1,%0\n\t"
    "decl %0"
    :"=a" (__res):"S" (s),"0" (c):);
  return __res;
}

static inline int strlen(const char * s)
{
    register int __res __asm__("cx");
    __asm__("cld\n\t"
            "repne\n\t"
            "scasb\n\t"
            "notl %0\n\t"
            "decl %0"
            :"=c" (__res):"D" (s),"a" (0),"0" (0xffffffff):);
    return __res;
}

static inline void * memcpy(void * dest, const void * src, int n)
{
  __asm__("cld\n\t"
          "rep\n\t"
          "movsb"
          ::"c" (n),"S" (src),"D" (dest)
          );
  return dest;
}

static inline void * memset(void * s, char c, int count)
{
  __asm__("cld\n\t"
          "rep\n\t"
          "stosb"
          ::"a" (c),"D" (s),"c" (count)
          :);
  return s;
}

#endif //RESEARCH_LINUX_STRING_H
