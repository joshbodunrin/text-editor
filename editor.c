/*** includes ***/

#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

/*** defines ***/

#define CTRL_KEY(k) ((k)  & 0x1f)

#define EDITOR_VERSION "0.0.1"

enum editorKey {
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  PAGE_UP,
  PAGE_DOWN
};

/*** data ***/

typedef struct erow {
  int size;
  char *chars;
} erow;

struct editorConfig {
  int cx, cy;
  int screenRows;
  int screenCols;
  int numRows;
  erow *row;
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
int editorReadKey() { // reads what key is pressed
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }
  if (c == '\x1b') {
    char seq[3];
    if (read(STDIN_FILENO, &seq[0],1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1],1) != 1) return '\x1b';

    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
	if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
	if (seq[2] == '~') {
	  switch (seq[1]) {
	  case '5': return PAGE_UP;
	  case '6': return PAGE_DOWN;
	  }
	}
      } else {
	switch (seq[1]) {
	case 'A': return ARROW_UP;
	case 'B': return ARROW_DOWN;
	case 'C': return ARROW_RIGHT;
	case 'D': return ARROW_LEFT;
	}
      }
    }
    return '\x1b';
  } else {
    return c;
  }
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

/*** row operations ***/

void editorAppendRow(char *s, size_t len) {
  E.row->size = len;
  E.row->chars = malloc(len + 1);
  memcpy(E.row->chars, s, len);
  E.row->chars[len] = '\0';
  E.numRows = 1;
}
    
  

/*** file i/o ***/

void editorOpen(char *filename) {

  FILE *fp = fopen(filename, "r");
  if (!fp) die("fopen");

  char *line = NULL;
  size_t linecap = 0;

  ssize_t linelen;
  linelen = getline(&line, &linecap, fp);
  if (linelen != 1) {
    while (linelen > 0 && (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
      linelen--;

    editorAppendRow(line, linelen);
    
  }
  free(line);
  fclose(fp);
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
  for (y = 0; y < E.screenRows; y++) {
    if (y >= E.numRows) {
    if (E.numRows == 0 && y == E.screenRows / 3) {
      char welcome[80];
      int welcomelen = snprintf(welcome, sizeof(welcome), "Editor -- varsion %s", EDITOR_VERSION);
      if (welcomelen > E.screenCols) welcomelen = E.screenCols;
      int padding = (E.screenCols - welcomelen) / 2; //find out where middle is
      if (padding) {
	abAppend(ab, "~", 1);
	padding--;
      }
      while (padding--) abAppend(ab, " ", 1); // till find middle to center message
      abAppend(ab, welcome, welcomelen);
    } else {
      abAppend(ab, "~", 1);
    }
    } else {
      int len = E.row->size;
      if (len > E.screenCols) len = E.screenCols;
      abAppend(ab, E.row->chars,len);
    }
    abAppend(ab, "\x1b[K",3); // clears line right of cursor, doing this instead clearing screen at once
    if (y < E.screenRows -1) {
      abAppend(ab,"\r\n",2);
    }
  }
}

void editorRefreshScreen() {
  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l",6); // hide cursor
  abAppend(&ab, "\x1b[H",3);  // repositons cursor to top left

  editorDrawRows(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
  abAppend(&ab, buf, strlen(buf));

  //  abAppend(&ab,"\x1b[H",3);
  abAppend(&ab, "\x1b[?25h",6); //cursor plz come back
  write(STDOUT_FILENO,ab.b,ab.len);
  abFree(&ab);
}

/*** input ***/

void editorMoveCursor(int key) {
  switch (key) {
  case ARROW_LEFT:
    if (E.cx != 0) E.cx--;
    break;
  case ARROW_RIGHT:
    if (E.cx != E.screenCols) E.cx++;
    break;
  case ARROW_DOWN:
    if (E.cy != E.screenRows - 1) E.cy++;
    break;
  case ARROW_UP:
    if (E.cy != 0) E.cy--;
    break;
  }
}
      

void editorProcessKeypress() { // maps keypresses read to correct actions
  int c = editorReadKey();

  switch(c) {
  case CTRL_KEY('q'):

    write(STDOUT_FILENO, "\x1b[2J",4);
    write(STDOUT_FILENO, "\x1b[H",3);
    exit(0);
    break;
  case PAGE_UP:
  case PAGE_DOWN:
    {
      int times = E.screenRows;
      while (times--) editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN); //if pageup use arrow up else, use arrow down
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
  E.numRows = 0;
  E.row = NULL;

  
  if (getWindowSize(&E.screenRows, &E.screenCols) == -1) die("getWindowSize");
} 

int main (int argc, char *argv[]) {
  enableRawMode();
  initEditor();
  if (argc >= 2) {
    editorOpen(argv[1]);
  }

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }
  return 0;
}
