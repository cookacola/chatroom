/*
 * fbputchar: Framebuffer character generator
 *
 * Assumes 32bpp
 *
 * References:
 *
 * https://web.mit.edu/~firebird/arch/sun4x_59/doc/html/emb-framebuffer-howto.html
 * https://web.archive.org/web/20110515014922/https://web.njit.edu/all_topics/Prog_Lang_Docs/html/qt/emb-framebuffer-howto.html
 *
 * https://web.archive.org/web/20110415224759/http://www.diskohq.com/docu/api-reference/fb_8h-source.html
 */

#include "fbputchar.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <string.h>

#include <linux/fb.h>

#define FBDEV "/dev/fb0"

#define FONT_WIDTH 8
#define FONT_HEIGHT 16
#define BITS_PER_PIXEL 32

struct fb_var_screeninfo fb_vinfo;
struct fb_fix_screeninfo fb_finfo;
unsigned char *framebuffer;
static unsigned char font[];

/*
 * Open the framebuffer to prepare it to be written to.  Returns 0 on success
 * or one of the FBOPEN_... return codes if something went wrong.
 */
int fbopen()
{
  int fd = open(FBDEV, O_RDWR); /* Open the device */
  if (fd == -1) return FBOPEN_DEV;

  if (ioctl(fd, FBIOGET_FSCREENINFO, &fb_finfo)) /* Get fixed info about fb */
    return FBOPEN_FSCREENINFO;

  if (ioctl(fd, FBIOGET_VSCREENINFO, &fb_vinfo)) /* Get varying info about fb */
    return FBOPEN_VSCREENINFO;

  if (fb_vinfo.bits_per_pixel != 32) return FBOPEN_BPP; /* Unexpected */

  framebuffer = mmap(0, fb_finfo.smem_len, PROT_READ | PROT_WRITE,
		     MAP_SHARED, fd, 0);
  if (framebuffer == (unsigned char *)-1) return FBOPEN_MMAP;

  return 0;
}

/*
 * Draw the given character at the given row/column.
 * fbopen() must be called first.
 */
void fbputchar(char c, int row, int col)
{
  int x, y;
  unsigned char pixels, *pixelp = font + FONT_HEIGHT * c;
  unsigned char mask;
  unsigned char *pixel, *left = framebuffer +
    (row * FONT_HEIGHT * 2 + fb_vinfo.yoffset) * fb_finfo.line_length +
    (col * FONT_WIDTH * 2 + fb_vinfo.xoffset) * BITS_PER_PIXEL / 8;
  for (y = 0 ; y < FONT_HEIGHT * 2 ; y++, left += fb_finfo.line_length) {
    pixels = *pixelp;
    pixel = left;
    mask = 0x80;
    for (x = 0 ; x < FONT_WIDTH ; x++) {
      if (pixels & mask) {	
	pixel[0] = 255; /* Red */
        pixel[1] = 255; /* Green */
        pixel[2] = 255; /* Blue */
        pixel[3] = 0;
      } else {
	pixel[0] = 0;
        pixel[1] = 0;
        pixel[2] = 0;
        pixel[3] = 0;
      }
      pixel += 4;
      if (pixels & mask) {
	pixel[0] = 255; /* Red */
        pixel[1] = 255; /* Green */
        pixel[2] = 255; /* Blue */
        pixel[3] = 0;
      } else {
	pixel[0] = 0;
        pixel[1] = 0;
        pixel[2] = 0;
        pixel[3] = 0;
      }
      pixel += 4;
      mask >>= 1;
    }
    if (y & 0x1) pixelp++;
  }
}

/*
 * Draw the given string at the given row/column.
 * String must fit on a single line: wrap-around is not handled.
 */
void fbputs(const char *s, int row, int col)
{
  char c;
  while ((c = *s++) != 0) fbputchar(c, row, col++);
}

/*
 * Clears the framebuffer
 */
void fbclear()
{
	for (int col = 0; col < 64; col++) {
		for (int row = 0; row < 24; row++) {
			fbputchar(' ', row, col);
		}
	}
}

void fbclearrow(int row)
{
	for (int col = 0; col < 64; col++) {
		fbputchar(' ', row, col);
	}
}

void fbclearreceive()
{
	for (int row = 0; row < 22; row++) {
		fbclearrow(row);
	}
}

