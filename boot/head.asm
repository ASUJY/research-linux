;head是汇编进入内核的一个入口，head.asm含有32位启动代码。
;注意！32位启动代码是从绝对地址0x00000000开始的，这里也同样是页目录将存在的地方，
;因此这里的启动代码将被页目录覆盖掉。

[SECTION .text]
[BITS 32]
extern main

global _start
_start:
    ;这里$0x10的含义是请求特权级0（位0-1=0）、选择全局描述符表（位2=0）、选择表中第2项（位3-15=2）。
    ;它正好指向表中的数据段描述符项。
    ;置ds，es，fs，gs中的选择符为setup.asm中构造的数据段（全局段描述符表的第2项）=0x10
    mov eax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    jmp $