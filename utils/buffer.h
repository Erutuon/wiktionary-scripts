#ifndef BUFFER_H
#define BUFFER_H

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

typedef struct {
	size_t len;
	size_t size;
	char * str;
} buffer_t;

#define buffer_size(buffer) ((buffer)->size)
#define buffer_string(buffer) ((buffer)->str)
#define buffer_length(buffer) ((buffer)->len)
#define buffer_end(buffer) (buffer_string(buffer) + buffer_length(buffer))

#define EPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define PRINT_BUF_STR(buffer) EPRINTF(#buffer ": %zu %zu %p; %s, line %d\n", \
	buffer_length(buffer), buffer_size(buffer), buffer_string(buffer), \
	__func__, __LINE__)

#undef PRINT_BUF_STR
#define PRINT_BUF_STR(...) ((void) 0)

bool buffer_realloc (buffer_t * buffer, size_t size);
bool buffer_append (buffer_t * buffer, const char * str, size_t len);
bool buffer_empty (buffer_t * buffer);
bool buffer_init (buffer_t * buffer, size_t size);
bool buffer_free (buffer_t * buffer);
void buffer_print (buffer_t * buffer);

#endif