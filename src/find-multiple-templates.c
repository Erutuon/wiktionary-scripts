#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <hat-trie/hat-trie.h>

#include "utils/buffer.h"
#include "utils/parser.h"
#include "commander.h"

#define MAX_PAGES 10

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define STATIC_LEN(str) (sizeof (str) - 1)
#define CONTAINS_STR(a, b) (strstr((a), (b)) != NULL)
#define STR_SLICE_END(slice) ((slice).str + (slice).len)

#define CHECK_FILE(file) \
	if ((file) == NULL) \
		perror("could not open file"), exit(-1)

typedef struct {
	size_t len;
	const char * str;
} str_slice_t;

// Type to cast `struct _template_name_t` and `struct _to_find_t`
// in additional_parse_data to.
typedef struct {
	size_t len;
	char str[1];
} str_arr_t;

typedef struct {
	unsigned int max_matches;
	unsigned int match_count;
	FILE * input_file, * output_file;
	char * filter;
	hattrie_t * template_names;
} additional_parse_data;

static inline void str_slice_init(str_slice_t * slice, const char * str, const size_t len) {
	if (slice == NULL)
		CRASH_WITH_MSG("!!!");
		
	slice->str = str;
	slice->len = len;
}

static inline str_slice_t trim (const str_slice_t slice) {
	const char * start = slice.str, * end = STR_SLICE_END(slice);
	str_slice_t trimmed;
	
	while (start < STR_SLICE_END(slice) && isspace(*start))
		++start;
		
	while (end - 1 > slice.str && isspace(end[-1]))
		--end;
		
	if (start != slice.str || end != STR_SLICE_END(slice))
		EPRINTF("trimmed string '%.*s'\n",
				(int) slice.len,
				slice.str);
	
	str_slice_init(&trimmed, start, end - start);
	return trimmed;
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
	return trim(template_name);
}

static inline bool template_name_matches(const str_slice_t template_name,
        hattrie_t * template_names) {
	return hattrie_tryget(template_names,
						template_name.str,
						template_name.len) != NULL;
}

static str_slice_t find_template (FILE * output_file,
                                  const char * const title,
								  const str_slice_t possible_template,
                                  hattrie_t * template_names,
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
			    = find_template(output_file,
			                    title,
								subslice,
			                    template_names,
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
		
		if (template_name_matches(template_name, template_names)) {
			if (!*found)
				fprintf(output_file, "\1%s\n", title);
			
			*found = true;
			fprintf(output_file, "%.*s\n", (int) template.len, template.str);
		}
	} else {
		int len = MIN(possible_template.len, 64);
		
		// Make sure printed string is valid UTF-8.
		while ((size_t) len < possible_template.len
		        && (unsigned) possible_template.str[len] > 127)
			++len;
			
		str_slice_init(&template, NULL, (size_t) -1);
		EPRINTF("invalid template at '%.*s'\n",
		        len,
		        possible_template.str);
	}
	
	return template;
}

static inline bool print_templates (FILE * output_file,
                                    const char * const title,
                                    const buffer_t * const str,
                                    hattrie_t * template_names) {
	const char * p = buffer_string(str);
	const char * const end = p + buffer_length(str);
	bool found_template = false;
	
	while (p < end) {
		const char * const open_template = strstr(p, "{{");
		
		if (open_template == NULL)
			break;
			
		str_slice_t possible_template;
		str_slice_init(&possible_template, open_template, end - open_template);
		str_slice_t template = find_template(output_file,
		                                     title,
											 possible_template,
		                                     template_names,
		                                     &found_template);
		                                     
		p = (template.str != NULL)
		    ? STR_SLICE_END(template)
		    : possible_template.str + STATIC_LEN("{{");
	}
	
	return found_template;
}

static bool handle_page (parse_info * info) {
	page_info * page = &info->page;
	buffer_t * buffer = &page->content;
	additional_parse_data * data = info->additional_data;
	
	bool found_template =
		print_templates(data->output_file,
						page->title,
						buffer,
						data->template_names);
						
	if (found_template && ++data->match_count >= data->max_matches)
		return false;
	
	return true;
}
static inline void get_input_file (command_t * commands) {
	additional_parse_data * data = commands->data;
	FILE * input_file = fopen(commands->arg, "rb");
	CHECK_FILE(input_file);
	data->input_file = input_file;
}

static inline void get_output_file (command_t * commands) {
	additional_parse_data * data = commands->data;
	FILE * output_file = fopen(commands->arg, "wb");
	CHECK_FILE(output_file);
	data->output_file = output_file;
}

