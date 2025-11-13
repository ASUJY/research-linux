//
// Created by asujy on 2025/10/31.
//

#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/segment.h>

#include <fcntl.h>
#include <errno.h>
#include <const.h>
#include <sys/stat.h>

/*
 * 主要实现了根据目录名或文件名找到对应的inode的函数
 */

#define ACC_MODE(x) ("\004\002\006\377"[(x)&O_ACCMODE])

/*
 * 如果想让文件名长度>NAME_LEN的字符被截掉，就把下面的定义注释掉
 */
/* #define NO_TRUNCATE */

#define MAY_EXEC 1      // 可执行(可进入)
#define MAY_WRITE 2     // 可写

/*
 * 文件权限检查，判断当前进程是否有权对指定的inode执行特定操作
 * args:
 *  - inode: 要检查权限的文件inode指针
 *  - mask: 请求的权限掩码（如读、写、执行）
 * return:
 *  - 0: 没权限
 *  - 1: 有权限
 */
static int permission(struct m_inode * inode, int mask) {
    int mode = inode->i_mode;

    /*
     * 文件权限检查，即使是root用户也不能读/写一个已被删除的文件
     * 文件所有者检查：如果当前进程的用户id(euid)与inode的用户id相同，则取文件宿主的用户访问权限
     * 文件组检查：如果当前进程的组id(egid)与inode的组id相同，则取组用户访问权限
     */
    if (inode->i_dev && !inode->i_nlinks) {
        return 0;
    } else if (current->euid == inode->i_uid) {
        mode >>= 6;
    } else if (current->egid == inode->i_gid) {
        mode >>= 3;
    }
    /* 检查权限位的低3位是否包含请求的所有权限（是否与屏蔽码相同） 或者 当前进程是root用户（拥有所有权限） */
    if (((mode & mask & 0007) == mask) || suser()) {
        return 1;
    }
    return 0;
}

/*
 * 文件名匹配，用于在目录查找过程中比较给定的文件名与目录项中的文件名是否匹配。
 * args:
 *  - len: 要匹配的文件名长度
 *  - name: 要匹配的文件名字符串
 *  - de: 目录项指针
 */
static int match(int len, const char * name, struct dir_entry * de) {
    register int same __asm__("ax");

    if (!de || !de->inode || len > NAME_LEN) {
        return 0;
    }
    if (len < NAME_LEN && de->name[len]) {
        return 0;
    }
    __asm__("cld\n\t"
            "fs ; repe ; cmpsb\n\t"
            "setz %%al"
            :"=a" (same)
            :"0" (0),"S" ((long) name),"D" ((long) de->name),"c" (len)
            :);
    return same;
}

/*
 * 一个目录中的目录项既可以是目录也可以是文件
 * 目录项查找函数,在指定的目录中寻找一个与name匹配的目录项（文件或子目录）。
 * 返回一个含有找到目录项的高速缓冲区以及目录项本身。
 * 处理特殊的".."目录情况，支持多数据块目录遍历，并返回找到的目录项及其缓冲区。
 * args:
 *  - dir: 要查找的目录的inode
 *  - name: 要查找的文件名
 *  - namelen: 文件名长度
 *  - res_dir: 返回找到的目录项
 */
