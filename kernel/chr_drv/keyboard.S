#include <linux/config.h>

.text
.globl keyboard_interrupt

/*
* 以下这些变量用于键盘读操作
*/
size = 1024 /* 数值必须是2的次方！并且必须和tty_io.c中的值一样 */

head = 4
tail = 8
proc_list = 12
buf = 16

/*
* mode是键盘特殊键的按下状态标志。
* 表示大小写转换键(CpasLk/cpas-lock)、交换键(alt)、控制键(ctrl)和换挡键(shift)的状态
* 位7：caps键按下
* 位6：caps键的状态
* 位5：右alt键按下
* 位4：左alt键按下
* 位3：右ctrl键按下
* 位2：左ctrl键按下
* 位1：右shift键按下
* 位0：左shift键按下
*/
mode: .byte 0

/*
* 数字锁定键(num-lock，数字键盘的num键)、大小写转换键(cpas-lock)
* 和滚动锁定键(scroll-lock)的LED发光管状态。
* 位7-3 全0不用
* 位2 caps-lock
* 位1 num-lock(初始置1，也即设置数字锁定键发光管为亮)
* 位0 scroll-lock
*/
leds: .byte 2

/*
* 扩展扫描码标志，有些键盘按钮以扩展扫描码开头，例如方向键
* 位0=1:收到0xe0标志
* 位1=1:收到0xe1标志
*/
e0: .byte 0

keyboard_interrupt:
    pushl %eax
    pushl %ebx
    pushl %ecx
    pushl %edx
    push %ds
    push %es
    movl $0x10, %eax    /* 将ds、es段寄存器设置为内核数据段 */
    mov %ax, %ds
    mov %ax, %es
    xor %al, %al        /* 清零al，准备接收扫描码(键盘扫描码，键盘码) */
    inb $0x60, %al      /* 从键盘端口0x60读取扫描码 */
    cmpb $0xe0, %al     /* 检查是否为0xe0扩展码 */
    je set_e0
    cmpb $0xe1, %al
    je set_e1
    call key_table(,%eax,4) /* 调用键盘处理程序（key_table + eax * 4） */
    movb $0, e0
e0_e1:
    inb $0x61, %al  /* 读取键盘控制器状态(端口0x61) */
    jmp 1f
1:  jmp 1f
1:
    orb $0x80, %al  /* 设置ACK确认位（第七位）,置1表示禁止键盘工作 */
    jmp 1f
1:  jmp 1f
1:
    outb %al, $0x61 /* 写回键盘控制器 */
    jmp 1f
1:  jmp 1f
1:
    andb $0x7f, %al /* 清除ACK位(位7)，启用键盘 */
    outb %al, $0x61 /* 写回键盘控制器 */
    movb $0x20, %al /* 准备EOI指令 */
    outb %al, $0x20 /* 向8259A芯片发送EOI(中断结束)信号 */
    pushl $0        /* 参数：tty设备号(0=控制台) */
    call do_tty_interrupt
    addl $4, %esp   /* 丢弃入栈的参数(pushl $0)，平栈 */
    pop %es
    pop %ds
    popl %edx
    popl %ecx
    popl %ebx
    popl %eax
    iret
set_e0:
    movb $1, e0
    jmp e0_e1
set_e1:
    movb $2, e0
    jmp e0_e1

put_queue:
    pushl %ecx
    pushl %edx
    movl table_list, %edx   /* 加载控制台tty中的读缓冲队列地址到edx */
    movl head(%edx), %ecx   /* 获取缓冲队列头指针head到ecx */
