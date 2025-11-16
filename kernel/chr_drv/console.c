//
// Created by asujy on 2025/6/13.
//
#include <linux/sched.h>
#include <linux/tty.h>
#include <asm/io.h>
#include <asm/system.h>

#define ORIG_X			(*(unsigned char *)0x90000)                         // 光标列号
#define ORIG_Y			(*(unsigned char *)0x90001)                         // 光标行号
#define ORIG_VIDEO_PAGE		(*(unsigned short *)0x90004)                    // 显示页面
#define ORIG_VIDEO_MODE		((*(unsigned short *)0x90006) & 0xff)           // 显示模式
#define ORIG_VIDEO_COLS 	(((*(unsigned short *)0x90006) & 0xff00) >> 8)  // 字符列数
#define ORIG_VIDEO_LINES	(25)                                            // 显示行数
#define ORIG_VIDEO_EGA_BX	(*(unsigned short *)0x9000a)                    // 显存大小

#define VIDEO_TYPE_MDA		0x10	/* 单色文本显示适配器 */
#define VIDEO_TYPE_CGA		0x11	/* CGA彩色显示适配器 */
#define VIDEO_TYPE_EGAM		0x20	/* EGA/VGA单色模式 */
#define VIDEO_TYPE_EGAC		0x21	/* EGA/VGA彩色模式 */

#define NPAR 16

extern void keyboard_interrupt(void);

static unsigned char	video_type;         // 使用的显示类型
static unsigned long	video_num_columns;	// 屏幕文本列数
static unsigned long	video_size_row;		// 每行使用的字节数
static unsigned long	video_num_lines;	// 屏幕文本行数
static unsigned char	video_page;		    // 初始显示页面
static unsigned long	video_mem_start;	// 显示内存起始地址
static unsigned long	video_mem_end;		// 显示内存结束地址 (某种程度上)
static unsigned short	video_port_reg;		// 显示控制索引寄存器端口
static unsigned short	video_port_val;		// 显示控制数据寄存器端口
static unsigned short	video_erase_char;	//  擦除字符属性与字符

static unsigned long	origin;		        // 屏幕显示的内容从origin指向的内存地址开始，滚屏起始内存地址，用于EGA/VGA快速滚屏，
static unsigned long	scr_end;	        // 滚屏末端内存地址，用于EGA/VGA的快速滚屏
static unsigned long	pos;                // 当前光标对应的显示内存位置
static unsigned long	x,y;                // 当前光标位置，x表示列号，y表示行号
static unsigned long	top,bottom;         // 屏幕滚动时顶行的行号和底行的行号
static unsigned long	state=0;            // 用于表明处理ESC转义序列时的当前步骤
static unsigned long	npar,par[NPAR];     // 用于存放ESC序列的中间处理参数，ANSI转义字符序列参数个数和参数数组
static unsigned long	ques=0;
static unsigned char	attr=0x07;          // 字符属性(黑底白字)

static void sysbeep(void);

/*
 * this is what the terminal answers to a ESC-Z or csi0c
 * query (= vt100 response).
 * 表示支持VT100标准，支持扩展视频属性
 */
#define RESPONSE "\033[?1;2c"

/**
 *
 * @param new_x 光标所在列号
 * @param new_y 光标所在行号
 * 更新当前光标位置，并修正pos指向光标在显存中的对应位置。
 * gotoxy函数 认为 x==video_num_columns 是正确的
 */
static inline void gotoxy(unsigned int new_x, unsigned int new_y) {
    if (new_x > video_num_columns || new_y >= video_num_lines) {
        return;
    }
    x = new_x;
    y = new_y;
    pos = origin + y * video_size_row + (x << 1);
}

/**
 * 设置origin
 * 屏幕从origin指向的字符之后开始显示内容，而不是从显存基址开始显示
 */
static inline void set_origin(void) {
    cli();  // 关中断，确保对寄存器的两次写操作是原子的，防止中间被中断导致寄存器状态不一致。
    outb_p(12, video_port_reg);
    outb_p(0xff&((origin - video_mem_start) >> 9), video_port_val); // 向寄存器12写入字符偏移量的高8位
    outb_p(13, video_port_reg);
    outb_p(0xff&((origin - video_mem_start) >> 1), video_port_val); // 向寄存器13写入字符偏移量的低8位
    sti();
}

