
.globl page_fault

page_fault:
    xchgl %eax, (%esp)
    pushl %ecx
    pushl %edx
    push %ds
    push %es
    push %fs
    movl $0x10, %edx
    mov %dx, %ds
    mov %dx, %es
    mov %dx, %fs
    movl %cr2, %edx     # 从CR2寄存器读取引发页故障的线性地址
    pushl %edx
    pushl %eax
    testl $1, %eax      # 测试错误码的最低位
    jne 1f              # 如果最低位为1（表示页存在但访问权限问题），跳转到标签1
    call do_no_page     # 调用缺页处理函数（页不存在的情况）
    jmp 2f
1:
    call do_wp_page     # fork函数 写时复制 处理例程
2:
    addl $8, %esp
    pop %fs
    pop %es
    pop %ds
    popl %edx
    popl %ecx
    popl %eax
    iret