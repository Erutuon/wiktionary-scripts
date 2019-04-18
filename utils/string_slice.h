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

/*
Unicode whitespace:
\x09 U+0009 (TAB)..\x0D U+000D (CR)
\x20 U+0020 (SPACE)
\xC2\x85 U+0085 (NEL)
\xC2\xA0 U+00A0 (NO-BREAK SPACE)
\xE1\x9A\x80 U+1680 (OGHAM SPACE MARK)
\xE2\x80\x80 U+2000 (EN QUAD)..\xE2\x80\x8A U+200A (HAIR SPACE)
\xE2\x80\xA8 U+2028 (LINE SEPARATOR)
\xE2\x80\xA9 U+2029 (PARAGRAPH SEPARATOR)
\xE2\x80\xAF U+202F (NARROW NO-BREAK SPACE)
\xE2\x81\x9F U+205F (MEDIUM MATHEMATICAL SPACE)
\xE3\x80\x80 U+3000 (IDEOGRAPHIC SPACE)
*/

static inline str_slice_t trim (const str_slice_t slice) {
	const unsigned char * start = (unsigned char *) slice.str,
						* end = (unsigned char *) STR_SLICE_END(slice);
	
	while (1)
		if (start < (unsigned char *) STR_SLICE_END(slice) && isspace(start[0]))
			++start;
		else if (start < (unsigned char *) STR_SLICE_END(slice) - 2
				&& start[0] == 0xC2u && (start[1] == 0x85u || start[1] == 0xA0u))
			start += 2;
		else if (start < (unsigned char *) STR_SLICE_END(slice) - 3
			&& ((start[0] == 0xE1u && start[1] == 0x9Au && start[2] == 0x80u)
			|| (start[0] == 0xE2u
				&& ((start[1] == 0x80u
					&& ((0x80u <= start[2] && start[2] <= 0x8Au)
					|| start[2] == 0xA8u || start[2] == 0xA9u || start[2] == 0xAFu))
				|| (start[1] == 0x81u && start[2] == 0x9Fu)))
			|| (start[0] == 0xE3u && start[1] == 0x80u && start[2] == 0x80u)))
			start += 3;
		else
			break;
		
	while (1)
		if (end > (unsigned char *) slice.str + 1 && isspace(end[-1]))
			--end;
		else if (end > (unsigned char *) slice.str + 2
				&& (end[-2] == 0xC2u && (end[-1] == 0x85u || end[-1] == 0xA0u)))
			end -= 2;
		else if (end > (unsigned char *) slice.str + 3
				&& ((end[-3] == 0xE1u && end[-2] == 0x9Au && end[-1] == 0x80u)
				|| (end[-3] == 0xE2u
					&& ((end[-2] == 0x80u
						&& ((0x80u <= end[-1] && end[-1] <= 0x8Au)
						|| end[-1] == 0xA8u || end[-1] == 0xA9u || end[-1] == 0xAFu))
					|| (end[-2] == 0x81u && end[-1] == 0x9Fu)))
				|| (end[-3] == 0xE3u && end[-2] == 0x80u && end[-1] == 0x80u)))
			end -= 3;
		else
			break;
		
	return str_slice_init((char *) start, end - start);
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