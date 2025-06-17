//
// Created by asujy on 2025/6/12.
//
#include <stdarg.h>
#include <string.h>

/* 为了不使用ctype库中的digit函数，这里定义一个宏用来判断字符是否是数字 */
#define is_digit(c)	((c) >= '0' && (c) <= '9')

/**
 * 例如: %236d,把其中控制宽度的236都提取出来，由字符类型转换成int类型
 * @param s printf("%236d"，123) 中的 "%236d"
 * @return 转换后的数字，例如236
 */
static int skip_atoi(const char **s) {
    int i = 0;
    while (is_digit(**s)) {
        i = i * 10 + *((*s)++) - '0';
    }
    return i;
}

// 利用位图机制（即二进制每一位代表某个操作，为1表示生效，为0表示无效）
#define ZEROPAD	1		/* 填充0 */
#define SIGN	2		/* unsigned/signed long */
#define PLUS	4		/* 显示 “+” 号 */
#define SPACE	8		/* space if plus */
#define LEFT	16		/* 左调整 */
#define SPECIAL	32		/* 0x */
#define SMALL	64		/* use 'abcdef' instead of 'ABCDEF'，使用小写字母 */

#define do_div(n, base) ({ \
int __res;                 \
__asm__("div %4":"=a" (n), "=d"(__res):"0" (n),"1" (0), "r" (base)); \
__res;                     \
})

static char* number(char* str, int num, int base, int size, int precision, int type) {
    char c, sign, tmp[36];
    const char* digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int i;

    if (type & SMALL) {
        digits = "0123456789abcdefghijklmnopqrstuvwxyz";
    }
    if (type & LEFT) {
        type &= ~ZEROPAD;
    }
    if (base < 2 || base > 36) {
        return 0;
    }
    c = (type & ZEROPAD) ? '0' : ' ';
    if (type & SIGN && num < 0) {
        sign = '-';
        num = -num;
    } else {
        sign = (type & PLUS) ? '+' : ((type & SPACE) ? ' ' : 0);
    }
    if (sign) {
        size--;
    }
    if (type & SPECIAL) {
        if (base == 16) {
            size -= 2;
        } else if (base == 8) {
            size--;
        }
    }
    i = 0;
    if (num == 0) {
        tmp[i++] = '0';
    } else while (num != 0) {
            tmp[i++] = digits[do_div(num, base)];
        }

    if (i > precision) {
        precision = i;
    }
    size -= precision;
    if (!(type & (ZEROPAD + LEFT))) {
        while (size-- > 0) {
            *str++ = ' ';
        }
    }
    if (sign) {
        *str++ = sign;
    }
    if (type & SPECIAL) {
        if (base == 8) {
            *str++ = '0';
        } else if (base == 16) {
            *str++ = '0';
            *str++ = digits[33];
        }
    }
    if (!(type & LEFT)) {
        while (size-- > 0) {
            *str++ = c;
        }
    }
    while (i < precision--) {
        *str++ = '0';
    }
    while (i-- > 0) {
        *str++ = tmp[i];
    }
    while (size-- > 0) {
        *str++ = ' ';
    }
    return str;
}

/**
 *
 * @param buf 输出字符串缓冲区
 * @param fmt 格式化串
 * @param args 参数列表（字符列表）
 * @return 返回输出字符串的长度
 */
