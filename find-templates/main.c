#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

#include "utils/buffer.h"
#include "utils/parser.h"
#include "commander.h"

#define MAX_PAGES 10



typedef struct {
	unsigned int max_matches;
	char template_name[PAGE_NAME_LEN];
	size_t template_name_len;
	char to_find[PAGE_NAME_LEN + sizeof "{{|" - 1];
	char * filter;
} additional_parse_data;

// Only works if template name doesn't contain templates (which should hold
// true in mainspace).
static const char * get_template_name (const char * const template_start,
                                       const size_t len,
                                       size_t * name_len) {
	const char * template_name = template_start + sizeof "{{" - 1;
	const char * p = template_name;
	const char * const end = template_start + len;
	
	while (p < end && !(p[0] == '|' || (p[0] == '}' && p[1] == '}')))
		p++;
		
	*name_len = p - template_name;
	return template_name;
}

static const char * get_template (const char * const template_start,
                                  const size_t len,
                                  size_t * template_len) {
	int nesting_level = 1;
	const char * p = template_start + sizeof "{{" - 1;
	const char * const end = template_start + len;
	
	while (nesting_level > 0 && p <= end - (sizeof "{{" - 1)) {
		if (p[0] == '{' && p[1] == '{') {
			p += sizeof "{{" - 1;
			++nesting_level;
		} else if (p[0] == '}' && p[1] == '}') {
			p += sizeof "}}" - 1;
			--nesting_level;
		} else
			++p;
	}
	
	if (nesting_level == 0) {
		*template_len = p - template_start;
		return template_start;
	}
	
	return NULL;
}

static bool print_templates (const char * const title,
                             const buffer_t * const str,
                             const char * const template_to_find,
                             const size_t template_to_find_len) {
	const char * p = buffer_string(str);
	const char * const end = p + buffer_length(str);
	bool found_template = false;
	
	printf("\1%s\n", title);
	
	while (p < end) {
		const char * const open_template = strstr(p, "{{");
		
		if (open_template != NULL) {
			size_t template_len, template_name_len;;
			const char * const template_name
			    = get_template_name(open_template,
			                        end - open_template,
			                        &template_name_len);
			                        
			if (template_name_len == template_to_find_len
			        && strncmp(template_name, template_to_find, template_to_find_len) == 0) {
				const char * const template = get_template(open_template,
				        end - open_template,
				        &template_len);
				        
				found_template = true;
				
				if (template != NULL)
					printf("%.*s\n", (int) template_len, template);
					
				else
					EPRINTF("non-matching template at '%.*s'\n",
					        (int) buffer_length(str),
					        buffer_string(str));
					        
				p = open_template + template_len;
			} else
				p = open_template + sizeof "{{" - 1;
		} else
			p = end;
	}
	
	return found_template;
}

static bool handle_page (parse_info * info) {
	page_info * page = &info->page;
	buffer_t * buffer = &page->content;
	additional_parse_data * data = info->additional_data;
	static unsigned int match_count = 0;
	
	if ((data->filter == NULL
	        || strstr(buffer_string(buffer), data->filter) != NULL)
	        && strstr(buffer_string(buffer), data->to_find) != NULL) {
		bool found_template =
		    print_templates(page->title,
		                    buffer,
		                    data->template_name,
		                    data->template_name_len);
		                    
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
		
	strcpy(data->template_name, commands->arg);
	data->template_name_len = strlen(data->template_name);
	sprintf(data->to_find, "{{%s|", data->template_name);
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
	data.template_name[0] = '\0';
	data.to_find[0] = '\0';
	data.filter = NULL;
	
	parse_arguments(argc, argv, &data);
	
	if (data.max_matches == 0)
		CRASH_WITH_MSG("--match-count required\n");
	else if (strlen(data.template_name) == 0)
		CRASH_WITH_MSG("--template-name required");
		
	do_parsing(stdin, handle_page, namespaces, &data);
	
	if (data.filter != NULL)
		free(data.filter);
}