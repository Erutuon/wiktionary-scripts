#include <ctype.h>
#include "get_header.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

const char * get_header (const char * line_start,
                         size_t line_len,
                         size_t * header_len,
                         header_level_t * header_level) {
	const char * p, * header_start, * header_end, * last_equals, * line_end;
	int start_equals, end_equals, header_equals;
	
	line_end = line_start + line_len;
	p = line_start;
	
	while (p < line_end && *p == '=')
		++p;
		
	header_start = p;
	
	start_equals = p - line_start;
	
	p = line_end - 1;
	
	// Skip comments and ASCII whitespace.
	while (p > line_start) {
		if (isspace(p[0]))
			--p;
		else if ((size_t) (p - line_start) < sizeof "<!---->" - 1)
			break;
		else if (p[-2] == '-' && p[-1] == '-' && p[0] == '>') {
			const char * tmp = &p[-3];
			
			while (tmp > line_start) {
				if (tmp[-3] == '<' && tmp[-2] == '!' && tmp[-1] == '-'
				        && tmp[0] == '-') {
					p = &tmp[-4];
					break;
				} else if ((tmp[-2] == '-' && tmp[-1] == '-' && tmp[0] == '>')
				           || (size_t) (tmp - line_start) < sizeof "<!--" - 1)
					return NULL;
					
				--tmp;
			}
		} else
			break;
	}
	
	if (*p != '=')
		return NULL;
		
	last_equals = p;
	
	// Handle headers consisting only of equals signs.
	if (line_start + start_equals == last_equals + 1) {
		int total_equals = start_equals;
		start_equals = (total_equals - 1) / 2;
		
		// Two equals signs don't make a header.
		if (total_equals <= 2)
			return NULL;
			
		*header_len = start_equals % 2 == 0 ? 2 : 1;
		if (header_level != NULL)
			*header_level = start_equals;
		return line_start + start_equals;
	}
	
	end_equals = 0;
	
	while (p > line_start && *p == '=')
		++end_equals, --p;
		
	header_end = p;
	
	header_equals = start_equals;
	
	if (start_equals != end_equals) {
		header_equals = MIN(start_equals, end_equals);
		header_start -= start_equals - header_equals;
		header_end += end_equals - header_equals;
	}
	
	while (header_start < line_end && isspace(*header_start))
		++header_start;
		
	while (header_end > line_start && isspace(*header_end))
		--header_end;
		
	if (header_level != NULL)
		*header_level = header_equals;
	
	*header_len = header_start > header_end
		? 0
		: (size_t) (header_end - header_start + 1);
		
	return header_start;
}