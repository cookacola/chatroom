/*
 *
 * CSEE 4840 Lab 2 for 2024
 *
 * Apurva Reddy (akr2177), Godwill Agbehonou (gea2118), Charles Chen (cc4919) 
 */
#include "fbputchar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "usbkeyboard.h"
#include <pthread.h>

/* Update SERVER_HOST to be the IP address of
 * the chat server you are connecting to
 */
/* arthur.cs.columbia.edu */
#define SERVER_HOST "128.59.19.114"
#define SERVER_PORT 42000
#define COLS 64
#define ROWS 24

#define BUFFER_SIZE 128

/*
 * References:
 *
 * https://web.archive.org/web/20130307100215/http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html
 *
 * http://www.thegeekstuff.com/2011/12/c-socket-programming/
 * 
 */

int sockfd; /* Socket file descriptor */

struct libusb_device_handle *keyboard;
uint8_t endpoint_address;

pthread_t network_thread;
void *network_thread_f(void *);
char bigMatrix[12][64];
int topRow = 0;
char buffer[1024];

int main()
{
  //these initial variables ar eimportant
  //we update err and col quite often, we use keystate 
  //we use the struct packet
  int err, col;

  struct sockaddr_in serv_addr;
  struct usb_keyboard_packet packet;
  int transferred;
  char keystate[12];

  if ((err = fbopen()) != 0) {
    fprintf(stderr, "Error: Could not open framebuffer: %d\n", err);
    exit(1);
  }

	/* Clear the screen */
	fbclear();

	/* Draw horizontal line */
	for (col = 0 ; col < COLS; col++) {
		fbputchar('=', 21, col);
	}

  /* Draw rows of asterisks across the top and bottom of the screen */
//  for (col = 0 ; col < 64 ; col++) {
//    fbputchar('*', 0, col);
//    fbputchar('*', 23, col);
//  }
//
//  fbputs("Hello CSEE 4840 World!", 4, 10);

  /* Open the keyboard */
  if ( (keyboard = openkeyboard(&endpoint_address)) == NULL ) {
    fprintf(stderr, "Did not find a keyboard\n");
    exit(1);
  }
    
  /* Create a TCP communications socket */
  if ( (sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    fprintf(stderr, "Error: Could not create socket\n");
    exit(1);
  }

  /* Get the server address */
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(SERVER_PORT);
  if ( inet_pton(AF_INET, SERVER_HOST, &serv_addr.sin_addr) <= 0) {
    fprintf(stderr, "Error: Could not convert host IP \"%s\"\n", SERVER_HOST);
    exit(1);
  }

  /* Connect the socket to the server */
  if ( connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    fprintf(stderr, "Error: connect() failed.  Is the server running?\n");
    exit(1);
  }

  /* Start the network thread */
  pthread_create(&network_thread, NULL, network_thread_f, NULL);

  /* Look for and handle keypresses */
  int cursor = 0;
  char ascii = ' ';
  char entry[2 * COLS + 1] = "";
  char coveredChar = '\0';
  for (;;) {
	/* Highlight the cursor */
	if (cursor > COLS && cursor < (2 * COLS)) {
		/* Cursor is on the second line */
		fbputchar('_', 23, cursor - COLS);
	} else if (cursor >= 0 && cursor < COLS) {
		/* Cursor is on the first line */
		fbputchar('_', 22, cursor);
	}
	if(cursor < strlen(entry))
		coveredChar = entry[cursor];
    libusb_interrupt_transfer(keyboard, endpoint_address,
			      (unsigned char *) &packet, sizeof(packet),
			      &transferred, 0);
	sleep(0.1);
    if (transferred == sizeof(packet)) {
      sprintf(keystate, "%02x %02x %02x", packet.modifiers, 
	  	packet.keycode[0],packet.keycode[1]);

      if (packet.keycode[0] == 0x29) { /* ESC pressed? */
		break;
      }

	  ascii = keyHandler(&packet);
	  if(ascii == '\0')
		continue;

	  /* user clicks enter */
	  if(ascii == '\n') {
		int n;
		cursor = 0;
  		if ((n = send(sockfd, entry, strlen(entry), 0)) >= 0 ) {
			printf("Sent %d bytes\n", n);
		} else {
			printf("Send failed");
			break;
		}
		entry[0] = '\0';
		fbclearrow(23);
		fbclearrow(22);
		continue;
	  }
	  
	  if(ascii == 2) {
		/* Cursor moves left */
		if(cursor > 0 && cursor != strlen(entry)) {
			if (cursor > COLS && cursor < (2 * COLS)) {
				/* Cursor is on the second line */
				fbputchar(coveredChar, 23, cursor - COLS);
			} else if (cursor >= 0 && cursor < COLS) {
				/* Cursor is on the first line */
				fbputchar(coveredChar, 22, cursor);
			}
			cursor--;
		} else if (cursor == strlen(entry)) {
			if (cursor > COLS && cursor < (2 * COLS)) {
				/* Cursor is on the second line */
				fbputchar(' ', 23, cursor - COLS);
			} else if (cursor >= 0 && cursor < COLS) {
				/* Cursor is on the first line */
				fbputchar(' ', 22, cursor);
			}
			cursor--;
		}
		continue;
	  }

	  if(ascii == 1) {
		/* Cursor moves right */
		if(cursor < strlen(entry)) {
				if (cursor > COLS && cursor < (2 * COLS)) {
					/* Cursor is on the second line */
					fbputchar(coveredChar, 23, cursor - COLS);
				} else if (cursor >= 0 && cursor < COLS) {
					/* Cursor is on the first line */
					fbputchar(coveredChar, 22, cursor);
				}
				cursor++;
		}
			continue;
	  }

		/* Backspce */
	  if(ascii == '\b') {
		/* Delete pressed */
		if (cursor == strlen(entry)) {
			/* End of the line */
			entry[cursor - 1] = '\0';
		} else if (cursor < strlen(entry) && cursor >= 0) {
			/* Middle of line */
			//entry[cursor] = ' ';
			int length = strlen(entry);
			for(int i = cursor; i < (strlen(entry)-1); i++){
					entry[i] = entry[i+1];
			}
			entry[length-1] = '\0';
			//entry[length] = '\0';
			for(int col = 0; col < COLS; col++){
					if(col < strlen(entry)){
							fbputchar(entry[col],22, col);
					}
					else{
							fbputchar(' ',22,col);
					}
					if(length-COLS-col > 0){
							printf("%d\n", strlen(entry)-COLS-col);
							fbputchar(entry[col+COLS],23,col);
					}
					else{
							fbputchar(' ',23,col);
					}
			}
		}
		/* Adjust the display */
	  	if (cursor > COLS && cursor < (2 * COLS)) {
			/* Cursor is on the second line */
			fbputchar(' ', 23, cursor - COLS);
	  	} else if (cursor >= 0 && cursor < COLS) {
			/* Cursor is on the first line */
			fbputchar(' ', 22, cursor);
		}
		if (cursor > 0) {
			cursor--;
		}
		continue;
	  }
		
		/* Printable Character */
	  printf("%c", ascii);
	  if (cursor > COLS && cursor < (2 * COLS)) {
		/* Cursor is on the second line */
	  	fbputchar(ascii, 23, cursor - COLS);
	  } else if (cursor >= 0 && cursor < COLS) {
		/* Cursor is on the first line */
	  	fbputchar(ascii, 22, cursor);
	  }

	  if (cursor == strlen(entry) && cursor < (2 * COLS)) {
		/* Cursor is at the end and does not overflow buffer*/
		entry[cursor] = ascii;
		entry[cursor + 1] = '\0';
		cursor++;
	  } else if (cursor < strlen(entry) && cursor >= 0) {
		/* Cursor is within the string */
		int maxLength = strlen(entry);
		for(int i = maxLength; i > cursor; i--){
				entry[i] = entry[i-1]; 
		}
		entry[cursor] = ascii;
		entry[maxLength+1] = '\0';
		cursor++;

		for(int col = 0; col < COLS; col++){
				if(col < strlen(entry)){
						fbputchar(entry[col],22, col);
				}
				else{
						fbputchar(' ',22,col);
				}
				if(maxLength-COLS-col > 0){
						printf("%d\n", strlen(entry)-COLS-col);
						fbputchar(entry[col+COLS],23,col);
				}
				else{
						fbputchar(' ',23,col);
				}
		}
	  } else {
		/* Cursor is at the end and may overflow buffer */
		continue;
	  }

      printf("%s\n", keystate);
      //fbputs(keystate, 6, 0);
    }
  }

  /* Terminate the network thread */
  pthread_cancel(network_thread);

  /* Wait for the network thread to finish */
  pthread_join(network_thread, NULL);

  return 0;
}

void *network_thread_f(void *ignored)
{
  char recvBuf[BUFFER_SIZE];
  int n;
  /* Receive data */
  while ( (n = read(sockfd, &recvBuf, BUFFER_SIZE - 1)) > 0 ) {
    recvBuf[n] = '\0';
    printf("%s", recvBuf);
	print_to_screen(recvBuf, &topRow, n);	
  }
  return NULL;
}
