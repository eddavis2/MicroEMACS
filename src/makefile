CC  = gcc
# -flto causes link errors
#CFLAGS = -std=c89 -Os -s -fomit-frame-pointer -flto -fno-ident -fno-asynchronous-unwind-tables
CFLAGS = -std=c89 -Os -s -fomit-frame-pointer       -fno-ident -fno-asynchronous-unwind-tables

ifeq ($(OS), Windows_NT)
    LDFLAGS = -s -flto
    EXE     = me.exe

    #check to see if cygwin
    ifeq ($(findstring .exe,$(SHELL)),)
        LDFLAGS = -s -flto
        EXE     = me
    endif
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S), Linux)
        LDFLAGS = -s -flto -fuse-ld=gold
        EXE     = me
    endif
endif

OBJ =         \
	ansi.o    \
	basic.o   \
	buffer.o  \
	display.o \
	file.o    \
	fileio.o  \
	line.o    \
	main.o    \
	ntterm.o  \
	random.o  \
	region.o  \
	search.o  \
	spawn.o   \
	window.o  \
	word.o

all: $(EXE)

$(EXE): $(OBJ)
	$(CC) -o $(EXE) $(OBJ) $(LDFLAGS)

ansi.o:     ansi.c     ed.h
basic.o:    basic.c    ed.h
buffer.o:   buffer.c   ed.h
display.o:  display.c  ed.h
file.o:     file.c     ed.h
fileio.o:   fileio.c   ed.h
line.o:     line.c     ed.h
main.o:     main.c     ed.h
ntterm.o:   ntterm.c   ed.h
random.o:   random.c   ed.h
region.o:   region.c   ed.h
search.o:   search.c   ed.h
spawn.o:    spawn.c    ed.h
window.o:   window.c   ed.h
word.o:     word.c     ed.h

clean:
	rm -f $(OBJ)
