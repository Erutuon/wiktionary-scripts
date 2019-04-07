#ifndef STRING_SET_H
#define STRING_SET_H

#include <stdlib.h>

typedef struct _str_set {
	size_t size, len;
	char * * val;
} str_set_t;

str_set_t * str_set_new ();

void str_set_free (str_set_t * set);

void str_set_add (str_set_t * set, const char * const str);

#endif