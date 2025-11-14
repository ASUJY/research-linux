//
// Created by asujy on 2025/11/14.
//

#ifndef TIME_H
#define TIME_H

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

#endif //TIME_H
