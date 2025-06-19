;head是汇编进入内核的一个入口，head.asm含有32位启动代码。
;注意！32位启动代码是从绝对地址0x00000000开始的，这里也同样是页目录将存在的地方，
;因此这里的启动代码将被页目录覆盖掉。

[SECTION .text]
[BITS 32]
extern main
extern stack_start
global _idt
global startup_32
startup_32:
    ;这里$0x10的含义是请求特权级0（位0-1=0）、选择全局描述符表（位2=0）、选择表中第2项（位3-15=2）。
    ;它正好指向表中的数据段描述符项。
    ;置ds，es，fs，gs中的选择符为setup.asm中构造的数据段（全局段描述符表的第2项）=0x10
    mov eax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    lss esp, [stack_start]  ;设置内核的栈段

    call setup_idt  ;设置中断描述符
    call setup_gdt  ;设置全局描述符

    mov eax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    lss esp, [stack_start]

    jmp after_page_tables
    ;call main
    ;jmp $

;下面这段是设置中断描述符表子程序setup_idt
;将中断描述符表idt设置成具有256个项，并都指向ignore_int中断门。
;然后加载中断描述符表寄存器（用lidt指令）。
;真正实用的中断门以后再安装。当我们在其它地方认为一切都正常时再开启中断。
;该子程序将会被页表覆盖掉。
setup_idt:
    lea edx, [ignore_int]   ;将ignore_int的有效地址(偏移值)值保存到edx寄存器
    mov eax, 0x00080000     ;将选择符0x0008置入eax的高16位中
    mov ax, dx              ;selector = 0x0008 = cs
                            ;偏移值的低16位置入eax的低16位中。此时eax含有门描述符低4字节的值
    mov dx, 0x8E00          ;interrupt gate - dpl = 0, present
                            ;此时edx含有门描述符高4字节的值
    lea edi, [_idt]         ;_idt是中断描述符表的地址
    mov ecx, 256
rp_sidt:
    mov [edi], eax          ;将哑中断门描述符存入表中
    mov [edi + 4], edx
    add edi, 8              ;edi指向表中下一项
    dec ecx
    jne rp_sidt
    lidt [idt_descr]        ;加载中断描述符表
    ret

;设置全局描述符表项setup_gdt
;这个子程序设置一个新的全局描述符表gdt，并加载。此时仅创建了两个表项，与前面的一样。
;该子程序将被页表覆盖掉。
setup_gdt:
    lgdt [gdt_descr]        ;加载全局描述符表寄存器
    ret

after_page_tables:
    push 0  ;envp
    push 0  ;argv
    push 0  ;argc
    push L6     ;main函数退出时会到L6处继续执行，也即死循环
    push main   ;因为main.c中的main函数没有用到参数，所以都压入0
    jmp setup_paging
L6:
    jmp L6

ignore_int:
    push eax
    push ecx
    push edx
    push ds         ;这里请注意!ds,es,fs,gs等虽然是16位的寄存器，但入栈后
    push es         ;仍然会以32位的形式入栈，也即需要占用4个字节的堆栈空间。
    push fs
    mov eax, 0x10   ;置段选择符(使ds,es,fs指向gdt表中的数据段)
    mov ds, ax
    mov es, ax
    mov fs, ax
    ;push int_msg
    ;call printk
    ;pop eax
    pop fs
    pop es
    pop ds
    pop edx
    pop ecx
    pop eax
    iret            ;中断返回(把中断调用时压入栈的CPU标志寄存器(32位)值也弹出)。

setup_paging:
    ret

idt_descr:              ;下面两行是lidt指令的6字节操作数:长度，基址
    dw 256 * 8 - 1      ;idt contains 256 entries
    dd _idt

gdt_descr:              ;下面两行是lgdt指令的6字节操作数:长度,基址
    dw 256 * 8 - 1      ;so does gdt (note that that's any magic number, but it works for me)
    dd _gdt
    align 8             ;按8字节方式对齐内存地址边界

_idt:
    times 256 dq 0      ;idt is uninitialized，256项，每项8字节，填0

;全局描述符表。前4项分别是空项（不用）、代码段描述符、数据段描述符、系统段描述符，其中系统段描述符linux没有派用处。
;后面还预留了252项的空间，用于放置所创建任务的局部描述符(LDT)和对应的任务状态段TSS的描述符。
;(0-nul，l-cs，2-ds，3-sys，4-TSS0，5-LDT0，6-TSS1，7-LDT1，8-TSS2 etc...)
_gdt:
    dq 0x0000000000000000    ;NULL descriptor
    dq 0x00c09a0000000fff    ;base addrees:0x0000 内核代码段最大长度16Mb, 0x08
    dq 0x00c0920000000fff    ;base addrees:0x0000 内核数据段最大长度16Mb, 0x10
    dq 0x0000000000000000    ;TEMPORARY - don't use
    times 252 dq 0           ;space for LDT's and TSS's etc