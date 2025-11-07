//
// Created by asujy on 2025/7/26.
//

#ifndef UNISTD_H
#define UNISTD_H

#include <sys/types.h>

#ifdef __LIBRARY__

#define __NR_setup	0
#define __NR_fork	1
#define __NR_write	2
#define __NR_open	3
#define __NR_pause	4
#define __NR_dup	5

#define _syscall0(type,name) \
type name(void) \
{ \
long __res; \
__asm__ volatile ("int $0x80" \
    : "=a" (__res) \
    : "0" (__NR_##name)); \
if (__res >= 0) \
    return (type) __res; \
errno = -__res; \
return -1; \
}

#define _syscall1(type,name,atype,a) \
type name(atype a) \
{ \
long __res; \
__asm__ volatile ("int $0x80" \
    : "=a" (__res) \
    : "0" (__NR_##name),"b" ((long)(a))); \
if (__res >= 0) \
    return (type) __res; \
errno = -__res; \
return -1; \
}

#define _syscall3(type,name,atype,a,btype,b,ctype,c) \
type name(atype a,btype b,ctype c) \
{ \
long __res; \
__asm__ volatile ("int $0x80" \
    : "=a" (__res) \
    : "0" (__NR_##name),"b" ((long)(a)),"c" ((long)(b)),"d" ((long)(c))); \
if (__res>=0) \
    return (type) __res; \
errno=-__res; \
return -1; \
}

#endif /* __LIBRARY__ */

extern int errno;

int dup(int fildes);
static int fork(void);
int open(const char * filename, int flag, ...);
static int pause(void);
int write(int fildes, const char * buf, off_t count);

#endif //UNISTD_H
