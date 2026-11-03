# C89 version of the xBase interpreter
CC      ?= cc
CFLAGS  ?= -O2 -std=c89 -pedantic -Wall -Wextra
LIBS    = -lm
# make GDSM=/path/to/mikebeer/gdsm: the full-screen console (SET GDSM ON,
# BROWSE, EDIT, APPEND) through GDSM's build/libgdsm.a
ifdef GDSM
CFLAGS += -DXB_GDSM -I$(GDSM)/include
LIBS   := $(GDSM)/build/libgdsm.a $(GDSM_LIBS) $(LIBS)
endif
SRC     = core.c ops.c eval.c exec.c main.c dot.c fs.c \
          lex.c pexpr.c pstmt.c pclass.c pcmd.c cmd.c \
          dbf.c lib1.c lib2.c lib3.c lib4.c io.c
OBJ     = $(SRC:.c=.o)

all: xbase-c

xbase-c: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LIBS)

%.o: %.c xb.h xb_consts.h xb_parse.h
	$(CC) $(CFLAGS) -c $<

lines:
	@awk 'length($$0)>71 {print FILENAME ": " FNR ": " length($$0)}' \
	  *.c *.h Makefile | grep . && exit 1 || echo "lines ok"

clean:
	rm -f xbase-c *.o

.PHONY: all lines clean