1:
    movb %al, buf(%edx, %ecx)   /* 将al中的字符写入缓冲队列中[edx + buf + ecx] */
    incl %ecx                   /* 头指针ecx前移一个字节（指向下一个位置） */
    andl $size-1, %ecx          /* 对头指针取模(size是缓冲区大小，确保循环队列) */
    cmpl tail(%edx), %ecx       /* 比较头指针和尾指针tail */
    je 3f                       /* 如果队列满（head == tail），跳转到标签3，即后面未放入的字符全部抛弃 */
    shrdl $8, %ebx, %eax        /* 双寄存器右移8位：ebx 低8位移入 eax 高8位，eax 整体右移8位，ebx不变 */
    je 2f                       /*  若移位后 eax 为0（无数据，即没有字符），跳转到标签 2（更新头指针） */
    shrl $8, %ebx               /* 将ebx中的比特位右移8位（准备下一字节） */
    jmp 1b                      /* 跳到标签1，继续写入下一个字节 */
2:
    movl %ecx, head(%edx)           /* 若已将所有字符都放入了队列，更新队列头指针 head 为新位置 */
    movl proc_list(%edx), %ecx    /* 获取等待队列的进程控制块指针 */
    testl %ecx, %ecx              /* 测试指针是否为空（有等待该队列的进程吗？） */
    je 3f                           /* 为空则跳转到标签 3（无需唤醒） */
    movl $0, (%ecx)               /* 有等待该队列的进程，则唤醒进程(向进程控制块写入0)，置该进程为可运行就绪状态 */
3:
    popl %edx
    popl %ecx
    ret

ctrl:
    movb $0x04, %al /* 加载ctrl的位掩码到al，0x4是模式标志mode中左ctrl键对应的比特位(位2) */
    jmp 1f
alt:
    movb $0x10, %al /* 加载alt的位掩码到al，0x10是模式标志mode中左alt键对应的比特位 */
1:
    cmpb $0, e0     /* e0标志置位了吗（按下的是右边的ctrl或alt键）？ */
    je 2f           /* 不是则跳到标签2执行 */
    addb %al, %al   /* 是，则改成置相应右键的标志位(位3或位5) */
2:
    orb %al, mode   /* 将al的位设置到mode中 */
    ret
unctrl:
    movb $0x04, %al
    jmp 1f
unalt:
    movb $0x10, %al
1:
    cmpb $0, e0
    je 2f
    addb %al, %al
2:
    notb %al        /* 将 AL 按位取反 */
    andb %al, mode  /* 用取反后的 AL 清除 mode 中的对应位 */
    ret

lshift:
    orb $0x01, mode     /* 将 mode 的第0位设置为1（表示左shift键按下） */
    ret
unlshift:
    andb $0xfe, mode    /* 将 mode 的第0位清零（表示左shift键松开） */
    ret
rshift:
    orb $0x02, mode     /* 将 mode 的第1位设置为1（表示右shift键按下） */
    ret
unrshift:
    andb $0xfd, mode    /* 将 mode 的第1位清零（表示右shift键松开） */
    ret

caps:
    testb $0x80, mode   /* 测试mode的最高位是否已经置位 */
    jne 1f              /* 如果最高位已置1(表示CapsLock已激活)，则跳转到标签1 */
    xorb $4, leds       /* 翻转leds变量的第2位(00000100)，对应CapsLock LED */
    xorb $0x40, mode    /* 翻转mode变量的第6位，对应CapsLock */
    orb $0x80, mode     /* 设置mode变量的最高位(10000000)，标记CapsLock激活(caps键已按下) */
set_leds:               /* 根据leds标志，开启或关闭LED指示器 */
    call kb_wait        /* 等待键盘控制器就绪 */
    movb $0xed, %al     /* 加载设置LED命令码(0xED)到AL寄存器 */
    outb %al, $0x60     /* 发送键盘命令0xed到0x60端口 */
    call kb_wait        /* 等待键盘控制器就绪 */
    movb leds, %al      /* 加载当前LED状态到AL寄存器 */
    outb %al, $0x60     /* 向端口0x60发送LED状态字节 */
    ret
uncaps:
    andb $0x7f, mode    /* 清除mode变量的最高位，表示CapsLock键已松开 */
    ret
