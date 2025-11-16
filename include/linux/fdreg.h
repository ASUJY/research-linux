//
// Created by asujy on 2025/11/14.
//

#ifndef FDREG_H
#define FDREG_H

extern int ticks_to_floppy_on(unsigned int nr);
extern void floppy_on(unsigned int nr);
extern void floppy_off(unsigned int nr);
extern void floppy_select(unsigned int nr);
extern void floppy_deselect(unsigned int nr);

/* Fd controller regs. S&C, about page 340 */
#define FD_STATUS	0x3f4
#define FD_DATA		0x3f5
#define FD_DOR		0x3f2		/* Digital Output Register */
#define FD_DIR		0x3f7		/* Digital Input Register (read) */
#define FD_DCR		0x3f7		/* Diskette Control Register (write)*/

/* Bits of main status register */
#define STATUS_BUSY	0x10		/* FDC busy */
#define STATUS_DIR	0x40		/* 0- cpu->fdc */
#define STATUS_READY	0x80		/* Data reg ready */

/* Values for FD_COMMAND */
#define FD_RECALIBRATE	0x07		/* move to track 0 */
#define FD_SEEK		0x0F		/* seek track */
#define FD_READ		0xE6		/* read with MT, MFM, SKip deleted */
#define FD_WRITE	0xC5		/* write with MT, MFM */
#define FD_SENSEI	0x08		/* Sense Interrupt Status */
#define FD_SPECIFY	0x03		/* specify HUT etc */

/* DMA commands */
#define DMA_READ	0x46
#define DMA_WRITE	0x4A

#endif //FDREG_H
