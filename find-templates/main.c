#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

#include "utils/buffer.h"
#include "utils/parser.h"
#include "commander.h"

#define MAX_PAGES 10

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define STATIC_LEN(str) (sizeof (str) - 1)
#define CONTAINS_STR(a, b) (strstr((a), (b)) != NULL)
#define STR_SLICE_END(slice) ((slice).str + (slice).len)

typedef struct {
	size_t len;
	const char * str;
} str_slice_t;

// Type to cast `struct _template_name_t` and `struct _to_find_t`
// in additional_parse_data to.
typedef struct {
	size_t len;
	char str[1];
} str_slice_arr_t;

typedef struct {
	unsigned int max_matches;
	char * filter;
	struct _template_name_t {
		size_t len;
		char str[PAGE_NAME_LEN];
	} template_name;
	struct _to_find_t {
		size_t len;
		char str[PAGE_NAME_LEN + STATIC_LEN("{{|")];
	} to_find;
} additional_parse_data;

static inline void str_slice_init(str_slice_t * slice, const char * str, const size_t len) {
	if (slice == NULL)
		CRASH_WITH_MSG("!!!");
		
	slice->str = str;
	slice->len = len;
}

// Will only work if template name doesn't contain templates (which should hold
// true in mainspace) and template is well-formed.
static inline str_slice_t get_template_name (const str_slice_t slice) {
	const char * name_start = slice.str + STATIC_LEN("{{");
	const char * p = name_start;
	const char * const end = STR_SLICE_END(slice);
	str_slice_t template_name;
	
	while (p < end && !(p[0] == '|' || (p[0] == '}' && p[1] == '}')))
		p++;
		
	str_slice_init(&template_name, name_start, p - name_start);
	return template_name;
}

static str_slice_t find_template (const str_slice_t possible_template,
                                  const str_slice_arr_t * template_to_find,
                                  bool * found) {
	const char * p = possible_template.str + STATIC_LEN("{{");
	const char * const end = STR_SLICE_END(possible_template);
	str_slice_t template, template_name;
	bool closed = false;
	
	template_name = get_template_name(possible_template);
		
	while (p <= end - STATIC_LEN("{{")) {
		if (p[0] == '{' && p[1] == '{') {
			str_slice_t subslice;
			str_slice_init(&subslice, p, end - p);
			str_slice_t inner_template
			    = find_template(subslice,
			                    template_to_find,
			                    found);
			                    
			if (inner_template.str == NULL)
				break;
				
			p += inner_template.len;
		} else if (p[0] == '}' && p[1] == '}') {
			p += STATIC_LEN("}}");
			closed = true;
			break;
		} else
			++p;
	}
	
	if (closed) {
		str_slice_init(&template, possible_template.str, p - possible_template.str);
		
		if (template_name.str != NULL
				&& template_name.len == template_to_find->len
				&& strncmp(template_name.str,
						   template_to_find->str,
						   template_to_find->len) == 0) {
			*found = true;
			printf("%.*s\n", (int) template.len, template.str);
		}
	} else {
		str_slice_init(&template, NULL, (size_t) -1);
		EPRINTF("invalid template at '%.*s'\n",
				(int) MIN(possible_template.len, 64),
				possible_template.str);
	}
	
	return template;
}

static inline bool print_templates (const char * const title,
                                    const buffer_t * const str,
                                    const str_slice_arr_t * template_to_find) {
	const char * p = buffer_string(str);
	const char * const end = p + buffer_length(str);
	bool found_template = false;
	
	printf("\1%s\n", title);
	
	while (p < end) {
		const char * const open_template = strstr(p, "{{");
		
		if (open_template == NULL)
			break;
		
		str_slice_t possible_template;
		str_slice_init(&possible_template, open_template, end - open_template);
		str_slice_t template = find_template(possible_template,
		                                     template_to_find,
		                                     &found_template);
		                                     
		if (template.str != NULL)
			p = STR_SLICE_END(template);
		else
			p = possible_template.str + STATIC_LEN("{{");
	}
	
	return found_template;
}

static bool handle_page (parse_info * info) {
	page_info * page = &info->page;
	buffer_t * buffer = &page->content;
	additional_parse_data * data = info->additional_data;
	static unsigned int match_count = 0;
	
	if ((data->filter == NULL
	        || CONTAINS_STR(buffer_string(buffer), data->filter))
	        && CONTAINS_STR(buffer_string(buffer), data->to_find.str)) {
		bool found_template =
		    print_templates(page->title,
		                    buffer,
		                    (str_slice_arr_t *) &data->template_name);
		                    
		if (found_template) {
			++match_count;
			
			if (match_count >= data->max_matches)
				return false;
		}
	}
	
	return true;
}

static void get_page_count (command_t * commands) {
	additional_parse_data * data = commands->data;
	unsigned int count = 0;
	
	if (strcmp(commands->arg, "max") == 0)
		count = UINT_MAX;
	else if (sscanf(commands->arg, "%u", &count) != 1 || count == 0)
		CRASH_WITH_MSG("expected positive integer greater than 0\n");
		
	data->max_matches = count;
}

static void get_template_to_find (command_t * commands) {
	additional_parse_data * data = commands->data;
	size_t len = strlen(commands->arg);
	
	if (len == 0)
		CRASH_WITH_MSG("expected non-empty string"); // ???
	else if (len >= PAGE_NAME_LEN)
		CRASH_WITH_MSG("template name too long");
		
	strcpy(data->template_name.str, commands->arg);
	data->template_name.len = strlen(data->template_name.str);
	data->to_find.len = sprintf(data->to_find.str,
	                            "{{%s|",
	                            data->template_name.str);
}

static void get_filter (command_t * commands) {
	additional_parse_data * data = commands->data;
	char * filter;
	
	if (strlen(commands->arg) == 0)
		CRASH_WITH_MSG("expected non-empty string"); // ???
		
	filter = strdup(commands->arg);
	
	if (filter == NULL)
		CRASH_WITH_MSG("not enough memory");
		
	data->filter = filter;
}

void parse_arguments (int argc, char * * argv, additional_parse_data * data) {
	command_t commands;
	
	command_init(&commands, argv[0], "0");
	
	commands.data = (void *) data;
	
	command_option(&commands, "-m", "--match-count <num>",
	               "number of matches to return",
	               get_page_count);
	               
	command_option(&commands, "-t", "--template-name <title>",
	               "title of template to find",
	               get_template_to_find);
	               
	command_option(&commands, "-f", "--filter <text to find>",
	               "only process pages containing this text",
	               get_filter);
	               
	command_parse(&commands, argc, argv);
	
	command_free(&commands);
}

int main (int argc, char * * argv) {
	additional_parse_data data;
	Wiktionary_namespace_t namespaces[] = { NAMESPACE_MAIN, NAMESPACE_NONE };
	
	data.max_matches = 0;
	data.template_name.str[0] = '\0';
	data.to_find.str[0] = '\0';
	data.filter = NULL;
	
	parse_arguments(argc, argv, &data);
	
	if (data.max_matches == 0)
		CRASH_WITH_MSG("--match-count required\n");
	else if (strlen(data.template_name.str) == 0)
		CRASH_WITH_MSG("--template-name required");
		
	do_parsing(stdin, handle_page, namespaces, &data);
	
	if (data.filter != NULL)
		free(data.filter);
}