int vsprintf(char *buf, const char *fmt, va_list args) {
    int len;
    int i;
    char *str;  //用于存放转换过程中的字符串
    char *s;
    int *ip;
    int flags;  /* flags to number() */
    int field_width;    /* 输出字段的宽度，即最终要输出到屏幕的结果占多少位 */
    int precision;      /* min. # of digits for integers; max number of chars for from string， 对应的是精度 */
    int qualifier;      /* 'h', 'l', or 'L' for integer fields, 对应的是类型长度 */
    // 将字符指针str指向buf，然后扫描格式字符串，对各个格式转换指示进行相应的处理。
    for (str = buf; *fmt; ++fmt) {  // str指向buf数组中的元素，把要输出的内容都保存到buf数组中
        if (*fmt != '%') {
            *str++ = *fmt;
            continue;   // *str = *fmt, 然后str++，即指针移动到下一个buf元素所在的内存地址
        }

        /* 接下来处理 % 后面的内容，例如 %s %d这些，即处理%后面的标志，将这些标志对应的常量放入flags变量中 */
        flags = 0;
        repeat:
            ++fmt;  /* this also skips first '%' */
            switch (*fmt) {                                 // 处理printf中的标志字符-、+、#、空格
                case '-': flags |= LEFT; goto repeat;       // %- 结果左对齐，右边填空格
                case '+': flags |= PLUS; goto repeat;       // %+ 输出符号(正号或负号)
                case ' ': flags |= SPACE; goto repeat;      // %  输出值为正时冠以空格，为负时冠以负号
                case '#': flags |= SPECIAL; goto repeat;    // %# 对c，s，d，u类无影响；对o类，在输出时加前缀0；对x类， 在输出时加前缀0x或者0X；对g，G 类防止尾随0被删除；对于所有的浮点形式，#保证了即使不跟任何数字，也打印一个小数点字符
                case '0': flags |= ZEROPAD; goto repeat;    // %0 表示空位填0
            }

        /* 获取输出宽度，例如: %2d */
        field_width = -1;
        if (is_digit(*fmt)) {
            field_width = skip_atoi(&fmt);
        } else if (*fmt == '*') {   // printf("%*.*s\n", a, b, s); /* 等同于printf("%a.bs\n", s) */
            field_width = va_arg(args, int);    // 这里field_width相当于取a的值
            if (field_width < 0) {
                field_width = -field_width;
                flags |= LEFT;
            }
        }

        /* 获取输出精度 */
        precision = -1;
        if (*fmt == '.') {
            ++fmt;
            if (is_digit(*fmt)) {
                precision = skip_atoi(&fmt);
            } else if (*fmt == '*') {   // printf("%*.*s\n", a, b, s); /* 等同于printf("%a.bs\n", s) */
                precision = va_arg(args, int);  // 这里precision相当于取b的值
            }
            if (precision < 0) {
                precision = 0;
            }
        }

        /* 获取转换限定符 */
        qualifier = -1;
        if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L') {
            qualifier = *fmt;
            ++fmt;
        }
        switch (*fmt) {
            case 'c':
                if (!(flags & LEFT)) {
                    while (--field_width > 0) {
                        *str++ = ' ';
                    }
                }
                *str++ = (unsigned char) va_arg(args, int);
                while (--field_width > 0) {
                    *str++ = ' ';
                }
                break;

            case 's':
                s = va_arg(args, char *);
                len = strlen(s);
                if (precision < 0) {
                    precision = len;
                } else if (len > precision) {
                    len = precision;
                }
                if (!(flags & LEFT)) {
                    while (len < field_width--) {
                        *str++ = ' ';
                    }
                }
                for (i = 0; i < len; ++i) {
                    *str++ = *s++;
                }
                while (len < field_width--) {
                    *str++ = ' ';
                }
                break;

            case 'o':
                str = number(str, va_arg(args, unsigned long), 8,
                             field_width, precision, flags);
                break;

            case 'p':
                if (field_width == -1) {
                    field_width = 8;
                    flags |= ZEROPAD;
                }
                str = number(str,
                         (unsigned long) va_arg(args, void *), 16,
                         field_width, precision, flags);
                break;

            case 'x':
                flags |= SMALL;
            case 'X':
                str = number(str, va_arg(args, unsigned long), 16,
                         field_width, precision, flags);
                break;

            case 'd':
            case 'i':
                flags |= SIGN;
            case 'u':
                str = number(str, va_arg(args, unsigned long), 10,
                         field_width, precision, flags);
                break;

            case 'n':
                ip = va_arg(args, int *);
                *ip = (str - buf);
                break;

            default:
                if (*fmt != '%') {
                    *str++ = '%';
                }
                if (*fmt) {
                    *str++ = *fmt;
                } else {
                    --fmt;
                }
                break;
        }
    }
    *str = '\0';
    return str-buf;
}