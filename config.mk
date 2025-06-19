CFLAGS:= -m32			# 32位的程序

CFLAGS+= -fno-builtin	# 不需要gcc内置函数
CFLAGS+= -nostdinc		# 不需要标准头文件
CFLAGS+= -fno-pic		# 不需要位置无关的代码 position
CFLAGS+= -fno-pie		# 不需要位置无关的可执行程序 position
CFLAGS+= -nostdlib		# 不需要标准库
CFLAGS+= -fno-stack-protector	# 不需要栈保护
CFLAGS+= -fomit-frame-pointer
CFLAGS+= -fstrength-reduce
CFLAGS+= -Wall
CFLAGS:=$(strip ${CFLAGS})

LDFLAGS := -m elf_i386
ASFLAGS := --32

DEBUG:= -g
DEBUG+= -O0
export CFLAGS
export DEBUG
export LDFLAGS
export ASFLAGS