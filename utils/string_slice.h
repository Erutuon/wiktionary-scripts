#ifndef STRING_SLICE_H
#define STRING_SLICE_H

#include <ctype.h>

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