static struct buffer_head * find_entry(struct m_inode ** dir,
        const char * name, int namelen, struct dir_entry ** res_dir)
{
    int entries;
    int block;
    int i;
    struct buffer_head * bh;
    struct dir_entry * de;
    struct super_block * sb;
/*
 * NO_TRUNCATE模式下，超长文件名直接返回失败
 * 普通模式下，截断超长文件名到最大长度
 */
#ifdef NO_TRUNCATE
    if (namelen > NAME_LEN) {
        return NULL;
    }
#else
    if (namelen > NAME_LEN) {
        namelen = NAME_LEN;
    }
#endif
    entries = (*dir)->i_size / (sizeof (struct dir_entry)); // 计算目录中总目录项数量
    *res_dir = NULL;
    if (!namelen) {
        return NULL;
    }
    /*
     * 特殊处理".."目录
     *  检查是否为".."目录
     *  如果当前目录是进程根目录，".."退化为"."（根目录的父目录还是根目录）
     *  如果当前目录是文件系统根目录且被挂载，切换到挂载点的inode
     */
    if (namelen == 2 && get_fs_byte(name) == '.' && get_fs_byte(name + 1) == '.') {
        if ((*dir) == current->root) {
            namelen = 1;
        } else if ((*dir)->i_num == ROOT_INO) {
            sb = get_super((*dir)->i_dev);
            if (sb->s_imount) {
                iput(*dir);
                (*dir) = sb->s_imount;
                (*dir)->i_count++;
            }
        }
    }
    if (!(block = (*dir)->i_zone[0])) { // 获取目录的第一个数据块号
        return NULL;
    }
    if (!(bh = bread((*dir)->i_dev, block))) {  // 读取该数据块到缓冲区
        return NULL;
    }

    /*
     * 在目录项数据块中查找匹配指定文件名的目录项
     */
    i = 0;
    de = (struct dir_entry *) bh->b_data;
    while (i < entries) {
        /*
         * 如果当前目录项数据已经搜索完，还没找到匹配的目录项，
         * 则释放当前缓冲区，通过bmap查找下一个数据块，读取新的数据块继续遍历
         */
        if ((char *)de >= BLOCK_SIZE + bh->b_data) {
            brelse(bh);
            bh = NULL;
            if (!(block = bmap(*dir, i / DIR_ENTRIES_PER_BLOCK)) ||
                !(bh = bread((*dir)->i_dev, block))) {
                i += DIR_ENTRIES_PER_BLOCK;
                continue;
            }
            de = (struct dir_entry *) bh->b_data;
        }
        /*
        * 使用match函数比较文件名
        * 如果匹配成功，设置结果指针并返回对应的缓冲区
         */
        if (match(namelen, name, de)) {
            *res_dir = de;
            return bh;
        }
        de++;
        i++;
    }
    brelse(bh);
    return NULL;
}

/*
 * 目录项添加函数，往指定目录中添加一个目录项(目录或者文件)
 * args:
 *  - dir: 目录inode指针
 *  - name: 要添加的文件名
 *  - namelen: 文件名长度
 *  - res_dir: 返回找到的目录项指针
 */
static struct buffer_head * add_entry(struct m_inode * dir,
        const char * name, int namelen, struct dir_entry ** res_dir)
{
    int block;
    int i;
    struct buffer_head * bh;
    struct dir_entry * de;

    *res_dir = NULL;
#ifdef NO_TRUNCATE
    if (namelen > NAME_LEN) {
        return NULL;
    }
#else
    if (namelen > NAME_LEN) {
        namelen = NAME_LEN;
    }
#endif
    if (!namelen) {
        return NULL;
    }
    /* 读取第一个目录块：获取目录的第一个数据块号，读取该数据块到缓冲区 */
    if (!(block = dir->i_zone[0])) {
        return NULL;
    }
    if (!(bh = bread(dir->i_dev,block))) {
        return NULL;
    }
    i = 0;
    de = (struct dir_entry *) bh->b_data;
    while (1) {
        /*
         * 如果当前目录项数据已经搜索完，还没找到匹配的目录项，
         * 则释放当前缓冲区，通过create_block创建一个数据块，从磁盘读取新的数据块继续遍历
         */
        /*
         * 检查是否超出当前缓冲区范围
         * 如果超出，释放当前缓冲区，创建新的目录块
         * 读取新分配的目录块继续遍历
         */
        if ((char *)de >= BLOCK_SIZE + bh->b_data) {
            brelse(bh);
            bh = NULL;
            block = create_block(dir, i / DIR_ENTRIES_PER_BLOCK);
            if (!block) {
                return NULL;
            }
            if (!(bh = bread(dir->i_dev, block))) {
                i += DIR_ENTRIES_PER_BLOCK;
                continue;
            }
            de = (struct dir_entry *) bh->b_data;
        }

        if (i * sizeof(struct dir_entry) >= dir->i_size) {
            de->inode = 0;
            dir->i_size = (i + 1) * sizeof(struct dir_entry);
            dir->i_dirt = 1;
            dir->i_ctime = CURRENT_TIME;
        }

        if (!de->inode) {
            dir->i_mtime = CURRENT_TIME;
            for (i = 0; i < NAME_LEN; i++) {
                de->name[i] = (i < namelen) ? get_fs_byte(name + i) : 0;
            }
            bh->b_dirt = 1;
            *res_dir = de;
            return bh;
        }
        de++;
        i++;
    }
    brelse(bh);
    return NULL;
}