/**
 * 将屏幕窗口向下移动一行（屏幕内容往上滚动一行）。
 */
static void scrup(void) {
    if (video_type == VIDEO_TYPE_EGAC || video_type == VIDEO_TYPE_EGAM) { // 显示类型是EGA
        // 移动起始行top=0，移动最底行bottom=vieo_num_lines=25，表示整屏窗口向下移动
        if (!top && bottom == video_num_lines) {
            origin += video_size_row;
            pos += video_size_row;
            scr_end += video_size_row;
            if (scr_end > video_mem_end) {
                // 屏幕末端最后一个显示字符所对应的显存指针scr_end超出了实际显存的大小，
                // 则将屏幕内容内存数据移动到显存的起始位置video_mem_start处，并在出现的新行上填入空格字符
                __asm__("cld\n\t"
                        "rep\n\t"
                        "movsl\n\t"
                        "movl video_num_columns,%1\n\t"
                        "rep\n\t"
                        "stosw"
                        ::"a" (video_erase_char),
                "c" ((video_num_lines - 1) * video_num_columns >> 1),
                "D" (video_mem_start),
                "S" (origin)
                        :);
                scr_end -= origin - video_mem_start;
                pos -= origin - video_mem_start;
                origin = video_mem_start;
            } else {    // 屏幕末端最后一个显示字符所对应的显存指针scr_end没有超出显存的大小，则只需在新行上填入擦除字符(黑底白字的空格)
                __asm__("cld\n\t"
                        "rep\n\t"
                        "stosw"
                        ::"a" (video_erase_char),
                "c" (video_num_columns),
                "D" (scr_end - video_size_row):);
            }
            set_origin();
        } else {
            // 不是整屏移动，从指定行top开始的所有行向上移动1行
            __asm__("cld\n\t"
                    "rep\n\t"
                    "movsl\n\t"
                    "movl video_num_columns,%%ecx\n\t"
                    "rep\n\t"
                    "stosw"
                    ::"a" (video_erase_char),
            "c" ((bottom - top - 1) * video_num_columns >> 1),
            "D" (origin + video_size_row * top),
            "S" (origin + video_size_row * (top + 1))
                    :);
        }
    }
    else		/* Not EGA/VGA */
    {
        __asm__("cld\n\t"
                "rep\n\t"
                "movsl\n\t"
                "movl video_num_columns,%%ecx\n\t"
                "rep\n\t"
                "stosw"
                ::"a" (video_erase_char),
        "c" ((bottom - top - 1) * video_num_columns >> 1),
        "D" (origin + video_size_row * top),
        "S" (origin + video_size_row * (top + 1))
                :);
    }
}

/**
 * 将屏幕窗口向上移动一行（屏幕显示的内容往下移动一行）。
 * 与scrup函数类似，不过是相反方向的操作
 */
static void scrdown(void) {
    if (video_type == VIDEO_TYPE_EGAC || video_type == VIDEO_TYPE_EGAM) {
        __asm__("std\n\t"
                "rep\n\t"
                "movsl\n\t"
                "addl $2,%%edi\n\t"	/* %edi has been decremented by 4 */
                "movl video_num_columns,%%ecx\n\t"
                "rep\n\t"
                "stosw"
                ::"a" (video_erase_char),
        "c" ((bottom - top - 1) * video_num_columns >> 1),
        "D" (origin + video_size_row * bottom - 4),
        "S" (origin + video_size_row * (bottom - 1) - 4)
                :);
    }
    else		/* Not EGA/VGA */
    {
        __asm__("std\n\t"
                "rep\n\t"
                "movsl\n\t"
                "addl $2,%%edi\n\t"	/* %edi has been decremented by 4 */
                "movl video_num_columns,%%ecx\n\t"
                "rep\n\t"
                "stosw"
                ::"a" (video_erase_char),
        "c" ((bottom - top - 1) * video_num_columns >> 1),
        "D" (origin + video_size_row * bottom - 4),
        "S" (origin + video_size_row * (bottom - 1) - 4)
                :);
    }
}

/**
 * 换行，将光标位置下移一行
 * */