static inline void add_template_names_to_trie (FILE * template_names_file,
											hattrie_t * trie) {
	char line[BUFSIZ];
	int count = 0;
	
	while (fgets(line, BUFSIZ, template_names_file) != NULL) {
		const char * tab = strchr(line, '\t');
		size_t len = tab ? (size_t) (tab - line) : strlen(line);
		++count;
		while (&line[len - 1] > line && line[len - 1] == '\n')
			--len;
		if (&line[len] == line)
			CRASH_WITH_MSG("file contains empty lines\n");
		value_t * entry = hattrie_tryget(trie, line, len);
		if (entry != NULL)
			CRASH_WITH_MSG("two entries for '%.*s' in file\n", (int) len, line);
		else {
			entry = hattrie_get(trie, line, len);
			if (tab != NULL) {
				const char * filename = &tab[1];
				CRASH_WITH_MSG("filename '%s' specified but can't handle it yet", filename);
			} else
				*entry = (value_t) NULL; // print to default output file
		}
	}
	
	if (ferror(template_names_file))
		perror("error in reading file of template names"), exit(EXIT_FAILURE);
	else if (count == 0)
		CRASH_WITH_MSG("no template names found in file\n");
}

static void get_templates_to_find (command_t * commands) {
	additional_parse_data * data = commands->data;
	size_t len = strlen(commands->arg);
	
	if (len == 0)
		CRASH_WITH_MSG("expected non-empty string"); // ???
	
	FILE * template_names_file = fopen(commands->arg, "rb");
	CHECK_FILE(template_names_file);
	
	if (data->template_names == NULL) {
		data->template_names = hattrie_create();
		if (data->template_names == NULL)
			CRASH_WITH_MSG("could not create trie\n");
	}
	
	add_template_names_to_trie(template_names_file, data->template_names);
	
	EPRINTF("added template names from '%s'\n", commands->arg);
	
	fclose(template_names_file);
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

static void print_template_names (hattrie_t * template_names) {
	hattrie_iter_t * iter;
	int i = 0;
	for (iter = hattrie_iter_begin(template_names, true);
			!hattrie_iter_finished(iter);
			hattrie_iter_next(iter)) {
		size_t len;
		const char * key = hattrie_iter_key(iter, &len);
		
		if (i++ > 0)
			fprintf(stderr, ", ");
		
		fprintf(stderr, "'%s'", key);
	}
	putc('\n', stderr);
	hattrie_iter_free(iter);
}

static void parse_arguments (int argc, char * * argv, additional_parse_data * data) {
	command_t commands;
	
	data->max_matches = 0;
	data->match_count = 0;
	data->filter = NULL;
	data->template_names = NULL;
	
	command_init(&commands, argv[0], "0");
	
	commands.data = (void *) data;
	
	command_option(&commands, "-i", "--input <file>",
	               "XML page dump file",
	               get_input_file);
	               
	command_option(&commands, "-o", "--output <file>",
	               "place output here",
	               get_output_file);
	               
	command_option(&commands, "-t", "--template-names <file>",
	               "file containing optional template names with optional tab and output filename",
	               get_templates_to_find);
	               
	command_option(&commands, "-p", "--page-count <num>",
	               "return matches from this many pages (not counting non-matching pages)",
	               get_page_count);
	               
	command_option(&commands, "-f", "--filter <text to find>",
	               "only process pages containing this text",
	               get_filter);
	               
	command_parse(&commands, argc, argv);
	
	command_free(&commands);
	
	if (data->input_file == NULL)
		CRASH_WITH_MSG("input file required\n");
	else if (data->output_file == NULL)
		CRASH_WITH_MSG("output file required\n");
	else if (data->max_matches == 0)
		CRASH_WITH_MSG("--page-count required\n");
	else if (data->template_names == NULL)
		CRASH_WITH_MSG("--template-names required\n");
	
	size_t template_count = hattrie_size(data->template_names);
	EPRINTF("searching for instances of %zu template%s on up to %u pages",
	        template_count,
			template_count == 1 ? "" : "s",
			data->max_matches);
	
	if (data->filter != NULL)
		EPRINTF(" containing '%s'", data->filter);
		
	EPRINTF("\n");
	        
	EPRINTF("list of templates:\n");
	print_template_names(data->template_names);
}

int main (int argc, char * * argv) {
	additional_parse_data data;
	Wiktionary_namespace_t namespaces[] = { NAMESPACE_MAIN, NAMESPACE_NONE };
	
	parse_arguments(argc, argv, &data);
	
	parse_Wiktionary_page_dump(data.input_file, handle_page, namespaces, &data);
	
	EPRINTF("found templates on %u pages\n", data.match_count);
	        
	if (data.filter != NULL)
		free(data.filter);
	
	return 0;
}