/*
 * 根据给出的路径名进行搜索，直到达到最顶端的目录。
 * 返回目录inode
 */
static struct m_inode * get_dir(const char * pathname)
{
    char c;
    const char * thisname;
    struct m_inode * inode;
    struct buffer_head * bh;
    int namelen;
    int inr;
    int idev;
    struct dir_entry * de;

    /* 确保进程有有效的根目录inode */
    if (!current->root || !current->root->i_count) {
        panic("No root inode");
    }
    /* 确保进程有有效的当前工作目录inode */
    if (!current->pwd || !current->pwd->i_count) {
        panic("No cwd inode");
    }
    /*
     * 确定起始目录：
     *  如果路径以'/'开头，使用根目录作为起始点（绝对路径）
     *  如果路径非空且不以'/'开头，使用当前工作目录作为起始点（相对路径）
     *  空路径返回NULL
     */
    if ((c = get_fs_byte(pathname)) == '/') {
        inode = current->root;
        pathname++;
    } else if (c) {
        inode = current->pwd;
    } else {
        return NULL;
    }

    inode->i_count++;   // 增加inode引用计数：防止在解析过程中inode被释放
    while (1) {
        thisname = pathname;
        /* 检查当前inode确实是目录类型，检查对目录是否有执行权限（搜索权限）*/
        if (!S_ISDIR(inode->i_mode) || !permission(inode, MAY_EXEC)) {
            iput(inode);    // 如果检查失败，释放inode
            return NULL;
        }
        for (namelen = 0; (c = get_fs_byte(pathname++)) && (c != '/'); namelen++)
            /* nothing */ ;
        if (!c) {
            return inode;
        }
        /*
         * 调用find_entry在指定目录中查找目录项(在当前处理目录中寻找子目录项)
         */
        if (!(bh = find_entry(&inode, thisname, namelen, &de))) {
            iput(inode);
            return NULL;
        }
        /*
         * 获取找到的目录项的inode号和设备号,释放包含该目录项的缓冲区和inode
         */
        inr = de->inode;
        idev = inode->i_dev;
        brelse(bh);
        iput(inode);
        /*
         * 进入下一级目录：
         *  根据设备号和inode号获取下一级目录的inode
         */
        if (!(inode = iget(idev, inr))) {
            return NULL;
        }
    }
}

/*
 * 返回指定目录名的inode，以及在最顶层目录的名称(路径)
 * 目录名称分离函数，用于将完整路径名分解为目录部分和文件名部分。
 * args:
 *  pathname: 输入的完整路径名
 *  namelen: 输出参数，返回文件名的长度
 *  name: 输出参数，返回文件名的起始指针
 */
static struct m_inode * dir_namei(const char * pathname,
        int * namelen, const char ** name)
{
    char c;
    const char *basename;
    struct m_inode *dir;
    /* 调用get_dir函数解析路径的目录部分，获取指定路径名最顶层的目录的inode */
    if (!(dir = get_dir(pathname))) {
        return NULL;
    }
    /* 定位文件名起始位置(basename指向最后一个'/'之后的文件名部分) */
    basename = pathname;
    while (c = get_fs_byte(pathname++)) {
        if (c == '/') {
            basename = pathname;
        }
    }
    *namelen = pathname - basename - 1;
    *name = basename;
    return dir;
}

struct m_inode * namei(const char * pathname)
{
    const char * basename;
    int inr;
    int dev;
    int namelen;
    struct m_inode * dir;
    struct buffer_head * bh;
    struct dir_entry * de;
    /* 查找指定路径最后一层所在目录的inode，
     * namelen和basename分别是路径中最后一个文件的长度和名称 */
    if (!(dir = dir_namei(pathname,&namelen,&basename))) {
        return NULL;
    }
    if (!namelen) {
        /* special case: '/usr/' etc */
        return dir;
    }
    /* 在文件路径中的最下层目录中查找文件(/usr/include/xx.h，在include目录中查找xx.h) */
    bh = find_entry(&dir, basename, namelen, &de);
    if (!bh) {
        iput(dir);
        return NULL;
    }
    inr = de->inode;          // 获取文件对应的inode编号
    dev = dir->i_dev;
    brelse(bh);
    iput(dir);
    dir = iget(dev, inr);   // 获取文件对应的inode
    if (dir) {
        dir->i_atime = CURRENT_TIME;
        dir->i_dirt = 1;
    }
    return dir;
}