/*
 * Converts the hexadecimal keystate into
 * ASCII characters
 */
 char hex2ascii	(int hex)
 {
	char ascii;
	if (hex > 29) { // number is pressed
		ascii = (char)hex + 19;
	} else { // letter is pressed
		ascii = (char)hex + 93; 
	}

	return ascii;
 }

 /*
  * Capitalizes the ASCII characters
  */
 char capitalize(char c)
 {
	return c - 32;
 }

 /*
  * Handles a keypress
  * Takes a keypress and returns the
  * correct format
  */
char keyHandler(struct usb_keyboard_packet *packet)
{
	int modifiers = packet->modifiers;
	int keycode0 = (packet->keycode)[0];
	int keycode1 = (packet->keycode)[1];
	 /* Return NULL character if no key is pressed */
    if (keycode0 == 0)
	return '\0';

    /* Handle Letter Keys */
    if ((modifiers == 0x02 || modifiers == 0x20) && keycode0 > 3 && keycode0 < 30 && keycode1 == 0) {
	return capitalize(hex2ascii(keycode0));  // Capitalized letters
    } else if (modifiers == 0x00 && keycode0 > 3 && keycode0 < 30 && keycode1 == 0) {
	return hex2ascii(keycode0);  // Lowercase letters
    }

    /* Handle Numbers and Punctuation */
    if (keycode0 >= 30 && keycode0 <= 39) {
	// Number keys (1-9, 0) and their Shifted symbols (!@#$%^&*())
	char normal[] = "1234567890";
	char shifted[] = "!@#$%^&*()";
	int index = keycode0 - 30;
	return (modifiers & 0x02) ? shifted[index] : normal[index];
    }

    /* Handle Space, Enter, Backspace, Tab, Escape */
    switch (keycode0) {
	case 44: return ' ';  // Space
	case 40: return '\n'; // Enter
	case 42: return '\b'; // Backspace
	case 43: return '\t'; // Tab
	case 41: return 27;   // Escape (ASCII code 27)
    }

    /* Handle Special Symbols (Punctuation) */
    if (keycode0 >= 45 && keycode0 <= 56) {
	// Mapping for `- = [ ] \ ; ' , . /` and their Shifted versions
	char normal[] = "-=[]\\;'\n,./";
	char shifted[] = "_+{}|:\"<>?";
	int index = keycode0 - 45;
	return (modifiers & 0x02) ? shifted[index] : normal[index];
    }

    /* Handle Function Keys (F1-F12) */
    if (keycode0 >= 58 && keycode0 <= 69) {
	return (char)(keycode0 - 58 + 'F');  // Return 'F' for Function keys
    }

    /* Handle Arrow Keys */
    switch (keycode0) {
	case 79: return 1;   // Right Arrow
	case 80: return 2;   // Left Arrow
	case 81: return 3;   // Down Arrow
	case 82: return 4;   // Up Arrow
    }

    return '\0';  // No valid key detected
}

/*
 * Tokenizes a string into 64 size chunnks
 */
