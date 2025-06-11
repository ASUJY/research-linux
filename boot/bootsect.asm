BOOTSEG     equ 0x07c0      ;bootsect的原始加载段地址，BIOS会把bootsect加载到0x07c00处。
INITSEG     equ 0x9000      ;将bootsect复制到段地址0x9000处，即0x90000处。
SETUPLEN    equ 4           ;setup扇区数量。
SETUPSEG    equ 0x9020      ;将setup扇区加载到段地址0x09020处。
SYSSEG      equ 0x1000      ;将system模块加载到0x10000处
SYSSIZE     equ 0x3000      ;编译链接后system模块的大小
ENDSEG      equ SYSSEG + SYSSIZE

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
    mov ax, SYSSEG
    mov es, ax
    call read_it

    jmp SETUPSEG:0      ;跳转到setup.asm继续执行

;输入: es - 开始内存地址段值
read_it:
    mov ax, es
    test ax, 0x0fff         ;测试输入的段值。从盘上读入的数据必须存放在位于内存地址0x10000的边界开始处，否则进入死循环
die:
    jne die
    xor bx, bx              ;清空bx寄存器，用于表示当前段内存放数据的开始位置，即段内偏移位置

;判断是否已经读入全部数据。比较当前所读段是否就是系统数据末端所处的段(ENDSEG)
;如果不是就跳转至下面ok1_read标号处继续读数据。否则退出子程序返回。
rp_read:
    mov ax, es
    cmp ax, ENDSEG
    jb ok1_read
    ret

;计算和验证当前磁道需要读取的扇区数，放在ax寄存器中
;根据当前磁道还未读取的扇区数以及段内数据字节开始偏移位置，计算如果全部读取这些未读扇区所读总字节数
;是否会超过64KB段长度的限制。若会超过，则根据此次最多能读入的字节数(64KB-段内偏移位置)，反算出此次需要读取的扇区数
ok1_read:
    mov ax, [sectors]   ;取每磁道扇区数
    sub ax, [sread]     ;减去当前磁道已读取的扇区数
    mov cx, ax
    shl cx, 9           ;cx = cx * 512字节
    add cx, bx          ;cx = cx + 段内当前偏移值(bx) = 此次读操作后，段内共读入的字节数。
    jnc ok2_read        ;若没有超过64KB字节，则跳转至ok2_read处执行。
    je ok2_read
    xor ax, ax          ;若加上此次将要读取的磁道上的所有未读扇区时会超过64KB，则计算此时最多能读入的字节数(64KB-段内读偏移位置)
    sub ax, bx
    shr ax, 9           ;再转换成需要读取的扇区数
ok2_read:
    call read_track
    mov cx, ax          ;cx = 该次操作已读取的扇区数
    add ax, [sread]     ;当前磁道上已经读取的扇区数
    cmp ax, [sectors]   ;如果当前磁道上还有扇区未读，则跳转到ok3_read处
    jne ok3_read
    mov ax, 1           ;读该磁道的下一磁头面(1号磁头)上的数据。如果已经完成，则去读下一磁道。
    sub ax, [head]      ;判断当前磁头号
    jne ok4_read        ;如果是0磁头，则再去读1磁头面上的扇区数据
    inc word [track]    ;否则去读下一磁道
ok4_read:
    mov [head], ax      ;保存当前磁头号
    xor ax, ax          ;清空当前磁道已读扇区数
ok3_read:
    mov [sread], ax     ;保存当前磁道已读扇区数
    shl cx, 9           ;上次已读扇区数*512字节
    add bx, cx          ;调整当前段内数据开始位置
    jnc rp_read         ;若小于64KB边界值，则跳转到rp_read(156行)处，继续读数据。否则调整当前段，为读下一段数据做准备
    mov ax, es
    add ax, 0x1000      ;将段基址调整为指向下一个64KB内存开始处。
    mov es, ax
    xor bx, bx          ;清空段内数据开始偏移值
    jmp rp_read         ;跳转至rp_read（156行）处，继续读数据。

;读当前磁道上指定开始扇区和需要读取扇区数的数据到es:bx开始处
;al - 需要读取的扇区数量
;es:bx - 缓冲区开始位置
read_track:
    push ax
    push bx
    push cx
    push dx
    mov dx, [track]     ;读取当前磁道号
    mov cx, [sread]     ;读取当前磁道上已读的扇区数量
    inc cx              ;cl = 开始读取的扇区号
    mov ch, dl          ;ch = 当前磁道号
    mov dx, [head]      ;获取当前磁头号
    mov dh, dl          ;dh = 磁头号
    mov dl, 0x80        ;dl = 驱动器号(0x80表示当前读取硬盘)
    and dx, 0x0180      ;磁头号不大于1
    mov ah, 2           ;ah = 2, 读磁盘扇区功能号
    int 0x13
    jc bad_rt           ;若出错，则跳转至bad_rt
    pop dx
    pop cx
    pop bx
    pop ax
    ret

;执行驱动器复位操作(磁盘中断功能号0)，再跳转到read_track处重试
bad_rt:
    mov ax, 0x0000
    mov dx, 0x0080
    int 0x13
    pop dx
    pop cx
    pop bx
    pop ax
    jmp read_track



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

times 510 - ($ - $$) db 0
dw 0xAA55   ;MBR扇区标识