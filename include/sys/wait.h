//
// Created by asujy on 2025/11/13.
//

#ifndef WAIT_H
#define WAIT_H

#include <sys/types.h>

#define WNOHANG		1
#define WUNTRACED	2

pid_t wait(int *stat_loc);
pid_t waitpid(pid_t pid, int *stat_loc, int options);

#endif //WAIT_H
