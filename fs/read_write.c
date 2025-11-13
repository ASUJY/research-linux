//
// Created by asujy on 2025/11/6.
//

#include <sys/stat.h>
#include <errno.h>

#include <linux/kernel.h>
#include <linux/sched.h>

extern int rw_char(int rw,int dev, char * buf, int count, off_t * pos);

extern int file_read(struct m_inode * inode, struct file * filp,
                char * buf, int count);
extern int file_write(struct m_inode * inode, struct file * filp,
                char * buf, int count);

int sys_lseek(unsigned int fd,off_t offset, int origin) {

}

int sys_read(unsigned int fd,char * buf,int count)
{
    struct file * file;
    struct m_inode * inode;

    if (fd >= NR_OPEN || count < 0 || !(file = current->filp[fd])) {
        return -EINVAL;
    }
    if (!count) {
        return 0;
    }
    verify_area(buf,count);
    inode = file->f_inode;
    // if (inode->i_pipe) {
    //     return (file->f_mode&1)?read_pipe(inode,buf,count):-EIO;
    // }
    // if (S_ISCHR(inode->i_mode)) {
    //     return rw_char(READ,inode->i_zone[0],buf,count,&file->f_pos);
    // }
    // if (S_ISBLK(inode->i_mode)) {
    //     return block_read(inode->i_zone[0],&file->f_pos,buf,count);
    // }
    if (S_ISDIR(inode->i_mode) || S_ISREG(inode->i_mode)) {
        if (count + file->f_pos > inode->i_size) {
            count = inode->i_size - file->f_pos;
        }
        if (count <= 0) {
            return 0;
        }
        return file_read(inode,file,buf,count);
    }
    printk("(Read)inode->i_mode=%06o\n\r",inode->i_mode);
    return -EINVAL;
}

/*
 * 负责将数据从用户空间(内存)写入到各种类型的文件中(字符设备、块设备、普通文件等)
 */
int sys_write(unsigned int fd,char * buf,int count) {
    struct file * file;
    struct m_inode * inode;

    if (fd >= NR_OPEN || count < 0 || !(file = current->filp[fd])) {
        return -EINVAL;
    }
    if (!count) {
        return 0;
    }
    /*
     * 根据inode的信息来判断文件的类型，如果是字符设备文件，则调用rw_char进行操作
     */
    inode = file->f_inode;
    // if (inode->i_pipe) {
    //     return (file->f_mode&2) ? write_pipe(inode,buf, count) : -EIO;
    // }
    if (S_ISCHR(inode->i_mode)) {
        return rw_char(WRITE, inode->i_zone[0], buf, count, &file->f_pos);
    }
    // if (S_ISBLK(inode->i_mode)) {
    //     return block_write(inode->i_zone[0], &file->f_pos, buf, count);
    // }
    if (S_ISREG(inode->i_mode)) {
        return file_write(inode, file, buf, count);
    }
    printk("(Write)inode->i_mode=%06o\n\r", inode->i_mode);
    return -EINVAL;
}