//
// Created by asujy on 2025/11/7.
//

#include <a.out.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <asm/segment.h>

extern int sys_close(int fd);

#define MAX_ARG_PAGES 32

static unsigned long * create_tables(char * p,int argc,int envc)
{
    unsigned long *argv;
    unsigned long *envp;
    unsigned long * sp;

    sp = (unsigned long *) (0xfffffffc & (unsigned long) p);
    sp -= envc + 1;
    envp = sp;
    sp -= argc + 1;
    argv = sp;
    put_fs_long((unsigned long)envp, --sp);
    put_fs_long((unsigned long)argv, --sp);
    put_fs_long((unsigned long)argc, --sp);
    while (argc-- > 0) {
        put_fs_long((unsigned long) p, argv++);
        while (get_fs_byte(p++)) /* nothing */ ;
    }
    put_fs_long(0, argv);
    while (envc-- > 0) {
        put_fs_long((unsigned long) p, envp++);
        while (get_fs_byte(p++)) /* nothing */ ;
    }
    put_fs_long(0, envp);
    return sp;
}

/*
 * count() counts the number of arguments/envelopes
 */
static int count(char ** argv)
{
    int i = 0;
    char **tmp;

    if (tmp = argv) {
        while (get_fs_long((unsigned long *) (tmp++))) {
            i++;
        }
    }

    return i;
}

/*
 *复制参数字符串到指定内存区域
 * args:
 *  - argc:要复制的参数个数
 *  - argv: 参数指针数组
 *  - page:
 *  - p: 在参数表空间中的偏移指针
 *  - from_kmem: 字符串的来源
*   * from_kmem     argv *        argv **
 *    0          user space    user space
 *    1          kernel space  user space
 */
static unsigned long copy_strings(int argc,char ** argv,unsigned long *page,
                unsigned long p, int from_kmem)
{
    char *tmp;
    char *pag;
    int len;
    int offset = 0;
    unsigned long old_fs;
    unsigned long new_fs;

    if (!p) {
        return 0;   /* bullet-proofing */
    }
    new_fs = get_ds();  // 取ds寄存器值到new_fs，并保存原fs寄存器值到old_fs
    old_fs = get_fs();
    if (from_kmem == 2) {
        set_fs(new_fs);
    }
    /* 从最后一个参数开始逆向复制 */
    while (argc-- > 0) {
        if (from_kmem == 1) {
            set_fs(new_fs);
        }
        if (!(tmp = (char *)get_fs_long(((unsigned long *)argv) + argc))) {
            panic("argc is wrong");
        }
        if (from_kmem == 1) {
            set_fs(old_fs);
        }
        len = 0;		/* remember zero-padding */
        do {
            len++;
        } while (get_fs_byte(tmp++));   // 计算字符串长度
        if (p - len < 0) {	/* this shouldn't happen - 128kB */
            set_fs(old_fs);
            return 0;
        }
        while (len) {
            --p;
            --tmp;
            --len;
            if (--offset < 0) {
                offset = p % PAGE_SIZE; // 计算新页内的偏移量
                if (from_kmem == 2) {
                    set_fs(old_fs);
                }
                if (!(pag = (char *) page[p/PAGE_SIZE]) &&
                    !(pag = (char *) (page[p/PAGE_SIZE] =
                     (unsigned long *) get_free_page()))) { // 检查页是否存在，不存在则分配新页
                    return 0;
                }
                if (from_kmem == 2) {
                    set_fs(new_fs);
                }
            }
            *(pag + offset) = get_fs_byte(tmp); // 将字节从源复制到目标页
        }
    }
    if (from_kmem == 2) {
        set_fs(old_fs);
    }
    return p;
}

static unsigned long change_ldt(unsigned long text_size,unsigned long * page)
{
    unsigned long code_limit;
    unsigned long data_limit;
    unsigned long code_base;
    unsigned long data_base;
    int i;

    code_limit = text_size + PAGE_SIZE -1;
    code_limit &= 0xFFFFF000;
    data_limit = 0x4000000;
    code_base = get_base(current->ldt[1]);
    data_base = code_base;
    set_base(current->ldt[1], code_base);
    set_limit(current->ldt[1], code_limit);
    set_base(current->ldt[2], data_base);
    set_limit(current->ldt[2], data_limit);
    /* make sure fs points to the NEW data segment */
    __asm__("pushl $0x17\n\tpop %%fs"::);
    data_base += data_limit;
    for (i = MAX_ARG_PAGES - 1; i >= 0; i--) {
        data_base -= PAGE_SIZE;
        if (page[i]) {
            put_page(page[i], data_base);
        }
    }
    return data_limit;
}