/*
 * open()专用的namei函数，实现了几乎完整的文件打开逻辑
 * 包括路径解析、权限检查、文件创建、截断处理等。
 * args:
 *  - pathname: 文件名路径
 *  - flag: 打开标志(O_CREAT等)
 *  - mode: 创建文件时的权限模式
 *  - res_inode: 返回打开的文件的inode
 */
int open_namei(const char * pathname, int flag, int mode,
        struct m_inode ** res_inode) {

    const char * basename;
    int inr;
    int dev;
    int namelen;
    struct m_inode *dir;
    struct m_inode *inode;
    struct buffer_head *bh;
    struct dir_entry *de;

    if ((flag & O_TRUNC) && !(flag & O_ACCMODE)) {  // 如果指定了O_TRUNC但没有指定访问模式，自动添加O_WRONLY
        flag |= O_WRONLY;
    }
    mode &= 0777 & ~current->umask; // 用umask过滤权限位
    mode |= I_REGULAR;              // 设置文件类型为普通文件
    if (!(dir = dir_namei(pathname, &namelen, &basename))) {    // 路径解析：调用dir_namei分离目录和文件名部分
        return -ENOENT;
    }
    /*
     * 目录特殊情况处理：
     * 如果文件名为空（路径以'/'结尾），说明打开的是目录本身
     * 如果只是只读打开目录，允许并返回目录inode
     */
    if (!namelen) {			/* special case: '/usr/' etc */
        if (!(flag & (O_ACCMODE | O_CREAT | O_TRUNC))) {
            *res_inode = dir;
            return 0;
        }
        iput(dir);
        return -EISDIR;
    }
    bh = find_entry(&dir, basename, namelen, &de);  // 在目录中查找文件对应的目录项
    if (!bh) {
        if (!(flag & O_CREAT)) {
            iput(dir);
            return -ENOENT;
        }
        if (!permission(dir, MAY_WRITE)) {  // 目录写权限检查：创建文件需要对目录有写权限
            iput(dir);
            return -EACCES;
        }
        inode = new_inode(dir->i_dev);          // 为新建文件分配inode
        if (!inode) {
            iput(dir);
            return -ENOSPC;
        }
        /* 初始化inode属性：设置所有者、权限模式，标记为脏 */
        inode->i_uid = current->euid;
        inode->i_mode = mode;
        inode->i_dirt = 1;
        bh = add_entry(dir, basename, namelen, &de);    // 添加目录项：在目录中创建新的目录项
        if (!bh) {
            inode->i_nlinks--;
            iput(inode);
            iput(dir);
            return -ENOSPC;
        }
        de->inode = inode->i_num;
        bh->b_dirt = 1;
        brelse(bh);
        iput(dir);
        *res_inode = inode;
        return 0;
    }
    inr = de->inode;
    dev = dir->i_dev;
    brelse(bh);
    iput(dir);
    if (flag & O_EXCL) {
        return -EEXIST;
    }
    if (!(inode = iget(dev, inr))) {    // 根据设备号和inode编号获取文件inode
        return -EACCES;
    }
    if ((S_ISDIR(inode->i_mode) && (flag & O_ACCMODE)) ||
        !permission(inode, ACC_MODE(flag))) {
        iput(inode);
        return -EPERM;
    }
    inode->i_atime = CURRENT_TIME;
    if (flag & O_TRUNC) {
        truncate(inode);
    }
    *res_inode = inode;
    return 0;
}

int sys_mknod(const char * filename, int mode, int dev) {

}

int sys_mkdir(const char * pathname, int mode) {

}

int sys_rmdir(const char * name) {

}

int sys_unlink(const char * name) {

}

int sys_link(const char * oldname, const char * newname) {

}