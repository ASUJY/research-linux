//
// Created by asujy on 2025/6/13.
//

#ifndef RESEARCH_LINUX_CTYPE_H
#define RESEARCH_LINUX_CTYPE_H

#define _U	0x01	/* upper */
#define _L	0x02	/* lower */
#define _D	0x04	/* digit */
#define _C	0x08	/* cntrl */
#define _P	0x10	/* punct */
#define _S	0x20	/* white space (space/lf/tab) */
#define _X	0x40	/* hex digit */
#define _SP	0x80	/* hard space (0x20) */

extern unsigned char _ctype[];
extern char _ctmp;

#define islower(c) ((_ctype+1)[c]&(_L))
#define isupper(c) ((_ctype+1)[c]&(_U))

#define tolower(c) (_ctmp=c,isupper(_ctmp)?_ctmp-('A'-'a'):_ctmp)
#define toupper(c) (_ctmp=c,islower(_ctmp)?_ctmp-('a'-'A'):_ctmp)   // 小写转大写

#endif //RESEARCH_LINUX_CTYPE_H
