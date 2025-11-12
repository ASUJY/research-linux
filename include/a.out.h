//
// Created by asujy on 2025/11/11.
//

#ifndef A_OUT_H
#define A_OUT_H

struct exec {
    unsigned long a_magic;	/* Use macros N_MAGIC, etc for access */
    unsigned a_text;		/* length of text, in bytes */
    unsigned a_data;		/* length of data, in bytes */
    unsigned a_bss;		/* length of uninitialized data area for file, in bytes */
    unsigned a_syms;		/* length of symbol table data in file, in bytes */
    unsigned a_entry;		/* start address */
    unsigned a_trsize;		/* length of relocation info for text, in bytes */
    unsigned a_drsize;		/* length of relocation info for data, in bytes */
};

#ifndef N_MAGIC
#define N_MAGIC(exec) ((exec).a_magic)
#endif

#ifndef OMAGIC
/* Code indicating demand-paged executable.  */
#define ZMAGIC 0413
#endif /* not OMAGIC */

#define _N_HDROFF(x) (SEGMENT_SIZE - sizeof (struct exec))

#ifndef N_TXTOFF
#define N_TXTOFF(x) \
(N_MAGIC(x) == ZMAGIC ? _N_HDROFF((x)) + sizeof (struct exec) : sizeof (struct exec))
#endif

#define SEGMENT_SIZE 1024

#endif //A_OUT_H
