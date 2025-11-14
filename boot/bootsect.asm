BOOTSEG     equ 0x07c0      ;bootsect的原始加载段地址，BIOS会把bootsect加载到0x07c00处。
INITSEG     equ 0x9000      ;将bootsect复制到段地址0x9000处，即0x90000处。
SETUPLEN    equ 4           ;setup扇区数量。
SETUPSEG    equ 0x9020      ;将setup扇区加载到段地址0x09020处。
SYSSEG      equ 0x1000      ;将system模块加载到0x10000处
SYSSIZE     equ 0x5000      ;编译链接后system模块的大小
ENDSEG      equ SYSSEG + SYSSIZE

ROOT_DEV equ 0x306  ;0x306 第2块硬盘的第一个分区

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

;加载setup模块的代码到内存中（从硬盘的第2个扇区开始，把数据加载到内存0x90200处，共加载4个扇区，
;其实不用加载4个扇区，因为setup的大小 小于1个扇区）
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

    ;现在开始将system模块加载到0x10000处，即把硬盘第6个扇区开始往后的240个扇区加载到内存0x10000处
    ;将硬盘中的内核读取到0x1200开始的内存地址
    mov ax, SYSSEG
    mov es, ax
    mov di, 0
    mov cx, 5       ; 从硬盘中的第5个扇区开始读，硬盘的扇区从0开始编号
    mov bl, 250     ; 读取250个扇区
    call read_hd    ; 执行从硬盘指定位置读取数据到指定内存位置中的操作

    jmp SETUPSEG:0      ;跳转到setup.asm继续执行

;把硬盘中指定的扇区内容读入到指定的内存中,有以下几个入参
;di = 内存地址  bl = 读多少个扇区
;ecx = 从第几个扇区开始读数据
read_hd:
    mov dx, 0x1f2
    mov al, bl
    out dx, al

    inc dx
    mov al, cl
    out dx, al

    inc dx
    mov al, ch
    out dx, al

    inc dx
    shr cx, 16     ; shr：右移指令， 这里是把ecx中的数据右移16位
    mov al, cl
    out dx, al

    inc dx
    mov al, ch
    or al, 0b1110_0000
    out dx, al

    inc dx
    mov al, 0x20
    out dx, al

    ;读取多个扇区就要循环多少次
    mov cl, bl
.start_read:
    push cx
    call .wait_hd_prepare       ;等待数据准备完毕
    call read_hd_data           ;读取一个扇区
    pop cx
    loop .start_read
.return:
    ret

; 一直等待，直到硬盘的状态是：不繁忙，数据已准备好
; 即第7位为0，第3位为1，第0位为0
.wait_hd_prepare:
    mov dx, 0x1f7
.check:
    in al, dx
    and al, 0b1000_1001
    cmp al, 0b0000_1000
    jnz .check
    ret

; 读硬盘，一次读两个字节，读256次，刚好读一个扇区
read_hd_data:
    mov dx, 0x1f0
    mov cx, 256
.read_word:
    in ax, dx
    mov [es:di], ax
    add di, 2
    jnc .no_segment_adjust  ; 检查是否跨越段边界
    mov ax, es
    add ax, 0x1000  ;调整es寄存器到下一个内存段，因为一个内存段最大映射64KB地址，并且di最大值也是64KB
    mov es, ax
.no_segment_adjust:
    loop .read_word
    ret



sread:
    dw 1 + SETUPLEN     ;当前磁道中已读的扇区数。开始时已经读进1扇区的引导扇区。这里表示的是bootsect和setup所占的扇区数
head:
    dw 0                ;当前磁头号
track:
    dw 0                ;当前磁道号

sectors:
    dw 0                ;存放当前启动硬盘每磁道的扇区数

msg1:
    db 13, 10
    db "Loading system ..."
    db 13, 10, 13, 10

times 508 - ($ - $$) db 0
root_dev:
	dw ROOT_DEV
dw 0xAA55   ;MBR扇区标识