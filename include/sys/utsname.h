//
// Created by asujy on 2025/11/14.
//

#ifndef UTSNAME_H
#define UTSNAME_H

struct utsname {
    char sysname[9];
    char nodename[9];
    char release[9];
    char version[9];
    char machine[9];
};

#endif //UTSNAME_H