scroll:
    xorb $1, leds       /* 翻转leds变量的第0位，对应ScrollLock LED，表示scroll键已按下 */
    jmp set_leds        /* 根据leds标志重新开启或关闭LED指示器 */
num:
    xorb $2, leds       /* 翻转leds变量的第1位，对应NumLock LED，表示num键已按下 */
    jmp set_leds

/*
* 处理小键盘按键，例如方向键(光标控制键)、数字小键盘
*/
cursor:
    subb $0x47, %al     /* AL 减去 0x47（小键盘基础扫描码） */
    jb 1f               /* 若 AL < 0（无效扫描码）则跳转到标签 1（返回） */
    cmpb $12, %al
    ja 1f               /* 若 AL > 12（无效扫描码）则跳转到标签 1（返回） */
    jne cur2            /* 若 AL ≠ 12（非小键盘"/"键）则跳转到 cur2 */
    /* 检查 Ctrl-Alt-Del 组合键，如果等于12，则说明del键已被按下，
     * 继续判断ctrl和alt是否也同时按下
    */
    testb $0x0c, mode   /* 测试 mode 的位 2(0x04) 和 位 3(0x08)（Ctrl+Alt） */
    je cur2             /* 若未同时按下 Ctrl+Alt，跳转到 cur2 */
    testb $0x30, mode   /* 测试 mode 的位 4(0x10) 和 位 5(0x20)（特殊状态） */
    jne reboot          /* 若满足条件则触发系统重启 */
cur2:
    cmpb $0x01, e0      /* e0置位表示光标移动 */
    je cur              /* 若 e0=1 则跳转到 cur（光标移动处理模块） */
    testb $0x02, leds   /* 测试 leds 的位 1(num-lock键）是否置位 */
    je cur              /* 如果没有置位(numlock的led不亮)，则跳转到 cur */
    testb $0x03, mode   /* 测试 mode 的位 0和 位 1（Shift是否被按下） */
    jne cur             /* 如果有shift键按下，则也跳转到cur */
    /* 数字模式处理 */
    xorl %ebx, %ebx             /* 清空 EBX（put_queue 参数） */
    movb num_table(%eax), %al   /* 从 num_table 加载对应字符 */
    jmp put_queue               /* 将字符放入缓冲队列中 */
1:
    ret

/* 处理光标的移动 */
cur:
    movb cur_table(%eax), %al   /* 从 cur_table 加载光标控制字符 */
    cmpb $'9, %al               /* 比较 AL 和 ASCII '9'，大于9则说明是上一页、下一页、插入或删除键 */
    ja ok_cur                   /* 若 AL > '9'（字母字符）则跳转到 ok_cur */
    movb $'~, %ah               /* 为数字字符添加前缀 '~'（构造转义序列） */
ok_cur:
    shll $16, %eax              /* 将 EAX 左移 16 位（将ax的内容移到eax高字节中） */
    movw $0x5b1b, %ax           /* 在ax中放入'esc ['字符，与eax高字的字符组成光标移动序列 */
    xorl %ebx, %ebx             /* 清空 EBX（put_queue 参数） */
    jmp put_queue               /* 将字符放入缓冲队列中 */

/* 数字映射表（地区相关） */
#if defined(KBD_FR)
num_table:
    .ascii "789 456 1230."  /* 法语布局（使用点号） */
#else
num_table:
    .ascii "789 456 1230,"  /* 其他布局（使用逗号） */
#endif
/* 光标控制映射表 */
cur_table:
    .ascii "HA5 DGC YB623"

/* 处理功能键(F1-F12) */
func:
    pushl %eax
    pushl %ecx
    pushl %edx
    // call show_stat      /* 调用显示各任务状态函数 */
    popl %edx
    popl %ecx
    popl %eax
    subb $0x3B, %al /* 功能键F1的扫描码是0x3B，此时的al中是功能键索引号 */
    jb end_func     /* 若结果<0（非功能键），跳转到end_func返回 */
    cmpb $9, %al    /* 比较al和9（处理F1-F10）。 */
    jbe ok_func     /* 若al≤9（F1-F10），跳转到ok_func。 */
    subb $18, %al   /* F11/F12处理：扫描码F11=87, F12=88，减去59后为28/29，再减18得10/11。 */
    cmpb $10, %al   /* 比较al和10（F11对应值） */
    jb end_func     /* 若al<10（不是F11），跳转到end_func */
    cmpb $11, %al   /* 比较al和11（F12对应值） */
    ja end_func     /* 若al>11（不是F12），跳转到end_func。 */
