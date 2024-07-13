#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

struct termios orig_termios; //global variable to return to original state

void die(const char *s) {
  perror(s);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios)) die ("tcsetattr");
}
void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &orig_termios)) die("tcgetattr");
  atexit(disableRawMode);
  
  struct termios raw = orig_termios;

  raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP); // turns off ctrl-s(stop) and ctrl-q(resume) | ctrl-m
  raw.c_oflag &= ~(OPOST); // turns of output processing
  raw.c_cflag |= (CS8); //char size to 8 bits ber byte
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN); //echo makes so cants se what typing | reading byte by byte now| sig events now too | ctrl-v
  raw.c_cc[VMIN] = 0; //min #of bytes to read
  raw.c_cc[VTIME] = 1; // time to wait before read
  
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw)) die("tcsetattr");
}

int main () {
  enableRawMode();

  while (1) {
    char c = '\0';
    if (read(STDIN_FILENO, &c, 1) == -1 && errno != EAGAIN) die("read");
    if (iscntrl(c)) {
      printf("%d\r\n", c);
    } else {
      printf("%d ('%c')\r\n",c,c);
    }
    if (c == 'q') break;
  }
  return 0;
}
