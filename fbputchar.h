#ifndef _FBPUTCHAR_H
#  define _FBPUTCHAR_H

#define FBOPEN_DEV -1          /* Couldn't open the device */
#define FBOPEN_FSCREENINFO -2  /* Couldn't read the fixed info */
#define FBOPEN_VSCREENINFO -3  /* Couldn't read the variable info */
#define FBOPEN_MMAP -4         /* Couldn't mmap the framebuffer memory */
#define FBOPEN_BPP -5          /* Unexpected bits-per-pixel */

#include "usbkeyboard.h"

extern int fbopen(void);
extern void fbputchar(char, int, int);
extern void fbputs(const char *, int, int);
extern void fbclear(void);
extern char hex2ascii(int hex);
extern char keyHandler(struct usb_keyboard_packet *packet);
extern void tok64(char **, char *, char *, int *);
extern void fbclearrow(int);
extern void fbclearreceive(void);
extern void print_to_screen(const char*, int*, int);

#endif
