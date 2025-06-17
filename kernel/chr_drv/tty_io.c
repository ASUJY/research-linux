#include <ctype.h>

#include <linux/tty.h>
#include <asm/segment.h>

#define _L_FLAG(tty,f)	((tty)->termios.c_lflag & f)
#define _I_FLAG(tty,f)	((tty)->termios.c_iflag & f)
#define _O_FLAG(tty,f)	((tty)->termios.c_oflag & f)

#define L_CANON(tty)	_L_FLAG((tty),ICANON)
#define L_ISIG(tty)	_L_FLAG((tty),ISIG)
#define L_ECHO(tty)	_L_FLAG((tty),ECHO)
#define L_ECHOCTL(tty)	_L_FLAG((tty),ECHOCTL)

#define I_UCLC(tty)	_I_FLAG((tty),IUCLC)
#define I_NLCR(tty)	_I_FLAG((tty),INLCR)
#define I_CRNL(tty)	_I_FLAG((tty),ICRNL)
#define I_NOCR(tty)	_I_FLAG((tty),IGNCR)

#define O_POST(tty)	_O_FLAG((tty),OPOST)
#define O_NLCR(tty)	_O_FLAG((tty),ONLCR)
#define O_CRNL(tty)	_O_FLAG((tty),OCRNL)
#define O_NLRET(tty)	_O_FLAG((tty),ONLRET)
#define O_LCUC(tty)	_O_FLAG((tty),OLCUC)

struct tty_struct tty_table[1] = {
    {
        {ICRNL,            /* 将输入的CR转换为NL */
         OPOST | ONLCR,    /* 将输出的NL转为CRNL */
         0,             /* 控制模式标志初始化为0 */
         ISIG | ICANON | ECHO | ECHOCTL | ECHOKE,/* 本地模式标志 */
         0,                    /* 控制台 termio */
         INIT_C_CC},        /* 控制字符数组 */
        0,                    /* 初始停止标志 */
        con_write,          /* tty写函数 */
        {0, 0, 0, 0, {0}},        /* console read-queue */
        {0, 0, 0, 0, {0}},        /* console write-queue */
        {0, 0, 0, 0, {0}}
    }
};

void tty_init(void) {
    con_init();
}

void copy_to_cooked(struct tty_struct * tty) {
    signed char c;

    while (!EMPTY(tty->read_q) && !FULL(tty->secondary)) {
        GETCH(tty->read_q, c);
        if (c == 13)
            if (I_CRNL(tty))
                c = 10;
            else if (I_NOCR(tty))
                continue;
            else ;
        else if (c == 10 && I_NLCR(tty))
            c=13;
        if (I_UCLC(tty))
            c = tolower(c);
        if (L_CANON(tty)) {
            if (c == KILL_CHAR(tty)) {
                /* deal with killing the input line */
                while (!(EMPTY(tty->secondary) ||
                        (c = LAST(tty->secondary)) == 10 ||
                        c == EOF_CHAR(tty))) {
                    if (L_ECHO(tty)) {
                        if (c < 32)
                            PUTCH(127, tty->write_q);
                        PUTCH(127, tty->write_q);
                        tty->write(tty);
                    }
                    DEC(tty->secondary.head);
                }
                continue;
            }
            if (c == ERASE_CHAR(tty)) {
                if (EMPTY(tty->secondary) ||
                    (c = LAST(tty->secondary)) == 10 ||
                    c == EOF_CHAR(tty))
                    continue;
                if (L_ECHO(tty)) {
                    if (c < 32)
                        PUTCH(127, tty->write_q);
                    PUTCH(127, tty->write_q);
                    tty->write(tty);
                }
                DEC(tty->secondary.head);
                continue;
            }
            if (c == STOP_CHAR(tty)) {
                tty->stopped = 1;
                continue;
            }
            if (c == START_CHAR(tty)) {
                tty->stopped = 0;
                continue;
            }
        }
        if (L_ISIG(tty)) {
            if (c == INTR_CHAR(tty)) {

            }
            if (c == QUIT_CHAR(tty)) {

            }
        }
        if (c == 10 || c == EOF_CHAR(tty))
            tty->secondary.data++;
        if (L_ECHO(tty)) {
            if (c == 10) {
                PUTCH(10, tty->write_q);
                PUTCH(13, tty->write_q);
            } else if (c < 32) {
                if (L_ECHOCTL(tty)) {
                    PUTCH('^', tty->write_q);
                    PUTCH(c+64, tty->write_q);
                }
            } else
                PUTCH(c, tty->write_q);
            tty->write(tty);
        }
        PUTCH(c, tty->secondary);
    }
}

/**
 * channel - 子设备号
 * buf - 缓冲区指针
 * nr - 写字节数
 * return：返回写入的字节数
 * 把用户缓冲区中的字符写入tty的写队列中
 * */
int tty_write(unsigned channel, char * buf, int nr) {
    static cr_flag = 0; // 回车标志
    struct tty_struct *tty;
    char c;
    char *b = buf;

    // 当前版本的Linux内核终端只有三个子设备，分别是控制台(0)/串口终端1(1)/串口终端2(2)。
    // 所有任何大于2的子设备都是非法的，写的字节数也不能小于0。
    if (channel > 2 || nr < 0) {
        return -1;
    }
    tty = channel + tty_table;
    while (nr > 0) {
        /* 当要写的字节数>0并且tty的写队列不满时，循环处理字符 */
        while (nr > 0 && !FULL(tty->write_q)) {
            c = get_fs_byte(b);
            // 如果终端输出模式标志集中的执行输出处理标志OPOST置位，则执行下列输出时处理过程。
            if (O_POST(tty)) {
                if (c=='\r' && O_CRNL(tty)) // 将输出的回车符转换为换行符
                    c='\n';
                else if (c=='\n' && O_NLRET(tty))   // 将输出的换行符转换为回车符
                    c='\r';
                if (c=='\n' && !cr_flag && O_NLCR(tty)) {   // 将输出的换行符转换为回车换行符
                    cr_flag = 1;
                    PUTCH(13,tty->write_q); // 将回车符放入写队列中
                    continue;
                }
                if (O_LCUC(tty))    // 小写转大写标志OLCUC置位
                    c=toupper(c);   // 将字符转换成对应的大写字符
            }
            b++; nr--;
            cr_flag = 0;
            PUTCH(c,tty->write_q);  // 将字符放入tty的写队列中
        }
        /* 若全部字节写完，或者写队列已满，则调用对应的tty的写函数来将字符打印到屏幕中 */
        tty->write(tty);
    }
    return (b - buf);
}