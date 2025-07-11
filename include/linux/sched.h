//
// Created by asujy on 2025/6/13.
//

#ifndef RESEARCH_LINUX_SCHED_H
#define RESEARCH_LINUX_SCHED_H

#include <linux/head.h>
#include <linux/mm.h>

#define HZ 100

extern void sched_init(void);
extern void trap_init(void);

#endif //RESEARCH_LINUX_SCHED_H
