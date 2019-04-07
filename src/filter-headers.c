#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include <time.h>
#include <hat-trie/hat-trie.h>

#include "utils/buffer.h"
#include "utils/parser.h"
#include "utils/get_header.h"
#include "utils/string_slice.h"
#include "commander.h"

#define HEADER_COUNTS_SIZE MAX_HEADER_LEVEL * sizeof (header_count_t)

#define MALLOC_FAIL CRASH_WITH_MSG("could not allocate memory\n")

#define MAX_HEADERS_PER_PAGE 4

#define CHECK_FILE(file) \
	if ((file) == NULL) \
		perror("could not open file"), exit(-1)

typedef uint32_t page_count_t;

typedef struct additional_parse_data {
	page_count_t pages_to_process;
	hattrie_t * trie;
	hattrie_t * headers_to_ignore;
} additional_parse_data;

typedef struct {
	page_count_t pages_to_process;
	hattrie_t * headers;
	Wiktionary_namespace_t * namespaces;
	FILE * input_file;
	FILE * output_file;
} option_t;

typedef struct {
	header_count_t counts[HEADER_COUNTS_SIZE];
	char header[1];
} header_info_t;

#define PAGE_COUNT_SCAN_FORMAT "%" SCNu32
#define PAGE_COUNT_PRINT_FORMAT "%" PRIu32

#define MAX_NAMESPACES 7

#define STR_SET hattrie_t
#define STR_SET_NEW hattrie_create
#define STR_SET_FREE hattrie_free
#define STR_SET_ADD hattrie_get_slice
#define STR_SET_ITER hattrie_iter_t
#define STR_SET_ITER_FREE hattrie_iter_free
#define STR_SET_FOR(set, iter) \
	for (iter = hattrie_iter_begin(set, true); \
	        !hattrie_iter_finished(iter); \
	        hattrie_iter_next(iter))
#define STR_SET_ITER_KEY hattrie_iter_key_slice

static inline str_slice_t get_line (str_slice_t slice) {
	const char * p = slice.str;
	
	while (p < STR_SLICE_END(slice) && *p != '\n')
		++p;
		
	return str_slice_init(slice.str, p - slice.str);
}

static inline void * add_trie_mem (hattrie_t * trie,
                                   str_slice_t key) {
	value_t * storage;
	STR_SET * set = STR_SET_NEW();
	
	storage = hattrie_get_slice(trie, key);
	*storage = (value_t) set;
	
	return set;
}

static inline void free_all_trie_mem (hattrie_t * trie) {
	hattrie_iter_t * iter;
	
	for (iter = hattrie_iter_begin(trie, false);
	        !hattrie_iter_finished(iter);
	        hattrie_iter_next(iter)) {
		value_t * storage = hattrie_iter_val(iter);
		STR_SET * set = (STR_SET *) *storage;
		STR_SET_FREE(set);
		*storage = (value_t) NULL;
	}
	
	hattrie_iter_free(iter);
}

static inline bool str_slice_eq (const char * str, str_slice_t slice) {
	return strlen(str) == slice.len
	       && memcmp(str, slice.str, slice.len) == 0
	       && str[slice.len] == '\0';
}

static inline void add_to_set (hattrie_t * titles_by_header,
                               str_slice_t header,
                               const char * title) {
	value_t * storage = hattrie_tryget_slice(titles_by_header, header);
	str_slice_t title_slice = str_slice_init(title, strlen(title));
	STR_SET * titles;
	
	if (storage == NULL) {
		titles = add_trie_mem(titles_by_header,
		                       header);
	} else
		titles = *(STR_SET * *) storage;
		
	STR_SET_ADD(titles, title_slice);
}

// Skip line and one or more newlines.
static inline str_slice_t skip_to_next_line (str_slice_t slice) {
	const char * p = slice.str, * end = STR_SLICE_END(slice);
	
	while (p < end
	        && *p != '\n')
		++p;
		
	while (p < end && *p == '\n')
		++p;
		
	return str_slice_init(p, end - p > 0 ? end - p : 0);
}

static inline void find_headers (hattrie_t * titles_by_header,
                                 hattrie_t * headers_to_ignore,
                                 const char * title,
                                 str_slice_t buffer) {
	while (buffer.str < STR_SLICE_END(buffer)) {
		str_slice_t line = get_line(buffer);
		
		if (buffer.str[0] == '=') {
			str_slice_t header = get_header(line, NULL);
			
			if (header.str != NULL && header.len > 0
			        && hattrie_tryget_slice(headers_to_ignore, header) == NULL)
				add_to_set(titles_by_header, header, title);
				
			buffer = str_slice_init(STR_SLICE_END(line),
			                        STR_SLICE_END(buffer) - STR_SLICE_END(line));
		} else
			buffer = skip_to_next_line(buffer);
	}
}

