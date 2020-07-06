/*	syntax.c
 *
 *	The functions in this file handle syntax highlight.
 */

#include "estruct.h"

#if COLOR

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"
#include "hashtab.h"
#include "syntax.h"

#define IDX_MACRO		0
#define IDX_PREPROC_IF		1
#define IDX_PREPROC_ELSE	2
#define IDX_TYPE		3
#define IDX_CONSTANT		4
#define IDX_STRUCT		5
#define IDX_STORAGE		6
#define IDX_STATE		7
#define IDX_LABEL		8
#define IDX_COND		9
#define IDX_REPEAT		10
#define IDX_OPERATOR		11

#define SYNLEN	20
static char synbuf[SYNLEN + 1];

typedef struct color_S
{
	int fcolor;
	int bcolor;
} color_T;

typedef struct colorindex_S
{
	short_u	ci_index;	/* index of color_T */
	char_u	ci_keyw[1];	/* keyword */
} colorindex_T;

#define CI_KEY_OFF	offsetof(colorindex_T, ci_keyw)
#define HI2CI(hi)	((colorindex_T *)((hi)->hi_key - CI_KEY_OFF))

/* syntax highlight color */
static int speckeyfg = 0x5FD7FF;	/* special key forgrnd color */
static int speccharfg = 0xFFD7D7;	/* special char forgrnd color */
static int commentfg = 0x34E2E2;	/* comment forgrnd color */
static int stringfg = 0xAD7FA8;		/* string forgrnd color */
static int preprocfg = 0x5FD7FF;	/* preprocess forgrnd color */
static int macrofg = 0x5FD7FF;		/* macro forgrnd color */
//static int typefg = 0x87FFAF;		/* type forgrnd color */
//static int constantfg = 0xAD7FA8;	/* constant forgrnd color */
//static int structfg = 0x87FFAF;	/* structure forgrnd color */
//static int storagefg = 0x87FFAF;	/* storage class forgrnd color */
//static int statefg = 0xFCE94F;	/* statement forgrnd color */
//static int labelfg = 0xFCE94F;	/* label forgrnd color */
//static int condfg = 0xFCE94F;		/* conditional forgrnd color */
//static int repeatfg = 0xFCE94F;	/* repeat forgrnd color */
static int numberfg = 0xAD7FA8;		/* number forgrnd color */
static int oct1stfg = 0x5FD7FF;		/* octal first forgrnd color */
static int errorfg = 0xEEEEEC;		/* error forgrnd color */
static int errorbg = 0xEF2929;		/* error bacgrnd color */

static color_T arr_color[] = {
	{0x5FD7FF, CLR_NONE},	/* macro fg and bg color */
	{0x5FD7FF, CLR_NONE},	/* preproc_if fg and bg color */
	{0x5FD7FF, CLR_NONE},	/* preproc_else fg and bg color */
	{0x87FFAF, CLR_NONE},	/* type fg and bg color */
	{0xAD7FA8, CLR_NONE},	/* constant fg and bg color */
	{0x87FFAF, CLR_NONE},	/* struct fg and bg color */
	{0x87FFAF, CLR_NONE},	/* storage fg and bg color */
	{0xFCE94F, CLR_NONE},	/* state fg and bg color */
	{0xFCE94F, CLR_NONE},	/* label fg and bg color */
	{0xFCE94F, CLR_NONE},	/* cond fg and bg color */
	{0xFCE94F, CLR_NONE},	/* repeat fg and bg color */
	{0xFCE94F, CLR_NONE}	/* operator fg and bg color */
};