ok_func:
    cmpl $4, %ecx   /* 比较ecx和4（检查缓冲区剩余空间） */
    jl end_func     /* 若ecx<4（空间不足，因为需要放入4个字符序列），跳转到end_func。 */
    movl func_table(,%eax,4), %eax  /* 以eax为索引，从func_table加载功能键对应的4字节转义序列到eax，func_table + eax * 4 */
    xorl %ebx, %ebx /* 清空 EBX（put_queue 参数） */
    jmp put_queue   /* 将字符放入缓冲队列中 */
end_func:
    ret

/*
 * 功能键发送的扫描码， F1:'esc [ [ A' F2:'esc [ [ B' etc.
 */
func_table:
	.long 0x415b5b1b, 0x425b5b1b, 0x435b5b1b, 0x445b5b1b
	.long 0x455b5b1b, 0x465b5b1b, 0x475b5b1b, 0x485b5b1b
	.long 0x495b5b1b, 0x4a5b5b1b, 0x4b5b5b1b, 0x4c5b5b1b

/*
* 根据在config.h中定义的键盘类型(FINNISH, US, GERMEN, FRANCH)，将相应的按键的扫描码
* 映射到ASCII字符。
*/
/* 芬兰键盘布局 */
#if	defined(KBD_FINNISH)
key_map:
	.byte 0,27
	.ascii "1234567890+'"
	.byte 127,9
	.ascii "qwertyuiop}"
	.byte 0,13,0
	.ascii "asdfghjkl|{"
	.byte 0,0
	.ascii "'zxcvbnm,.-"
	.byte 0,'*,0,32		/* 36-39 */
	.fill 16,1,0		/* 3A-49 */
	.byte '-,0,0,0,'+	/* 4A-4E */
	.byte 0,0,0,0,0,0,0	/* 4F-55 */
	.byte '<
	.fill 10,1,0

shift_map:
	.byte 0,27
	.ascii "!\"#$%&/()=?`"
	.byte 127,9
	.ascii "QWERTYUIOP]^"
	.byte 13,0
	.ascii "ASDFGHJKL\\["
	.byte 0,0
	.ascii "*ZXCVBNM;:_"
	.byte 0,'*,0,32		/* 36-39 */
	.fill 16,1,0		/* 3A-49 */
	.byte '-,0,0,0,'+	/* 4A-4E */
	.byte 0,0,0,0,0,0,0	/* 4F-55 */
	.byte '>
	.fill 10,1,0

alt_map:        /* alt键同时按下的映射表 */
	.byte 0,0   /* 扫描码 0x00-0x01 保留（通常为错误码或保留位）,前两个字节填充 0（扫描码 0x00 和 0x01 无映射）*/
	.ascii "\0@\0$\0\0{[]}\\\0"
	.byte 0,0
	.byte 0,0,0,0,0,0,0,0,0,0,0
	.byte '~,13,0
	.byte 0,0,0,0,0,0,0,0,0,0,0
	.byte 0,0
	.byte 0,0,0,0,0,0,0,0,0,0,0
	.byte 0,0,0,0		/* 36-39 */
	.fill 16,1,0		/* 3A-49 */
	.byte 0,0,0,0,0		/* 4A-4E */
	.byte 0,0,0,0,0,0,0	/* 4F-55 */
	.byte '|
	.fill 10,1,0

/* 美式键盘布局 */
#elif defined(KBD_US)

