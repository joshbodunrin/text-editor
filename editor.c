/*** includes ***/
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;
  if (write(STDOUT_FILENO, "\x1b[6n",4) != 4) return -1;

  while(i < sizeof(buf) - 1) {
    if(read(STDIN_FILENO,&buf[i],1) != 1) break;
    if(buf[i] == 'R') break;
    i++;
  }
  buf[i] = '\0';
  //  printf("\r\n&buf[1]: '%s'\r\n", &buf[1]);

  if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  if (sscanf(&buf[2], "%d;%d", rows,cols) != 2) return -1;

  return 0;


}
int getWindowSize(int *rows, int *cols) {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    return getCursorPosition(rows,cols);
   } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*** append buffer ***/

struct abuf {
  char  *b;
  int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b,ab->len + len); //resize buffer and point to new location (*new)

  if (new == NULL) return;
  memcpy(&new[ab->len], s, len); //add new stuff in new
  ab->b = new; 
  ab->len += len;
}

void abFree(struct abuf *ab) {
  free(ab->b);
}
/*** output ***/
void editorDrawRows(struct abuf *ab) {
  int y;
  for (y = 0; y < E.numRows; y++) {
    abAppend(ab, "~", 1);
    if (y < E.numRows -1) {
      abAppend(ab,"\r\n",2);
    }
  }
}

void editorRefreshScreen() {
  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l",6); // hide cursor
  abAppend(&ab, "\x1b[2J", 4); // clears out screen
  abAppend(&ab, "\x1b[H",3);  // repositons cursor to top left

  editorDrawRows(&ab);

  abAppend(&ab,"\x1b[H",3);
  abAppend(&ab, "\x1b[?25h",6); //cursor plz come back
  write(STDOUT_FILENO,ab.b,ab.len);
  abFree(&ab);
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
