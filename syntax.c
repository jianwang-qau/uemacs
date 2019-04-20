/*	syntax.c
 *
 *	The functions in this file handle syntax highlight.
 */

#include "estruct.h"

#if COLOR

#include <string.h>
#include "util.h"
#include "hashtab.h"
#include "syntax.h"

#define SYNLEN	20
static char synbuf[SYNLEN + 1];

/* syntax highlight color */
static int speckeyfg = 0x5FD7FF;	/* special key forgrnd color */
static int speccharfg = 0xFFD7D7;	/* special char forgrnd color */
static int commentfg = 0x34E2E2;	/* comment forgrnd color */
static int stringfg = 0xAD7FA8;		/* string forgrnd color */
static int preprocfg = 0x5FD7FF;	/* preprocess forgrnd color */
static int macrofg = 0x5FD7FF;		/* macro forgrnd color */
static int typefg = 0x87FFAF;		/* type forgrnd color */
static int constantfg = 0xAD7FA8;	/* constant forgrnd color */
static int structfg = 0x87FFAF;		/* structure forgrnd color */
static int storagefg = 0x87FFAF;	/* storage class forgrnd color */
static int statefg = 0xFCE94F;		/* statement forgrnd color */
static int labelfg = 0xFCE94F;		/* label forgrnd color */
static int condfg = 0xFCE94F;		/* conditional forgrnd color */
static int repeatfg = 0xFCE94F;		/* repeat forgrnd color */
static int numberfg = 0xAD7FA8;		/* number forgrnd color */
static int oct1stfg = 0x5FD7FF;		/* octal first forgrnd color */
static int errorfg = 0xEEEEEC;		/* error forgrnd color */
static int errorbg = 0xEF2929;		/* error bacgrnd color */

static char *arr_macro[] = {"define", "undef", NULL};
static char *arr_preproc_if[] = {"if", "ifdef" , "ifndef", NULL};
static char *arr_preproc_else[] = {"else", "endif", NULL};
static char *arr_type[] = {
	"int", "long", "short", "char", "void",
	"signed", "unsigned", "float", "double",
	NULL
};
static char *arr_constant[] = {
	"EOF", "NULL", "stderr", "stdin", "stdout", NULL
};
static char *arr_struct[] = {"struct", "union" , "enum", "typedef", NULL};
static char *arr_storage[] = {
	"static", "register", "auto", "volatile", "extern", "const", NULL
};
static char *arr_state[] = {
	"goto", "break", "return", "continue", "asm", NULL
};
static char *arr_label[] = {"case", "default", NULL};
static char *arr_cond[] = {"if", "else", "switch", NULL};
static char *arr_repeat[] = {"while", "for", "do", NULL};

/* syntax hash table */
hashtab_T htab_macro;
hashtab_T htab_preproc_if;
hashtab_T htab_preproc_else;
hashtab_T htab_type;
hashtab_T htab_constant;
hashtab_T htab_struct;
hashtab_T htab_storage;
hashtab_T htab_state;
hashtab_T htab_label;
hashtab_T htab_cond;
hashtab_T htab_repeat;

/* syntax highlight flag */
static int hi_sstring;		/* single line string */
static int hi_scomment;		/* single line comment */
static int hi_mcomment;		/* multi line comment */
static int hi_include;		/* #include */
static int hi_macro;		/* macro */
static int hi_preproc_if;	/* #if */
static int hi_preproc;		/* preproc */

static int hi_mcomment_idx;	/* multi line comment index */
static int hi_pound_idx;	/* # index */
static int hi_less_idx;		/* #include < index */
static int hi_nonempty_idx;	/* nonempty char index */

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

static void hash_addarr(hashtab_T *ht, char **arr);
static int hash_haskey(hashtab_T *ht, char *key);
static int is_separator(int c);

/*
 * Initialize the data structures used by the syntax code
 */
void syninit(void)
{
	hash_init(&htab_macro);
	hash_init(&htab_preproc_if);
	hash_init(&htab_preproc_else);
	hash_init(&htab_type);
	hash_init(&htab_constant);
	hash_init(&htab_struct);
	hash_init(&htab_storage);
	hash_init(&htab_state);
	hash_init(&htab_label);
	hash_init(&htab_cond);
	hash_init(&htab_repeat);

	hash_addarr(&htab_macro, arr_macro);
	hash_addarr(&htab_preproc_if, arr_preproc_if);
	hash_addarr(&htab_preproc_else, arr_preproc_else);
	hash_addarr(&htab_type, arr_type);
	hash_addarr(&htab_constant, arr_constant);
	hash_addarr(&htab_struct, arr_struct);
	hash_addarr(&htab_storage, arr_storage);
	hash_addarr(&htab_state, arr_state);
	hash_addarr(&htab_label, arr_label);
	hash_addarr(&htab_cond, arr_cond);
	hash_addarr(&htab_repeat, arr_repeat);
}