key_map:
	.byte 0,27
	.ascii "1234567890-="
	.byte 127,9
	.ascii "qwertyuiop[]"
	.byte 13,0
	.ascii "asdfghjkl;'"
	.byte '`,0
	.ascii "\\zxcvbnm,./"
	.byte 0,'*,0,32		/* 36-39 */
	.fill 16,1,0		/* 3A-49 */
	.byte '-,0,0,0,'+	/* 4A-4E */
	.byte 0,0,0,0,0,0,0	/* 4F-55 */
	.byte '<
	.fill 10,1,0

shift_map:
	.byte 0,27
	.ascii "!@#$%^&*()_+"
	.byte 127,9
	.ascii "QWERTYUIOP{}"
	.byte 13,0
	.ascii "ASDFGHJKL:\""
	.byte '~,0
	.ascii "|ZXCVBNM<>?"
	.byte 0,'*,0,32		/* 36-39 */
	.fill 16,1,0		/* 3A-49 */
	.byte '-,0,0,0,'+	/* 4A-4E */
	.byte 0,0,0,0,0,0,0	/* 4F-55 */
	.byte '>
	.fill 10,1,0

alt_map:
	.byte 0,0
	.ascii "\0@\0$\0\0{[]}\\\0"
	.byte 0,0
	.byte 0,0,0,0,0,0,0,0,0,0,0
	.byte '~,13,0
	.byte 0,0,0,0,0,0,0,0,0,0,0
	.byte 0,0
	.byte 0,0,0,0,0,0,0,0,0,0,0
	.byte 0,0,0,0		/* 36-39 */
	.fill 16,1,0		/* 3A-49 */
	.byte 0,0,0,0,0		/* 4A-4E */
	.byte 0,0,0,0,0,0,0	/* 4F-55 */
	.byte '|
	.fill 10,1,0

/* 德语键盘布局 */
#elif defined(KBD_GR)

key_map:
	.byte 0,27
	.ascii "1234567890\\'"
	.byte 127,9
	.ascii "qwertzuiop@+"
	.byte 13,0
	.ascii "asdfghjkl[]^"
	.byte 0,'#
	.ascii "yxcvbnm,.-"
	.byte 0,'*,0,32		/* 36-39 */
	.fill 16,1,0		/* 3A-49 */
	.byte '-,0,0,0,'+	/* 4A-4E */
	.byte 0,0,0,0,0,0,0	/* 4F-55 */
	.byte '<
	.fill 10,1,0

shift_map:
	.byte 0,27
	.ascii "!\"#$%&/()=?`"
	.byte 127,9
	.ascii "QWERTZUIOP\\*"
	.byte 13,0
	.ascii "ASDFGHJKL{}~"
	.byte 0,''
	.ascii "YXCVBNM;:_"
	.byte 0,'*,0,32		/* 36-39 */
	.fill 16,1,0		/* 3A-49 */
	.byte '-,0,0,0,'+	/* 4A-4E */
	.byte 0,0,0,0,0,0,0	/* 4F-55 */
	.byte '>
	.fill 10,1,0

alt_map:
	.byte 0,0
	.ascii "\0@\0$\0\0{[]}\\\0"
	.byte 0,0
	.byte '@,0,0,0,0,0,0,0,0,0,0
	.byte '~,13,0
	.byte 0,0,0,0,0,0,0,0,0,0,0
	.byte 0,0
	.byte 0,0,0,0,0,0,0,0,0,0,0
	.byte 0,0,0,0		/* 36-39 */
	.fill 16,1,0		/* 3A-49 */
	.byte 0,0,0,0,0		/* 4A-4E */
	.byte 0,0,0,0,0,0,0	/* 4F-55 */
	.byte '|
	.fill 10,1,0

/* 法语键盘布局 */
#elif defined(KBD_FR)

