#ifndef SYNTAX_H_
#define SYNTAX_H_

#include "display.h"

void syntax_specialkey(struct text *v_text, int start, int len);

/* syntax for c language */
void syntax_mcomment_init(int state);
void syntax_c_line_init(void);
void syntax_c_handle(struct text *v_text, int vtcol);
void syntax_c_line_end(struct text *v_text, int vtcol);

#endif /* SYNTAX_H_ */
