#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "utils/string_set.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define EPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define CRASH_WITH_MSG(...) (EPRINTF(__VA_ARGS__), exit(EXIT_FAILURE))
#define TEST_MEM(mem) if (mem == NULL) CRASH_WITH_MSG("not enough memory\n")
#define TEST_PTR(vec) if (vec == NULL) return

static void str_set_realloc (str_set_t * set, size_t size) {
	TEST_PTR(set);
	char * * new_mem = realloc(set->val, sizeof (set->val) * size);
	TEST_MEM(new_mem);
	set->val = new_mem;
	set->size = size;
}

str_set_t * str_set_new () {
	str_set_t * set = malloc(sizeof (*set));
	TEST_MEM(set);
	*set = (str_set_t) {
		0, 0, NULL
	};
	str_set_realloc(set, 4);
	memset(set->val, 0, set->size * sizeof set->val[0]);
	return set;
}

void str_set_free (str_set_t * set) {
	TEST_PTR(set);
	
	for (size_t i = 0; i < set->len; ++i)
		free(set->val[i]);
		
	free(set->val);
	free(set);
}

static size_t str_set_get_insertion_index (str_set_t * set,
        const char * const str) {
	if (set->len == 0)
		return 0;
		
	// Binary search would be more efficient.
	for (size_t i = 0; i < set->len; ++i) {
		int cmp = strcmp(str, set->val[i]);
		
		if (cmp == 0)
			return (size_t) -1;
		else if (cmp < 0)
			return i;
	}
	
	return set->len;
}

void str_set_add (str_set_t * set, const char * const str) {
	TEST_PTR(set);
	TEST_PTR(str);
	size_t index = str_set_get_insertion_index(set, str);
	
	if (index == (size_t) -1) // already in set
		return;
		
	if (set->len >= set->size)
		str_set_realloc(set, set->size * 2);
		
	char * copy = strdup(str);
	
	TEST_MEM(copy);
	
	if (index < set->len) {
		memmove(&set->val[index + 1],
		        &set->val[index],
		        (set->len - index) * sizeof set->val[0]);
	}
	
	set->val[index] = copy;
	++set->len;
}