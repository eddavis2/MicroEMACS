/*
 * This program is in public domain; written by Dave G. Conroy.
 * This file contains the main driving routine, and some keyboard processing
 * code, for the MicroEMACS screen editor.
 *
 * REVISION HISTORY:
 *
 * 1.0  Steve Wilhite, 30-Nov-85
 *      - Removed the old LK201 and VT100 logic. Added code to support the
 *        DEC Rainbow keyboard (which is a LK201 layout) using the the Level
 *        1 Console In ROM INT. See "rainbow.h" for the function key definitions.
 *
 * 2.0  George Jones, 12-Dec-85
 *      - Ported to Amiga.
 */
#include        <stdio.h>
#include        "ed.h"

#if     VMS
#include        <ssdef.h>
#define GOOD    (SS$_NORMAL)
#endif

#ifndef GOOD
#define GOOD    0
#endif

int     currow;                         /* Working cursor row           */
int     curcol;                         /* Working cursor column        */
int     fillcol;                        /* Current fill column          */
int     thisflag;                       /* Flags, this command          */
int     lastflag;                       /* Flags, last command          */
int     curgoal;                        /* Goal column                  */
BUFFER  *curbp;                         /* Current buffer               */
WINDOW  *curwp;                         /* Current window               */
BUFFER  *bheadp;                        /* BUFFER listhead              */
WINDOW  *wheadp;                        /* WINDOW listhead              */
BUFFER  *blistp;                        /* Buffer list BUFFER           */
short   kbdm[NKBDM] = {CTLX|')'};       /* Macro                        */
short   *kbdmip;                        /* Input  for above             */
short   *kbdmop;                        /* Output for above             */
char    pat[NPAT];                      /* Pattern                      */

typedef struct  {
        short   k_code;                 /* Key code                     */
        int     (*k_fp)();              /* Routine to handle it         */
}       KEYTAB;

extern  int     ctrlg();                /* Abort out of things          */
extern  int     quit();                 /* Quit                         */
extern  int     ctlxlp();               /* Begin macro                  */
extern  int     ctlxrp();               /* End macro                    */
extern  int     ctlxe();                /* Execute macro                */
extern  int     fileread();             /* Get a file, read only        */
extern  int     filevisit();            /* Get a file, read write       */
extern  int     filewrite();            /* Write a file                 */
extern  int     filesave();             /* Save current file            */
extern  int     filename();             /* Adjust file name             */
extern  int     getccol();              /* Get current column           */
extern  int     gotobol();              /* Move to start of line        */
extern  int     forwchar();             /* Move forward by characters   */
extern  int     gotoeol();              /* Move to end of line          */
extern  int     backchar();             /* Move backward by characters  */
extern  int     forwline();             /* Move forward by lines        */
extern  int     backline();             /* Move backward by lines       */
extern  int     forwpage();             /* Move forward by pages        */
extern  int     backpage();             /* Move backward by pages       */
extern  int     gotobob();              /* Move to start of buffer      */
extern  int     gotoeob();              /* Move to end of buffer        */
extern  int     setfillcol();           /* Set fill column.             */
extern  int     setmark();              /* Set mark                     */
extern  int     swapmark();             /* Swap "." and mark            */
extern  int     forwsearch();           /* Search forward               */
extern  int     backsearch();           /* Search backwards             */
extern  int     showcpos();             /* Show the cursor position     */
extern  int     nextwind();             /* Move to the next window      */
extern  int     prevwind();             /* Move to the previous window  */
extern  int     onlywind();             /* Make current window only one */
extern  int     splitwind();            /* Split current window         */
extern  int     mvdnwind();             /* Move window down             */
extern  int     mvupwind();             /* Move window up               */
extern  int     enlargewind();          /* Enlarge display window.      */
extern  int     shrinkwind();           /* Shrink window.               */
extern  int     listbuffers();          /* Display list of buffers      */
extern  int     usebuffer();            /* Switch a window to a buffer  */
extern  int     killbuffer();           /* Make a buffer go away.       */
extern  int     reposition();           /* Reposition window            */
extern  int     refresh();              /* Refresh the screen           */
extern  int     twiddle();              /* Twiddle characters           */
extern  int     tab();                  /* Insert tab                   */
extern  int     newline();              /* Insert CR-LF                 */
extern  int     indent();               /* Insert CR-LF, then indent    */
extern  int     openline();             /* Open up a blank line         */
extern  int     deblank();              /* Delete blank lines           */
extern  int     quote();                /* Insert literal               */
extern  int     backword();             /* Backup by words              */
extern  int     forwword();             /* Advance by words             */
extern  int     forwdel();              /* Forward delete               */
extern  int     backdel();              /* Backward delete              */
extern  int     kill();                 /* Kill forward                 */
extern  int     yank();                 /* Yank back from killbuffer.   */
extern  int     upperword();            /* Upper case word.             */
extern  int     lowerword();            /* Lower case word.             */
extern  int     upperregion();          /* Upper case region.           */
extern  int     lowerregion();          /* Lower case region.           */
extern  int     capword();              /* Initial capitalize word.     */
extern  int     delfword();             /* Delete forward word.         */
extern  int     delbword();             /* Delete backward word.        */
extern  int     killregion();           /* Kill region.                 */
extern  int     copyregion();           /* Copy region to kill buffer.  */
extern  int     spawncli();             /* Run CLI in a subjob.         */
extern  int     spawn();                /* Run a command in a subjob.   */
extern  int     quickexit();            /* low keystroke style exit.    */

