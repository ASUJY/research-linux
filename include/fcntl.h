//
// Created by asujy on 2025/10/31.
//

#ifndef FCNTL_H
#define FCNTL_H

/* open/fcntl - NOCTTY, NDELAY isn't implemented yet */
#define O_ACCMODE	00003
#define O_RDONLY	   00
#define O_WRONLY	   01
#define O_RDWR		   02
#define O_CREAT		00100	/* not fcntl */
#define O_EXCL		00200	/* not fcntl */
#define O_TRUNC		01000	/* 截断标志 */

extern int open(const char * filename, int flags, ...);

#endif //FCNTL_H
