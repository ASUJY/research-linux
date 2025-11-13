//
// Created by asujy on 2025/7/25.
//

#ifndef ERROR_H
#define ERROR_H

extern int errno;

#define ERROR           99
#define EPERM		 1
#define ENOENT		 2
#define EINTR		 4
#define EIO		 5
#define ENOEXEC		 8
#define EBADF		 9
#define ECHILD		10
#define EAGAIN		11    // 资源暂时不可用
#define ENOMEM		12    // 内存不足
#define EACCES		13
#define EFAULT		14
#define EEXIST		17
#define ENODEV		19
#define EISDIR		21
#define EINVAL		22
#define EMFILE		24
#define ENOTTY		25
#define ENOSPC		28

#endif //ERROR_H
