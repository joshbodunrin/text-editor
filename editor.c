/*** includes ***/
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

/*** defines ***/

#define CTRL_KEY(k) ((k)  & 0x1f)

/*** data ***/
struct editorConfig {
  int numRows;
  int numCols;
  struct termios orig_termios; //global variable to return to original state
};

struct editorConfig E;

/*** terminal ***/

void die(const char *s) {

  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
  
  perror(s);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios)) die ("tcsetattr");
}
void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios)) die("tcgetattr");
  atexit(disableRawMode);
  
  struct termios raw = E.orig_termios;

  raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP); // turns off ctrl-s(stop) and ctrl-q(resume) | ctrl-m
  raw.c_oflag &= ~(OPOST); // turns of output processing
  raw.c_cflag |= (CS8); //char size to 8 bits ber byte
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN); //echo makes so cants se what typing | reading byte by byte now| sig events now too | ctrl-v
  raw.c_cc[VMIN] = 0; //min #of bytes to read
  raw.c_cc[VTIME] = 1; // time to wait before read
  
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)) die("tcsetattr");
}
char editorReadKey() { // reads what key is pressed
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }
  return c;
}
int getWindowSize(int *rows, int *cols) {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    return -1;
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** output ***/
void editorDrawRows() {
  int y;
  for (y = 0; y < E.numRows; y++) {
    write(STDOUT_FILENO, "~\r\n",3);
  }
}

void editorRefreshScreen() {
  write(STDOUT_FILENO, "\x1b[2J",4); //clears out screen
  write(STDOUT_FILENO, "\x1b[H", 3); // repositons cursor to top left

  editorDrawRows();

  write(STDOUT_FILENO, "\x1b[H",3);
}

/*** input ***/

void editorProcessKeypress() { // maps keypresses read to correct actions
  char c = editorReadKey();

  switch(c) {
  case CTRL_KEY('q'):

    write(STDOUT_FILENO, "\x1b[2J",4);
    write(STDOUT_FILENO, "\x1b[H",3);
    exit(0);
    break;
  }
}
/*** init ***/

void initEditor() {
  if (getWindowSize(&E.numRows, &E.numCols) == -1) die("getWindowSize");
} 

int main () {
  enableRawMode();
  initEditor();

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }
  return 0;
}
