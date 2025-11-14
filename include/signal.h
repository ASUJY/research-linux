//
// Created by asujy on 2025/7/16.
//

#ifndef SIGNAL_H
#define SIGNAL_H

typedef unsigned int sigset_t;		/* 32 bits */

#define SIGHUP		 1
#define SIGKILL		 9
#define SIGSEGV		11
#define SIGPIPE		13
#define SIGALRM		14
#define SIGCHLD		17
#define SIGSTOP		19

#define SA_NOMASK	0x40000000
#define SA_ONESHOT	0x80000000

#define SIG_IGN		((void (*)(int))1)

struct sigaction {
    void (*sa_handler)(int);
    sigset_t sa_mask;
    int sa_flags;
    void (*sa_restorer)(void);
};

#endif //SIGNAL_H
