/*
 * words.c
 * Robert Morris (rtm@eecs.harvard.edu)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include "cword.h"

void add_to_words(char *w);
int letter_goodness(char *word);

/*
 * maintain the dictionary in an array,
 * and search it.
 */

struct wlist *words[MAXLEN+1];

struct wlist *
new_wlist()
{
	struct wlist *wl;

	wl = NEW(struct wlist);
	if(wl == 0){
		fprintf(stderr, "cword: out of memory in new_wlist\n");
		exit(1);
	}
	wl->size = 8;
	wl->len = 0;
	wl->words = (char **) malloc(wl->size * sizeof(char *));
	if(wl->words == 0){
		fprintf(stderr, "cword: out of memory new_wlist words\n");
		exit(1);
	}
	wl->shared = 0;
	wl->refs = 1;
	wl->preserve = 0;

	return(wl);
}

void
free_wlist(struct wlist *wl)
{
	static int depth;

	assert(depth < 20);
	depth++;

	assert(wl->words);
	assert(wl->refs > 0);
	wl->refs -= 1;
	if(wl->refs == 0){
		if(wl->shared)
			free_wlist(wl->shared);
		else
			free(wl->words);
		wl->words = 0;
		free(wl);
	}
	--depth;
}

void
add_to_wlist(wl, w)
struct wlist *wl;
char *w;
{
	if(wl->len >= wl->size){
		if(wl->size > 500)
			wl->size += 512;
		else
			wl->size *= 2;
		wl->words = (char **) realloc(wl->words,
					      wl->size * sizeof(char *));
		if(wl->words == 0){
			fprintf(stderr, "cword: out of memory add_to_wlist\n");
			exit(1);
		}
	}
	wl->words[wl->len++] = w;
}

int nfiles;
char *starts[10], *ends[10];

void add_to_words(char *w);

void
read_words(char *file)
{
	int fd;
	char *buf, *p;
	int len;

	fd = open(file, 0);
	if(fd < 0){
		perror(file);
		exit(1);
	}
	len = lseek(fd, 0, 2);
	lseek(fd, 0, 0);

	buf = malloc(len);
	if(read(fd, buf, len) != len){
		perror("read");
		exit(1);
	}
	close(fd);
	starts[nfiles] = buf;
	ends[nfiles] = buf + len;

	p = buf + len - 1;
	while(p > buf){
		*p = '\0';
		while(p > buf && *p != '\n')
			--p;
		if(*p == '\n')
			p++;
		add_to_words(p);
		--p;
	}
	nfiles++;
}

#if 0
struct wlist *wcache[MAXLEN+1][MAXLEN+1][128];
int wcused, caching;
#endif

void
add_to_words(char *w)
{
	int wlen;

	for(wlen = 0; w[wlen]; wlen++){
		if(isupper(w[wlen]))
			w[wlen] = tolower(w[wlen]);
		if(!islower(w[wlen]))
			return;
	}
	if(wlen > MAXLEN)
		return;
	if(words[wlen] == 0)
		words[wlen] = new_wlist();
	add_to_wlist(words[wlen], w);
}

struct wlist *
match(w, wl)
char *w;
struct wlist *wl;
{
	extern struct wlist *realmatch();
	int i, wlen;
	struct wlist *res;

	wlen = strlen(w);
	if(wl == 0){
		wl = words[wlen];
		if(wl == 0)
			wl = words[wlen] = new_wlist();
	}

	if(wl->words == words[wlen]->words){
		for(i = 0; i < wlen; i++)
			if(w[i] != WILDCARD)
				break;
		if(i == wlen){
			/* all ?s */
			res = sharewlist(wl);
			goto out;
		}

#if 0
		if(caching){
			besti = getbesti(w);
			if(!wcache[wlen][besti][w[besti]]){
				strcpy(word1, w);
				for(i = 0; word1[i]; i++)
					if(i != besti)
						word1[i] = '?';
				wl = wcache[wlen][besti][w[besti]]
				   = realmatch(word1, wl);
				wcused += sizeof(*wl);
				wcused += wl->size * 4;
			}
			wl = wcache[wlen][besti][w[besti]];
		}
#endif
	}

	res = realmatch(w, wl);

#ifdef notdef
	{
		struct wlist *res1;
		res1 = realmatch(w, words[wlen]);
		assert(res1->len == res->len);
		free_wlist(res1);
	}
#endif

out:
	if(res->len == 0){
		free_wlist(res);
		return((struct wlist *) 0);
	}
	return(res);
}

struct wlist *
realmatch(w, wl)
char *w;
struct wlist *wl;
{
	struct wlist *res;
	char *a, *b;
	long i;

	res = new_wlist();
	for(i = 0; i < wl->len; i++){
		a = w;
		b = wl->words[i];
		for( ; *a; a++, b++){
			if(*a == WILDCARD)
				continue;
			if(*a != *b)
				break;
		}
		if(*a == *b)
			add_to_wlist(res, wl->words[i]);
	}
	return(res);
}

