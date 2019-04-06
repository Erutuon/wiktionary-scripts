#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <hat-trie/hat-trie.h>

#include "utils/buffer.h"
#include "utils/parser.h"
#include "utils/string_slice.h"
#include "commander.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define STATIC_LEN(str) (sizeof (str) - 1)
#define CONTAINS_STR(a, b) (strstr((a), (b)) != NULL)

#define SIG_PTR_BITS 48
#define CHAR_BITS 8
#define PTR_BITS (sizeof (void *) * CHAR_BITS)
#define SIG_PTR_BITS 48
#define MASK_N(n) ((1ULL << (n)) - 1)
#define MASK_LEFT_N(n) (MASK_N(n) << (PTR_BITS - (n)))
#define SIG_PTR_VAL(ptr) ((uintptr_t) (ptr) & MASK_N(SIG_PTR_BITS))
#define SHIFT_VAL(val) ((uintptr_t) val << SIG_PTR_BITS))
#define UNSHIFT_VAL(val) ((uintptr_t) val >> SIG_PTR_BITS)
#define SIGN_EXTEND(ptr, n) (((intptr_t) ptr << n) >> n)
#define STORE_IN_PTR(ptr, val) ((void *) (SIG_PTR_VAL(ptr) | SHIFT_VAL(val))
#define GET_STORED_BITS(ptr) ((ptr) & MASK_LEFT_N(PTR_BITS - SIG_PTR_BITS))
#define GET_STORED_VAL(ptr) (UNSHIFT_VAL(GET_STORED_BITS(ptr)))
#define GET_PTR_VAL(ptr) ((void *) SIGN_EXTEND(ptr, 16))

#define STORE_BIT_INDEX(ptr) STORE_IN_PTR(ptr, increment_output_file_bit_index())
#define SET_BIT_AT(val, n, bool) ((val) | (((uintptr_t) bool) << n))
#define GET_BIT_AT(val, n) (((val) & (((uintptr_t) 1) << n)) >> n)

// Assign each output file a number, and when processing each page,
// use bits to track whether the title of the page currently being processed
// has been printed to the file yet.
// The index of each file's bit is kept in the highest 16 bits of the pointer,
// and is set with STORE_IN_PTR, gotten with GET_STORED_VAL, and removed
// by GET_PTR_VAL (so that the pointer can be safely dereferenced).
// https://stackoverflow.com/questions/16198700/using-the-extra-16-bits-in-64-bit-pointers
typedef unsigned int output_file_mask_t;
static uint16_t output_file_bit_index = 0;
#define MAX_OUTPUT_FILES (sizeof (output_file_mask_t) * CHAR_BITS)

#define CHECK_FILE(file) \
	if ((file) == NULL) \
		perror("could not open file"), exit(-1)

typedef struct {
	bool verbose;
	unsigned int max_matches;
	unsigned int match_count;
	char * filter;
	hattrie_t * template_names; // template name to FILE pointer
} additional_parse_data;

typedef struct {
	bool need_default_output_file;
	hattrie_t * file_paths; // output file path to FILE pointer
	FILE * input_file, * default_output_file;
	additional_parse_data parse_data;
} option_data;

// Will only work if template name doesn't contain templates (which should hold
// true in mainspace) and template is well-formed.
static inline str_slice_t get_template_name (const str_slice_t slice) {
	const char * name_start = slice.str + STATIC_LEN("{{");
	const char * p = name_start;
	const char * const end = STR_SLICE_END(slice);
	
	while (p < end && !(p[0] == '|' || (p + 1 < end && p[0] == '}' && p[1] == '}')))
		p++;
		
	return trim(str_slice_init(name_start, p - name_start));
}

