#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <hat-trie/hat-trie.h>

#include "utils/buffer.h"
#include "utils/parser.h"
#include "utils/get_header.h"
#include "commander.h"

typedef uint32_t page_count_t;

typedef struct additional_parse_data {
	page_count_t pages_to_process;
	hattrie_t * trie;
} additional_parse_data;

typedef struct {
	page_count_t pages_to_process;
	FILE * input_file, * output_file;
} option_t;

#define PAGE_COUNT_SCAN_FORMAT "%" SCNu32

#define CHECK_FILE(file) \
	if ((file) == NULL) \
		perror("could not open file"), exit(-1)

#define MAKE_HATTRIE_FUNC(name) \
	static inline value_t * hattrie_##name##_slice (hattrie_t * trie, \
                                                    str_slice_t slice) { \
		return hattrie_##name(trie, slice.str, slice.len); \
	}

MAKE_HATTRIE_FUNC(tryget)
MAKE_HATTRIE_FUNC(get)

#undef MAKE_HATTRIE_FUNC

static inline str_slice_t hattrie_iter_key_slice (hattrie_iter_t * iter) {
	size_t len;
	const char * key = hattrie_iter_key(iter, &len);
	return str_slice_init(key, len);
}

static inline void increment_count (hattrie_t * trie,
                                    str_slice_t key) {
	value_t * count = hattrie_tryget_slice(trie, key);
	
	if (count == NULL) {
		count = hattrie_get_slice(trie, key);
		*count = (value_t) 1;
	} else
		++*count;
}