/*
 * Command table.
 * This table  is *roughly* in ASCII order, left to right across the
 * characters of the command. This expains the funny location of the
 * control-X commands.
 */
KEYTAB  keytab[] = {
        CTRL|'@',               &setmark,
        CTRL|'A',               &gotobol,
        CTRL|'B',               &backchar,
        CTRL|'C',               &spawncli,      /* Run CLI in subjob.   */
        CTRL|'D',               &forwdel,
        CTRL|'E',               &gotoeol,
        CTRL|'F',               &forwchar,
        CTRL|'G',               &ctrlg,
        CTRL|'H',               &backdel,
        CTRL|'I',               &tab,
        CTRL|'J',               &indent,
        CTRL|'K',               &kill,
        CTRL|'L',               &refresh,
        CTRL|'M',               &newline,
        CTRL|'N',               &forwline,
        CTRL|'O',               &openline,
        CTRL|'P',               &backline,
        CTRL|'Q',               &quote,         /* Often unreachable    */
        CTRL|'R',               &backsearch,
        CTRL|'S',               &forwsearch,    /* Often unreachable    */
        CTRL|'T',               &twiddle,
        CTRL|'V',               &forwpage,
        CTRL|'W',               &killregion,
        CTRL|'Y',               &yank,
        CTRL|'Z',               &quickexit,     /* quick save and exit  */
        CTLX|CTRL|'B',          &listbuffers,
        CTLX|CTRL|'C',          &quit,          /* Hard quit.           */
        CTLX|CTRL|'F',          &filename,
        CTLX|CTRL|'L',          &lowerregion,
        CTLX|CTRL|'O',          &deblank,
        CTLX|CTRL|'N',          &mvdnwind,
        CTLX|CTRL|'P',          &mvupwind,
        CTLX|CTRL|'R',          &fileread,
        CTLX|CTRL|'S',          &filesave,      /* Often unreachable    */
        CTLX|CTRL|'U',          &upperregion,
        CTLX|CTRL|'V',          &filevisit,
        CTLX|CTRL|'W',          &filewrite,
        CTLX|CTRL|'X',          &swapmark,
        CTLX|CTRL|'Z',          &shrinkwind,
        CTLX|'!',               &spawn,         /* Run 1 command.       */
        CTLX|'=',               &showcpos,
        CTLX|'(',               &ctlxlp,
        CTLX|')',               &ctlxrp,
        CTLX|'1',               &onlywind,
        CTLX|'2',               &splitwind,
        CTLX|'B',               &usebuffer,
        CTLX|'E',               &ctlxe,
        CTLX|'F',               &setfillcol,
        CTLX|'K',               &killbuffer,
        CTLX|'N',               &nextwind,
        CTLX|'P',               &prevwind,
        CTLX|'Z',               &enlargewind,
        META|CTRL|'H',          &delbword,
        META|'!',               &reposition,
        META|'.',               &setmark,
        META|'>',               &gotoeob,
        META|'<',               &gotobob,
        META|'B',               &backword,
        META|'C',               &capword,
        META|'D',               &delfword,
        META|'F',               &forwword,
        META|'L',               &lowerword,
        META|'U',               &upperword,
        META|'V',               &backpage,
        META|'W',               &copyregion,
        META|0x7F,              &delbword,
        0x7F,                   &backdel
};

