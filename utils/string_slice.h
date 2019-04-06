#ifndef STRING_SLICE_H
#define STRING_SLICE_H

typedef struct {
	size_t len;
	const char * str;
} str_slice_t;

#define STR_SLICE_END(slice) ((slice).str + (slice).len)

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

#endif