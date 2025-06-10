BOOTSEG     equ 0x07c0  ;bootsect的原始加载段地址，BIOS会把bootsect加载到0x07c00处。
INITSEG     equ 0x9000  ;将bootsect复制到段地址0x9000处，即0x90000处。
SETUPLEN    equ 4       ;setup扇区数量。
SETUPSEG    equ 0x9020  ;将setup扇区加载到段地址0x09020处。

[SECTION .text]
[BITS 16]
start:
    ;将自己(bootsect)从当前位置0x07c00复制到0x90000，共256字(512字节)，
    ;然后跳转到移动后代码的go标号处继续执行。
    mov ax, BOOTSEG
    mov ds, ax
    mov ax, INITSEG
    mov es, ax
    mov cx, 256
    sub si, si
    sub di, di
    rep movsw
    jmp INITSEG:go  ;CPU跳转到0x9000:go处继续执行代码，即继续执行mov ax,cs往下的代码。
go:
    mov ax, cs
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0xff00  ;将堆栈指针sp指向0x9000:0xff00处

load_setup:
    mov dx, 0x0080      ;Linux0.11原本是0x0000，但是这里改为0x0080，因为现在从硬盘读取数据，而不是从软盘读取数据
    mov cx, 0x0002
    mov bx, 0x0200
    mov ax, 0x0200 + SETUPLEN
    int 0x13            ;把硬盘中的数据加载到内存
    jnc ok_load_setup   ;如果加载成功，转到ok_load_setup继续执行
    mov dx, 0x0080      ;复位硬盘驱动器
    mov ax, 0x0000
    int 0x13
    jmp load_setup

ok_load_setup:
    mov dl, 0x80
    mov ax, 0x0800
    int 0x13            ;获取磁盘每扇区磁道数
    mov ch, 0x00
    mov [sectors], cx   ;保存磁盘每扇区磁道数
    mov ax, INITSEG
    mov es, ax          ;因为获取磁盘参数的时候 0x13中断 改掉了es的值，这里重新改回来


    ;在屏幕上打印msg1信息
    mov ah, 0x03
    xor bh, bh
    int 0x10        ;读取光标位置
    mov cx, 24
    mov bx, 0x0007
    mov bp, msg1    ;指向要显示的字符串
    mov ax, 0x1301
    int 0x10

    jmp SETUPSEG:0      ;跳转到setup.asm继续执行

sectors:
    dw 0                ;存放当前启动硬盘每磁道的扇区数

msg1:
    db 13, 10
    db "Loading system ..."
    db 13, 10, 13, 10

times 510 - ($ - $$) db 0
dw 0xAA55   ;MBR扇区标识