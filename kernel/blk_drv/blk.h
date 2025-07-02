#ifndef BLK_H
#define BLK_H

#define NR_BLK_DEV	7

struct blk_dev_struct {
    void (*request_fn)(void);
    //struct request * current_request;
};

extern struct blk_dev_struct blk_dev[NR_BLK_DEV];    // 内核的块设备数组

#ifdef MAJOR_NR
#if (MAJOR_NR == 1)
/* ram disk */
#define DEVICE_REQUEST do_rd_request

#elif
/* unknown blk device */
#error "unknown blk device"

#endif
#endif

#endif