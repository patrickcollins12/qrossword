/*
 * cword.h
 * Robert Morris (rtm@eecs.harvard.edu)
 */

/* extendable word list */
struct wlist{
	long size;	/* size of allocated storage */
	long len;	/* number of entries */
	char **words;	/* pointer to array of char pointers */
	struct wlist *shared;
	int refs;
	int preserve;
};

extern struct wlist *new_wlist(), *match(), *sharewlist();

enum dir { DOWN, ACROSS };
#define OTHERDIR(d) ((d) == ACROSS ? DOWN : ACROSS)

struct coord{
	int x, y;
	enum dir dir;
};
extern struct coord findblank(), prevcoord(), nextcoord(), findstart();
#define INBOARD(b, c) (c.x>=0 && c.y>=0 && c.x<b->xsize && c.y<b->ysize)

#define MAXLEN 50	/* max word length or board dimension */

struct board{
	int xsize, ysize;
	char data[MAXLEN][MAXLEN];
	struct wlist *hints[2][MAXLEN][MAXLEN];
	struct coord starts[2][MAXLEN][MAXLEN];
	struct coord ostarts[2][MAXLEN][MAXLEN];
};
extern struct board *readboard();

void read_words(char *file);
void randomize_all();
void analyze_all();
void getword(struct board *b, struct coord c, char *word);
void add_to_words(char *w);
void putword(struct board *b, struct coord c, char *word);
void free_wlist(struct wlist *wl);

#define BOARD(b, x, y) ((b)->data[x][y])
#define HINT(b, c) ((b)->hints[c.dir==ACROSS][c.x][c.y])
#define START(b, c) ((b)->starts[c.dir==ACROSS][c.x][c.y])
#define OSTART(b, c) ((b)->ostarts[c.dir==ACROSS][c.x][c.y])
#define WILDCARD '?'

#define NEW(t) ((t *)malloc(sizeof(t)))
#define NSTR(s) (strcpy(malloc(strlen(s)+1), s))

# ifndef NDEBUG
# define _assert(ex)	{if (!(ex)){(void)fprintf(stderr,"Assertion failed: file \"%s\", line %d\n", __FILE__, __LINE__);abort();}}
# define assert(ex)	_assert(ex)
# else
# define _assert(ex)
# define assert(ex)
# endif
