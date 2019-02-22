#include <stdio.h>
#include "buffer.h"

#define max(a, b) ((a) > (b) ? (a) : (b))

bool buffer_realloc (buffer_t * buffer, size_t size) {
	if (buffer == NULL)
		return false;
		
	size = max(max(size, buffer_size(buffer) * 2), 64);
	
	char * mem = realloc(buffer_string(buffer), size);
	
	if (mem == NULL)
		return false;
		
	buffer_string(buffer) = mem;
	buffer_size(buffer) = size;
	
	return true;
}

bool buffer_append (buffer_t * buffer, const char * str, size_t len) {
	PRINT_BUF_STR(buffer);
	
	if (buffer == NULL || str == NULL)
		return false;
		
	size_t new_len = buffer_length(buffer) + len;
	
	if (new_len + 1 > buffer_size(buffer)) {
		if (!buffer_realloc(buffer, new_len + 1)) // space for null terminator
			return false;
	}
	
	memcpy(buffer_end(buffer), str, len);
	buffer_string(buffer)[new_len] = 0;
	buffer_length(buffer) = new_len;
	
	return true;
}

bool buffer_empty (buffer_t * buffer) {
	if (buffer == NULL)
		return false;
		
	buffer_string(buffer)[0] = '\0';
	buffer_length(buffer) = 0;
	return true;
}

bool buffer_init (buffer_t * buffer, size_t size) {
	buffer_length(buffer) = 0;
	buffer_size(buffer) = 0;
	buffer_string(buffer) = NULL;
	return buffer_realloc(buffer, size);
}

bool buffer_free (buffer_t * buffer) {
	free(buffer_string(buffer));
	return true;
}

void buffer_print (buffer_t * buffer) {
	if (buffer == NULL)
		return;
		
	printf("size: %zu; length: %zu; value: '%.*s'\n",
	       buffer_size(buffer), buffer_length(buffer),
		   (int) buffer_length(buffer), buffer_string(buffer));
}