key_map:
	.byte 0,27
	.ascii "&{\"'(-}_/@)="
	.byte 127,9
	.ascii "azertyuiop^$"
	.byte 13,0
	.ascii "qsdfghjklm|"
	.byte '`,0,42		/* coin sup gauche, don't know, [*|mu] */
	.ascii "wxcvbn,;:!"
	.byte 0,'*,0,32		/* 36-39 */
	.fill 16,1,0		/* 3A-49 */
	.byte '-,0,0,0,'+	/* 4A-4E */
	.byte 0,0,0,0,0,0,0	/* 4F-55 */
	.byte '<
	.fill 10,1,0

shift_map:
	.byte 0,27
	.ascii "1234567890]+"
	.byte 127,9
	.ascii "AZERTYUIOP<>"
	.byte 13,0
	.ascii "QSDFGHJKLM%"
	.byte '~,0,'#
	.ascii "WXCVBN?./\\"
	.byte 0,'*,0,32		/* 36-39 */
	.fill 16,1,0		/* 3A-49 */
	.byte '-,0,0,0,'+	/* 4A-4E */
	.byte 0,0,0,0,0,0,0	/* 4F-55 */
	.byte '>
	.fill 10,1,0

alt_map:
	.byte 0,0
	.ascii "\0~#{[|`\\^@]}"
	.byte 0,0
	.byte '@,0,0,0,0,0,0,0,0,0,0
	.byte '~,13,0
	.byte 0,0,0,0,0,0,0,0,0,0,0
	.byte 0,0
	.byte 0,0,0,0,0,0,0,0,0,0,0
	.byte 0,0,0,0		/* 36-39 */
	.fill 16,1,0		/* 3A-49 */
	.byte 0,0,0,0,0		/* 4A-4E */
	.byte 0,0,0,0,0,0,0	/* 4F-55 */
	.byte '|
	.fill 10,1,0

#else
#error "KBD-type not defined"
#endif

/*
* do_self用于处理普通键，也即含义没有变化并且只有一个字符返回的键
* 例如字母/数字/符号
*/
do_self:
    lea alt_map, %ebx   /* 将alt_map(Alt键映射表)的地址加载到EBX寄存器，作为默认映射表 */
    testb $0x20, mode   /* 测试mode变量的第5位，检查右Alt键(alt-gr)是否按下 */
    jne 1f              /* 如果按下了，则跳转到标签1处。 */
    lea shift_map, %ebx /* (AltGr未按下时) 加载shift_map（Shift键映射表）地址到EBX。 */
    testb $0x03, mode   /* 测试mode的低两位(0x03)，检测左/右Shift键是否按下。 */
    jne 1f              /* 如果Shift键按下（非零），跳转到标签1处 */
    lea key_map, %ebx   /* （无修饰键时）加载默认key_map(普通映射表)地址到EBX。 */
1:
    movb (%ebx, %eax), %al  /* [%ebx + %eax]，用EAX中的扫描码索引映射表，将对应的ASCII字符存入AL。 */
    orb %al, %al
    je none                 /* 检查al的值是否为0（无效键），若是，则跳转到none标签 */
    /* 若ctrl键已按下或caps键锁定，并且字符在'a'-'}'(0x61-0x7D)范围内，则将其转出大写字符(0x41-0x5D) */
    testb $0x4c, mode   /* 测试mode的第2/3/6位(0x4C)，检测Ctrl或CapsLock是否激活 */
    je 2f               /* 如果未激活，跳转到标签2 */
    cmpb $'a, %al
    jb 2f               /* 如果AL < 'a'，跳转到2:（非小写字母不处理）。 */
    cmpb $'}, %al
    ja 2f               /* 如果AL > '}'，跳转到2:（超出小写字母范围）。 */
    subb $32, %al       /* 将小写字母转成大写字母 */
