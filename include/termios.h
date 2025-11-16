//
// Created by asujy on 2025/6/13.
//

#ifndef RESEARCH_LINUX_TERMIOS_H
#define RESEARCH_LINUX_TERMIOS_H

#define TCGETS		0x5401
#define TCSETS		0x5402
#define TCSETSW		0x5403
#define TCSETSF		0x5404
#define TCGETA		0x5405
#define TCSETA		0x5406
#define TCSETAW		0x5407
#define TCSETAF		0x5408
#define TCSBRK		0x5409
#define TCXONC		0x540A
#define TCFLSH		0x540B
#define TIOCEXCL	0x540C
#define TIOCNXCL	0x540D
#define TIOCSCTTY	0x540E
#define TIOCGPGRP	0x540F
#define TIOCSPGRP	0x5410
#define TIOCOUTQ	0x5411
#define TIOCSTI		0x5412
#define TIOCGWINSZ	0x5413
#define TIOCSWINSZ	0x5414
#define TIOCMGET	0x5415
#define TIOCMBIS	0x5416
#define TIOCMBIC	0x5417
#define TIOCMSET	0x5418
#define TIOCGSOFTCAR	0x5419
#define TIOCSSOFTCAR	0x541A
#define TIOCINQ		0x541B

#define NCC 8
struct termio {
    unsigned short c_iflag;		/* input mode flags */
    unsigned short c_oflag;		/* output mode flags */
    unsigned short c_cflag;		/* control mode flags */
    unsigned short c_lflag;		/* local mode flags */
    unsigned char c_line;		/* line discipline */
    unsigned char c_cc[NCC];	/* control characters */
};

#define NCCS 17
struct termios {
    unsigned long c_iflag;		/* input mode flags */
    unsigned long c_oflag;		/* output mode flags */
    unsigned long c_cflag;		/* control mode flags */
    unsigned long c_lflag;		/* local mode flags */
    unsigned char c_line;		/* line discipline */
    unsigned char c_cc[NCCS];	/* control characters */
};

/* c_iflag bits */
#define INLCR	0000100
#define IGNCR	0000200
#define ICRNL	0000400
#define IUCLC	0001000


/* c_oflag bits */
#define OPOST	0000001 // 启用输出字符处理，当设置 OPOST 时，终端驱动会对输出字符进行额外处理
#define OLCUC	0000002 // 输出小写转大写标志，将输出的字符转换成对应的大写字符
#define ONLCR	0000004 // 将输出的换行符转换为回车换行符
#define OCRNL	0000010 // 将输出的回车符转换为换行符
#define ONLRET	0000040 // 将输出的换行符转换为回车符

/* c_cflag bit meaning */
#define CBAUD	0000017
#define  B2400	0000013
#define   CS8	0000060

/* c_lflag bits */
#define ISIG	0000001
#define ICANON	0000002
#define ECHO	0000010
#define ECHOCTL	0001000
#define ECHOKE	0004000

/* c_cc characters */
#define VINTR 0
#define VQUIT 1
#define VERASE 2
#define VKILL 3
#define VEOF 4
#define VTIME 5
#define VMIN 6
#define VSTART 8
#define VSTOP 9

#endif //RESEARCH_LINUX_TERMIOS_H