static void lf(void) {
    /* 如果光标没有处在倒数第一行，则直接修改光标当前行变量y，并调整光标对应显存位置pos（加上屏幕一行字符所对应的内存长度） */
    if (y + 1 < bottom) {
        y++;
        pos += video_size_row;
        return;
    }
    scrup();    // 光标处在屏幕最后一行，则屏幕内容上移一行
}

/**
 * 光标上移一行
 */
static void ri(void) {
    if (y > top) {    // 如果光标不在第一行，则直接修改光标当前行变量y，并调整光标对应显存位置pos（减去屏幕上一行字符所对应的内存长度字节数）
        y--;
        pos -= video_size_row;
        return;
    }
    scrdown();   // 屏幕内容下移一行
}

/* 回车，光标回到第0列 */
static void cr(void) {
    pos -= x<<1;    // 光标所在的列号*2，即0列到光标所在列对应的内存字节长度
    x = 0;
}

/* 擦除光标前一字符（用空格代替） */
static void del(void) {
    // 如果光标没有在第0列，则将光标对应内存位置指针pos后退2字节（对应屏幕上一个字符），
    // 然后将当前光标变量x值减一，并将光标所在位置字符擦除
    if (x) {
        pos -= 2;
        x--;
        *(unsigned short *)pos = video_erase_char;
    }
}

/**
 * 删除屏幕上与光标位置相关的部分，以屏幕为单位。csi - 控制序列引导码(Control Sequence Introducer)。
 * ANSI转义序列：'ESC [sJ' (s=0删除光标到屏幕底端；1删除屏幕开始到光标处；2整屏删除)
 * @param par - 对应上面的s
 */
static void csi_J(int par) {
    long count __asm__("cx");   // 设为寄存器变量
    long start __asm__("di");

    switch (par) {
        case 0:	/* erase from cursor to end of display */
            count = (scr_end - pos) >> 1;
            start = pos;
            break;
        case 1:	/* erase from start to cursor */
            count = (pos - origin) >> 1;
            start = origin;
            break;
        case 2: /* erase whole display */
            count = video_num_columns * video_num_lines;
            start = origin;
            break;
        default:
            return;
    }
    // 使用擦除字符(空格字符)填写删除字符的地方
    __asm__("cld\n\t"
            "rep\n\t"
            "stosw\n\t"
            ::"c" (count),
    "D" (start),"a" (video_erase_char)
            :);
}

/**
 * 删除行内与光标位置相关的部分，以一行为单位。
 * ANSI转义序列：'ESC [sK'(s=0删除到行尾；1从开始删除；2整行都删除)
 * @param par - 对应上面的s
 */
static void csi_K(int par) {
    long count __asm__("cx");
    long start __asm__("di");

    switch (par) {
        case 0:	/* erase from cursor to end of line */
            if (x >= video_num_columns)
                return;
            count = video_num_columns - x;
            start = pos;
            break;
        case 1:	/* erase from start of line to cursor */
            start = pos - (x<<1);
            count = (x < video_num_columns) ? x : video_num_columns;
            break;
        case 2: /* erase whole line */
            start = pos - (x<<1);
            count = video_num_columns;
            break;
        default:
            return;
    }
    // 使用擦除字符(空格字符)填写删除字符的地方
    __asm__("cld\n\t"
            "rep\n\t"
            "stosw\n\t"
            ::"c" (count),
    "D" (start),"a" (video_erase_char)
            :);
}

/**
*  表示改变光标处字符的显示属性，比如加粗，加下划线，闪烁，反显等。
*  ANSI转义字符序列'ESC [nm'，n=0正常显示；1加粗；4加下划线；7反显；27正常显示。
 */
void csi_m(void) {
    int i;
    for (i = 0; i <= npar; i++) {
        switch (par[i]) {
            case 0:attr=0x07;break;
            case 1:attr=0x0f;break;
            case 4:attr=0x0f;break;
            case 7:attr=0x70;break;
            case 27:attr=0x07;break;
        }
    }
}

/**
 * 设置光标位置
 * (pos - video_mem_start) 计算光标在显存中的字节偏移量
 */
static inline void set_cursor(void) {
    cli();
    outb_p(14, video_port_reg);
    outb_p(0xff&((pos - video_mem_start) >> 9), video_port_val);
    outb_p(15, video_port_reg);
    outb_p(0xff&((pos - video_mem_start) >> 1), video_port_val);
    sti();
}

