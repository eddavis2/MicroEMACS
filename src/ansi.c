/*
 * The routines in this file provide support for ANSI style terminals
 * over a serial line. The serial I/O services are provided by routines in
 * "termio.c". It compiles into nothing if not an ANSI device.
 */

#if !defined(_WIN32) && !defined(_WIN64)

#include <stdio.h>
#include <termios.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>

#undef CTRL
#include "ed.h"

#if     ANSI

#define NROW    23                      /* Screen size.                 */
#define NCOL    77                      /* Edit if you want to.         */
#define BEL     0x07                    /* BEL character.               */
#define ESC     0x1B                    /* ESC character.               */

extern  int     ttopen();               /* Forward references.          */
extern  int     ttgetc();
extern  int     ttputc();
extern  int     ttflush();
extern  int     ttclose();
extern  int     ansimove();
extern  int     ansieeol();
extern  int     ansieeop();
extern  int     ansibeep();
extern  int     ansiopen();

struct  termios ostate;

/*
 * Standard terminal interface dispatch table. Most of the fields point into
 * "termio" code.
 */
TERM    term    = {
        NROW-1,
        NCOL,
        &ttopen,
        &ttclose,
        &ttgetc,
        &ttputc,
        &ttflush,
        &ansimove,
        &ansieeol,
        &ansieeop,
        &ansibeep
};

/*
 * Write a character to the display. On VMS, terminal output is buffered, and
 * we just put the characters in the big array, after checking for overflow.
 * On CPM terminal I/O unbuffered, so we just write the byte out. Ditto on
 * MS-DOS (use the very very raw console output routine).
 */
ttputc(c) {
    fputc(c, stdout);
}

ansimove(row, col) {
    fprintf(stdout, "\033[%d;%dH", row + 1, col + 1);
}

ansieeol() {
    fputs("\033[K", stdout);
}

ansieeop() {
    fputs("\033[J", stdout);
}

ansibeep() {
        ttputc(BEL);
        ttflush();
}

ttopen() {
  struct winsize w;
  struct  termios nstate;

  /* save terminal flags */
  if ((tcgetattr(0, &ostate) < 0)) {
      puts ("Can't read terminal capabilites\n");
      exit (1);
  }
  nstate = ostate;
  cfmakeraw(&nstate);		/* set raw mode */
  nstate.c_cc[VMIN] = 1;
  nstate.c_cc[VTIME] = 0;	/* block indefinitely for a single char */
  if (tcsetattr(0, TCSADRAIN, &nstate) < 0) {
      puts ("Can't set terminal mode\n");
      exit (1);
  }
  /* provide a smaller terminal output buffer so that the type ahead
   * detection works better (more often) */
  /*setbuffer(stdout, &tobuf[0], TBUFSIZ);*/
  signal(SIGTSTP, SIG_DFL);

  if (ioctl(0, TIOCGWINSZ, &w) >= 0) {
      term.t_ncol = w.ws_col;
      term.t_nrow = w.ws_row - 1;
  }
}

/*
 * Flush terminal buffer. Does real work where the terminal output is buffered
 * up. A no-operation on systems where byte at a time terminal I/O is done.
 */
ttflush() {
    fflush(stdout);
}

/*
 * This function gets called just before we go back home to the command
 * interpreter. On VMS it puts the terminal back in a reasonable state.
 * Another no-operation on CPM.
 */
ttclose() {
  ttflush ();
  if (tcsetattr(0, TCSADRAIN, &ostate) < 0) {
      puts ("Can't restore terminal flags");
      exit (1);
  }
}

/*
 * Read a character from the terminal, performing no editing and doing no echo
 * at all. More complex in VMS that almost anyplace else, which figures. Very
 * simple on CPM, because the system can do exactly what you want.
 */
ttgetc() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) return -1;
  }

  if (c == '\x1b') {
    char seq[3];

    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1]) {
            case '1': return CTRL|'A'; /* HOME_KEY; */
            case '3': return CTRL|'D'; /* DEL_KEY;  */
            case '4': return CTRL|'E'; /* END_KEY;  */
            case '5': return META|'V'; /* PAGE_UP;  */
            case '6': return CTRL|'V'; /* PAGE_DOWN;*/
            case '7': return CTRL|'A'; /* HOME_KEY; */
            case '8': return CTRL|'E'; /* END_KEY;  */
          }
        }
      } else {
        switch (seq[1]) {
          case 'A': return CTRL|'P'; /* ARROW_UP;   */
          case 'B': return CTRL|'N'; /* ARROW_DOWN; */
          case 'C': return CTRL|'F'; /* ARROW_RIGHT;*/
          case 'D': return CTRL|'B'; /* ARROW_LEFT; */
          case 'H': return CTRL|'A'; /* HOME_KEY;   */
          case 'F': return CTRL|'E'; /* END_KEY;    */
        }
      }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
        case 'H': return CTRL|'A'; /* HOME_KEY;*/
        case 'F': return CTRL|'E'; /* END_KEY; */
      }
    }

    return '\x1b';
  } else {
    return c;
  }
}

#endif
#endif
