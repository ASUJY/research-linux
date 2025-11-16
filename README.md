Linux0.11 内核源代码，已在 Linux 环境下成功运行。

# 执行环境
ubuntu20.04

## 所需工具
gcc 9.4.0  
ld 2.34  
ar 2.34  
gdb 9.2  
bochs 2.7  
qemu 4.2.1  

# 如何构建？
```shell
make        # compile
make bochs  # compile and boot it on bochs
make qemu   # compile and boot it on qemu
make qemug  # compile and debug it on qemu, you'd start gdb to connect it.
```