#define NKEYTAB (sizeof(keytab)/sizeof(keytab[0]))

#if RAINBOW

#include "rainbow.h"

/*
 * Mapping table from the LK201 function keys to the internal EMACS character.
 */

short lk_map[][2] = {
        Up_Key,                         CTRL+'P',
        Down_Key,                       CTRL+'N',
        Left_Key,                       CTRL+'B',
        Right_Key,                      CTRL+'F',
        Shift+Left_Key,                 META+'B',
        Shift+Right_Key,                META+'F',
        Control+Left_Key,               CTRL+'A',
        Control+Right_Key,              CTRL+'E',
        Prev_Scr_Key,                   META+'V',
        Next_Scr_Key,                   CTRL+'V',
        Shift+Up_Key,                   META+'<',
        Shift+Down_Key,                 META+'>',
        Cancel_Key,                     CTRL+'G',
        Find_Key,                       CTRL+'S',
        Shift+Find_Key,                 CTRL+'R',
        Insert_Key,                     CTRL+'Y',
        Options_Key,                    CTRL+'D',
        Shift+Options_Key,              META+'D',
        Remove_Key,                     CTRL+'W',
        Shift+Remove_Key,               META+'W',
        Select_Key,                     CTRL+'@',
        Shift+Select_Key,               CTLX+CTRL+'X',
        Interrupt_Key,                  CTRL+'U',
        Keypad_PF2,                     META+'L',
        Keypad_PF3,                     META+'C',
        Keypad_PF4,                     META+'U',
        Shift+Keypad_PF2,               CTLX+CTRL+'L',
        Shift+Keypad_PF4,               CTLX+CTRL+'U',
        Keypad_1,                       CTLX+'1',
        Keypad_2,                       CTLX+'2',
        Do_Key,                         CTLX+'E',
        Keypad_4,                       CTLX+CTRL+'B',
        Keypad_5,                       CTLX+'B',
        Keypad_6,                       CTLX+'K',
        Resume_Key,                     META+'!',
        Control+Next_Scr_Key,           CTLX+'N',
        Control+Prev_Scr_Key,           CTLX+'P',
        Control+Up_Key,                 CTLX+CTRL+'P',
        Control+Down_Key,               CTLX+CTRL+'N',
        Help_Key,                       CTLX+'=',
        Shift+Do_Key,                   CTLX+'(',
        Control+Do_Key,                 CTLX+')',
        Keypad_0,                       CTLX+'Z',
        Shift+Keypad_0,                 CTLX+CTRL+'Z',
        Main_Scr_Key,                   CTRL+'C',
        Keypad_Enter,                   CTLX+'!',
        Exit_Key,                       CTLX+CTRL+'C',
        Shift+Exit_Key,                 CTRL+'Z'
        };

#define lk_map_size     (sizeof(lk_map)/2)

#endif