static bool handle_page (parse_info * info) {
	additional_parse_data * data = info->additional_data;
	hattrie_t * titles_by_header = data->trie;
	hattrie_t * headers_to_ignore = data->headers_to_ignore;
	static uint32_t page_count = 0;
	
	// Skip user talk page moved into mainspace.
	if (memcmp(info->page.title, "User-talk:", sizeof "User-talk:" - 1) == 0) {
		EPRINTF("Skipped '%s'\n", info->page.title);
		return true;
	}
	
	if (++page_count > data->pages_to_process)
		return false;
		
	find_headers(titles_by_header,
	             headers_to_ignore,
	             info->page.title,
	             buffer_to_str_slice(&info->page.content));
	             
	return true;
}

static inline void print_header_info (const str_slice_t header,
                                      STR_SET * headers,
                                      FILE * output_file) {
	STR_SET_ITER * iter;
	int i = 0;
	fprintf(output_file, "%.*s\t", (int) header.len, header.str);
	
	STR_SET_FOR(headers, iter) {
		str_slice_t title = STR_SET_ITER_KEY(iter);
		
		if (i++ > 0)
			putc('\t', output_file);
			
		fprintf(output_file, "%.*s", (int) title.len, title.str);
	}
	
	putc('\n', output_file);
	
	STR_SET_ITER_FREE(iter);
}

static inline void print_titles_by_header (hattrie_t * trie,
        FILE * output_file) {
	hattrie_iter_t * iter;
	
	for (iter = hattrie_iter_begin(trie, true);
	        !hattrie_iter_finished(iter);
	        hattrie_iter_next(iter)) {
		str_slice_t header = hattrie_iter_key_slice(iter);
		value_t * value = hattrie_iter_val(iter);
		
		if (value == NULL)
			CRASH_WITH_MSG("!!!\n");
			
		STR_SET * titles = *(STR_SET * *) value;
		print_header_info(header, titles, output_file);
	}
	
	hattrie_iter_free(iter);
}

static inline void process_pages (page_count_t pages_to_process,
                                  hattrie_t * headers_to_ignore,
                                  Wiktionary_namespace_t * namespaces,
                                  FILE * input_file,
                                  FILE * output_file) {
	hattrie_t * titles_by_header = hattrie_create();
	additional_parse_data data;
	
	data.trie = titles_by_header;
	data.pages_to_process = pages_to_process;
	data.headers_to_ignore = headers_to_ignore;
	
	parse_Wiktionary_page_dump(input_file, handle_page, namespaces, &data);
	
	size_t title_count = hattrie_size(titles_by_header);
	EPRINTF("gathered headers from %zu page%s\n",
	        title_count, title_count == 1 ? "" : "s");
	        
	print_titles_by_header(titles_by_header, output_file);
	
	free_all_trie_mem(titles_by_header);
	hattrie_free(titles_by_header);
}

static inline void get_pages_to_process (command_t * commands) {
	page_count_t count;
	
	if (sscanf(commands->arg, PAGE_COUNT_SCAN_FORMAT, &count) != 1)
		EPRINTF("'%s' is not a valid integer!", commands->arg);
		
	option_t * options = commands->data;
	options->pages_to_process = count;
}

static inline void add_headers (command_t * commands) {
	option_t * options = commands->data;
	hattrie_t * headers = options->headers;
	const char * filename = commands->arg;
	FILE * header_list = fopen(filename, "rb");
	
	CHECK_FILE(header_list);
	
	char buf[BUFSIZ];
	size_t line_number = 0;
	
	while (fgets(buf, sizeof buf, header_list) != NULL) {
		++line_number;
		const char * newline = strchr(buf, '\n');
		
		if (newline == NULL && !feof(header_list)) {
			char byte_count_buf[BYTE_COUNT_BUF_SIZE];
			format_byte_count(sizeof buf, byte_count_buf, sizeof byte_count_buf);
			CRASH_WITH_MSG("line %zu in '%s' is too long for buffer (%s)!\n",
			               line_number, filename, byte_count_buf);
		}
		
		str_slice_t header = str_slice_init(buf,
		                                    newline != NULL
		                                    ? (size_t) (newline - buf)
		                                    : sizeof buf - 1);
		                                    
		if (header.len > 0)
			hattrie_get_slice(headers, header);
	}
	
	if (ferror(header_list))
		perror("error while reading file"), exit(-1);
		
	fclose(header_list);
}

