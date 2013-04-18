/*
 * cword.c
 * Robert Morris (rtm@eecs.harvard.edu)
 */

#include <stdio.h>
#include <signal.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "cword.h"

extern void printstatus();
extern long makelc(), checkfind();
void addwords(struct board *b);
void addword(struct board *b, struct coord c);
int tryaword(struct board *b, struct coord c, char *word, long *bad);
void orderwords(struct board *b, struct coord c);
int trynext(struct board *b, int sp, int firsttime);
void sortorder(struct board *b, int first, int n);
void reorder(struct board *b, int sp);
void myqsort(char *a[], long n, int width, int (*compar)());
void xqsort(char **a, long first, long last);

struct board *xxb;
int debug;

int
main(argc, argv)
char *argv[];
{
	int i;
	struct board *b;

	if(argc > 1 && strcmp(argv[1], "-d") == 0){
		--argc;
		argv++;
		debug = 1;
	}
	if(argc > 1 && strcmp(argv[1], "-t") == 0){
		--argc;
		argv++;
		alarm(60);
	}
	if(argc < 3){
		fprintf(stderr, "Usage: cword pattern dictionaries...\n");
		exit(1);
	}

#ifdef SIGQUIT
	signal(SIGINT, (void (*)())exit);
	signal(SIGQUIT, SIG_IGN);
#endif
	for(i = 2; i < argc; i++) {
		read_words(argv[i]);
	}
	
	randomize_all();
	
	analyze_all();
	
	/* sort_all(); */

	xxb = b = readboard(argv[1]);
	

#ifdef SIGQUIT
	signal(SIGQUIT, printstatus);
#endif

	addwords(b);
	

	exit(0);
}

struct board *
readboard(file)
char *file;
{
	FILE *fp;
	struct board *b;
	char buf[512], word[MAXLEN+1];
	int x, y;
	struct coord c, s, c1;

	fp = fopen(file, "r");
	if(fp == 0){
		perror(file);
		exit(1);
	}

	b = NEW(struct board);
	bzero(b, sizeof(*b));
	fgets(buf, sizeof(buf), fp);
	b->xsize = strlen(buf) - 1;
	if(b->xsize < 3){
		fprintf(stderr, "cword: pattern %s too small\n", file);
		exit(1);
	}
	if(b->xsize > MAXLEN){
		fprintf(stderr, "cword: pattern %s too large\n", file);
		exit(1);
	}

	b->ysize = 0;
	do{
		if(b->ysize > MAXLEN){
			fprintf(stderr, "cword: pattern %s too long\n", file);
			exit(1);
		}
		if(strlen(buf)-1 != b->xsize){
			fprintf(stderr, "cword: line <%s> too short\n", buf);
			exit(1);
		}
		for(x = 0; x < b->xsize; x++){
			if(isupper(buf[x]))
				buf[x] = tolower(buf[x]);
			if(buf[x] == ' ')
				buf[x] = WILDCARD;
			BOARD(b, x, b->ysize) = buf[x];
		}
		b->ysize++;
	} while(fgets(buf, sizeof(buf), fp));
	fclose(fp);

	if(b->ysize < 3){
		fprintf(stderr, "cword: pattern %s too short\n", file);
		exit(1);
	}

	for(x = 0; x < b->xsize; x++){
		for(y = 0; y < b->ysize; y++){
			if(BOARD(b, x, y) == '#')
				continue;
			c.x = x;
			c.y = y;

			c.dir = ACROSS;
			s = START(b, c) = findstart(b, c, c.dir);
			if(!HINT(b, s)){
				getword(b, s, word);
				HINT(b, s) = match(word, 0);
				if(word[0] && !strchr(word, '?')){
					if(HINT(b, s) == 0){
						add_to_words(NSTR(word));
						HINT(b, s) = match(word, 0);
					}
					assert(HINT(b, s));
					HINT(b, s)->preserve = 1;
				}
				if(HINT(b, s) == 0){
					fprintf(stderr, "no words for %s",
						word);
					fprintf(stderr, " at %d %d %s\n",
						s.x, s.y,
						s.dir==ACROSS?"across":"down");
					exit(1);
				}
			}
			assert(HINT(b, s) && HINT(b, s)->words);

			c.dir = DOWN;
			s = START(b, c) = findstart(b, c, c.dir);
			if(!HINT(b, s)){
				getword(b, s, word);
				HINT(b, s) = match(word, 0);
				if(word[0] && !strchr(word, '?')){
					if(HINT(b, s) == 0){
						add_to_words(NSTR(word));
						HINT(b, s) = match(word, 0);
					}
					assert(HINT(b, s));
					HINT(b, s)->preserve = 1;
				}
				if(HINT(b, s) == 0){
					fprintf(stderr, "no words for %s",
						word);
					fprintf(stderr, " at %d %d %s\n",
						s.x, s.y,
						s.dir==ACROSS?"across":"down");
					exit(1);
				}
			}
			assert(HINT(b, s) && HINT(b, s)->words);

			c1 = c;
			c1.dir = OTHERDIR(c1.dir);
			OSTART(b, c) = START(b, c1);
			OSTART(b, c1) = START(b, c);
		}
	}

	return(b);
}

