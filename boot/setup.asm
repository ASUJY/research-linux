;setup.s负责从BIOS中获取系统数据，并将这些数据放到系统内存的适当地方。
;此时setup.s已经由bootsect加载到内存中。

;这段代码询问bios有关内存/磁盘/其它参数，并将这些参数放到一个
;“安全的”地方：0x90000-0x901FF，也即原来bootsect代码块曾经在
;的地方，然后在被缓冲块覆盖掉之前由保护模式的system读取。

[SECTION .data]
INITSEG     equ 0x9000  ;bootsect所处的内存位置(段地址)
SETUPSEG    equ 0x9020  ;setup.asm所在的段地址

[SECTION .text]
start:
    mov ax, INITSEG
    mov ds, ax
    mov ah, 0x03
    xor bh, bh
    int 0x10        ;读取光标位置，保存以备今后使用
    mov [0], dx     ;将光标位置信息存放在0x90000处，控制台初始化时会来取

    ;获取从0x10000(1M)处开始扩展的内存的大小(KB)
    mov ah, 0x88
    int 0x15
    mov [2], ax     ;将扩展内存数值存放在0x90002处

    ;获取显卡当前的显示模式, 0x90004存放当前页，0x90006存放显示模式，0x90007存放字符列数
    mov ah, 0x0f
    int 0x10
    mov [4], bx     ;bh = 当前显示页
    mov [6], ax     ;ah = 字符列数（例如 0x50 表示80列） al = 显示模式，0x03 表示80x25彩色文本模式

    ;获取显示方式(EGA/VGA)并获取参数
    mov ah, 0x12
    mov bl, 0x10
    int 0x10
    mov [8], ax
    mov [10], bx    ;0x9000A存放安装的显存的大小，0x9000B存放显示状态(彩色/单色)
    mov [12], cx    ;显卡特性参数

    ;获取第一个硬盘的信息
    mov ax, 0x0000
    mov ds, ax
    lds si, [4 * 0x41]  ;获取0x0000:0x0104中的值，保存到ds和si中，也即hd0参数表的地址保存到ds:si中
    mov ax, INITSEG
    mov es, ax
    mov di, 0x0080      ;传输的目的地址: 0x9000:0x0080（es:di）
    mov cx, 0x10        ;共传输0x10个字节
    rep movsb

    ;获取第二个硬盘的信息
    mov ax, 0x0000
    mov ds, ax
    lds si, [4 * 0x46]
    mov ax, INITSEG
    mov es, ax
    mov di, 0x0090
    mov cx, 0x10
    rep movsb

    ;检查系统是否存在第2个磁盘，如果不存在则第2个表清零
    mov ax, 0x1500
    mov dl, 0x81
    int 0x13
    jc no_disk1
    cmp ah, 3              ;是硬盘吗?
    je is_disk1            ;可以看出，如果不是硬盘，也没有任何防范措施。
no_disk1:
    mov ax, INITSEG ;第2个磁盘不存在，则对保存到0x90090的第2个磁盘参数表清零
    mov es, ax
    mov di, 0x0090
    mov cx, 0x10
    mov ax, 0x00
    rep stosb
is_disk1:
    ;现在要进入保护模式了
    cli     ;此时不允许中断

    mov ax, SETUPSEG
    mov ds, ax
    lidt [idt_48]   ;加载中断描述符表寄存器(IDTR)
    lgdt [gdt_48]   ;加载全局描述符表寄存器(GDTR)

    ;开启A20地址线
    call empty_8042
    mov al, 0xD1
    out 0x64, al
    call empty_8042
    mov al, 0xDF
    out 0x60, al
    call empty_8042

    jmp $

empty_8042:
    dw 0x00eb, 0x00eb   ;相当于空转指令（NOP操作），确保8042控制器有足够时间响应
    in al, 0x64         ;读取8042状态寄存器
    test al, 2          ;检查状态寄存器的第二位是否为1
    jnz empty_8042      ;如果为1，表示缓冲区非空，需要继续等待
    ret                 ;缓冲区为空，确保后续发送给8042键盘控制器的数据不会覆盖未处理的数据

; 全局描述符表，描述符表由多个8字节长的描述符项组成。
; 这里有3个描述符项。第1项无用，但必须存在。
; 第2项是内核代码段描述符，第3项是内核数据段描述符。
gdt:
    dw 0, 0, 0, 0

CODE_DESCRIPTOR:    ;这里在gdt表中的偏移量为0x08，当加载代码段寄存器（段选择符）时，使用的是这个偏移值。
    dw 0x07FF
    dw 0x0000
    dw 0x9A00
    dw 0x00C0

DATA_DESCRIPTOR:    ;这里在gdt表中的偏移量是0x10，当加载数据段寄存器（ds）时，使用的是这个偏移值。
    dw 0x07FF
    dw 0x0000
    dw 0x9200
    dw 0x00C0

idt_48:
    dw 0
    dw 0, 0

gdt_48:
    dw 0x800            ;gdt的大小, 256 GDT entries
    dw 512+gdt, 0x9     ;gdt的位置 = 0x90200 + gdt