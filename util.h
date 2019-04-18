#ifndef UTIL_H_
#define UTIL_H_

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
void mystrscpy(char *dst, const char *src, int size);

int is_octal(char ch);
int is_digit(char ch);
int is_hex(char ch);

int is_hex_str(const char *str);
int is_octal_str(const char *str);
int is_int_str(const char *str);
int is_float_str(const char *str);

#endif  /* UTIL_H_ */
