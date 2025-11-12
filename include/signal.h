//
// Created by asujy on 2025/7/16.
//

#ifndef SIGNAL_H
#define SIGNAL_H

typedef unsigned int sigset_t;		/* 32 bits */

#define SIGHUP		 1
#define SIGSEGV		11
#define SIGCHLD		17

#define SIG_IGN		((void (*)(int))1)

struct sigaction {
    void (*sa_handler)(int);
    sigset_t sa_mask;
    int sa_flags;
    void (*sa_restorer)(void);
};

#endif //SIGNAL_H
