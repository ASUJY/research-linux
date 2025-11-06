//
// Created by asujy on 2025/7/25.
//

#ifndef SYS_H
#define SYS_H

extern int sys_setup();
extern int sys_fork();  // 创建进程
extern int sys_open();
extern int sys_pause();

/* 系统调用函数表 */
fn_ptr sys_call_table[] = { sys_setup, sys_fork, sys_open, sys_pause };

#endif //SYS_H