static inline void add_namespaces (command_t * commands) {
	option_t * options = commands->data;
	Wiktionary_namespace_t * namespaces = options->namespaces;
	int characters_read = 0, total_characters_read = 0, i = 0;
	Wiktionary_namespace_t namespace;
	const char * arg = commands->arg;
	
	if (namespaces[0] != NAMESPACE_NONE)
		CRASH_WITH_MSG("cannot supply namespaces twice");
		
	if (strcmp(arg, "all") == 0) // Search all namespaces.
		return;
		
	if (sscanf(arg + total_characters_read,
	           "%d%n",
	           &namespace,
	           &characters_read) == 1) {
		total_characters_read += characters_read;
		
		if (i >= MAX_NAMESPACES - 1)
			CRASH_WITH_MSG("too many namespaces; max %d\n", MAX_NAMESPACES);
			
		namespaces[i++] = namespace;
	}
	
	namespaces[i] = NAMESPACE_NONE;
	
	size_t arg_len = strlen(arg);
	
	if ((size_t) total_characters_read != arg_len)
		EPRINTF("Did not read all characters in list of namespaces in '%s'\n",
		        commands->arg);
}

static inline void get_input_file (command_t * commands) {
	option_t * options = commands->data;
	FILE * input_file = fopen(commands->arg, "rb");
	CHECK_FILE(input_file);
	options->input_file = input_file;
}

static inline void get_output_file (command_t * commands) {
	option_t * options = commands->data;
	FILE * output_file = fopen(commands->arg, "wb");
	CHECK_FILE(output_file);
	options->output_file = output_file;
}

#define MAX_PAGES_TO_PROCESS UINT32_MAX

static inline void process_args (option_t * options,
                                 int argc,
                                 char * * argv,
                                 hattrie_t * headers_to_ignore,
                                 Wiktionary_namespace_t * namespaces) {
	command_t commands;
	command_init(&commands, argv[0], "0");
	commands.data = options;
	char command_desc[128]; // Use this in only one command.
	
	options->pages_to_process = 0;
	options->headers = headers_to_ignore;
	options->namespaces = namespaces;
	options->input_file = NULL;
	options->output_file = NULL;
	namespaces[0] = NAMESPACE_NONE; // sentinel!
	
	command_option(&commands, "-i", "--input <file>",
	               "XML page dump file",
	               get_input_file);
	               
	command_option(&commands, "-o", "--output <file>",
	               "place output here",
	               get_output_file);
	               
	command_option(&commands, "-n", "--namespaces <numbers>",
	               "list of namespace numbers",
	               add_namespaces);
	               
	command_option(&commands, "-x", "--exclude-headers <file>",
	               "file containing newline-separated headers "
	               "that should not be tracked",
	               add_headers);
	               
	snprintf(command_desc,
	         sizeof command_desc,
	         "number of pages in the given namespaces to process "
	         "(default: " PAGE_COUNT_PRINT_FORMAT ")",
	         MAX_PAGES_TO_PROCESS);
	command_option(&commands, "-p", "--pages <count>",
	               command_desc,
	               get_pages_to_process);
	               
	command_parse(&commands, argc, argv);
	
	size_t count = hattrie_size(options->headers);
	
	if (count == 0)
		EPRINTF("no headers to ignore; will return all headers "
		        "on the pages being processed!\n");
	else
		EPRINTF("%zu headers will be ignored\n", count);
		
	if (options->pages_to_process == 0) {
		options->pages_to_process = MAX_PAGES_TO_PROCESS;
		EPRINTF("no page limit given; set to %u\n", options->pages_to_process);
	}
	
	command_free(&commands);
	
	if (options->input_file == NULL)
		CRASH_WITH_MSG("input file required\n");
	else if (options->output_file == NULL)
		CRASH_WITH_MSG("output file required\n");
}

int main (int argc, char * * argv) {
	option_t options;
	hattrie_t * headers_to_ignore = hattrie_create();
	Wiktionary_namespace_t namespaces[MAX_NAMESPACES];
	time_t start_time, end_time;
	
	start_time = clock();
	
	if (headers_to_ignore == NULL)
		CRASH_WITH_MSG("failed to create trie\n");
		
	process_args(&options, argc, argv, headers_to_ignore, namespaces);
	
	process_pages(options.pages_to_process,
	              options.headers,
	              options.namespaces,
	              options.input_file,
	              options.output_file);
	              
	hattrie_free(headers_to_ignore);
	
	if (fclose(options.output_file) == EOF
	        || fclose(options.input_file) == EOF)
		perror("could not close file"), exit(-1);
		
	end_time = clock();
	EPRINTF("total time: %f seconds\n",
	        ((double) end_time - start_time) / CLOCKS_PER_SEC);
	        
	return 0;
}
