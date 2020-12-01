#include "ed.h"

#if WIN32

#include <windows.h>
#include <conio.h>

extern nteeop();
extern nteeol();
extern ntmove(int y, int x);
extern ntputc(int ch);
extern ntflush();
extern ntopen();
extern ntclose();
extern int ntgetc();
extern ntbeep();

static int LINES, COLS;    /* number of screens lines/cols */

static HANDLE con_out, old_con_out, con_in;
static DWORD save_con_out_mode, save_con_in_mode;
static CONSOLE_CURSOR_INFO orig_cursor_info;
static int text_attr = 7;
static char *screen;
static char *flags;
static int line, col;      /* current line/col in screen 0,0 */
static int _crlf = 1;

#if !defined(ENABLE_EXTENDED_FLAGS)
    #define ENABLE_EXTENDED_FLAGS 0x0080
#endif

TERM    term    = {
        0,
        0,
        ntopen,
        ntclose,
        ntgetc,
        ntputc,
        ntflush,
        ntmove,
        nteeol,
        nteeop,
        &ntbeep,
};

nteeop() {
    int scr_size, left;

    if ((line < 0 || line >= LINES) || (col < 0 || col >= COLS))
        return;

    scr_size = LINES * COLS;
    left = scr_size - (line * COLS + col);

    if (left > 0) {
        memset(screen + (line * COLS) + col, ' ', left);
        memset(flags + line, 1, LINES - line);
    }
}

nteeol() {
    if ((line < 0 || line >= LINES) || (col < 0 || col >= COLS))
        return;
    memset(screen + (line * COLS) + col, ' ', COLS - col);
    flags[line] = 1;
}

ntmove(int y, int x) {
    line = y;
    col  = x;
}

clear() {
    ntmove(0, 0);
    nteeop();
}

ntputc(int ch) {
    int n;

    if ((line < 0 || line >= LINES) || (col < 0 || col >= COLS))
        return;
    switch (ch) {
        case '\t':
            n = 8 - (col & 7);
            while (0 < n--)
                ntputc(' ');
            break;
        case '\r':
            col = 0;
            break;
        case '\n':
            nteeol();
            if (_crlf)
                col = 0;
            ++line;
            break;
        case '\b':
            if (0 < col)
                --col;
            else if (0 < line)
                --line;
            break;
        default:
            if (*(screen + (line * COLS) + col) != ch) {
                *(screen + (line * COLS) + col) = ch;
                flags[line] = 1;
            }
            ++col;
    }
    if (COLS <= col) {
        col = 0;
        ++line;
    }
}

ntflush() {
    int i;
    COORD c;
    DWORD n;

    c.X = 0;
    for (i = 0; i < LINES; ++i) {
        if (flags[i]) {
            c.Y = i;
            WriteConsoleOutputCharacter(con_out, &screen[i * COLS], COLS, c, &n);
            FillConsoleOutputAttribute(con_out, text_attr, COLS, c, &n);
            flags[i] = 0;
        }
    }
    c.X = col;
    c.Y = line;
    SetConsoleCursorPosition(con_out, c);
}

BOOL WINAPI nthandler(DWORD ctrl_type) {
    switch (ctrl_type) {
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            break;
    }
    return TRUE;
}

get_term_size0() {
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    GetConsoleScreenBufferInfo(con_out, &csbi);
    COLS = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    LINES = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    screen = realloc(screen, LINES * COLS);
    flags  = realloc(flags, LINES);
    memset(flags, 1, LINES);
    term.t_nrow = LINES - 1;
    term.t_ncol = COLS;
}

ntopen() {
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    /* screen */
    old_con_out = GetStdHandle(STD_OUTPUT_HANDLE);

    con_out = CreateConsoleScreenBuffer(GENERIC_READ|GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
    SetConsoleActiveScreenBuffer(con_out);

    GetConsoleScreenBufferInfo(con_out, &csbi);
    GetConsoleMode(con_out, &save_con_out_mode);
    SetConsoleMode(con_out, 0);
    text_attr = csbi.wAttributes;
    get_term_size0();
    clear();

    /* keyboard */
    con_in = CreateFile("CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
    GetConsoleMode(con_in, &save_con_in_mode);
    SetConsoleMode(con_in, ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS | ENABLE_MOUSE_INPUT);

    SetConsoleCtrlHandler(nthandler, TRUE);

    GetConsoleCursorInfo(con_out, &orig_cursor_info);
}

ntclose() {
    /* screen */
    if (old_con_out) {
        SetConsoleMode(con_out, save_con_out_mode);
        SetConsoleActiveScreenBuffer(old_con_out);
        CloseHandle(con_out);
        old_con_out = 0;
    }

    SetConsoleCtrlHandler(nthandler, FALSE);
}

/*
 * Read a character from the terminal, performing no editing and doing no echo
 * at all.
 */
int ntgetc() {
    int c;

    for (;;) {
        c = getch();
        if (c != 0 && (c != 224 && !kbhit()))
            return c;

        c = getch();
        switch (c) {
            case 'K': return CTRL|'B';
            case 'M': return CTRL|'F';
            case 'H': return CTRL|'P';
            case 'P': return CTRL|'N';

            case 'I': return META|'V';
            case 'Q': return CTRL|'V';

            case 'G': return CTRL|'A';
            case 'O': return CTRL|'E';

            default: break;
        }
    }
}

ntbeep() {
        MessageBeep(0xffffffff);
}
#endif