void
printboard(b)
struct board *b;
{
	int x, y, c;

	for(y = 0; y < b->ysize; y++){
		for(x = 0; x < b->xsize; x++){
			c = BOARD(b, x, y);
			if(islower(c))
				c = toupper(c);
			printf("%c", c);
		}
		printf("\n");
	}
}

char *badwords[500];
int nbad;

void
addwords(struct board *b)
{
	struct coord c;

	nbad = 0;
	while(1){
		c = findblank(b);
		printf("found blank %d %d\n", c.x,c.y);
		if(c.x < 0){
			/* got one */
			printboard(b);
			exit(0);
		}
		addword(b, c);
	}
}

/*
 * try to fit a word in at c. returns 1 if it did,
 * 0 if not. for each word that fits, see if all the perpendiculars
 * have possibilities. not recursive.
 */
int
tryword(b, c)
struct board *b;
struct coord c;
{
	struct wlist *wl;
	long i;

	wl = HINT(b, c);
	assert(wl);

	for(i = 0; i < wl->len; i++){
		if(tryaword(b, c, wl->words[i], (long *)0)){
			badwords[nbad++] = wl->words[i];
			return(1);
		}
	}
	return(0);
}

int
isbad(w)
char *w;
{
	int i;

	for(i = 0; i < nbad; i++)
		if(w == badwords[i])
			return(1);
	return(0);
}

int
tryaword(struct board *b, struct coord c, char *word, long *bad)
{
	int j;
	char oword[MAXLEN+1], word1[MAXLEN+1];
	struct coord c1, c2;
	struct wlist *hints[MAXLEN];

	if(bad){
		for(j = 0; word[j]; j++)
			if(bad[j] & (1L<<(word[j]-'a')))
				return(0);
	}

	if(isbad(word))
		return(0);
	getword(b, c, oword);
	putword(b, c, word);

	c1 = c;
	for(j = 0; word[j]; j++){
		c2 = OSTART(b, c1);
		getword(b, c2, word1);
		assert(HINT(b, c2));
		hints[j] = match(word1, HINT(b, c2));
		if(hints[j] == 0){
			if(bad)
				bad[j] |= (1 << (word[j]-'a'));
			break;
		}

		c1 = nextcoord(c1);
	}
	if(word[j] == '\0'){
		/* word[] is OK at coord c */
		c1 = c;
		for(j = 0; word[j]; j++){
			c2 = OSTART(b, c1);
			if(HINT(b, c2)->preserve){
				free_wlist(hints[j]);
			} else {
				free_wlist(HINT(b, c2));
				HINT(b, c2) = hints[j];
			}
			c1 = nextcoord(c1);
		}
		return(1);
	}

	/* failure */
	while(--j >= 0)
		free_wlist(hints[j]);
	putword(b, c, oword);
	return(0);
}

struct coord
findblank(b)
struct board *b;
{
	struct coord c;
	struct coord best;
	int x, y, firsty;
	long l, bestlen;

	bestlen = 1000000;
	firsty = best.x = best.y = -1;
	for(y = 0; y < b->ysize; y++){
		for(x = 0; x < b->xsize; x++){
			c.dir = ACROSS;
again:
			if(c.dir == DOWN){
				if(y != 0 && BOARD(b, x, y-1) != '#')
					goto out;
			} else {
				if(x != 0 && BOARD(b, x-1, y) != '#')
					goto out;
			}
			c.x = x;
			c.y = y;
			l = checkfind(b, c);
			if(l && l < bestlen){
				bestlen = l;
				best = c;
				if(firsty == -1)
					firsty = c.y;
			}
out:
			if(c.dir == ACROSS){
				c.dir = DOWN;
				goto again;
			}
		}
	}
	return(best);
}

long
checkfind(b, c)
struct board *b;
struct coord c;
{
	char word[MAXLEN+1];
	int i;
	long len;
	struct wlist *wl;