/* 若ctrl键按下，并且字符在'`'--'_'(0x40-0x5F)之间，则将其转换为控制字符 */
2:
    testb $0x0c, mode   /* 测试mode的第2/3位(0x0C)，检测Ctrl键是否按下。 */
    je 3f               /* 未按下，跳转到标签3 */
    cmpb $64, %al
    jb 3f               /* 如果AL < 64(ASCII '@')，跳转到3 */
    cmpb $64+32, %al
    jae 3f              /* 如果AL >= 96(64+32，小写字母起始前），跳转到3 */
    subb $64, %al       /* 生成Ctrl组合键值（例如A=65 → Ctrl+A=1）,将字符转换为0x00-0x1f之间的控制字符 */
/* 若左Alt键同时按下，则将字符的位7置位 */
3:
    testb $0x10, mode   /* 测试mode的第4位(0x10)，检测左Alt键是否按下 */
    je 4f               /* 未按下，跳转到标签4 */
    orb $0x80, %al      /* 设置AL的最高位(0x80)，生成Alt组合键的扩展ASCII */
4:
    andl $0xff, %eax    /* 确保EAX高24位清零，仅保留AL的有效值 */
    xorl %ebx, %ebx     /* 清零EBX */
    call put_queue      /* 调用put_queue，将AL中的字符放入输入队列 */
none:
    ret

/*
* 减号有它自己的处理程序，因为在减号扫描码之前的0xe0意味着按下了数字小键盘上的斜杠键
*/
minus:
    cmpb $1, e0
    jne do_self     /* 如果e0 ≠ 1（即普通减号键），跳转到do_self（通用键处理例程） */
    movl $'/, %eax  /* 否则用'/'替换减号'-' */
    xorl %ebx, %ebx
    jmp put_queue

/*
 * 以下是一张子程序地址跳转表。当取得扫描码后就根据此表调用相应的扫描码处理子程序。
 * 大多数调用的子程序是do_self，或者是none，这取决于是按键(make)还是释放键(break)
 *
 */
