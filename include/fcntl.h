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
#define O_APPEND	02000
#define O_NONBLOCK	04000	/* not fcntl */

/* Defines for fcntl-commands. Note that currently
 * locking isn't supported, and other things aren't really
 * tested.
 */
#define F_DUPFD		0	/* dup */
#define F_GETFD		1	/* get f_flags */
#define F_SETFD		2	/* set f_flags */
#define F_GETFL		3	/* more flags (cloexec) */
#define F_SETFL		4
#define F_GETLK		5	/* not implemented */
#define F_SETLK		6
#define F_SETLKW	7

extern int open(const char * filename, int flags, ...);

#endif //FCNTL_H