static str_slice_t find_template (const char * const title,
                                  const str_slice_t possible_template,
                                  hattrie_t * template_names,
                                  bool * found,
                                  output_file_mask_t * output_file_mask,
                                  bool verbose) {
	const char * p = possible_template.str + STATIC_LEN("{{");
	const char * const end = STR_SLICE_END(possible_template);
	str_slice_t template, template_name;
	bool closed = false;
	
	template_name = get_template_name(possible_template);
	
	while (p + STATIC_LEN("{{") <= end) {
		if (p[0] == '{' && p[1] == '{') {
			str_slice_t subslice = str_slice_init(p, end - p);
			str_slice_t inner_template = find_template(title,
			                             subslice,
			                             template_names,
			                             found,
			                             output_file_mask,
			                             verbose);
			                             
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
		template = str_slice_init(possible_template.str, p - possible_template.str);
		
		if (template_name.len > 0) {
			value_t * entry;
			entry = hattrie_tryget(template_names,
			                       template_name.str,
			                       template_name.len);
			                       
			if (entry != NULL) {
				FILE * output_file = GET_PTR_VAL(*entry);
				int bit_pos = GET_STORED_VAL(*entry);
				
				*found = true;
				
				if (!GET_BIT_AT(*output_file_mask, bit_pos)) {
					fprintf(output_file, "\1%s\n", title);
					*output_file_mask = SET_BIT_AT(*output_file_mask, bit_pos, true);
				}
				
				fprintf(output_file, "%.*s\n", (int) template.len, template.str);
			}
		} else {
			if (verbose)
				EPRINTF("nameless template on page '%s' at '%.*s'\n",
				        title,
				        (int) template.len,
				        template.str);
		}
	} else {
		template = str_slice_init(NULL, (size_t) -1);
		
		if (verbose) {
			int len = MIN(possible_template.len, 64);
			
			// Print up to the end of a series of non-ASCII or ASCII graphical characters.
			while ((size_t) len < possible_template.len
			        && ((unsigned) possible_template.str[len] > 127
			            || isgraph(possible_template.str[len])))
				++len;
				
			EPRINTF("unclosed template on page '%s' at '%.*s'\n",
			        title,
			        len,
			        possible_template.str);
		}
	}
	
	return template;
}

static inline bool print_templates (const char * const title,
                                    const buffer_t * const str,
                                    hattrie_t * template_names,
                                    bool verbose) {
	const char * p = buffer_string(str);
	const char * const end = p + buffer_length(str);
	bool found_template = false;
	output_file_mask_t output_file_mask = 0;
	
	while (p < end) {
		const char * const open_template = strstr(p, "{{");
		
		if (open_template == NULL)
			break;
			
		str_slice_t possible_template =
		    str_slice_init(open_template,
		                   end - open_template);
		str_slice_t template = find_template(title,
		                                     possible_template,
		                                     template_names,
		                                     &found_template,
		                                     &output_file_mask,
		                                     verbose);
		                                     
		p = template.str != NULL
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
	    print_templates(page->title,
	                    buffer,
	                    data->template_names,
	                    data->verbose);
	                    
	return !(found_template && ++data->match_count >= data->max_matches);
}
static inline void get_input_file (command_t * commands) {
	option_data * options = commands->data;
	FILE * file = fopen(commands->arg, "rb");
	CHECK_FILE(file);
	options->input_file = file;
}

static inline uint16_t increment_output_file_bit_index() {
	if (output_file_bit_index >= MAX_OUTPUT_FILES)
		CRASH_WITH_MSG("too many output files");
	else
		return output_file_bit_index++;
}

static inline void get_default_output_file (command_t * commands) {
	option_data * options = commands->data;
	FILE * file = fopen(commands->arg, "wb");
	CHECK_FILE(file);
	options->default_output_file = STORE_BIT_INDEX(file);
}

static inline void add_template_names_to_trie (FILE * template_names_file,
        hattrie_t * template_trie,
        hattrie_t * filepath_trie,
        bool * need_default_output_file) {
	char line[BUFSIZ];
	int count = 0;
	
	while (fgets(line, BUFSIZ, template_names_file) != NULL) {
		const char * tab = strchr(line, '\t');
		size_t len = tab != NULL ? (size_t) (tab - line) : strlen(line);
		str_slice_t template_name = trim(str_slice_init(line, len));
		value_t * entry;
		++count;
		
		if (template_name.len == 0)
			CRASH_WITH_MSG("file contains an empty line\n");
			
		entry = hattrie_tryget(template_trie, template_name.str, template_name.len);
		
		if (entry != NULL)
			CRASH_WITH_MSG("two entries for '%.*s' in template name files\n", (int) len, line);
			
		entry = hattrie_get(template_trie, template_name.str, template_name.len);
		
		if (tab != NULL) {
			str_slice_t filename_slice = trim(str_slice_init(tab + 1, strlen(tab + 1)));
			char filename[PATH_MAX + 1];
			char path[PATH_MAX + 1];
			
			if (filename_slice.len < sizeof filename - 1)
				strncpy(filename, filename_slice.str, sizeof filename - 1);
			else
				CRASH_WITH_MSG("filename '%.*s' too long",
				               (int) filename_slice.len,
				               filename_slice.str);
				               
			filename[filename_slice.len] = '\0';
			
			if (realpath(filename, path) != NULL) {
				strncpy(path, filename, sizeof path - 1);
				path[sizeof path - 1] = '\0'; // just in case
			}
			
			value_t * filepath_entry = hattrie_tryget(filepath_trie,
			                           path,
			                           strlen(path));
			FILE * output_file;
			
			if (filepath_entry != NULL)
				output_file = (FILE *) *filepath_entry;
			else {
				output_file = fopen(path, "wb");
				CHECK_FILE(output_file);
				output_file = STORE_BIT_INDEX(output_file);
				filepath_entry = hattrie_get(filepath_trie,
				                             path,
				                             strlen(path));
				*filepath_entry = (value_t) output_file;
			}
			
			*entry = (value_t) output_file;
		} else { // File added later by add_default_output_file.
			*need_default_output_file = true;
			*entry = (value_t) NULL;
		}
	}
	
	if (ferror(template_names_file))
		perror("error in reading file of template names"), exit(EXIT_FAILURE);
	else if (count == 0)
		CRASH_WITH_MSG("no template names found in file\n");
}

static inline hattrie_t * create_trie_if_needed(hattrie_t * trie) {
	if (trie == NULL) {
		trie = hattrie_create();
		
		if (trie == NULL)
			CRASH_WITH_MSG("could not create trie\n");
	}
	
	return trie;
}

static void get_templates_to_find (command_t * commands) {
	option_data * options = commands->data;
	additional_parse_data * data = &options->parse_data;
	size_t len = strlen(commands->arg);
	
	if (len == 0)
		CRASH_WITH_MSG("expected non-empty string\n"); // ???
		
	FILE * template_names_file = fopen(commands->arg, "rb");
	CHECK_FILE(template_names_file);
	
	data->template_names = create_trie_if_needed(data->template_names);
	options->file_paths = create_trie_if_needed(options->file_paths);
	
	add_template_names_to_trie(template_names_file,
	                           data->template_names,
	                           options->file_paths,
	                           &options->need_default_output_file);
	                           
	EPRINTF("added template names from '%s'\n", commands->arg);
	
	if (fclose(template_names_file) == EOF)
		perror("could not close file"), exit(-1);
}

static void get_page_count (command_t * commands) {
	option_data * options = commands->data;
	additional_parse_data * data = &options->parse_data;
	unsigned int count = 0;
	
	if (strcmp(commands->arg, "max") == 0)
		count = UINT_MAX;
	else if (sscanf(commands->arg, "%u", &count) != 1 || count == 0)
		CRASH_WITH_MSG("expected positive integer greater than 0\n");
		
	data->max_matches = count;
}


static void get_filter (command_t * commands) {
	option_data * options = commands->data;
	additional_parse_data * data = &options->parse_data;
	char * filter;
	
	if (strlen(commands->arg) == 0)
		CRASH_WITH_MSG("expected non-empty string"); // ???
		
	filter = strdup(commands->arg);
	
	if (filter == NULL)
		CRASH_WITH_MSG("not enough memory");
		
	data->filter = filter;
}

static void make_verbose (command_t * commands) {
	option_data * options = commands->data;
	additional_parse_data * data = &options->parse_data;
	data->verbose = true;
}

static void print_trie_keys (hattrie_t * trie) {
	hattrie_iter_t * iter;
	int i = 0;
	
	for (iter = hattrie_iter_begin(trie, true);
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

static void add_default_output_file (hattrie_t * template_names,
                                     FILE * default_output_file) {
	hattrie_iter_t * iter;
	
	for (iter = hattrie_iter_begin(template_names, true);
	        !hattrie_iter_finished(iter);
	        hattrie_iter_next(iter)) {
		value_t * val = hattrie_iter_val(iter);
		
		if ((void *) *val == NULL) {
			if (default_output_file == NULL) {
				size_t len;
				const char * key = hattrie_iter_key(iter, &len);
				CRASH_WITH_MSG("default output file required for template "
				               "'%.*s', which has no output file specified\n",
				               (int) len, key);
			} else
				*val = (value_t) default_output_file;
		}
	}
	
	hattrie_iter_free(iter);
}

static inline void parse_arguments (int argc, char * * argv, option_data * options) {
	command_t commands;
	additional_parse_data * data = &options->parse_data;
	
	command_init(&commands, argv[0], "0");
	
	commands.data = (void *) options;
	
	command_option(&commands, "-i", "--input <file>",
	               "XML page dump file",
	               get_input_file);
	               
	command_option(&commands, "-o", "--output <file>",
	               "place output here if template names file does not give a filename",
	               get_default_output_file);
	               
	command_option(&commands, "-t", "--template-names <file>",
	               "file containing optional template names with optional tab and output filename",
	               get_templates_to_find);
	               
	command_option(&commands, "-p", "--page-count <num>",
	               "return matches from this many pages (not counting non-matching pages)",
	               get_page_count);
	               
	command_option(&commands, "-f", "--filter <text to find>",
	               "only process pages containing this text",
	               get_filter);
	               
	command_option(&commands, "-v", "--verbose",
	               "print all invalid templates to log",
	               make_verbose);
	               
	command_parse(&commands, argc, argv);
	
	command_free(&commands);
	
	if (options->input_file == NULL)
		CRASH_WITH_MSG("--input required\n");
	else if (data->max_matches == 0)
		CRASH_WITH_MSG("--page-count required\n");
	else if (data->template_names == NULL)
		CRASH_WITH_MSG("--template-names required\n");
		
	if (options->need_default_output_file)
		add_default_output_file(data->template_names, options->default_output_file);
	else if (options->default_output_file != NULL)
		EPRINTF("default output file provided, but not needed because every "
		        "template has an output file specified\n");
		        
	size_t template_count = hattrie_size(data->template_names);
	EPRINTF("searching for instances of %zu template%s on up to %u pages",
	        template_count,
	        template_count == 1 ? "" : "s",
	        data->max_matches);
	        
	if (data->filter != NULL)
		EPRINTF(" containing '%s'", data->filter);
		
	EPRINTF("\n");
	
	EPRINTF("list of templates:\n");
	print_trie_keys(data->template_names);
}

static inline void close_files (FILE * input_file,
                                FILE * default_output_file,
                                hattrie_t * file_paths) {
	if ((input_file != NULL && fclose(input_file) == EOF)
	        || (default_output_file != NULL
	            && fclose(GET_PTR_VAL(default_output_file)) == EOF))
		CRASH_WITH_MSG("failed to close file");
		
	hattrie_iter_t * iter;
	
	if (hattrie_size(file_paths) == 0)
		return;
		
	for (iter = hattrie_iter_begin(file_paths, false);
	        !hattrie_iter_finished(iter);
	        hattrie_iter_next(iter)) {
		value_t * val = hattrie_iter_val(iter);
		FILE * file = (FILE *) GET_PTR_VAL(*val);
		
		if (fclose(file) == EOF)
			CRASH_WITH_MSG("failed to close file");
	}
	
	hattrie_iter_free(iter);
}

static inline void clean_up(option_data * options) {
	close_files(options->input_file,
	            options->default_output_file,
	            options->file_paths);
	            
	hattrie_free(options->file_paths);
	options->file_paths = NULL;
	hattrie_free(options->parse_data.template_names);
	options->parse_data.template_names = NULL;
	
	if (options->parse_data.filter != NULL) {
		free(options->parse_data.filter);
		options->parse_data.filter = NULL;
	}
}

int main (int argc, char * * argv) {
	option_data options = {};
	Wiktionary_namespace_t namespaces[] = { NAMESPACE_MAIN, NAMESPACE_NONE };
	
	parse_arguments(argc, argv, &options);
	
	parse_Wiktionary_page_dump(options.input_file,
	                           handle_page,
	                           namespaces,
	                           &options.parse_data);
	                           
	EPRINTF("found templates on %u pages\n", options.parse_data.match_count);
	
	clean_up(&options);
	
	return 0;
}