key_table:
	.long none, do_self, do_self, do_self	    /* 00-03 s0 esc 1 2 */
	.long do_self, do_self, do_self, do_self	/* 04-07 3 4 5 6 */
	.long do_self, do_self, do_self, do_self	/* 08-0B 7 8 9 0 */
	.long do_self, do_self, do_self, do_self	/* 0C-0F + ' bs tab */
	.long do_self, do_self, do_self, do_self	/* 10-13 q w e r */
	.long do_self, do_self, do_self, do_self	/* 14-17 t y u i */
	.long do_self, do_self, do_self, do_self	/* 18-1B o p } ^ */
	.long do_self, ctrl, do_self, do_self	    /* 1C-1F enter ctrl a s */
	.long do_self, do_self, do_self, do_self	/* 20-23 d f g h */
	.long do_self, do_self, do_self, do_self	/* 24-27 j k l | */
	.long do_self, do_self, lshift, do_self	    /* 28-2B { para lshift , */
	.long do_self, do_self, do_self, do_self	/* 2C-2F z x c v */
	.long do_self, do_self, do_self, do_self	/* 30-33 b n m , */
	.long do_self, minus, rshift, do_self	    /* 34-37 . - rshift * */
	.long alt, do_self, caps, func		/* 38-3B alt sp caps f1 */
	.long func, func, func, func		/* 3C-3F f2 f3 f4 f5 */
	.long func, func, func, func		/* 40-43 f6 f7 f8 f9 */
	.long func, num, scroll, cursor		/* 44-47 f10 num scr home */
	.long cursor, cursor, do_self, cursor	/* 48-4B up pgup - left */
	.long cursor, cursor, do_self, cursor	/* 4C-4F n5 right + end */
	.long cursor, cursor, cursor, cursor	/* 50-53 dn pgdn ins del */
	.long none, none, do_self, func		/* 54-57 sysreq ? < f11 */
	.long func, none, none, none		/* 58-5B f12 ? ? ? */
	.long none, none, none, none		/* 5C-5F ? ? ? ? */
	.long none, none, none, none		/* 60-63 ? ? ? ? */
	.long none, none, none, none		/* 64-67 ? ? ? ? */
	.long none, none, none, none		/* 68-6B ? ? ? ? */
	.long none, none, none, none		/* 6C-6F ? ? ? ? */
	.long none, none, none, none		/* 70-73 ? ? ? ? */
	.long none, none, none, none		/* 74-77 ? ? ? ? */
	.long none, none, none, none		/* 78-7B ? ? ? ? */
	.long none, none, none, none		/* 7C-7F ? ? ? ? */
	.long none, none, none, none		/* 80-83 ? br br br */
	.long none, none, none, none		/* 84-87 br br br br */
	.long none, none, none, none		/* 88-8B br br br br */
	.long none, none, none, none		/* 8C-8F br br br br */
	.long none, none, none, none		/* 90-93 br br br br */
	.long none, none, none, none		/* 94-97 br br br br */
	.long none, none, none, none		/* 98-9B br br br br */
	.long none, unctrl, none, none		/* 9C-9F br unctrl br br */
	.long none, none, none, none		/* A0-A3 br br br br */
	.long none, none, none, none		/* A4-A7 br br br br */
	.long none, none, unlshift, none	/* A8-AB br br unlshift br */
	.long none, none, none, none		/* AC-AF br br br br */
	.long none, none, none, none		/* B0-B3 br br br br */
	.long none, none, unrshift, none	/* B4-B7 br br unrshift br */
	.long unalt, none, uncaps, none		/* B8-BB unalt br uncaps br */
	.long none, none, none, none		/* BC-BF br br br br */
	.long none, none, none, none		/* C0-C3 br br br br */
	.long none, none, none, none		/* C4-C7 br br br br */
	.long none, none, none, none		/* C8-CB br br br br */
	.long none, none, none, none		/* CC-CF br br br br */
	.long none, none, none, none		/* D0-D3 br br br br */
	.long none, none, none, none		/* D4-D7 br br br br */
	.long none, none, none, none		/* D8-DB br ? ? ? */
	.long none, none, none, none		/* DC-DF ? ? ? ? */
	.long none, none, none, none		/* E0-E3 e0 e1 ? ? */
	.long none, none, none, none		/* E4-E7 ? ? ? ? */
	.long none, none, none, none		/* E8-EB ? ? ? ? */
	.long none, none, none, none		/* EC-EF ? ? ? ? */
	.long none, none, none, none		/* F0-F3 ? ? ? ? */
	.long none, none, none, none		/* F4-F7 ? ? ? ? */
	.long none, none, none, none		/* F8-FB ? ? ? ? */
	.long none, none, none, none		/* FC-FF ? ? ? ? */

/*
* 用于等待键盘控制器（8042芯片）准备就绪（等待键盘控制器缓冲区变空）。
* 它确保在发送命令或数据到键盘控制器之前，其输入缓冲区为空。
* 这是一个阻塞等待，没有超时机制（若缓冲区永不空，系统会挂起）。
*/
kb_wait:
    pushl %eax
1:
    inb $0x64, %al      /* 从0x64端口读取键盘控制器的状态 */
    testb $0x02, %al    /* 测试状态寄存器的位1，该位表示键盘输入缓冲区状态；1：缓冲区满（控制器忙，不能接收新数据）；0：缓冲区空（控制器就绪） */
    jne 1b              /* 若状态寄存器bit 1 ≠ 0（缓冲区非空），跳转回标签1 */
    popl %eax
    ret

/*
* 通过设置键盘控制器(8042芯片)，触发硬件复位信号来实现系统重启
*/
reboot:
    call kb_wait        /* 确保键盘控制器就绪（输入缓冲区为空），可以接收新命令 */
    movw $0x1234, 0x472 /* 设置BIOS启动标志，0x472是BIOS数据区中的启动标志地址，0x1234告诉BIOS跳过内存检查（加快启动） */
    movb $0xfc, %al     /* 0xfc是发送给键盘控制器（端口0x64）的复位命令 */
    outb %al, $0x64
die:
    jmp die             /* 如果复位命令未立即生效（硬件故障），系统挂在此处 */