void print_to_screen(const char *received_str, int *freeRow, int received_chars) {
    int chars_remaining = received_chars;
    int offset = 0;

    while (chars_remaining > 0) {
        // Check if screen is full, reset if necessary
        if (*freeRow >= MAX_ROWS) {
            clear_receive();
            *freeRow = 0;
        }

        // Extract a 64-character chunk
        char buffer[MAX_COLS + 1];  // +1 for null terminator
        int chunk_size = (chars_remaining > MAX_COLS) ? MAX_COLS : chars_remaining;

        strncpy(buffer, received_str + offset, chunk_size);
        buffer[chunk_size] = '\0';  // Null terminate

        // Print to framebuffer at current free row
        fbputs(buffer, *freeRow, 0);

        // Move to the next line
        (*freeRow)++;
        offset += chunk_size;
        chars_remaining -= chunk_size;
    }

/* 8 X 16 console font from /lib/kbd/consolefonts/lat0-16.psfu.gz

od --address-radix=n --width=16 -v -t x1 -j 4 -N 2048 lat0-16.psfu

*/

static unsigned char font[] = {
  0x00, 0x00, 0x7e, 0xc3, 0x99, 0x99, 0xf3, 0xe7, 0xe7, 0xff, 0xe7, 0xe7, 0x7e, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x76, 0xdc, 0x00, 0x76, 0xdc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x6e, 0xf8, 0xd8, 0xd8, 0xdc, 0xd8, 0xd8, 0xd8, 0xf8, 0x6e, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x6e, 0xdb, 0xdb, 0xdf, 0xd8, 0xdb, 0x6e, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x10, 0x38, 0x7c, 0xfe, 0x7c, 0x38, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x88, 0x88, 0xf8, 0x88, 0x88, 0x00, 0x3e, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00,
  0x00, 0xf8, 0x80, 0xe0, 0x80, 0x80, 0x00, 0x3e, 0x20, 0x38, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x70, 0x88, 0x80, 0x88, 0x70, 0x00, 0x3c, 0x22, 0x3c, 0x24, 0x22, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x80, 0x80, 0x80, 0x80, 0xf8, 0x00, 0x3e, 0x20, 0x38, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00,
  0x11, 0x44, 0x11, 0x44, 0x11, 0x44, 0x11, 0x44, 0x11, 0x44, 0x11, 0x44, 0x11, 0x44, 0x11, 0x44,
  0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa,
  0xdd, 0x77, 0xdd, 0x77, 0xdd, 0x77, 0xdd, 0x77, 0xdd, 0x77, 0xdd, 0x77, 0xdd, 0x77, 0xdd, 0x77,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0,
  0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f,
  0x00, 0x88, 0xc8, 0xa8, 0x98, 0x88, 0x00, 0x20, 0x20, 0x20, 0x20, 0x3e, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x88, 0x88, 0x50, 0x50, 0x20, 0x00, 0x3e, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x0e, 0x38, 0xe0, 0x38, 0x0e, 0x00, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xe0, 0x38, 0x0e, 0x38, 0xe0, 0x00, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x06, 0x0c, 0xfe, 0x18, 0x30, 0xfe, 0x60, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x06, 0x1e, 0x7e, 0xfe, 0x7e, 0x1e, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xc0, 0xf0, 0xfc, 0xfe, 0xfc, 0xf0, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x18, 0x3c, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x3c, 0x18, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x0c, 0xfe, 0x0c, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x60, 0xfe, 0x60, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x18, 0x3c, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x3c, 0x18, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x6c, 0xfe, 0x6c, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x06, 0x36, 0x66, 0xfe, 0x60, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xfe, 0x6e, 0x6c, 0x6c, 0x6c, 0x6c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x18, 0x3c, 0x3c, 0x3c, 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x66, 0x66, 0x66, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x6c, 0x6c, 0xfe, 0x6c, 0x6c, 0x6c, 0xfe, 0x6c, 0x6c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x10, 0x10, 0x7c, 0xd6, 0xd0, 0xd0, 0x7c, 0x16, 0x16, 0xd6, 0x7c, 0x10, 0x10, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xc2, 0xc6, 0x0c, 0x18, 0x30, 0x60, 0xc6, 0x86, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x38, 0x6c, 0x6c, 0x38, 0x76, 0xdc, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x18, 0x18, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x0c, 0x18, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x18, 0x0c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x30, 0x18, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x66, 0x3c, 0xff, 0x3c, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x7e, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x30, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x7c, 0xc6, 0xce, 0xce, 0xd6, 0xd6, 0xe6, 0xe6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x18, 0x38, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7e, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x7c, 0xc6, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0, 0xc6, 0xfe, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x7c, 0xc6, 0x06, 0x06, 0x3c, 0x06, 0x06, 0x06, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x0c, 0x1c, 0x3c, 0x6c, 0xcc, 0xfe, 0x0c, 0x0c, 0x0c, 0x1e, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xfe, 0xc0, 0xc0, 0xc0, 0xfc, 0x06, 0x06, 0x06, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x38, 0x60, 0xc0, 0xc0, 0xfc, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xfe, 0xc6, 0x06, 0x06, 0x0c, 0x18, 0x30, 0x30, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0x7e, 0x06, 0x06, 0x06, 0x0c, 0x78, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x00, 0x00, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x0c, 0x18, 0x30, 0x60, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x7c, 0xc6, 0xc6, 0x0c, 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xde, 0xde, 0xde, 0xdc, 0xc0, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x10, 0x38, 0x6c, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0xc6, 0xc6, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xfc, 0x66, 0x66, 0x66, 0x7c, 0x66, 0x66, 0x66, 0x66, 0xfc, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x3c, 0x66, 0xc2, 0xc0, 0xc0, 0xc0, 0xc0, 0xc2, 0x66, 0x3c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xf8, 0x6c, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x6c, 0xf8, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xfe, 0x66, 0x62, 0x68, 0x78, 0x68, 0x60, 0x62, 0x66, 0xfe, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xfe, 0x66, 0x62, 0x68, 0x78, 0x68, 0x60, 0x60, 0x60, 0xf0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x3c, 0x66, 0xc2, 0xc0, 0xc0, 0xde, 0xc6, 0xc6, 0x66, 0x3a, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xfe, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x3c, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x1e, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0xcc, 0xcc, 0xcc, 0x78, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xe6, 0x66, 0x66, 0x6c, 0x78, 0x78, 0x6c, 0x66, 0x66, 0xe6, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xf0, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x62, 0x66, 0xfe, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xc6, 0xee, 0xfe, 0xfe, 0xd6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xc6, 0xe6, 0xf6, 0xfe, 0xde, 0xce, 0xc6, 0xc6, 0xc6, 0xc6, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xfc, 0x66, 0x66, 0x66, 0x7c, 0x60, 0x60, 0x60, 0x60, 0xf0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xd6, 0xde, 0x7c, 0x0c, 0x0e, 0x00, 0x00,
  0x00, 0x00, 0xfc, 0x66, 0x66, 0x66, 0x7c, 0x6c, 0x66, 0x66, 0x66, 0xe6, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x7c, 0xc6, 0xc6, 0x60, 0x38, 0x0c, 0x06, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x7e, 0x7e, 0x5a, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x6c, 0x38, 0x10, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xd6, 0xd6, 0xd6, 0xfe, 0xee, 0x6c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xc6, 0xc6, 0x6c, 0x7c, 0x38, 0x38, 0x7c, 0x6c, 0xc6, 0xc6, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xfe, 0xc6, 0x86, 0x0c, 0x18, 0x30, 0x60, 0xc2, 0xc6, 0xfe, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x3c, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x3c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x3c, 0x00, 0x00, 0x00, 0x00,
  0x10, 0x38, 0x6c, 0xc6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00,
  0x00, 0x30, 0x30, 0x30, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x0c, 0x7c, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0xe0, 0x60, 0x60, 0x78, 0x6c, 0x66, 0x66, 0x66, 0x66, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0xc6, 0xc0, 0xc0, 0xc0, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x1c, 0x0c, 0x0c, 0x3c, 0x6c, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0xc6, 0xfe, 0xc0, 0xc0, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x38, 0x6c, 0x64, 0x60, 0xf0, 0x60, 0x60, 0x60, 0x60, 0xf0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x76, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x7c, 0x0c, 0xcc, 0x78, 0x00,
  0x00, 0x00, 0xe0, 0x60, 0x60, 0x6c, 0x76, 0x66, 0x66, 0x66, 0x66, 0xe6, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x18, 0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x06, 0x06, 0x00, 0x0e, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x66, 0x66, 0x3c, 0x00,
  0x00, 0x00, 0xe0, 0x60, 0x60, 0x66, 0x6c, 0x78, 0x78, 0x6c, 0x66, 0xe6, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x70, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x34, 0x18, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xec, 0xfe, 0xd6, 0xd6, 0xd6, 0xd6, 0xc6, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xdc, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xdc, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7c, 0x60, 0x60, 0xf0, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x76, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x7c, 0x0c, 0x0c, 0x1e, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xdc, 0x76, 0x66, 0x60, 0x60, 0x60, 0xf0, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0xc6, 0x60, 0x38, 0x0c, 0xc6, 0x7c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x10, 0x30, 0x30, 0xfc, 0x30, 0x30, 0x30, 0x30, 0x36, 0x1c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x76, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xc6, 0xc6, 0xd6, 0xd6, 0xd6, 0xfe, 0x6c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xc6, 0x6c, 0x38, 0x38, 0x38, 0x6c, 0xc6, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0x7e, 0x06, 0x0c, 0xf8, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xcc, 0x18, 0x30, 0x60, 0xc6, 0xfe, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x0e, 0x18, 0x18, 0x18, 0x70, 0x18, 0x18, 0x18, 0x18, 0x0e, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x70, 0x18, 0x18, 0x18, 0x0e, 0x18, 0x18, 0x18, 0x18, 0x70, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x76, 0xdc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x66, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x18, 0x18, 0x18, 0x3c, 0x00, 0x00, 0x00, 0x00,
};

