//
// Created by asujy on 2025/7/2.
//

#ifndef MM_H
#define MM_H

#define PAGE_SIZE 4096

extern unsigned long get_free_page(void);
extern void free_page(unsigned long addr);

#endif //MM_H
