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

/*** terminal functions  ***/

/*
Prints the error code and terminates the program

Parameters:
  s: a pointer to a cons char string
*/
void die(const char *s) {
    perror(s);
    exit(1);
}

/*
Restores the terminal to its original state
*/
void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
        die("tcsetattr");
}

/*
raw mode (your inputs will no longer be visible)
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
Reads one byte at a time from stdin

Returns:
  that key/byte as a character
*/
char editorReadKey() {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1) != 1)) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    return c;
}

/*** input ***/

/*
Checks for certain key combinations in order to execute commands
*/
void editorProcessKeypress() {
    char c = editorReadKey();

    switch(c) {
        case CTRL_KEY('q'):
          exit(0);
          break;
    }
}

/*** init ***/

int main() {
    enableRawMode();

    while (1) {
        editorProcessKeypress();
    }
    return 0;
}