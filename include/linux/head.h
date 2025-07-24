//
// Created by asujy on 2025/6/19.
//

#ifndef HEAD_H
#define HEAD_H

typedef struct desc_struct {
    unsigned long a,b;
} desc_table[256];  // 描述符表的数据结构，说明每个描述符都是由8个字节组成，每个描述符表共有256项

extern unsigned long _pg_dir[1024];
extern desc_table _idt, _gdt;

#endif //HEAD_H