	getword(b, c, word);
	for(i = 0; word[i]; i++)
		if(word[i] == WILDCARD)
			break;
	if(word[i] && strlen(word) > 2){
		wl = match(word, 0);
		if(!wl){
			printf("nothing matches %s\n", word);
			exit(1);
		}
		len = wl->len;
		free_wlist(wl);
		return(len);
	}
	return(0);
}

struct coord
findstart(b, c, dir)
struct board *b;
struct coord c;
enum dir dir;
{
	int l;

	c.dir = dir;

	if(BOARD(b, c.x, c.y) == '#')
		return(c);
	while(1){
		if(!INBOARD(b, c)){
			c = nextcoord(c);
			return(c);
		}
		l = BOARD(b, c.x, c.y);
		if(l == '#'){
			c = nextcoord(c);
			return(c);
		}
		c = prevcoord(c);
	}
}

void
printstatus()
{
	printf("\n\n");
	printboard(xxb);
	fflush(stdout);
#ifdef SIGQUIT
	signal(SIGQUIT, printstatus);
#endif
}

struct coord order[400];
int norder;
long cutoff, maxcutoff;

/*
 * add a word at c. if necessary, modify the rest of the board
 * to make one fit. this involves putting an order on ALL the
 * words on the board, and doing a search through that exponential
 * space. this to avoid duplication of effort. the order changes
 * near words before far ones.
 */
void
addword(struct board *b, struct coord c)
{
	char word[MAXLEN+1];

	orderwords(b, c);

	if(tryword(b, c)){
		/* one fits with no modification */
		return;
	}

	cutoff = 20;
	while(1){
		maxcutoff = 0;
		getword(b, c, word);

/*
		printf("addword failed at %s cutoff %d\n", word, cutoff);
		printboard(b);
*/

		nbad = 0;
		if(trynext(b, norder - 1, 1))
			return;
		if(cutoff >= maxcutoff)
			break;
		cutoff += 5;
		if(cutoff > maxcutoff)
			cutoff = maxcutoff;
	}

	printf("addword %d %d %s failed\n", c.x, c.y, word);
	printf("maxcutoff %d\n", (int) maxcutoff);
	printboard(b);
	exit(1);
}

/*
 * add the cross words of the word starting at c to the order[]
 * list. don't add words that are already there. don't add
 * words that aren't completely filled in. if the current word
 * has no cross words (this does happen) try a parallel word.
 */
void
order1word(b, xc)
struct board *b;
struct coord xc;
{
	int i, on, hascross;
	struct coord c1, s, csave, c;
	char word[MAXLEN+1];

	c = xc;
	csave = c;
	on = norder;
	hascross = 0;
	for( ; INBOARD(b, c) && BOARD(b, c.x, c.y) != '#'; c = nextcoord(c)){
		s = OSTART(b, c);
		if(HINT(b, s)->preserve)
			continue;
		getword(b, s, word);
		if(strchr(word, '?'))
			continue;
		hascross = 1;
		for(i = 0; i < norder; i++)
			if(order[i].x == s.x && order[i].y == s.y
			   && order[i].dir == s.dir)
				break;
		if(i >= norder)
			order[norder++] = s;
	}
	if(hascross){
		sortorder(b, on, norder - on);
		return;
	}
	for(c = csave; INBOARD(b, c) && BOARD(b, c.x, c.y) != '#';
					c = nextcoord(c)){
		c1 = c; c1.dir = OTHERDIR(c1.dir);
		c1 = prevcoord(c1);
		if(INBOARD(b, c1) && isalpha(BOARD(b, c1.x, c1.y))){
			s = OSTART(b, c1);
			for(i = 0; i < norder; i++)
				if(order[i].x == s.x && order[i].y == s.y
				   && order[i].dir == s.dir)
					break;
			getword(b, s, word);
			if(HINT(b, s)->preserve == 0
			   && i >= norder && !strchr(word, '?')){
				printf("foo %s\n", word);
				order[norder++] = s;
			}
		}
		c1 = c; c1.dir = OTHERDIR(c1.dir);
		c1 = nextcoord(c1);
		if(INBOARD(b, c1) && isalpha(BOARD(b, c1.x, c1.y))){
			s = OSTART(b, c1);
			for(i = 0; i < norder; i++)
				if(order[i].x == s.x && order[i].y == s.y
				   && order[i].dir == s.dir)
					break;
			getword(b, s, word);
			if(HINT(b, s)->preserve == 0
			   && i >= norder && !strchr(word, '?')){
				printf("foo %s\n", word);
				order[norder++] = s;
			}
		}
		c = nextcoord(c);
	}
}