/**
 * 当终端发送设备查询序列 ESC [ c 时：
 * 1.终端驱动检测到查询请求
 * 2.调用此 respond() 函数
 * 3.内核返回 ESC [ ?1;2c 表示：
 *   - 支持 VT100 标准
 *   - 支持扩展视频属性
 * 4.终端模拟器（如 xterm/gnome-terminal）收到响应后：
 *   - 启用 VT100 兼容功能集
 *   - 启用高级文本属性支持
 *
 * 发送对终端VT100的响应序列
 * @param tty
 */
static void respond(struct tty_struct * tty) {
    char * p = RESPONSE;

    cli();
    while (*p) {
        PUTCH(*p, tty->read_q); // 将字符写入读队列
        p++;
    }
    sti();
    copy_to_cooked(tty);        // 触发行规范处理
}

/**
 * 在光标处插入一个空格字符
 */
static void insert_char(void) {
    int i = x;
    unsigned short tmp, old = video_erase_char;
    unsigned short * p = (unsigned short *) pos;

    // 光标开始处的所有字符右移一格，并将擦除字符插入在光标所在处
    while (i++ < video_num_columns) {
        tmp = *p;
        *p = old;
        old = tmp;
        p++;
    }
}

/**
 * 在光标处插入一行（则光标将处在新的空行上）
 * 将屏幕从光标所在行到屏幕底向下滚动一行
 */
static void insert_line(void) {
    int oldtop, oldbottom;

    oldtop    = top;
    oldbottom = bottom;
    top       = y;                  // 设置屏幕滚动开始行
    bottom    = video_num_lines;    // 设置屏幕滚动最后一行
    scrdown();
    top       = oldtop;
    bottom    = oldbottom;
}

/**
 * 删除光标处的一个字符
 */
static void delete_char(void) {
    int i;
    unsigned short * p = (unsigned short *) pos;

    if (x >= video_num_columns) {
        return;
    }
    // 从光标右一个字符开始到行末所有字符左移一格
    i = x;
    while (++i < video_num_columns) {
        *p = *(p + 1);
        p++;
    }
    // 最后一个字符处填入擦除字符（空格字符）
    *p = video_erase_char;
}

/**
 * 删除光标所在行，即从光标所在行开始屏幕内容上滚一行
 */
static void delete_line(void) {
    int oldtop, oldbottom;

    oldtop    = top;
    oldbottom = bottom;
    top       = y;                  // 设置屏幕滚动开始行
    bottom    = video_num_lines;    // 设置屏幕滚动最后一行
    scrup();
    top       = oldtop;
    bottom    = oldbottom;
}

/**
 * 在光标位置处插入nr个字符
 * ANSI转义字符序列'ESC [n@'
 * @param nr - 对应上面的n
 */
static void csi_at(unsigned int nr) {
    // 如果插入的字符数大于一行字符数，则截为一行字符数；若插入字符数nr为0，则插入1个字符。
    if (nr > video_num_columns) {
        nr = video_num_columns;
    } else if (!nr) {
        nr = 1;
    }
    // 循环插入指定的字符数。
    while (nr--) {
        insert_char();
    }
}

/**
 * 在光标位置处插入nr行
 * ANSI转义字符序列'ESC [nL'
 * @param nr - 对应上面的n
 */
static void csi_L(unsigned int nr) {
    // 如果插入的行数大于屏幕最多行数，则截为屏幕显示行数；若插入行数nr为0，则插入1行。
    if (nr > video_num_lines) {
        nr = video_num_lines;
    } else if (!nr) {
        nr = 1;
    }
    // 循环插入指定行数nr。
    while (nr--) {
        insert_line();
    }
}

/**
 * 在光标位置处删除nr个字符
 * ANSI转义字符序列'ESC [nP'
 * @param nr - 对应上面的n
 */
static void csi_P(unsigned int nr) {
    // 如果删除的字符数大于一行字符数，则截为一行字符数；若删除字符数nr为0，则删除1个字符。
    if (nr > video_num_columns) {
        nr = video_num_columns;
    } else if (!nr) {
        nr = 1;
    }
    // 循环删除指定字符数nr。
    while (nr--) {
        delete_char();
    }
}

