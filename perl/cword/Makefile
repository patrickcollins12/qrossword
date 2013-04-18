OBJS = cword.o words.o board.o
CFILES = cword.c words.c board.c
TARFILES = README $(CFILES) cword.h pattern1 pattern6 5x5 4x4 Makefile
CFLAGS = -O
CC = cc

cword	:	$(OBJS)
		$(CC) -o cword $(CFLAGS) $(OBJS)

mkpat	:	mkpat.c
		$(CC) -o mkpat mkpat.c -g

cwprint	:	cwprint.c
		$(CC) -o cwprint cwprint.c -g

clean	:
		rm -f cword $(OBJS) core mkpat cwprint

lint	:	
		lint $(CFILES)

tar		:
		tar cf cword.tar $(TARFILES)
