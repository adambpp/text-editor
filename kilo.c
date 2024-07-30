/*** includes  ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
/*** defines ***/

// strips the upper 3 bits from k to simulate what CTRL does in the terminal
#define CTRL_KEY(k) (k & 0x1f)

/*** data ***/

struct termios orig_termios;

/*** terminal ***/

/*
* Prints the error code and terminates the program
*
* Parameters:
*   s: a pointer to a cons char string
*/
void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
	
    perror(s);
    exit(1);
}

/*
* Restores the terminal to its original state
*/
void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
        die("tcsetattr");
}

/*
* raw mode (your inputs will no longer be visible)
*/
void enableRawMode() {
   if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");
    atexit(disableRawMode);

    struct termios raw = orig_termios;

    // disable various input/output/local flag modes 
    // via bitwise OR, NOT and bitwise ANDing
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP| IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= ~(CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

/*
* Reads one byte at a time from stdin
*
* Returns:
*   that key/byte as a character
*/
char editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1) != 1)) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    return c;
}

/*** output ***/

/*
* Draws a tilde at the beginning of each row in the editor.
*/
void editorDrawRows() {
    int y;
    for (y = 0; y < 24; y++) {
        write(STDOUT_FILENO, "~\r\n", 3);
    }
}

/*
* Writing 4 bytes to the terminal to Refresh/clear the screen
*
* \x1b[:
*   /x1b = escape char (27 in decimal)
*   [ = everything after this is a certain command
* commands:
*   2J = clear the entire screen
*    H = positions cursor at the first row and first column
*/
void editorRefreshScreen() {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    editorDrawRows();

    write(STDOUT_FILENO, "\x1b[H", 3);
}

/*** input ***/

/*
* Checks for certain key combinations in order to execute commands
*/
void editorProcessKeypress() {
    char c = editorReadKey();

    switch(c) {
        case CTRL_KEY('q'):
	    write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);	
            exit(0);
        break;
    }
}

/*** init ***/

int main() {
    enableRawMode();

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}