void
sortorder(struct board *b, int first, int n)
{
	int i, j;
	long ilen;
	struct coord tmp;

	for(i = first; i < first + n; i++){
		ilen = HINT(b, order[i])->len;
		for(j = i + 1; j < first + n; j++){
			if(HINT(b, order[j])->len > ilen){
				tmp = order[i];
				order[i] = order[j];
				order[j] = tmp;
				ilen = HINT(b, order[j])->len;
			}
		}
	}
}

void
orderwords(struct board *b, struct coord c)
{
	int on;

	on = norder = 0;
	order[norder++] = c;
	while(norder > on){
		order1word(b, order[on]);
		on++;
	}
}

short lc[MAXLEN][26];

int
trynext(struct board *b, int sp, int firsttime)
{
	struct wlist *wl;
	struct wlist *hints[MAXLEN];
	struct coord starts[MAXLEN];
	struct coord c, s;
	char word[MAXLEN+1];
	int i, len;
	long wn;
	long bv[MAXLEN];
	extern struct wlist *trysort();

	if(firsttime){
		badwords[nbad++] = HINT(b, order[sp])->words[0];
		if(sp > 0 && trynext(b, sp - 1, 1))
			return(1);
		--nbad;

		for(c = order[sp]; INBOARD(b, c) && BOARD(b, c.x, c.y) != '#';
							c = nextcoord(c)){
			s = OSTART(b, c);
			getword(b, s, word);
			if(strchr(word, '?')){
				BOARD(b, c.x, c.y) = '?';
				getword(b, s, word);
				free_wlist(HINT(b, s));
				HINT(b, s) = match(word, 0);
			}
		}
		free_wlist(HINT(b, order[sp]));
		getword(b, order[sp], word);
		HINT(b, order[sp]) = match(word, 0);
	}

	reorder(b, sp);

	getword(b, order[sp], word);
	wl = HINT(b, order[sp]);

	i = 0;
	for(c = order[sp]; INBOARD(b, c) && BOARD(b, c.x, c.y) != '#';
						c = nextcoord(c)){
		s = OSTART(b, c);
		starts[i] = s;
		hints[i] = HINT(b, s);
		HINT(b, s) = sharewlist(hints[i]);

#define ABS(x) ((x) < 0 ? -(x) : (x))
#define DIST(a, b) (ABS(a.x - b.x) + ABS(a.y - b.y))
		if(wl->len > 1)
			bv[i] = makelc(lc[i], hints[i], DIST(s, c));
		else
			bv[i] = 0;

		i++;
	}
	len = i;

	if(wl->len > maxcutoff)
		maxcutoff = wl->len;

	wl = trysort(wl);

#if 0
	checksort(b, order[sp], wl);
#endif

	/*
	 * could sort these words by the product of how many possibilities
	 * they allow for the cross-words!
	 */
	for(wn = 0; wn < wl->len && (wn < cutoff || sp == norder-1); wn++){
		if(tryaword(b, order[sp], wl->words[wn], bv)){
			if(debug)
				printf("%d %s -> %s\n", sp,
					word, wl->words[wn]);
			badwords[nbad++] = wl->words[wn];
			if(sp <= 0 || trynext(b, sp - 1, 0)){
				for(i = 0; i < len; i++)
					free_wlist(hints[i]);
				--nbad;
				free_wlist(wl);
				return(1);
			}
			--nbad;
			putword(b, order[sp], word);
			for(i = 0; i < len; i++){
				free_wlist(HINT(b, starts[i]));
				HINT(b, starts[i]) = sharewlist(hints[i]);
			}
		}
	}

	/* failed */
	for(i = 0; i < len; i++){
		/* prevent very long chains of shared wlists */
		free_wlist(HINT(b, starts[i]));
		HINT(b, starts[i]) = hints[i];
	}
	free_wlist(wl);
	return(0);
}

/*
 * find the most constrained word at or below this point in order[].
 */
void
reorder(struct board *b, int sp)
{
	int j, best;
	long bestlen;
	struct coord c;
	struct wlist *wl;

	best = -1;
	bestlen = 100000;
	for(j = 0; j <= sp; j++){
		c = order[j];
		wl = HINT(b, c);
		assert(wl);
		if(wl->len < bestlen){
			bestlen = wl->len;
			best = j;
		}
	}
	assert(best != -1);
	c = order[sp];
	order[sp] = order[best];
	order[best] = c;
}