static char *arr_macro[] = {"define", "undef", NULL};
static char *arr_preproc_if[] = {"if", "ifdef" , "ifndef", NULL};
static char *arr_preproc_else[] = {"else", "endif", NULL};
static char *arr_type[] = {
	"int", "long", "short", "char", "void",
	"signed", "unsigned", "float", "double",
	/* gnu */
	"__label__", "__complex__", "__volatile__",
	NULL
};
static char *arr_constant[] = {
	"NULL", "EOF", "SEEK_CUR", "SEEK_END", "SEEK_SET",
	"stderr", "stdin", "stdout",
	"__LINE__", "__FILE__", "__DATE__", "__TIME__",
	"__STDC__", "__STDC_VERSION__",
	/* gnu */
	"__GNUC__", "__FUNCTION__", "__PRETTY_FUNCTION__", "__func__",
	NULL
};
static char *arr_struct[] = {"struct", "union" , "enum", "typedef", NULL};
static char *arr_storage[] = {
	"static", "register", "auto", "volatile", "extern", "const",
	/* gnu */
	"inline", "__attribute__",
	NULL
};
static char *arr_state[] = {
	"goto", "break", "return", "continue", "asm",
	/* gnu */
	"__asm__",
	NULL
};
static char *arr_label[] = {"case", "default", NULL};
static char *arr_cond[] = {"if", "else", "switch", NULL};
static char *arr_repeat[] = {"while", "for", "do", NULL};
static char *arr_operator[] = {
	"sizeof",
	/* gnu */
	"typeof", "__real__", "__imag__",
	NULL
};

/* syntax keyword hashtable */
static hashtab_T poundtab;
static hashtab_T keywtab;

/* syntax highlight flag */
int hi_mcomment;		/* multi line comment */
static int hi_char;		/* char */
static int hi_sstring;		/* single line string */
static int hi_scomment;		/* single line comment */
static int hi_include;		/* #include */
static int hi_macro;		/* macro */
static int hi_preproc_if;	/* #if */
static int hi_preproc_else;	/* #else */
static int hi_preproc;		/* preproc */

static int hi_char_idx;		/* char index */
static int hi_mcomment_idx;	/* multi line comment index */
static int hi_pound_idx;	/* # index */
static int hi_less_idx;		/* #include < index */
static int hi_nonempty_idx;	/* nonempty char index */
static int hi_bslash_idx;	/* backslash index */
static int hi_percent_idx;	/* percent index */

static void syn_include(struct text *v_text, int vtcol);
static void syn_preproc(struct text *v_text, int vtcol);
static int syn_other(struct text *v_text, int vtcol);

static void syn_find(struct text *v_text, int begin, int end,
	int *pbegin, int *pend);
static void syn_trim(struct text *v_text, int begin, int end,
	int *pbegin, int *pend);
static void syn_fcolor(struct text *v_text, int begin, int end,
	int fcolor);
static void syn_color(struct text *v_text, int begin, int end,
	int fcolor, int bcolor);

static void hash_addarr(hashtab_T *ht, char **arr, short_u idx);
static int hash_findkey(hashtab_T *ht, char *key);
static int is_separator(int c);
static int is_escape(int c);
static int is_format_tail(int c);
static int is_format_middle(struct text *v_text, int begin, int end);

/*
 * Initialize the data structures used by the syntax code
 */
void syninit(void)
{
	hash_init(&poundtab);
	hash_addarr(&poundtab, arr_macro, IDX_MACRO);
	hash_addarr(&poundtab, arr_preproc_if, IDX_PREPROC_IF);
	hash_addarr(&poundtab, arr_preproc_else, IDX_PREPROC_ELSE);

	hash_init(&keywtab);
	hash_addarr(&keywtab, arr_type, IDX_TYPE);
	hash_addarr(&keywtab, arr_constant, IDX_CONSTANT);
	hash_addarr(&keywtab, arr_struct, IDX_STRUCT);
	hash_addarr(&keywtab, arr_storage, IDX_STORAGE);
	hash_addarr(&keywtab, arr_state, IDX_STATE);
	hash_addarr(&keywtab, arr_label, IDX_LABEL);
	hash_addarr(&keywtab, arr_cond, IDX_COND);
	hash_addarr(&keywtab, arr_repeat, IDX_REPEAT);
	hash_addarr(&keywtab, arr_operator, IDX_OPERATOR);
}

/*
 * free up all the dynamically allocated syntax structures
 */
void synfree(void)
{
	hash_clear_all(&poundtab, CI_KEY_OFF);
	hash_clear_all(&keywtab, CI_KEY_OFF);
}

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
	hi_char = FALSE;
	hi_sstring = FALSE;
	hi_scomment = FALSE;
	hi_include = FALSE;
	hi_macro = FALSE;
	hi_preproc_if = FALSE;
	hi_preproc_else = FALSE;
	hi_preproc = FALSE;

	hi_char_idx = -1;
	hi_mcomment_idx = -1;
	hi_pound_idx = -1;
	hi_less_idx = -1;
	hi_nonempty_idx = -1;
	hi_bslash_idx = -1;
	hi_percent_idx = -1;
}