/**
 * 在光标位置处删除nr行
 * ANSI转义字符序列'ESC [nM'
 * @param nr - 对应上面的n
 */
static void csi_M(unsigned int nr) {
    // 如果删除的行数大于屏幕最多行数，则截为屏幕显示行数；若删除的行数nr为0，则删除1行。
    if (nr > video_num_lines) {
        nr = video_num_lines;
    } else if (!nr) {
        nr = 1;
    }
    // 循环删除指定行数nr。
    while (nr--) {
        delete_line();
    }
}

static int saved_x = 0; // 保存的光标列号
static int saved_y = 0; // 保存的光标行号

/* 保存当前光标位置 */
static void save_cur(void) {
    saved_x = x;
    saved_y = y;
}

/* 恢复保存的光标位置 */
static void restore_cur(void) {
    gotoxy(saved_x, saved_y);
}

/**
 * 控制台写函数
 * 从终端对应的tty写缓冲队列中取字符，并显示在屏幕上
 * 转义序列解析，将tty字符流转换为VGA文本缓冲区（屏幕）的显示内容
 * */
void con_write(struct tty_struct *tty) {
    int nr;
    char c;

    // 获取写缓冲队列中现有字符数nr，然后针对每个字符进行处理
    nr = CHARS(tty->write_q);
    while (nr--) {
        GETCH(tty->write_q, c); // 从写队列中取出一个字符
        switch (state) {        // 通过state变量实现有限状态机解析转义序列
            case 0: // 初始状态（处理普通字符）
                if (c > 31 && c < 127) {            // 普通字符
                    if (x >= video_num_columns) {   // 自动换行处理；若当前光标处在行末端或末端以外，则将光标移到下行头列。并调整光标位置对应的内存指针pos
                        x -= video_num_columns;
                        pos -= video_size_row;
                        lf();
                    }
                    __asm__("movb attr,%%ah\n\t"
                            "movw %%ax,%1\n\t"
                            ::"a" (c), "m" (*(short *) pos) // 将字符写到显存的pos处
                            :);
                    pos += 2;   // pos向右移动2个字节
                    x++;        // 光标右移一列
                } else if (c == 27)                     // 如果是转义字符ESC，则转换状态state到1
                    state = 1;
                else if (c == 10 || c == 11 || c == 12) // 如果是换行符(\n，10)，垂直制表符VT(11)，换页符FF(12)，则移动光标到下一行
                    lf();
                else if (c == 13)                       // 如果是回车符(\r，13)，则将光标移动到头列(第0列)
                    cr();
                else if (c == ERASE_CHAR(tty))          // 如果是DEL(127)，则将光标右边一字符擦除（用空格字符替代），并将光标移到被擦除位置
                    del();
                else if (c == 8) {                      // 如果是BS(backspace,退格符，\b，8)，则将光标右移一格，并相应调整光标对应内存位置指针pos
                    if (x) {
                        x--;
                        pos -= 2;
                    }
                } else if (c == 9) {                    // 如果是水平制表符TAB(\t，9)，则将光标移到8的倍数上。若此时光标列数超出屏幕最大列数，则将光标移到下一行上。
                    c = 8 - (x & 7);
                    x += c;
                    pos += c << 1;
                    if (x > video_num_columns) {
                        x -= video_num_columns;
                        pos -= video_size_row;
                        lf();
                    }
                    c = 9;
                } else if (c == 7)                      // 如果是响铃符BEL(\a，7)，则调用蜂鸣函数，是扬声器发声。
                    sysbeep();
                break;
            case 1: // 收到 ESC(0x1B) 后的状态
                state = 0;
                if (c == '[')       // 如果字符是'['，表示CSI序列开始，则将状态state转到2
                    state = 2;
                else if (c == 'E')  // 如果字符是E，则光标移到下一行开始处(0列)
                    gotoxy(0, y + 1);
                else if (c == 'M')  // 如果字符是M，则光标上移一行
                    ri();
                else if (c == 'D')  // 如果字符是D，则光标下移一行
                    lf();
                else if (c == 'Z')  // 如果字符是Z，则发送终端应答序列
                    respond(tty);
                else if (c == '7')  // 如果字符是7，则保存当前光标位置。
                    save_cur();
                else if (c == '8')  // 如果字符是8，则恢复到原保存的光标位置
                    restore_cur();
                break;
            case 2: // 收到 ESC[ 后的状态（CSI序列开始）
                for (npar = 0; npar < NPAR; npar++) {
                    // 对ESC转义字符序列参数使用的处理数组par[]清零，索引变量npar指向首项，并且设置状态为3
                    par[npar] = 0;
                }
                npar = 0;
                state = 3;
                if (ques = (c == '?')) {
                    // 如字符不是'？'，则直接转到状态3去处理，否则去读一字符，再到状态3处理
                    break;
                }
            case 3: // 解析CSI参数（数字部分）
                if (c == ';' && npar < NPAR - 1) {  // 如果字符是';'，并且数组par未满，则索引值加1
                    npar++;
                    break;
                } else if (c >= '0' && c <= '9') {  // 如果字符是数字字符'0-9'，则将该字符转换成数值并与npar所索引的项组成10进制数。
                    par[npar] = 10 * par[npar] + c - '0';
                    break;
                } else state = 4;
            case 4: // 执行CSI命令
                state = 0;  // 复位状态
                switch (c) {
                    case 'G':   // 如果字符是'G'或者'`'，则par[]中第一个参数代表列号，若列号不为零，则将光标右移一格。
                    case '`':
                        if (par[0]) par[0]--;
                        gotoxy(par[0], y);
                        break;
                    case 'A':   // 如果字符是A，则第一个参数代表光标上移的行数。若参数为0则上移一行。
                        if (!par[0]) par[0]++;
                        gotoxy(x, y - par[0]);
                        break;
                    case 'B':
                    case 'e':   // 如果字符是'B'或'e'，则第一个参数代表光标下移的行数。若参数为0则下移一行。
                        if (!par[0]) par[0]++;
                        gotoxy(x, y + par[0]);
                        break;
                    case 'C':
                    case 'a':   // 如果字符是'C'或'a'，则第一个参数代表光标右移的格数。若参数为0则右移一格。
                        if (!par[0]) par[0]++;
                        gotoxy(x + par[0], y);
                        break;
                    case 'D':   // 如果字符是'D'，则第一个参数代表光标左移的格数。若参数为0则左移一格。
                        if (!par[0]) par[0]++;
                        gotoxy(x - par[0], y);
                        break;
                    case 'E':   // 如果字符是'E'，则第一个参数代表光标向下移动的行数，并回到0列。若参数为0则下移一行。
                        if (!par[0]) par[0]++;
                        gotoxy(0, y + par[0]);
                        break;
                    case 'F':   // 如果字符是F，则第一个参数代表光标向上移动的行数，并回到0列。若参数为0则上移一行。
                        if (!par[0]) par[0]++;
                        gotoxy(0, y - par[0]);
                        break;
                    case 'd':   // 如果字符是d，则第一个参数代表光标所需在的行号(从0计数)
                        if (par[0]) par[0]--;
                        gotoxy(x, par[0]);
                        break;
                    case 'H':
                    case 'f':   // 如果字符是'H'或'f'，则第一个参数代表光标移到的行号，第二个参数代表光标移到的列号。
                        if (par[0]) par[0]--;
                        if (par[1]) par[1]--;
                        gotoxy(par[1], par[0]);
                        break;
                    case 'J':
                        csi_J(par[0]);
                        break;
                    case 'K':
                        csi_K(par[0]);
                        break;
                    case 'L':
                        csi_L(par[0]);
                        break;
                    case 'M':
                        csi_M(par[0]);
                        break;
                    case 'P':
                        csi_P(par[0]);
                        break;
                    case '@':
                        csi_at(par[0]);
                        break;
                    case 'm':
                        csi_m();
                        break;
                    case 'r':       // 如果字符是r，则表示用两个参数设置滚屏的起始行号和终止行号
                        if (par[0]) par[0]--;
                        if (!par[1]) par[1] = video_num_lines;
                        if (par[0] < par[1] &&
                            par[1] <= video_num_lines) {
                            top = par[0];
                            bottom = par[1];
                        }
                        break;
                    case 's':   // 如果字符是s，则表示保存当前光标所在位置。
                        save_cur();
                        break;
                    case 'u':   // 如果字符是u，则表示恢复光标到原保存的位置处。
                        restore_cur();
                        break;
                }

        }
    }
    set_cursor();   // 最后根据上面设置的光标位置，向显示控制器发送光标显示位置
}

