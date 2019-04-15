#ifndef STRING_SLICE_H
#define STRING_SLICE_H

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "utils/buffer.h"

typedef struct {
	size_t len;
	const char * str;
} str_slice_t;

#define STR_SLICE_END(slice) ((slice).str + (slice).len)
#define NULL_SLICE ((str_slice_t) { 0, NULL })

static inline str_slice_t str_slice_init (const char * str,
        const size_t len) {
	str_slice_t slice;
	slice.str = str;
	slice.len = len;
	return slice;
}

static inline str_slice_t trim (const str_slice_t slice) {
	const char * start = slice.str, * end = STR_SLICE_END(slice);
	
	while (start < STR_SLICE_END(slice) && isspace(*start))
		++start;
		
	while (end - 1 > slice.str && isspace(end[-1]))
		--end;
		
	return str_slice_init(start, end - start);
}

static inline bool str_slice_cpy (str_slice_t slice, char * dest, size_t dest_len) {
	if (slice.len > dest_len - 1)
		return false;
	
	memcpy(dest, slice.str, slice.len);
	dest[slice.len] = '\0';
	
	return true;
}

static inline char * str_slice_to_str (str_slice_t slice) {
	char * copy = malloc(slice.len + 1);
	
	if (copy == NULL)
		fprintf(stderr, "could not allocate memory\n"), exit(EXIT_FAILURE);
	
	str_slice_cpy(slice, copy, slice.len + 1);
	
	return copy;
}

static inline str_slice_t str_slice_get_line (str_slice_t slice) {
	const char * p = slice.str;
	
	while (p < STR_SLICE_END(slice) && *p != '\n')
		++p;
		
	return str_slice_init(slice.str, p - slice.str);
}

// Skip line and one or more newlines.
static inline str_slice_t str_slice_skip_to_next_line (str_slice_t slice) {
	const char * p = slice.str, * end = STR_SLICE_END(slice);
	
	while (p < end && *p != '\n')
		++p;
		
	while (p < end && *p == '\n')
		++p;
		
	return str_slice_init(p, end - p > 0 ? end - p : 0);
}

static inline str_slice_t buffer_to_str_slice (buffer_t * buffer) {
	return str_slice_init(buffer_string(buffer), buffer_length(buffer));
}

#ifdef HATTRIE_HATTRIE_H

#define MAKE_HATTRIE_FUNC(name) \
	static inline value_t * hattrie_##name##_slice (hattrie_t * trie, \
                                                    str_slice_t slice) { \
		return hattrie_##name(trie, slice.str, slice.len); \
	}

MAKE_HATTRIE_FUNC(tryget)
MAKE_HATTRIE_FUNC(get)

#undef MAKE_HATTRIE_FUNC

static inline str_slice_t hattrie_iter_key_slice (hattrie_iter_t * iter) {
	size_t len;
	const char * key = hattrie_iter_key(iter, &len);
	return str_slice_init(key, len);
}

#endif

#endif