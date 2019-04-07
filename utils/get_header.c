#include <ctype.h>

#include "get_header.h"
#include "string_slice.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

str_slice_t get_header (str_slice_t line,
                         header_level_t * header_level) {
	const char * p, * header_last_char, * last_equals, * line_end;
	int start_equals, end_equals, header_equals;
	str_slice_t header = { 0, NULL };
	
	line_end = STR_SLICE_END(line);
	p = line.str;
	
	while (p < line_end && *p == '=')
		++p;
		
	header.str = p;
	
	start_equals = p - line.str;
	
	p = line_end - 1;
	
	// Skip comments and ASCII whitespace.
	while (p > line.str) {
		if (isspace(p[0]))
			--p;
		else if ((size_t) (p - line.str) < sizeof "<!---->" - 1)
			break;
		else if (p[-2] == '-' && p[-1] == '-' && p[0] == '>') {
			const char * tmp = &p[-3];
			
			while (tmp > line.str) {
				if (tmp[-3] == '<' && tmp[-2] == '!' && tmp[-1] == '-'
				        && tmp[0] == '-') {
					p = &tmp[-4];
					break;
				} else if ((tmp[-2] == '-' && tmp[-1] == '-' && tmp[0] == '>')
				           || (size_t) (tmp - line.str) < sizeof "<!--" - 1)
					return NULL_SLICE;
					
				--tmp;
			}
		} else
			break;
	}
	
	if (*p != '=')
		return NULL_SLICE;
		
	last_equals = p;
	
	// Handle headers consisting only of equals signs.
	if (line.str + start_equals == last_equals + 1) {
		int total_equals = start_equals;
		start_equals = (total_equals - 1) / 2;
		
		// Two equals signs don't make a header.
		if (total_equals <= 2)
			return NULL_SLICE;
		
		if (header_level != NULL)
			*header_level = start_equals;
			
		header.len = start_equals % 2 == 0 ? 2 : 1;
		
		header.str = line.str + start_equals;
		
		return header;
	}
	
	end_equals = 0;
	
	while (p > line.str && *p == '=')
		++end_equals, --p;
		
	header_last_char = p;
	
	header_equals = start_equals;
	
	if (start_equals != end_equals) {
		header_equals = MIN(start_equals, end_equals);
		header.str -= start_equals - header_equals;
		header_last_char += end_equals - header_equals;
	}
		
	if (header_level != NULL)
		*header_level = header_equals;
	
	header.len = header.str > header_last_char
		? 0
		: (size_t) (header_last_char - header.str + 1);
		
	return trim(header);
}