/**
 * 初始化屏幕显示相关的参数
 * 设置显存的开始和结束地址
 * 设置显卡的索引寄存器和数据寄存器端口
 */
void con_init(void) {
    register unsigned char a;
    char *display_desc = "????";
    char *display_ptr;

    video_num_columns = ORIG_VIDEO_COLS;
    video_size_row = video_num_columns * 2;
    video_num_lines = ORIG_VIDEO_LINES;
    video_page = ORIG_VIDEO_PAGE;
    video_erase_char = 0x0720;          // 黑底白字，空格

    if (ORIG_VIDEO_MODE == 7)			/* 古老的单色显示器 */
    {
        video_mem_start = 0xb0000;      // 单色显示器的显存起始地址
        video_port_reg = 0x3b4;         // 单色显示器索引寄存器端口（CRTC寄存器）
        video_port_val = 0x3b5;         // 单色显示器数据寄存器端口（CRTC寄存器）
        if ((ORIG_VIDEO_EGA_BX & 0xff) != 0x10)
        {
            video_type = VIDEO_TYPE_EGAM;   // EGA单色模式
            video_mem_end = 0xb8000;        // EGA单色模式下的显存结束地址
            display_desc = "EGAm";
        }
        else
        {
            video_type = VIDEO_TYPE_MDA;    // 标准MDA模式
            video_mem_end	= 0xb2000;      // MDA模式模式下的显存结束地址
            display_desc = "*MDA";
        }
    }
    else								/* 彩色显示器 */
    {
        video_mem_start = 0xb8000;  // 彩色显示器的显存起始地址
        video_port_reg	= 0x3d4;    // 彩色显示器索引寄存器端口（CRTC寄存器）
        video_port_val	= 0x3d5;    // 彩色显示器数据寄存器端口（CRTC寄存器）
        if ((ORIG_VIDEO_EGA_BX & 0xff) != 0x10) // EGA彩色模式
        {
            video_type = VIDEO_TYPE_EGAC;
            video_mem_end = 0xbc000;    // EGA彩色模式下的显存结束地址
            display_desc = "EGAc";
        }
        else // CGA彩色模式
        {
            video_type = VIDEO_TYPE_CGA;
            video_mem_end = 0xba000;    // CGA彩色模式下的显存结束地址
            display_desc = "*CGA";
        }
    }

    // 在屏幕的右上角显示当前显示器的显示模式
    display_ptr = ((char *)video_mem_start) + video_size_row - 8;
    while (*display_desc)
    {
        *display_ptr++ = *display_desc++;
        display_ptr++;
    }

    /* 初始化用于滚屏的变量 (主要用于 EGA/VGA模式)	*/
    origin	= video_mem_start;
    scr_end	= video_mem_start + video_num_lines * video_size_row;
    top	= 0;
    bottom	= video_num_lines;

    gotoxy(ORIG_X,ORIG_Y);  // 初始化光标位置x,y和对应的内存位置pos
    set_trap_gate(0x21, &keyboard_interrupt);   // 设置中断向量0x21的中断描述符(键盘中断)
    outb_p(inb_p(0x21)&0xfd, 0x21);             // 允许键盘中断信号通过8259a芯片送往CPU
    a=inb_p(0x61);                              // 读取键盘端口0x61(8255A端口PB)
    outb_p(a|0x80, 0x61);                       // 设置禁止键盘工作(位7置位)
    outb(a, 0x61);                              // 允许键盘工作，复位键盘
}

void sysbeepstop(void)
{
    /* disable counter 2 */
    outb(inb_p(0x61)&0xFC, 0x61);
}

int beepcount = 0;
/* 开通蜂鸣功能 */
void sysbeep(void) {
    /* enable counter 2 */
    outb_p(inb_p(0x61)|3, 0x61);
    /* set command for counter 2, 2 byte write */
    outb_p(0xB6, 0x43);
    /* send 0x637 for 750 HZ */
    outb_p(0x37, 0x42);
    outb(0x06, 0x42);
    /* 蜂鸣时间为 1/8 second */
    beepcount = HZ/8;
}