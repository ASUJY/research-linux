//
// Created by asujy on 2025/7/26.
//

#ifndef UNISTD_H
#define UNISTD_H

#ifdef __LIBRARY__

#define __NR_setup	0
#define __NR_fork	1
#define __NR_open	2
#define __NR_pause	3

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

#endif /* __LIBRARY__ */

extern int errno;

static int fork(void);
int open(const char * filename, int flag, ...);
static int pause(void);

#endif //UNISTD_H
