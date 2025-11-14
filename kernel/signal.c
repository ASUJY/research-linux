//
// Created by asujy on 2025/11/13.
//

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>

#include <signal.h>

int sys_sgetmask()
{
    return current->blocked;
}

int sys_ssetmask(int newmask)
{
    int old = current->blocked;

    current->blocked = newmask & ~(1<<(SIGKILL-1));
    return old;
}

int sys_signal(int signum, long handler, long restorer)
{
    struct sigaction tmp;

    if (signum<1 || signum>32 || signum==SIGKILL) {
        return -1;
    }
    tmp.sa_handler = (void (*)(int)) handler;
    tmp.sa_mask = 0;
    tmp.sa_flags = SA_ONESHOT | SA_NOMASK;
    tmp.sa_restorer = (void (*)(void)) restorer;
    handler = (long) current->sigaction[signum-1].sa_handler;
    current->sigaction[signum-1] = tmp;
    return handler;
}

static inline void save_old(char * from,char * to)
{
    int i;

    verify_area(to, sizeof(struct sigaction));
    for (i = 0; i < sizeof(struct sigaction); i++) {
        put_fs_byte(*from, to);
        from++;
        to++;
    }
}

static inline void get_new(char * from,char * to)
{
    int i;

    for (i = 0; i < sizeof(struct sigaction); i++) {
        *(to++) = get_fs_byte(from++);
    }
}

int sys_sigaction(int signum, const struct sigaction * action,
        struct sigaction * oldaction)
{
    struct sigaction tmp;

    if (signum<1 || signum>32 || signum==SIGKILL) {
        return -1;
    }
    tmp = current->sigaction[signum-1];
    get_new((char *) action,
            (char *) (signum-1+current->sigaction));
    if (oldaction) {
        save_old((char *) &tmp,(char *) oldaction);
    }
    if (current->sigaction[signum-1].sa_flags & SA_NOMASK) {
        current->sigaction[signum-1].sa_mask = 0;
    } else {
        current->sigaction[signum-1].sa_mask |= (1<<(signum-1));
    }
    return 0;
}