long
makelc(lc, wl, ind)
short lc[26];
struct wlist *wl;
int ind;	/* nth letter of wl->words[] */
{
	long bv, i;
	int c, len;
	extern int word_lcs[MAXLEN+1][MAXLEN][26];
	extern struct wlist *words[];

	len = strlen(wl->words[0]);
	if(wl->words == words[len]->words){
		bv = 0;
		for(i = 0; i < 26; i++){
			lc[i] = word_lcs[len][ind][i];
			if(lc[i] == 0)
				bv |= 1L << i;
		}
		return(bv);
	}

	for(i = 0; i < 26; i++)
		lc[i] = 0;

	bv = 0xffffffff;
	for(i = 0; i < wl->len; i++){
		c = wl->words[i][ind] - 'a';
		lc[c] += 1;
		bv &= ~(1L << c);
	}
	return(bv);
}

struct wlist *
trysort(wl)
struct wlist *wl;
{
	extern int tscompar();

	if(wl->len < 2){
		wl->refs += 1;
		return(wl);
	}

	if(wl->shared || wl->refs > 1){
		struct wlist *wl1;
		wl1 = NEW(struct wlist);
		wl1->size = wl1->len = wl->len;
		wl1->words = (char **)malloc(4*wl->len);
		wl1->refs = 1;
		wl1->preserve = 0;
		wl1->shared = 0;
		bcopy(wl->words, wl1->words, 4*wl->len);
		wl = wl1;
	} else
		wl->refs += 1;

	myqsort(wl->words, wl->len < 100L ? wl->len : 100L, 4, tscompar);

	return(wl);
}

int
tscompar(ap, bp)
char **ap, **bp;
{
	char *a = *ap, *b = *bp;
	int i, aprod, bprod;

	aprod = bprod = 1;
	for(i = 0; a[i]; i++){
		aprod *= lc[i][a[i]-'a'];
		bprod *= lc[i][b[i]-'a'];
	}
	return(bprod - aprod);
}

int
tscomp(w)
char *w;
{
	int prod, nprod, x, i;

	prod = 1;
	for(i = 0; w[i]; i++){
		x = lc[i][w[i]-'a'];
		if(x == 0)
			return(0);
		nprod = prod * x;
		if(nprod < prod)
			prod = 0x7fffffff;
		else
			prod = nprod;
	}
	return(prod);
}

void
myqsort(char *a[], long n, int width, int (*compar)())
{
#if 1
	xqsort(a, 0, n);
#else
	int i, j, iv, jv;
	char *tmp;

	for(i = 0; i < n-1; i++){
		iv = tscomp(a[i]);
		for(j = i+1; j < n; j++){
			jv = tscomp(a[j]);
			if(jv > iv){
				tmp = a[i];
				a[i] = a[j];
				a[j] = tmp;
				iv = jv;
			}
		}
	}
#endif
}

/* cribbed from the Think C 4.0 library */
void
xqsort(char **a, long first, long last)
{
	static long i;		/*  "static" to save stack space  */
	long j;
	long firstv;
	char *tmp;
 
	while (last - first > 1) {
		i = first;
		j = last;
		firstv = tscomp(a[first]);
		for (;;) {
			while (++i < last && tscomp(a[i]) > firstv)
				;
			while (--j > first && tscomp(a[j]) < firstv)
				;
			if (i >= j)
 				break;
			tmp = a[i];
			a[i] = a[j];
			a[j] = tmp;
		}
		if (j == first) {
			++first;
			continue;
		}
		tmp = a[first];
		a[first] = a[j];
		a[j] = tmp;
		if (j - first < last - (j + 1)) {
			xqsort(a, first, j);
			first = j + 1;	/*  qsort1(j + 1, last);  */
		}
		else {
			xqsort(a, j + 1, last);
			last = j;		/*  qsort1(first, j);  */
		}
	}
}

void
checksort(b, c, wl)
struct board *b;
struct coord c;
struct wlist *wl;
{
	struct coord c1;
	char word[MAXLEN+1], oword[MAXLEN+1];
	int len, i;
	long wn, nthis, this, last = 0;
	struct wlist *wl1;

	getword(b, c, oword);
	len = strlen(oword);

	for(wn = 0; wn < wl->len; wn++){
		putword(b, c, wl->words[wn]);
		this = 1.0;
		c1 = c;
		for(i = 0; i < len; i++){
			getword(b, OSTART(b, c1), word);
			wl1 = match(word, HINT(b, OSTART(b, c1)));
			if(wl1){
				nthis = this * wl1->len;
				if(nthis < this)
					this = 0x7fffffff;
				else
					this = nthis;
				free_wlist(wl1);
			} else {
				this = 0;
				break;
			}
			c1 = nextcoord(c1);
		}
		assert(wn == 0 || this <= last);
		last = this;
	}

	putword(b, c, oword);
}
