#ifndef BLK_H
#define BLK_H

#define NR_BLK_DEV	7

struct blk_dev_struct {
    void (*request_fn)(void);
    //struct request * current_request;
};

extern struct blk_dev_struct blk_dev[NR_BLK_DEV];    // 内核的块设备数组

#ifdef MAJOR_NR

/*
 * Add entries as needed. Currently the only block devices
 * supported are hard-disks and floppies.
 */

#if (MAJOR_NR == 1)
/* ram disk */
#define DEVICE_NAME "ramdisk"
#define DEVICE_REQUEST do_rd_request

#elif (MAJOR_NR == 2)
/* floppy */
#define DEVICE_NAME "floppy"

#elif (MAJOR_NR == 3)
/* harddisk */
#define DEVICE_NAME "harddisk"
#define DEVICE_INTR do_hd
#define DEVICE_REQUEST do_hd_request

#elif
/* unknown blk device */
#error "unknown blk device"

#endif

#ifdef DEVICE_INTR
void (*DEVICE_INTR)(void) = NULL;
#endif

#endif

#endif