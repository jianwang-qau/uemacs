#include <string.h>
#include "util.h"

#define TRUE  1
#define FALSE 0

/* Safe zeroing, no complaining about overlap */
void mystrscpy(char *dst, const char *src, int size)
{
	if (!size)
		return;
	while (--size) {
		char c = *src++;
		if (!c)
			break;
		*dst++ = c;
	}
	*dst = 0;
}

int is_octal(char ch)
{
	return ch >= '0' && ch <= '7';
}

int is_digit(char ch)
{
	return ch >= '0' && ch <= '9';
}

int is_hex(char ch)
{
	return (ch >= '0' && ch <= '9') ||
		(ch >= 'A' && ch <= 'F') ||
		(ch >= 'a' && ch <= 'f');
}

/* min len is 3, must start with 0x or 0X */
int is_hex_str(const char *str)
{
	int i;
	int len = strlen(str);

	if (len < 3)
		return FALSE;
	if (str[0] != '0' || (str[1] != 'x' && str[1] != 'X'))
		return FALSE;

	for (i = 2; i < len; i++) {
		if (is_hex(str[i]) == FALSE)
			return FALSE;
	}
	return TRUE;
}

/* min len is 2, must start with 0 */
int is_octal_str(const char *str)
{
	int i;
	int len = strlen(str);

	if (len < 2)
		return FALSE;
	if (str[0] != '0')
		return FALSE;

	for (i = 1; i < len; i++) {
		if (is_octal(str[i]) == FALSE)
			return FALSE;
	}
	return TRUE;
}

int is_int_str(const char *str)
{
	int i;
	int len = strlen(str);

	if (len > 1 && str[0] == '0')
		return FALSE;

	for (i = 0; i < len; i++) {
		if (is_digit(str[i]) == FALSE)
			return FALSE;
	}
	return TRUE;
}

/* min len is 2, must contain ONLY one dot */
int is_float_str(const char *str)
{
	int i;
	int dot_idx = -1;
	int len = strlen(str);

	if (len < 2)
		return FALSE;
	if (str[0] == '0' && str[1] != '.')
		return FALSE;

	for (i = 0; i < len; i++) {
		if (str[i] != '.' && is_digit(str[i]) == FALSE)
			return FALSE;
		if (str[i] == '.') {
			if (dot_idx >= 0)
				return FALSE;
			dot_idx = i;
		}
	}
	return dot_idx >= 0;
}

