/*
 * board.c
 * Robert Morris (rtm@eecs.harvard.edu)
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "cword.h"

void
getword(struct board *b, struct coord c, char *word)
{
	int l, x, y;
	enum dir dir;

	x = c.x;
	y = c.y;
	dir = c.dir;
	while(1){
		if(x >= b->xsize || y >= b->ysize)
			break;
		l = BOARD(b, x, y);
		if(l == '#')
			break;
		*word++ = l;
		if(dir == DOWN)
			y++;
		else
			x++;
	}
	*word = '\0';
}

void
putword(struct board *b, struct coord c, char *word)
{
	int i;

	i = 0;
	while(word[i]){
		assert(c.x < b->xsize && c.y < b->ysize);
		BOARD(b, c.x, c.y) = word[i];
		i++;
		c = nextcoord(c);
	}
}

struct coord
nextcoord(c)
struct coord c;
{
	if(c.dir == ACROSS)
		c.x++;
	else
		c.y++;
	return(c);
}

struct coord
prevcoord(c)
struct coord c;
{
	if(c.dir == ACROSS)
		--c.x;
	else
		--c.y;
	return(c);
}
