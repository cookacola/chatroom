# Chat Room

## Features

The chatroom is divided into two sections, entry and receive. The entry space is the bottom two lines. The receive space is the rest.

### Entry Space

The user can enter characters, numbers, and punctuation on bottom two lines of the screen. 

Both shift keys should work.

The user can also delete characters and see their cursor location.

The user can use the left and right arrow keys to change the location of the cursor. Wherever the cursor is, the user can replace the text.

When the user types more than what fits in the entry space, nothing happens.

Upon hitting enter, the entry space is cleared and the user-typed entry is sent to the server. 

### Recieve Space

The user can see what other users send in the chatroom as well as their own messages.

Each message starts on its own distinct line and wraps around.

When the area is filled up, the area is cleared and new entries start on row one.

## Implementation

### Entry Space

The entry in the entry space is saved as a statically allocated string of characters. A pointer keeps track of where the user's cursor currently is. Once text input is received, the cursor changes the character present at the index and increments.

The pointer cannot exceed the length of two lines.

Characters are immediately sent to fbputchar() when typed, replacing what is displayed on the page.

Once the enter key is pressed, the entry is sent to the server, the entry is cleared and the cursor pointer is reset.

Keys are software debounced with sleep.

### Receive Space

A global variable, freeRow, tells us the next free row. If freeRow exceeds the bounds, it will call `clear_receive` and reset to the first row.

When the client receives a packet, it will call the `print_to_screen` function. The function takes the received string, a pointer to freeRow, the number of characters received, and prints it on the next available line.

The function handles wrapping the characters by tokenizing the received string into 64 character strings, printing them on freeRow, and incrementing freeRow.

