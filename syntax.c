/*	syntax.c
 *
 *	The functions in this file handle syntax highlight.
 */

#include "estruct.h"

#if COLOR

#include <string.h>
#include "syntax.h"

#define SYNLEN	20
static char synbuf[SYNLEN + 1];

/* syntax highlight color */
static int speckeyfg = 0x00D8FF;	/* special key forgrnd color */
static int speccharfg = 0xFFD2D3;	/* special char forgrnd color */
static int commentfg = 0x00E8E6;	/* comment forgrnd color */
static int stringfg = 0xBB75A6;	/* string forgrnd color */
static int preprocfg = 0x00D8FF;	/* preprocess forgrnd color */

/* syntax highlight flag */
static int hi_sstring;	/* single line string */
static int hi_scomment;	/* single line comment */
static int hi_include;	/* #include */
static int hi_pound_idx;	/* # index */
static int hi_less_idx;	/* #include < index */

static void syn_include(struct text *v_text, int vtcol);

static void syn_find(struct text *v_text, int begin, int end, int *pbegin, int *pend);
static void syn_trim(struct text *v_text, int begin, int end, int *pbegin, int *pend);
static void syn_fcolor(struct text *v_text, int begin, int end, int fcolor);

/* syntax highlight for special key */
void syntax_specialkey(struct text *v_text, int start, int len)
{
	int i;
	for (i = 0; i < len; i++)
		v_text[start + i].t_fcolor = speckeyfg;
}

/* syntax highlight line init for c */
void syntax_c_line_init()
{
	hi_sstring = FALSE;
	hi_scomment = FALSE;
	hi_include = FALSE;
	hi_pound_idx = -1;
	hi_less_idx = -1;
}

/* syntax highlight handle for c */
void syntax_c_handle(struct text *v_text, int vtcol)
{
	int i;
	int c = v_text[vtcol].t_char;

	if (hi_scomment == TRUE)
		v_text[vtcol].t_fcolor = commentfg;
	else if (c == '"') {
		if (hi_sstring == FALSE) {
			if (hi_pound_idx >= 0)
				syn_include(v_text, vtcol);
			hi_sstring = TRUE;
			v_text[vtcol].t_fcolor = stringfg;
		} else {
			if (v_text[vtcol - 1].t_char != '\\' ||
				hi_include == TRUE) {
				hi_sstring = FALSE;
				v_text[vtcol].t_fcolor = stringfg;
			} else {
				v_text[vtcol].t_fcolor = speccharfg;
				v_text[vtcol - 1].t_fcolor = speccharfg;
			}
		}
	} else if (hi_sstring == TRUE) {
		if (hi_include == FALSE &&
			v_text[vtcol - 1].t_char == '\\' &&
			( c == '\\' || c == 'a' || c == 'b' || c == 'e'
			|| c == 'n' || c == 'r' || c == 't' || c == 'v')) {
			v_text[vtcol].t_fcolor = speccharfg;
			v_text[vtcol - 1].t_fcolor = speccharfg;
		} else
			v_text[vtcol].t_fcolor = stringfg;
	} else if (c == '#' && hi_pound_idx < 0) {
		for (i = 0; i < vtcol; i++) {
			if (v_text[i].t_char != ' ')
				break;
		}
		if (i == vtcol)
			hi_pound_idx = vtcol;
	} else if (c == '<' && hi_pound_idx >= 0 && hi_less_idx < 0) {
		syn_include(v_text, vtcol);
		if (hi_include == TRUE) {
			v_text[vtcol].t_fcolor = preprocfg;
			hi_less_idx = vtcol;
		}
	} else if (c == '>' && hi_include == TRUE) {
		for (i = hi_less_idx; i <= vtcol; i++)
			v_text[i].t_fcolor = stringfg;
	} else if (c == '/' && vtcol > 0 && v_text[vtcol - 1].t_char == '/') {
		v_text[vtcol].t_fcolor = commentfg;
		v_text[vtcol - 1].t_fcolor = commentfg;
		hi_scomment = TRUE;
	}
}

/* syntax highlight for #include */
static void syn_include(struct text *v_text, int vtcol)
{
	int begin, end;

	syn_find(v_text, hi_pound_idx + 1, vtcol - 1, &begin, &end);

	if (strcmp("include", synbuf) == 0) {
		v_text[hi_pound_idx].t_fcolor = preprocfg;
		syn_fcolor(v_text, begin, end, preprocfg);
		hi_include = TRUE;
	}
}

static void syn_find(struct text *v_text, int begin, int end, int *pbegin, int *pend)
{
	int i, len;

	syn_trim(v_text, begin, end, pbegin, pend);
	len = *pend - *pbegin + 1;

	if (len <= 0 || len > SYNLEN) {
		synbuf[0] = '\0';
		return;
	}

	for (i = 0; i < len; i++)
		synbuf[i] = v_text[*pbegin + i].t_char;

	synbuf[len] = '\0';
}

static void syn_trim(struct text *v_text, int begin, int end, int *pbegin, int *pend)
{
	int i, j;

	for (i = begin; i<= end;) {
		if (v_text[i].t_char != ' ')
			break;
		i++;
	}

	for (j = end; j > i;) {
		if (v_text[j].t_char != ' ')
			break;
		j--;
	}

	*pbegin = i;
	*pend = j;
}

static void syn_fcolor(struct text *v_text, int begin, int end, int fcolor)
{
	int i;

	for (i = begin; i <= end; i++)
		v_text[i].t_fcolor = fcolor;
}

#endif /* COLOR */
