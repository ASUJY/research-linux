//
// Created by asujy on 2025/9/4.
//

#include <linux/sched.h>
#include "blk.h"

struct blk_dev_struct blk_dev[NR_BLK_DEV] = {
  { NULL, NULL },		/* no_dev */
  { NULL, NULL },		/* dev mem */
  { NULL, NULL },		/* dev fd */
  { NULL, NULL },		/* dev hd */
  { NULL, NULL },		/* dev ttyx */
  { NULL, NULL },		/* dev tty */
  { NULL, NULL }		/* dev lp */
};