/*** includes  ***/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/

#define KILO_VERSION "0.0.1"

// strips the upper 3 bits from k to simulate what CTRL does in the terminal
#define CTRL_KEY(k) (k & 0x1f)

// editor keys with unique integer values
enum editorKey {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

/*** data ***/

typedef struct erow { // erow = "editor row"
    int size;
    char *chars;
} erow;


struct editorConfig {
    int cx, cy; // (cursor x pos (column), cursor y pos (row))
    int screenrows;
    int screencols;
    int numrows;
    erow row;
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
* Reads one byte at a time from stdin.
* If an escape sequence is read, it then checks what is after the [ to 
* determine what to do next.
*
* Returns:
*   that key/byte as a an int
*/
int editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1) != 1)) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    
    if (c == '\x1b') {
        char seq[3];

        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                // keys with the escape seq of "<esc>[x~"
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            } else {
                // keys with the escape seq of "<esc>[x"
                switch (seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        } else if (seq[0] == 'O') {
            // special case on certain devices for these keys
            switch (seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }

        return '\x1b';
    } else {
        return c;
    }
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

/*** file i/o ***/

/*
This function will be for opening and reading a file from disk (doesn't do that yet though)
*/
void editorOpen() {
    char *line = "Hello, world!";
    ssize_t linelen = 13;

    E.row.size = linelen;
    E.row.chars = malloc(linelen + 1);
    memcpy(E.row.chars, line, linelen);
    E.row.chars[linelen] = '\0';
    E.numrows = 1;
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
* Also currently draws a welcome message onto the center of the screen.
*
* Parameters:
*  *ab: pointer to the append buffer struct
*
* Command(s):
*   K = erases part of the line to the right of the cursor
*/
void editorDrawRows(struct abuf *ab) {
    int y;
    for (y = 0; y < E.screenrows; y++) {
        // check if we are currently drawing a row that is part of the text buffer or not
        if (y >= E.numrows) {
            // if not then draw the welcome message or tilde otherwise
            if (y == E.screenrows / 3) {
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome),
                "Kilo editor -- version %s", KILO_VERSION);
                if (welcomelen > E.screencols) welcomelen = E.screencols;

                // centering the welcome message & adding tilde to start of row if possible
                int padding = (E.screencols - welcomelen) / 2;
                if (padding) {
                    abAppend(ab, "~", 1);
                    padding--;
                }
                while (padding--) abAppend(ab, " ", 1);
                abAppend(ab, welcome, welcomelen);
            } else {
                abAppend(ab, "~", 1);
            }  
        } else {
            // drawing a row that's part of the text buffer but first making sure it can
            // fit within the screen length
            int len = E.row.size;
            if (len > E.screencols) len = E.screencols;
            abAppend(ab, E.row.chars, len);
        } 

        abAppend(ab, "\x1b[K", 3);
        // doesn't write a newline after the last row
        if (y < E.screenrows - 1) {
            abAppend(ab, "\r\n", 2);
        }
    }
}

/*
* Refreshes the terminal screen, updates the cursor position, and manages cursor visibility.
*
* \x1b[:
*   /x1b = escape char (27 in decimal)
*   [ = everything after this is a certain command
* commands:
*    H = positions cursor at the first row and first column
* ?25l = disable cursor visibility setting
* ?25h = re-enable cursor visibility
*/
void editorRefreshScreen() {
    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
    abAppend(&ab, buf, strlen(buf));

    abAppend(&ab, "\x1b[?25h", 6);


    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

/*** input ***/

/*
* Moves the cursor based on corresponding key
*
* Parameters:
*   key: the char of the inputted keypress
*/
void editorMoveCursor(int key) {
    switch (key) {
        case ARROW_LEFT:
        if (E.cx != 0) {
          E.cx--;
        }
          break;
        case ARROW_RIGHT:
        if (E.cx != E.screencols - 1) {
          E.cx++;
        }
          break;
        case ARROW_UP:
          if (E.cy != 0) {
            E.cy--;
          }
          break;
        case ARROW_DOWN:
        if (E.cy != E.screenrows - 1) {
          E.cy++;
        }
          break;
    }
}

/*
* Checks for certain keys/key combinations in order to execute commands
*/
void editorProcessKeypress() {
    int c = editorReadKey();

    switch(c) {
        case CTRL_KEY('q'):
	      write(STDOUT_FILENO, "\x1b[2J", 4);
          write(STDOUT_FILENO, "\x1b[H", 3);	
          exit(0);
        break;

        case HOME_KEY:
          E.cx = 0;
          break;

        case END_KEY:
          E.cx = E.screencols - 1;
          break;

        case PAGE_UP:
        case PAGE_DOWN:
          // moves the cursor all the way up or down
          {
            int times = E.screenrows;
            while (times--)
              editorMoveCursor(c == PAGE_UP ? ARROW_UP: ARROW_DOWN);
          }
          break;

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
          editorMoveCursor(c);
          break;
    }
}

/*** init ***/

void initEditor() {
    E.cx = 0;
    E.cy = 0;
    E.numrows = 0;

    if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}

int main() {
    enableRawMode();
    initEditor();
    editorOpen();

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}
