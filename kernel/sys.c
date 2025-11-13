//
// Created by asujy on 2025/11/13.
//

#include <errno.h>

#include <linux/sched.h>
#include <linux/kernel.h>

int sys_ftime()
{

}

int sys_break()
{

}

int sys_ptrace()
{

}

int sys_stty()
{

}

int sys_gtty()
{

}

int sys_rename()
{

}

int sys_prof()
{

}

int sys_setregid(int rgid, int egid)
{

}

int sys_setgid(int gid)
{

}

int sys_acct()
{

}

int sys_phys()
{

}

int sys_lock()
{

}

int sys_mpx()
{

}

int sys_ulimit()
{

}

int sys_time(long * tloc) {

}

int sys_setreuid(int ruid, int euid)
{

}

int sys_setuid(int uid)
{

}

int sys_stime(long * tptr)
{

}

int sys_times(struct tms * tbuf)
{

}

int sys_brk(unsigned long end_data_seg)
{
    if (end_data_seg >= current->end_code &&
        end_data_seg < current->start_stack - 16384) {
        current->brk = end_data_seg;
    }
    return current->brk;
}

int sys_setpgid(int pid, int pgid)
{

}

int sys_getpgrp(void)
{

}

int sys_setsid(void)
{
    if (current->leader && !suser()) {
        return -EPERM;
    }
    current->leader = 1;
    current->session = current->pgrp = current->pid;
    current->tty = -1;
    return current->pgrp;
}

int sys_uname(struct utsname * name)
{

}

int sys_umask(int mask)
{

}