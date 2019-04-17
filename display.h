#ifndef DISPLAY_H_
#define DISPLAY_H_

#include "utf8.h"

struct text {
#if COLOR
	int t_fcolor;
	int t_bcolor;
#endif
	unicode_t t_char;
};

#endif /* DISPLAY_H_ */