/*
 * 'do_execve()' executes a new program.
 * args:
 *  - eip：调用int 0x80的下一条指令的地址（即execve中调用sys_execve的下一条指令地址）
 *  - tmp: 系统中断在调用sys_execve的下一条指令时的返回地址，无用（system_call中调用sys_execve的下一条指令地址）
 *  - filename: 被执行的文件名
 *  - argv: 命令行参数指针数组
 *  - envp: 环境变量指针数组
 */
int do_execve(unsigned long * eip,long tmp,char * filename,
        char ** argv, char ** envp) {
    struct m_inode *inode;
    struct buffer_head *bh;
    struct exec ex;                     // 执行文件头部数据
    unsigned long page[MAX_ARG_PAGES];  // 页面数组，用于存储参数和环境变量的页面
    int i;
    int argc;
    int envc;
    int e_uid;
    int e_gid;
    int retval;
    int sh_bang = 0;    // 控制是否需要执行脚本处理代码
    unsigned long p = PAGE_SIZE * MAX_ARG_PAGES - 4;    // 初始化为指向该内存页面的最后一个长字处（用于在参数和环境变量拷贝时跟踪当前栈指针）

    if ((0xffff & eip[1]) != 0x000f) {
        panic("execve called from supervisor mode");
    }
    for (i = 0; i < MAX_ARG_PAGES; i++) {
        /* clear page-table */
        page[i] = 0;
    }
    if (!(inode = namei(filename))) {       //获取可执行文件对应的inode
        return -ENOENT;
    }
    /* 计算参数个数和环境变量个数 */
    argc = count(argv);
    envc = count(envp);

restart_interp:
    if (!S_ISREG(inode->i_mode)) {	/* 执行文件必须是常规文件 */
        retval = -EACCES;
        goto exec_error2;
    }
    /* 检查 被执行文件 的执行权限，判断当前进程是否有权限执行这个文件 */
    i = inode->i_mode;
    e_uid = (i & S_ISUID) ? inode->i_uid : current->euid;
    e_gid = (i & S_ISGID) ? inode->i_gid : current->egid;

    if (current->euid == inode->i_uid) {
        i >>= 6;
    } else if (current->egid == inode->i_gid) {
        i >>= 3;
    }
    if (!(i & 1) && !((inode->i_mode & 0111) && suser())) {
        retval = -ENOEXEC;
        goto exec_error2;
    }
    /* 读取执行文件的第一块数据到高速缓冲区中(内存) */
    if (!(bh = bread(inode->i_dev, inode->i_zone[0]))) {
        retval = -EACCES;
        goto exec_error2;
    }
    ex = *((struct exec *) bh->b_data);	/* 对执行文件的 exec-header 进行处理 */
    if ((bh->b_data[0] == '#') && (bh->b_data[1] == '!') && (!sh_bang)) {
        /*
         * This section does the #! interpretation.
         * Sorta complicated, but hopefully it will work.  -TYT
         */

        char buf[1023];
        char *cp;
        char *interp;
        char *i_name;
        char *i_arg;
        unsigned long old_fs;

        strncpy(buf, bh->b_data + 2, 1022);
        brelse(bh);
        iput(inode);
        buf[1022] = '\0';
        if (cp = strchr(buf, '\n')) {
            *cp = '\0';
            for (cp = buf; (*cp == ' ') || (*cp == '\t'); cp++);
        }
        if (!cp || *cp == '\0') {
            retval = -ENOEXEC; /* No interpreter name found */
            goto exec_error1;
        }
        interp = i_name = cp;
        i_arg = 0;
        for ( ; *cp && (*cp != ' ') && (*cp != '\t'); cp++) {
            if (*cp == '/') {
                i_name = cp + 1;
            }
        }
        if (*cp) {
            *cp++ = '\0';
            i_arg = cp;
        }
        /*
         * OK, we've parsed out the interpreter name and
         * (optional) argument.
         */
        if (sh_bang++ == 0) {
            p = copy_strings(envc, envp, page, p, 0);
            p = copy_strings(--argc, argv+1, page, p, 0);
        }
        /*
         * Splice in (1) the interpreter's name for argv[0]
         *           (2) (optional) argument to interpreter
         *           (3) filename of shell script
         *
         * This is done in reverse order, because of how the
         * user environment and arguments are stored.
         */
        p = copy_strings(1, &filename, page, p, 1);
        argc++;
        if (i_arg) {
            p = copy_strings(1, &i_arg, page, p, 2);
            argc++;
        }
        p = copy_strings(1, &i_name, page, p, 2);
        argc++;
        if (!p) {
            retval = -ENOMEM;
            goto exec_error1;
        }
        /*
         * OK, now restart the process with the interpreter's inode.
         */
        old_fs = get_fs();
        set_fs(get_ds());
        if (!(inode = namei(interp))) { /* get executables inode */
            set_fs(old_fs);
            retval = -ENOENT;
            goto exec_error1;
        }
        set_fs(old_fs);
        goto restart_interp;
    }
    brelse(bh);// 对文件头进行检查，对于以下情况将不执行程序
    if (N_MAGIC(ex) != ZMAGIC || ex.a_trsize || ex.a_drsize ||
        ex.a_text+ex.a_data+ex.a_bss > 0x3000000 ||
        inode->i_size < ex.a_text + ex.a_data + ex.a_syms + N_TXTOFF(ex)) {
        retval = -ENOEXEC;
        goto exec_error2;
    }
    if (N_TXTOFF(ex) != BLOCK_SIZE) {
        printk("%s: N_TXTOFF != BLOCK_SIZE. See a.out.h.", filename);
        retval = -ENOEXEC;
        goto exec_error2;
    }
    if (!sh_bang) { // 拷贝参数和环境变量
        p = copy_strings(envc,envp,page,p,0);
        p = copy_strings(argc,argv,page,p,0);
        if (!p) {
            retval = -ENOMEM;
            goto exec_error2;
        }
    }
    /* OK, This is the point of no return */
    if (current->executable) {  // 如果当前进程也是一个执行程序，则释放其inode，并让进程指向新的可执行文件inode
        iput(current->executable);
    }
    current->executable = inode;
    for (i = 0; i < 32; i++) {
        if (current->sigaction[i].sa_handler == SIG_IGN){continue;}
        current->sigaction[i].sa_handler = NULL;    // 清除所有信号处理函数
    }
    for (i = 0; i < NR_OPEN; i++) {
        if ((current->close_on_exec >> i) & 1) {    // 关闭标记为close_on_exec的文件描述符
            sys_close(i);
        }
    }
    current->close_on_exec = 0;
    free_page_tables(get_base(current->ldt[1]), get_limit(0x0f));   // 释放代码段和数据段对应的页表。
    free_page_tables(get_base(current->ldt[2]), get_limit(0x17));   // 此时被执行程序没有占用主内存任何页面。在执行时会引起内存管理程序执行缺页处理而为其申请内存页面，并把程序读入内存。
    if (last_task_used_math == current) {
        last_task_used_math = NULL;
    }
    current->used_math = 0;
    p += change_ldt(ex.a_text,page) - MAX_ARG_PAGES * PAGE_SIZE;    // 设置新的局部描述符表（LDT），并返回新的代码段基址。然后调整p。
    p = (unsigned long) create_tables((char *)p,argc,envc);         // 在用户栈中创建参数和环境变量的指针表，并返回新的栈顶指针
    current->brk = ex.a_bss +
                (current->end_data = ex.a_data +
                (current->end_code = ex.a_text));   // 设置代码段、数据段、bss段的结束地址，以及堆栈起始地址。
    current->start_stack = p & 0xfffff000;
    current->euid = e_uid;
    current->egid = e_gid;
    i = ex.a_text + ex.a_data;
    while (i & 0xfff) {     // 初始化一页bss段数据，全为0
        put_fs_byte(0, (char *) (i++));
    }
    eip[0] = ex.a_entry;		/* eip, magic happens :-) */
    eip[3] = p;			        /* stack pointer */
    return 0; // 设置新的eip和esp；返回指令将弹出这些堆栈数据，使得CPU去执行新的执行程序，因此不会返回到原调用系统中断的程序中执行了。

exec_error2:
    iput(inode);
exec_error1:
    for (i = 0; i < MAX_ARG_PAGES; i++) {
        free_page(page[i]);
    }
    return(retval);
}