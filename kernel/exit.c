//
// Created by asujy on 2025/6/25.
//


int do_exit(long code)
{
    return (-1);
}

int sys_exit(int error_code)
{
    return do_exit((error_code&0xff)<<8);
}