//
// Created by asujy on 2025/11/14.
//

#ifndef TIMES_H
#define TIMES_H

#include <sys/types.h>

struct tms {
    time_t tms_utime;
    time_t tms_stime;
    time_t tms_cutime;
    time_t tms_cstime;
};

#endif //TIMES_H