/*
 * free up all the dynamically allocated syntax structures
 */
void synfree(void)
{
	hash_clear(&htab_macro);
	hash_clear(&htab_preproc_if);
	hash_clear(&htab_preproc_else);
	hash_clear(&htab_type);
	hash_clear(&htab_constant);
	hash_clear(&htab_struct);
	hash_clear(&htab_storage);
	hash_clear(&htab_state);
	hash_clear(&htab_label);
	hash_clear(&htab_cond);
	hash_clear(&htab_repeat);
}

/* syntax highlight for special key */
void syntax_specialkey(struct text *v_text, int start, int len)
{
	int i;
	for (i = 0; i < len; i++)
		v_text[start + i].t_fcolor = speckeyfg;
}

/* syntax highlight mcomment init */
void syntax_mcomment_init(int state)
{
	hi_mcomment = state;
}

/* syntax highlight line init for c */
void syntax_c_line_init()
{
	hi_sstring = FALSE;
	hi_scomment = FALSE;
	hi_include = FALSE;
	hi_macro = FALSE;
	hi_preproc_if = FALSE;
	hi_preproc = FALSE;

	hi_mcomment_idx = -1;
	hi_pound_idx = -1;
	hi_less_idx = -1;
	hi_nonempty_idx = -1;
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
			ret = syn_other(v_text, vtcol);
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
	} else if (c == '"') {
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
		syn_fcolor(v_text, begin, end, preprocfg);
		hi_include = TRUE;
		hi_preproc = TRUE;
	}
}

/* syntax highlight for preproc */
static void syn_preproc(struct text *v_text, int vtcol)
{
	int begin, end;
	int fcolor = CLR_NONE;

	syn_find(v_text, hi_pound_idx + 1, vtcol - 1, &begin, &end);

	if (hash_haskey(&htab_macro, synbuf)) {
		fcolor = macrofg;
		hi_macro = TRUE;
	} else if (hash_haskey(&htab_preproc_if, synbuf)) {
		fcolor = preprocfg;
		hi_preproc_if = TRUE;
		hi_nonempty_idx = -1;
	} else if (hash_haskey(&htab_preproc_else, synbuf))
		fcolor = preprocfg;

	if (fcolor != CLR_NONE) {
		v_text[hi_pound_idx].t_fcolor = fcolor;
		syn_fcolor(v_text, begin, end, fcolor);
		hi_preproc = TRUE;
	}
}

/* syntax highlight for other */
static int syn_other(struct text *v_text, int vtcol)
{
	int begin, end;
	int len;

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

	if (hash_haskey(&htab_type, synbuf)) {
		syn_fcolor(v_text, begin, end, typefg);
		return TRUE;
	}
	if (hash_haskey(&htab_constant, synbuf)) {
		syn_fcolor(v_text, begin, end, constantfg);
		return TRUE;
	}
	if (hash_haskey(&htab_struct, synbuf)) {
		syn_fcolor(v_text, begin, end, structfg);
		return TRUE;
	}
	if (hash_haskey(&htab_storage, synbuf)) {
		syn_fcolor(v_text, begin, end, storagefg);
		return TRUE;
	}
	if (hash_haskey(&htab_state, synbuf)) {
		syn_fcolor(v_text, begin, end, statefg);
		return TRUE;
	}
	if (hash_haskey(&htab_label, synbuf)) {
		syn_fcolor(v_text, begin, end, labelfg);
		return TRUE;
	}
	if (hash_haskey(&htab_cond, synbuf)) {
		syn_fcolor(v_text, begin, end, condfg);
		return TRUE;
	}
	if (hash_haskey(&htab_repeat, synbuf)) {
		syn_fcolor(v_text, begin, end, repeatfg);
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

static void hash_addarr(hashtab_T *ht, char **arr)
{
	int i;

	for (i = 0; arr[i] != NULL; i++)
		hash_add(ht, (char_u *)arr[i]);
}

static int hash_haskey(hashtab_T *ht, char *key)
{
	hashitem_T *hi;

	hi = hash_find(ht, (char_u *)key);
	return !HASHITEM_EMPTY(hi);
}

static int is_separator(int c)
{
	int i;
	static char *str = " (){}[];,>=<+-*/%&|";

	if (c > 0x7f)
		return FALSE;
	for (i = 0; str[i] != '\0'; i++) {
		if (c == str[i])
			return TRUE;
	}
	return FALSE;
}

#endif /* COLOR */