main(argc, argv)
char    *argv[];
{
        register int    c;
        register int    f;
        register int    n;
        register int    mflag;
        char            bname[NBUFN];

        strcpy(bname, "main");                  /* Work out the name of */
        if (argc > 1)                           /* the default buffer.  */
                makename(bname, argv[1]);
        vtinit();                               /* Displays.            */
        edinit(bname);                          /* Buffers, windows.    */
        if (argc > 1) {
                update();                       /* You have to update   */
                readin(argv[1]);                /* in case "[New file]" */
        }
        lastflag = 0;                           /* Fake last flags.     */
loop:
        update();                               /* Fix up the screen    */
        c = getkey();
        if (mpresf != FALSE) {
                mlerase();
                update();
                if (c == ' ')                   /* ITS EMACS does this  */
                        goto loop;
        }
        f = FALSE;
        n = 1;
        if (c == (CTRL|'U')) {                  /* ^U, start argument   */
                f = TRUE;
                n = 4;                          /* with argument of 4 */
                mflag = 0;                      /* that can be discarded. */
                mlwrite("Arg: 4");
                while ((c=getkey()) >='0' && c<='9' || c==(CTRL|'U') || c=='-'){
                        if (c == (CTRL|'U'))
                                n = n*4;
                        /*
                         * If dash, and start of argument string, set arg.
                         * to -1.  Otherwise, insert it.
                         */
                        else if (c == '-') {
                                if (mflag)
                                        break;
                                n = 0;
                                mflag = -1;
                        }
                        /*
                         * If first digit entered, replace previous argument
                         * with digit and set sign.  Otherwise, append to arg.
                         */
                        else {
                                if (!mflag) {
                                        n = 0;
                                        mflag = 1;
                                }
                                n = 10*n + c - '0';
                        }
                        mlwrite("Arg: %d", (mflag >=0) ? n : (n ? -n : -1));
                }
                /*
                 * Make arguments preceded by a minus sign negative and change
                 * the special argument "^U -" to an effective "^U -1".
                 */
                if (mflag == -1) {
                        if (n == 0)
                                n++;
                        n = -n;
                }
        }
        if (c == (CTRL|'X'))                    /* ^X is a prefix       */
                c = CTLX | getctl();
        if (kbdmip != NULL) {                   /* Save macro strokes.  */
                if (c!=(CTLX|')') && kbdmip>&kbdm[NKBDM-6]) {
                        ctrlg(FALSE, 0);
                        goto loop;
                }
                if (f != FALSE) {
                        *kbdmip++ = (CTRL|'U');
                        *kbdmip++ = n;
                }
                *kbdmip++ = c;
        }
        execute(c, f, n);                       /* Do it.               */
        goto loop;
}

/*
 * Initialize all of the buffers and windows. The buffer name is passed down
 * as an argument, because the main routine may have been told to read in a
 * file by default, and we want the buffer name to be right.
 */
edinit(bname)
char    bname[];
{
        register BUFFER *bp;
        register WINDOW *wp;

        bp = bfind(bname, TRUE, 0);             /* First buffer         */
        blistp = bfind("[List]", TRUE, BFTEMP); /* Buffer list buffer   */
        wp = (WINDOW *) malloc(sizeof(WINDOW)); /* First window         */
        if (bp==NULL || wp==NULL || blistp==NULL)
                exit(1);
        curbp  = bp;                            /* Make this current    */
        wheadp = wp;
        curwp  = wp;
        wp->w_wndp  = NULL;                     /* Initialize window    */
        wp->w_bufp  = bp;
        bp->b_nwnd  = 1;                        /* Displayed.           */
        wp->w_linep = bp->b_linep;
        wp->w_dotp  = bp->b_linep;
        wp->w_doto  = 0;
        wp->w_markp = NULL;
        wp->w_marko = 0;
        wp->w_toprow = 0;
        wp->w_ntrows = term.t_nrow-1;           /* "-1" for mode line.  */
        wp->w_force = 0;
        wp->w_flag  = WFMODE|WFHARD;            /* Full.                */
}

/*
 * This is the general command execution routine. It handles the fake binding
 * of all the keys to "self-insert". It also clears out the "thisflag" word,
 * and arranges to move it to the "lastflag", so that the next command can
 * look at it. Return the status of command.
 */
execute(c, f, n)
{
        register KEYTAB *ktp;
        register int    status;

        ktp = &keytab[0];                       /* Look in key table.   */
        while (ktp < &keytab[NKEYTAB]) {
                if (ktp->k_code == c) {
                        thisflag = 0;
                        status   = (*ktp->k_fp)(f, n);
                        lastflag = thisflag;
                        return (status);
                }
                ++ktp;
        }

        /*
         * If a space was typed, fill column is defined, the argument is non-
         * negative, and we are now past fill column, perform word wrap.
         */
        if (c == ' ' && fillcol > 0 && n>=0 && getccol(FALSE) > fillcol)
                wrapword();

        if ((c>=0x20 && c<=0x7E)                /* Self inserting.      */
        ||  (c>=0xA0 && c<=0xFE)) {
                if (n <= 0) {                   /* Fenceposts.          */
                        lastflag = 0;
                        return (n<0 ? FALSE : TRUE);
                }
                thisflag = 0;                   /* For the future.      */
                status   = linsert(n, c);
                lastflag = thisflag;
                return (status);
        }
        lastflag = 0;                           /* Fake last flags.     */
        return (FALSE);
}