static int syntax_c_common(struct text *v_text, int vtcol)
{
	int ret = FALSE;
	int c = v_text[vtcol].t_char;

	if (c == '/' && vtcol > 0 && v_text[vtcol - 1].t_char == '/') {
		v_text[vtcol].t_fcolor = commentfg;
		v_text[vtcol - 1].t_fcolor = commentfg;
		hi_scomment = TRUE;
		ret = TRUE;
	} else if (c == '*' && vtcol > 0 && v_text[vtcol - 1].t_char == '/') {
		v_text[vtcol].t_fcolor = commentfg;
		v_text[vtcol - 1].t_fcolor = commentfg;
		hi_mcomment_idx = vtcol;
		hi_mcomment = TRUE;
		ret = TRUE;
	} else if (is_separator(c)) {
		if (hi_nonempty_idx >= 0)
			syn_other(v_text, vtcol);
		hi_nonempty_idx = -1;
	}
	return ret;
}

/* syntax highlight handle for c */
void syntax_c_handle(struct text *v_text, int vtcol)
{
	int i, ret;
	int c = v_text[vtcol].t_char;

	if (c != ' ' && hi_nonempty_idx < 0)
		hi_nonempty_idx = vtcol;

	if (hi_scomment == TRUE)
		v_text[vtcol].t_fcolor = commentfg;
	else if (hi_mcomment == TRUE) {
		v_text[vtcol].t_fcolor = commentfg;
		if (c == '/' && vtcol > 0 && v_text[vtcol - 1].t_char == '*') {
			if (hi_mcomment_idx > 0) {
				if (vtcol - hi_mcomment_idx >= 2)
					hi_mcomment = FALSE;
			} else
				hi_mcomment = FALSE;
		} else if (c == '*' && vtcol > 0 && v_text[vtcol - 1].t_char == '/') {
			syn_color(v_text, vtcol - 1, vtcol - 1, errorfg, errorbg);
		}
	} else if (hi_macro == TRUE) {
		ret = syntax_c_common(v_text, vtcol);
		if (ret == FALSE)
			v_text[vtcol].t_fcolor = macrofg;
	} else if (hi_preproc_if == TRUE) {
		ret = syntax_c_common(v_text, vtcol);
		if (ret == FALSE)
			v_text[vtcol].t_fcolor = preprocfg;
	} else if (hi_preproc_else == TRUE) {
		ret = syntax_c_common(v_text, vtcol);
		if (ret == FALSE)
			v_text[vtcol].t_fcolor = preprocfg;
	} else if (c == '\'') {
		if (hi_char == FALSE) {
			if (hi_sstring == FALSE) {
				hi_char = TRUE;
				hi_char_idx = vtcol;
			} else {
				if (v_text[vtcol - 1].t_char == '\\') {
					v_text[vtcol - 1].t_fcolor = speccharfg;
					v_text[vtcol].t_fcolor = speccharfg;
				}
				else
					v_text[vtcol].t_fcolor = stringfg;
			}
		} else {
			if (v_text[vtcol - 1].t_char != '\\') {
				int delta = vtcol - hi_char_idx;
				if (delta == 2) {
					hi_char = FALSE;
					syn_fcolor(v_text, hi_char_idx, vtcol, stringfg);
				} else if (delta == 3 &&
					v_text[vtcol - 2].t_char == '\\') {
					hi_char = FALSE;
					syn_fcolor(v_text, hi_char_idx, vtcol, speccharfg);
				} else
					hi_char_idx = vtcol;
			}
		}
	} else if (c == '"') {
		if (hi_sstring == FALSE) {
			if (hi_char == FALSE) {
				if (hi_pound_idx >= 0)
					syn_include(v_text, vtcol);
				hi_sstring = TRUE;
				v_text[vtcol].t_fcolor = stringfg;
			}
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
		if (hi_include == FALSE) {
			if (v_text[vtcol - 1].t_char == '\\' &&
				(is_escape(c) || is_digit(c))) {
				v_text[vtcol].t_fcolor = speccharfg;
				v_text[vtcol - 1].t_fcolor = speccharfg;
				if (is_digit(c))
					hi_bslash_idx = vtcol - 1;
			} else if (hi_bslash_idx > 0 && is_digit(c)) {
				v_text[vtcol].t_fcolor = speccharfg;
			} else if (hi_percent_idx >= 0 && is_format_tail(c)) {
				if (is_format_middle(v_text, hi_percent_idx + 1, vtcol - 1)) {
					syn_fcolor(v_text, hi_percent_idx, vtcol, speccharfg);
					hi_percent_idx = -1;
				} else {
					v_text[vtcol].t_fcolor = stringfg;
					hi_bslash_idx = -1;
				}
			} else {
				v_text[vtcol].t_fcolor = stringfg;
				hi_bslash_idx = -1;
				if (c == '%')
					hi_percent_idx = vtcol;
			}
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
	} else if (c == '*' && vtcol > 0 && v_text[vtcol - 1].t_char == '/') {
		v_text[vtcol].t_fcolor = commentfg;
		v_text[vtcol - 1].t_fcolor = commentfg;
		hi_mcomment_idx = vtcol;
		hi_mcomment = TRUE;
	} else if (c == '/' && vtcol > 0 && v_text[vtcol - 1].t_char == '*' &&
		hi_mcomment == FALSE) {
		syn_color(v_text, vtcol - 1, vtcol, errorfg, errorbg);
	} else if (c == ' ' && hi_pound_idx >= 0 && hi_preproc == FALSE)
		syn_preproc(v_text, vtcol);
	else if (is_separator(c)) {
		if (hi_nonempty_idx >= 0)
			syn_other(v_text, vtcol);
		hi_nonempty_idx = -1;
	}
}

/* syntax highlight line end for c */
void syntax_c_line_end(struct text *v_text, int vtcol)
{
	if (hi_mcomment == TRUE)
		return;
	if (hi_pound_idx >= 0 && hi_preproc == FALSE)
		syn_preproc(v_text, vtcol);
	else if (hi_nonempty_idx >= 0)
		syn_other(v_text, vtcol);
}

/* syntax highlight for #include */
static void syn_include(struct text *v_text, int vtcol)
{
	int begin, end;

	syn_find(v_text, hi_pound_idx + 1, vtcol - 1, &begin, &end);

	if (strcmp("include", synbuf) == 0) {
		v_text[hi_pound_idx].t_fcolor = preprocfg;
		syn_fcolor(v_text, begin, vtcol - 1, preprocfg);
		hi_include = TRUE;
		hi_preproc = TRUE;
	}
}

/* syntax highlight for preproc */
static void syn_preproc(struct text *v_text, int vtcol)
{
	int begin, end;
	int fcolor = CLR_NONE;
	int idx;

	syn_find(v_text, hi_pound_idx + 1, vtcol - 1, &begin, &end);

	/* hash table search */
	idx = hash_findkey(&poundtab, synbuf);

	if (idx < 0)
		return;

	if (idx == IDX_MACRO) {
		fcolor = arr_color[idx].fcolor;
		hi_macro = TRUE;
		hi_nonempty_idx = -1;
	} else if (idx == IDX_PREPROC_IF) {
		fcolor = arr_color[idx].fcolor;
		hi_preproc_if = TRUE;
	} else if (idx == IDX_PREPROC_ELSE) {
		fcolor = arr_color[idx].fcolor;
		hi_preproc_else = TRUE;
		hi_nonempty_idx = -1;
	}

	if (fcolor != CLR_NONE) {
		v_text[hi_pound_idx].t_fcolor = fcolor;
		syn_fcolor(v_text, begin, end, fcolor);
		hi_preproc = TRUE;
		end = vtcol;
	}
}

/* syntax highlight for other */
static int syn_other(struct text *v_text, int vtcol)
{
	int begin, end;
	int len;
	int idx;

	syn_find(v_text, hi_nonempty_idx, vtcol - 1, &begin, &end);

	/* it is number */
	if (synbuf[0] == '.' || is_digit(synbuf[0])) {
		len = strlen(synbuf);
		if (is_hex_str(synbuf)) {
			syn_fcolor(v_text, begin, end, numberfg);
			return TRUE;
		}
		if (synbuf[len - 1] == 'f' || synbuf[len - 1] == 'F') {
			synbuf[len - 1] = '\0';
			if (is_octal_str(synbuf) || is_int_str(synbuf) ||
				is_float_str(synbuf)) {
				syn_fcolor(v_text, begin, end, numberfg);
				return TRUE;
			}
		}
		if (is_octal_str(synbuf)) {
			syn_fcolor(v_text, begin, begin, oct1stfg);
			syn_fcolor(v_text, begin + 1, end, numberfg);
			return TRUE;
		} else if (is_int_str(synbuf) || is_float_str(synbuf)) {
			syn_fcolor(v_text, begin, end, numberfg);
			return TRUE;
		}
		return FALSE;
	}

	/* hash table search */
	idx = hash_findkey(&keywtab, synbuf);

	if (idx >= 0) {
		if (arr_color[idx].bcolor == CLR_NONE)
			syn_fcolor(v_text, begin, end, arr_color[idx].fcolor);
		else
			syn_color(v_text, begin, end, arr_color[idx].fcolor,
				arr_color[idx].bcolor);
		return TRUE;
	}
	return FALSE;
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

static void syn_fcolor(struct text *v_text, int begin, int end,
	int fcolor)
{
	int i;
	for (i = begin; i <= end; i++)
		v_text[i].t_fcolor = fcolor;
}

static void syn_color(struct text *v_text, int begin, int end,
	int fcolor, int bcolor)
{
	int i;
	for (i = begin; i <= end; i++) {
		v_text[i].t_fcolor = fcolor;
		v_text[i].t_bcolor = bcolor;
	}
}

static void hash_addarr(hashtab_T *ht, char **arr, short_u idx)
{
	int i;
	hash_T hash;
	hashitem_T *hi;
	colorindex_T *ci;
	char_u *p;

	for (i = 0; arr[i] != NULL; i++) {
		p = (char_u *)arr[i];
		hash = hash_hash(p);
		hi = hash_lookup(ht, p, hash);
		if (HASHITEM_EMPTY(hi)) {
			ci = (colorindex_T *)malloc((unsigned)(sizeof(colorindex_T) + STRLEN(p)));
			if (ci == NULL)
				return;
			STRCPY(ci->ci_keyw, p);
			ci->ci_index = idx;
			hash_add_item(ht, hi, ci->ci_keyw, hash);
		}
	}
}

static int hash_findkey(hashtab_T *ht, char *key)
{
	hashitem_T *hi;
	colorindex_T *ci;

	hi = hash_find(ht, (char_u *)key);
	if (!HASHITEM_EMPTY(hi)) {
		ci = HI2CI(hi);
		return ci->ci_index;
	}
	return -1;
}

static int is_char_in(int c, char *str)
{
	int i;

	if (c > 0x7f)
		return FALSE;
	for (i = 0; str[i] != '\0'; i++) {
		if (c == str[i])
			return TRUE;
	}
	return FALSE;
}

static int is_separator(int c)
{
	static char *str = " (){}[];,>=<+-*/%&|~";
	return is_char_in(c, str);
}

static int is_escape(int c)
{
	static char *str = "\\abefnrtv";
	return is_char_in(c, str);
}

static int is_format_tail(int c)
{
	static char *str = "bdiuoxXDOUfeEgGcCsSpn";
	return is_char_in(c, str);
}

static int is_format_head(int c)
{
	static char *str = "-+' #0*";
	return is_char_in(c, str);
}

static int is_format_middle(struct text *v_text, int begin, int end)
{
	int i, j, k;
	int c;

	if (begin > end)
		return TRUE;

	syn_trim(v_text, begin, end, &j, &k);

	for (i = j; i <= k; i++) {
		c = v_text[i].t_char;
		if (!is_format_head(c))
			break;
	}

	for (; i <= k; i++) {
		c = v_text[i].t_char;
		if (!is_digit(c) && c != '.')
			break;
	}

	if (i > k)
		return TRUE;

	c = v_text[i].t_char;

	if (i == k && (c == 'h' || c == 'l' || c == 'L'))
		return TRUE;
	else if (i + 1 == k && c == 'l' &&
		v_text[i + 1].t_char == 'l')
		return TRUE;
	return FALSE;
}

#endif /* COLOR */
