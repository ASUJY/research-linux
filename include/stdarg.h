//
// Created by asujy on 2025/6/12.
//

#ifndef RESEARCH_LINUX_STDARG_H
#define RESEARCH_LINUX_STDARG_H

#ifndef _VA_LIST
typedef __builtin_va_list va_list;
#define _VA_LIST
#endif

#define va_start(ap, param) __builtin_va_start(ap, param)
#define va_end(ap)          __builtin_va_end(ap)
#define va_arg(ap, type)    __builtin_va_arg(ap, type)


#endif //RESEARCH_LINUX_STDARG_H