/*
 * Read in a key.
 * Do the standard keyboard preprocessing. Convert the keys to the internal
 * character set.
 */
getkey()
{
        register int    c;

        c = (*term.t_getchar)();

#if RAINBOW

        if (c & Function_Key)
                {
                int i;

                for (i = 0; i < lk_map_size; i++)
                        if (c == lk_map[i][0])
                                return lk_map[i][1];
                }
        else if (c == Shift + 015) return CTRL | 'J';
        else if (c == Shift + 0x7F) return META | 0x7F;
#endif

        if (c == METACH) {                      /* Apply M- prefix      */
                c = getctl();
                return (META | c);
        }

        if (c>=0x00 && c<=0x1F)                 /* C0 control -> C-     */
                c = CTRL | (c+'@');
        return (c);
}

/*
 * Get a key.
 * Apply control modifications to the read key.
 */
getctl()
{
        register int    c;

        c = (*term.t_getchar)();
        if (c>='a' && c<='z')                   /* Force to upper       */
                c -= 0x20;
        if (c>=0x00 && c<=0x1F)                 /* C0 control -> C-     */
                c = CTRL | (c+'@');
        return (c);
}

/*
 * Fancy quit command, as implemented by Norm. If the current buffer has
 * changed do a write current buffer and exit emacs, otherwise simply exit.
 */
quickexit(f, n)
{
        if ((curbp->b_flag&BFCHG) != 0          /* Changed.             */
        && (curbp->b_flag&BFTEMP) == 0)         /* Real.                */
                filesave(f, n);
        quit(f, n);                             /* conditionally quit   */
}

/*
 * Quit command. If an argument, always quit. Otherwise confirm if a buffer
 * has been changed and not written out. Normally bound to "C-X C-C".
 */
quit(f, n)
{
        register int    s;

        if (f != FALSE                          /* Argument forces it.  */
        || anycb() == FALSE                     /* All buffers clean.   */
        || (s=mlyesno("Quit")) == TRUE) {       /* User says it's OK.   */
                vttidy();
                exit(GOOD);
        }
        return (s);
}

/*
 * Begin a keyboard macro.
 * Error if not at the top level in keyboard processing. Set up variables and
 * return.
 */
ctlxlp(f, n)
{
        if (kbdmip!=NULL || kbdmop!=NULL) {
                mlwrite("Not now");
                return (FALSE);
        }
        mlwrite("[Start macro]");
        kbdmip = &kbdm[0];
        return (TRUE);
}

/*
 * End keyboard macro. Check for the same limit conditions as the above
 * routine. Set up the variables and return to the caller.
 */
ctlxrp(f, n)
{
        if (kbdmip == NULL) {
                mlwrite("Not now");
                return (FALSE);
        }
        mlwrite("[End macro]");
        kbdmip = NULL;
        return (TRUE);
}

/*
 * Execute a macro.
 * The command argument is the number of times to loop. Quit as soon as a
 * command gets an error. Return TRUE if all ok, else FALSE.
 */
ctlxe(f, n)
{
        register int    c;
        register int    af;
        register int    an;
        register int    s;

        if (kbdmip!=NULL || kbdmop!=NULL) {
                mlwrite("Not now");
                return (FALSE);
        }
        if (n <= 0)
                return (TRUE);
        do {
                kbdmop = &kbdm[0];
                do {
                        af = FALSE;
                        an = 1;
                        if ((c = *kbdmop++) == (CTRL|'U')) {
                                af = TRUE;
                                an = *kbdmop++;
                                c  = *kbdmop++;
                        }
                        s = TRUE;
                } while (c!=(CTLX|')') && (s=execute(c, af, an))==TRUE);
                kbdmop = NULL;
        } while (s==TRUE && --n);
        return (s);
}

/*
 * Abort.
 * Beep the beeper. Kill off any keyboard macro, etc., that is in progress.
 * Sometimes called as a routine, to do general aborting of stuff.
 */
ctrlg(f, n)
{
        (*term.t_beep)();
        if (kbdmip != NULL) {
                kbdm[0] = (CTLX|')');
                kbdmip  = NULL;
        }
        return (ABORT);
}