static inline str_slice_t get_line (str_slice_t slice) {
	const char * p = slice.str;
	
	while (p < STR_SLICE_END(slice) && *p != '\n')
		++p;
		
	return str_slice_init(slice.str, p - slice.str);
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

static inline void find_headers (hattrie_t * header_line_trie,
                                 str_slice_t buffer) {
	const char * const end = STR_SLICE_END(buffer);
	
	while (buffer.str < end) {
		if (buffer.str[0] == '=') {
			str_slice_t line = get_line(buffer);
			increment_count(header_line_trie, line);
			buffer = str_slice_init(STR_SLICE_END(line),
			                        STR_SLICE_END(buffer) - STR_SLICE_END(line));
		} else
			buffer = skip_to_next_line(buffer);
	}
}

static bool handle_page (parse_info * info) {
	additional_parse_data * data = info->additional_data;
	hattrie_t * header_line_trie = data->trie;
	static uint32_t page_count = 0;
	
	// Skip user talk page moved into mainspace.
	if (memcmp(info->page.title, "User-talk:", sizeof "User-talk:" - 1) == 0) {
		EPRINTF("Skipped '%s'\n", info->page.title);
		return true;
	}
	
	if (++page_count > data->pages_to_process)
		return false;
		
	buffer_t * buffer = &info->page.content;
	find_headers(header_line_trie,
		str_slice_init(buffer_string(buffer), buffer_length(buffer)));
	
	return true;
}

#define HEADER_COUNTS_SIZE MAX_HEADER_LEVEL * sizeof (header_count_t)
static inline void * add_trie_mem (hattrie_t * trie,
                                   str_slice_t key,
                                   size_t mem_size) {
	value_t * storage;
	header_count_t * mem = malloc(mem_size);
	
	if (mem == NULL)
		CRASH_WITH_MSG("could not allocate memory");
		
	memset(mem, '\0', mem_size);
	
	storage = hattrie_get_slice(trie, key);
	*storage = (value_t) mem;
	
	return mem;
}

static inline void free_all_trie_mem (hattrie_t * trie) {
	hattrie_iter_t * iter;
	
	for (iter = hattrie_iter_begin(trie, false);
	        !hattrie_iter_finished(iter);
	        hattrie_iter_next(iter)) {
		value_t * storage = hattrie_iter_val(iter);
		void * mem = (void *) *storage;
		free(mem);
		*storage = (value_t) NULL;
	}
	
	hattrie_iter_free(iter);
}

static inline void add_header_count (hattrie_t * header_trie,
                                     str_slice_t header,
                                     const header_level_t header_level,
                                     const header_count_t header_count) {
	value_t * header_storage;
	header_count_t * header_counts;
	
	// Check if header was already inserted.
	header_storage = hattrie_tryget_slice(header_trie, header);
	
	if (header_storage == NULL) // Insert key and add pointer to array.
		header_counts = add_trie_mem(header_trie, header, HEADER_COUNTS_SIZE);
	else
		header_counts = (header_count_t *) *header_storage;
		
	header_counts[header_level - 1] += header_count;
}

// Take trie of header lines and add any valid header text to new trie.
static inline void filter_headers (hattrie_t * header_trie,
                                   hattrie_t * header_line_trie) {
	hattrie_iter_t * iter;
	
	for (iter = hattrie_iter_begin(header_line_trie, true);
	        !hattrie_iter_finished(iter);
	        hattrie_iter_next(iter)) {
		str_slice_t header_line, header;
		header_level_t header_level;
		header_line = hattrie_iter_key_slice(iter);
		                               
		header = get_header(header_line,
		                    &header_level);
		                    
		// Insert genuine headers that are not empty strings and count the
		// number of times that they occur.
		if (header.str != NULL) {
			if (header.len > 0) {
				value_t * header_line_storage = hattrie_iter_val(iter);
				header_count_t header_count;
				
				if (header_line_storage == NULL)
					CRASH_WITH_MSG("!!!");
					
				header_count = (header_count_t) * header_line_storage;
				
				add_header_count(header_trie,
				                 header,
				                 header_level,
				                 header_count);
			} else
				EPRINTF("found empty header\n");
		} else
			EPRINTF("no header found in potential header line '%.*s'\n",
			        (int) header_line.len, header_line.str);
	}
	
	hattrie_iter_free(iter);
}

static inline void print_header_counts (FILE * output_file,
                                        const header_count_t * const header_counts) {
	bool first = true;
	
	for (header_level_t header_level = 1;
	        header_level <= MAX_HEADER_LEVEL;
	        ++header_level) {
		const header_count_t count = header_counts[header_level - 1];
		
		if (count > 0) {
			fprintf(output_file, "%s%hd:%d", first ? "" : ";",
			        header_level, count);
			        
			first = false;
		}
	}
}

static inline void print_header_info (FILE * output_file,
                                      str_slice_t key,
                                      const header_count_t * const header_counts) {
	fprintf(output_file, "%.*s\t", (int) key.len, key.str);
	print_header_counts(output_file, header_counts);
	putc('\n', output_file);
}

static inline void print_header_trie (FILE * output_file, hattrie_t * trie) {
	hattrie_iter_t * iter;
	
	for (iter = hattrie_iter_begin(trie, true);
	        !hattrie_iter_finished(iter);
	        hattrie_iter_next(iter)) {
		str_slice_t key = hattrie_iter_key_slice(iter);
		value_t * value = hattrie_iter_val(iter);
		
		if (value == NULL)
			CRASH_WITH_MSG("!!!");
			
		header_count_t * header_counts = (header_count_t *) *value;
		print_header_info(output_file, key, header_counts);
	}
	
	hattrie_iter_free(iter);
}

static inline void print_trie_info (hattrie_t * header_line_trie,
                                    hattrie_t * header_trie) {
	char byte_count_buf[BYTE_COUNT_BUF_SIZE];
	
	EPRINTF("found %zu potential header lines and %zu unique headers\n",
	        hattrie_size(header_line_trie), hattrie_size(header_trie));
	        
	format_byte_count(hattrie_sizeof(header_line_trie),
	                  byte_count_buf,
	                  sizeof byte_count_buf);
	EPRINTF("used %s in header line trie and ", byte_count_buf);
	
	format_byte_count(hattrie_sizeof(header_trie),
	                  byte_count_buf,
	                  sizeof byte_count_buf);
	EPRINTF("%s in header trie\n", byte_count_buf);
}

static inline void process_pages (page_count_t pages_to_process,
                                  Wiktionary_namespace_t * namespaces,
                                  FILE * input_file,
                                  FILE * output_file) {
	hattrie_t * header_line_trie = hattrie_create();
	hattrie_t * header_trie;
	additional_parse_data data;
	
	data.trie = header_line_trie;
	data.pages_to_process = pages_to_process;
	
	parse_Wiktionary_page_dump(input_file, handle_page, namespaces, &data);
	
	header_trie = hattrie_create();
	
	filter_headers(header_trie, header_line_trie);
	
	print_trie_info(header_line_trie, header_trie);
	
	print_header_trie(output_file, header_trie);
	
	hattrie_free(header_line_trie);
	free_all_trie_mem(header_trie);
	hattrie_free(header_trie);
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

static void get_pages_to_process (command_t * commands) {
	page_count_t count;
	option_t * options = commands->data;
	
	if (sscanf(commands->arg, PAGE_COUNT_SCAN_FORMAT, &count) != 1)
		CRASH_WITH_MSG("'%s' is not a valid integer!\n", commands->arg);
		
	options->pages_to_process = count;
}

static inline void process_args (int argc, char * * argv, option_t * options) {
	command_t commands;
	
	options->pages_to_process = 0;
	options->input_file = NULL;
	options->output_file = NULL;
	
	command_init(&commands, argv[0], "0");
	commands.data = options;
	
	command_option(&commands, "-i", "--input <file>",
	               "XML page dump file",
	               get_input_file);
	               
	command_option(&commands, "-o", "--output <file>",
	               "place output here",
	               get_output_file);
	               
	command_option(&commands, "-p", "--pages <count>",
	               "number of pages to process",
	               get_pages_to_process);
	               
	command_parse(&commands, argc, argv);
	
	command_free(&commands);
	
	if (options->pages_to_process == 0) {
		options->pages_to_process = UINT32_MAX;
		EPRINTF("No page limit given; set to %u\n", options->pages_to_process);
	} else if (options->input_file == NULL)
		CRASH_WITH_MSG("input file required\n");
	else if (options->output_file == NULL)
		CRASH_WITH_MSG("output file required\n");
}

int main (int argc, char * * argv) {
	option_t options;
	Wiktionary_namespace_t namespaces[] = { NAMESPACE_MAIN, NAMESPACE_NONE };
	
	process_args(argc, argv, &options);
	
	process_pages(options.pages_to_process,
	              namespaces,
	              options.input_file,
	              options.output_file);
	              
	return 0;
}
