/*** includes  ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
/*** defines ***/

// strips the upper 3 bits from k to simulate what CTRL does in the terminal
#define CTRL_KEY(k) (k & 0x1f)

/*** data ***/
struct editorConfig {
    int screenrows;
    int screencols;
    struct termios orig_termios;
};

struct editorConfig E;

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
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
        die("tcsetattr");
}

/*
* raw mode (your inputs will no longer be visible)
*/
void enableRawMode() {
   if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");
    atexit(disableRawMode);

    struct termios raw = E.orig_termios;

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

/*
* Gets the cursor position (used for fallback way of getting window size)
* 
* Parameters:
*   *rows: num of rows
*   *cols: num of cols
*
* Returns:
*  0 to indiciate success, -1 otherwise for error indication
*
* Commands:
*  6n = Device Status Report that queries the terminal for the cursor pos.
*/
int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

    return 0;
}

/*
* Gets the terminal window size via ioctl (Input/Output/Control)
*
* Parameters:
*   *rows: will be the number of rows
*   *cols: will be the number of columns
*
* Commands:
*   C = Cursor Forward
*   B = Curson Down
*/
int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    if (1 || ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return getCursorPosition(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*** append buffer ***/

struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0}

/*
* Append to the current buffer via realloc and memcpy.
*
* Parameters:
*   *ab: pointer to a abuf struct
*    *s: pointer to a string or char
*   len: length of the string being appended to the buffer
*/
void abAppend(struct abuf *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len + len);

    if (new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

/*
* Deallocates the dynamic memory used by an abuf
*/
void abFree(struct abuf *ab) {
    free(ab->b);
}

/*** output ***/

/*
* Draws a tilde at the beginning of each row in the editor.
*/
void editorDrawRows() {
    int y;
    for (y = 0; y < E.screenrows; y++) {
        write(STDOUT_FILENO, "~", 1);

        // doesn't write a newline after the last row
        if (y < E.screenrows - 1) {
            write(STDOUT_FILENO, "\r\n", 2);
        }
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

void initEditor() {
    if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}

int main() {
    enableRawMode();
    initEditor();

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}
