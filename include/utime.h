//
// Created by asujy on 2025/11/14.
//

#ifndef UTIME_H
#define UTIME_H

#include <sys/types.h>	/* I know - shouldn't do this, but .. */

struct utimbuf {
    time_t actime;
    time_t modtime;
};

#endif //UTIME_H
