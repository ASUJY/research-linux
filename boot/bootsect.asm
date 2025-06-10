BOOTSEG equ 0x07c0  ;bootsect的原始加载段地址，BIOS会把bootsect加载到0x07c00处。
INITSEG equ 0x9000  ;将bootsect复制到段地址0x9000处，即0x90000处。

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

    ;在屏幕上打印msg1信息
    mov ah, 0x03
    xor bh, bh
    int 0x10        ;读取光标位置
    mov cx, 24
    mov bx, 0x0007
    mov bp, msg1    ;指向要显示的字符串
    mov ax, 0x1301
    int 0x10
    jmp $

msg1:
    db 13, 10
    db "Loading system ..."
    db 13, 10, 13, 10

times 510 - ($ - $$) db 0
dw 0xAA55   ;MBR扇区标识