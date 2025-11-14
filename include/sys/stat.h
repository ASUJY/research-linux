//
// Created by asujy on 2025/10/29.
//

#ifndef STAT_H
#define STAT_H

#include <sys/types.h>

struct stat {
  dev_t	st_dev;
  ino_t	st_ino;
  umode_t	st_mode;
  nlink_t	st_nlink;
  uid_t	st_uid;
  gid_t	st_gid;
  dev_t	st_rdev;
  off_t	st_size;
  time_t	st_atime;
  time_t	st_mtime;
  time_t	st_ctime;
};

#define S_IFMT  00170000
#define S_IFREG  0100000
#define S_IFBLK  0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_ISUID  0004000
#define S_ISGID  0002000
#define S_ISVTX  0001000

#define S_ISREG(m)	(((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)	(((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)	(((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)	(((m) & S_IFMT) == S_IFBLK)

#endif //STAT_H