int
match1(a, b)
char *a, *b;
{
	for( ; *a && *b; a++, b++){
		if(*a == WILDCARD)
			continue;
		if(*a != *b)
			return(0);
	}
	return(*a == *b);
}

int
could_match(w, wl)
char *w;
struct wlist *wl;
{
	long i;

	for(i = 0; i < wl->len; i++)
		if(match1(w, wl->words[i]))
			return(i);
	return(-1);
}

void
randomize(wl)
struct wlist *wl;
{
	long i, j;
	char *tmp;

	for(i = 0; i < wl->len; i++){
		j = random() % wl->len;
		tmp = wl->words[j];
		wl->words[j] = wl->words[i];
		wl->words[i] = tmp;
	}
}

void
randomize_all()
{
	int i;

 	srandom(getpid());
	for(i = 0; i <= MAXLEN; i++)
		if(words[i])
			randomize(words[i]);
}

char vowels[256];
#define isvowel(c) vowels[c]
extern int compar();
struct wtmp{
	char *word;
	int n;
};
struct wtmp *wtmp;

void analyze_letter_goodness(struct wlist *wl);

void
analyze_all()
{
	int i;

	for(i = 0; i <= MAXLEN; i++)
		if(words[i])
			analyze_letter_goodness(words[i]);
}

int
count_vts(w)
char *w;
{
	int n;

	n = 0;
	for(w++; *w; w++)
		if(isvowel((int)(*w)) != isvowel((int)(*(w-1))))
			n++;
	return(n);
}

void
sort_wlist(wl)
struct wlist *wl;
{
	long i, nt, sum;
	struct wlist *lists[MAXLEN+1];

	/*
	 * sort by # of vowel/consonant transitions.
	 */
	vowels['a'] = vowels['e'] = vowels['i'] = vowels['o'] = vowels['u'] =1;

	for(i = 0; i <= MAXLEN; i++)
		lists[i] = new_wlist();
	for(i = 0; i < wl->len; i++){
		nt = count_vts(wl->words[i]);
		/* nt += 5 * (nfiles - wordfileno(wl->words[i]) - 1); */
		add_to_wlist(lists[nt], wl->words[i]);
	}

	wl->len = 0;
	for(nt = MAXLEN; nt >= 0; --nt){
		for(i = 0; i < lists[nt]->len; i++)
			wl->words[wl->len++] = lists[nt]->words[i];
	}

	/*
	 * sort by # of common letters
	 */

	wtmp = (struct wtmp *) malloc(wl->len * sizeof(struct wtmp));
	for(i = 0; i < wl->len; i++){
		wtmp[i].word = wl->words[i];
		wtmp[i].n = letter_goodness(wl->words[i]);
	}

	sum = 0;
	for(i = 0; i <= MAXLEN; i++){
		qsort(wtmp+sum, lists[i]->len, sizeof(struct wtmp), compar);
		sum += lists[i]->len;
	}

	for(i = 0; i < wl->len; i++)
		wl->words[i] = wtmp[i].word;
	free(wtmp);

	for(i = 0; i <= MAXLEN; i++)
		free_wlist(lists[i]);
}

void
sort_all()
{
	int i;
	for(i = 0; i <= MAXLEN; i++)
		if(words[i])
			sort_wlist(words[i]);
}

int
wordfileno(w)
char *w;
{
	int i;

	for(i = 0; i < nfiles; i++)
		if(w >= starts[i] && w < ends[i])
			return(i);
	return(0);
}

int
compar(ap, bp)
struct wtmp *ap, *bp;
{
	return(bp->n - ap->n);
}

/*
 * for each letter, figure out how many times it appears
 * in words in wl;
 */
int letter_counts[128];
int word_lcs[MAXLEN+1][MAXLEN][26];

void
analyze_letter_goodness(struct wlist *wl)
{
	long i;
	int len = 0, j;

	if(wl->len > 0)
		len = strlen(wl->words[0]);

	for(i = 0; i < wl->len; i++){
		for(j = 0; j < len; j++){
			letter_counts[wl->words[i][j]]++;
			word_lcs[len][j][wl->words[i][j]-'a'] += 1;
		}
	}
}

int
letter_goodness(char *word)
{
	int n = 0;

	while(*word)
		n += letter_counts[*word++];
	return(n);
}

/*
 * return the index of the least common letter in w.
 */
int
getbesti(w)
char *w;
{
	int i, besti;

	letter_counts['?'] = 10000000;
	besti = 0;
	for(i = 0; w[i]; i++)
		if(letter_counts[w[i]] < letter_counts[w[besti]])
			besti = i;
	return(besti);
}

struct wlist *
sharewlist(wl)
struct wlist *wl;
{
	struct wlist *wl1;

	wl1 = NEW(struct wlist);
	if(wl1 == 0){
		fprintf(stderr, "cword: out of memory in sharewlist\n");
		exit(1);
	}
	*wl1 = *wl;
	wl1->shared = wl;
	wl->refs += 1;
	wl1->refs = 1;
	return(wl1);
}
