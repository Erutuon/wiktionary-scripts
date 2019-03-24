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

static inline void increment_count (hattrie_t * trie,
                                    const char * str,
                                    size_t len) {
	value_t * count = hattrie_tryget(trie, str, len);
	
	if (count == NULL) {
		count = hattrie_get(trie, str, len);
		*count = (value_t) 1;
	} else
		++*count;
}

static inline size_t get_line_len (const char * const line_start,
                                   const char * const end) {
	const char * p = line_start;
	
	while (p < end && *p != '\n')
		++p;
		
	return p - line_start;
}

static inline void find_headers (hattrie_t * header_line_trie,
                                 const char * buffer,
                                 size_t len) {
	const char * str = buffer;
	const char * const end = buffer + len;
	
	while (str < end) {
		if (str[0] == '=') {
			const char * line_start = str;
			size_t line_len = get_line_len(line_start, end);
			
			increment_count(header_line_trie, line_start, line_len);
			
			str += line_len;
		} else {
			// Skip line and one or more newlines.
			while (str < end && *str != '\n')
				++str;
				
			while (str < end && *str == '\n')
				++str;
		}
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
	find_headers(header_line_trie, buffer_string(buffer), buffer_length(buffer));
	
	return true;
}

#define HEADER_COUNTS_SIZE MAX_HEADER_LEVEL * sizeof (header_count_t)
static inline void * add_trie_mem (hattrie_t * trie,
                                   const char * key,
                                   size_t key_len,
                                   size_t mem_size) {
	value_t * storage;
	header_count_t * mem = malloc(mem_size);
	
	if (mem == NULL)
		CRASH_WITH_MSG("could not allocate memory");
		
	memset(mem, '\0', mem_size);
	
	storage = hattrie_get(trie, key, key_len);
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
                                     const char * const header,
                                     const size_t header_len,
                                     const header_level_t header_level,
                                     const header_count_t header_count) {
	value_t * header_storage;
	header_count_t * header_counts;
	
	// Check if header was already inserted.
	header_storage = hattrie_tryget(header_trie, header, header_len);
	
	if (header_storage == NULL) // Insert key and add pointer to array.
		header_counts = add_trie_mem(header_trie, header, header_len, HEADER_COUNTS_SIZE);
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
		size_t header_line_len, header_len;
		const char * header_line, * header;
		header_level_t header_level;
		header_line = hattrie_iter_key(iter,
		                               &header_line_len);
		                               
		header = get_header(header_line,
		                    header_line_len,
		                    &header_len,
		                    &header_level);
		                    
		// Insert genuine headers that are not empty strings and count the
		// number of times that they occur.
		if (header != NULL) {
			if (header_len > 0) {
				value_t * header_line_storage = hattrie_iter_val(iter);
				header_count_t header_count;
				
				if (header_line_storage == NULL)
					CRASH_WITH_MSG("!!!");
					
				header_count = (header_count_t) * header_line_storage;
				
				add_header_count(header_trie,
				                 header,
				                 header_len,
				                 header_level,
				                 header_count);
			} else
				EPRINTF("found empty header\n");
		} else
			EPRINTF("no header found in potential header line '%.*s'\n",
			        (int) header_line_len, header_line);
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
                                      const char * const key,
                                      const size_t len,
                                      const header_count_t * const header_counts) {
	fprintf(output_file, "%.*s\t", (int) len, key);
	print_header_counts(output_file, header_counts);
	putc('\n', output_file);
}

static inline void print_header_trie (FILE * output_file, hattrie_t * trie) {
	hattrie_iter_t * iter;
	
	for (iter = hattrie_iter_begin(trie, true);
	        !hattrie_iter_finished(iter);
	        hattrie_iter_next(iter)) {
		size_t len;
		const char * key = hattrie_iter_key(iter, &len);
		value_t * value = hattrie_iter_val(iter);
		
		if (value == NULL)
			CRASH_WITH_MSG("!!!");
			
		header_count_t * header_counts = (header_count_t *) *value;
		print_header_info(output_file, key, len, header_counts);
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
	
	do_parsing(input_file, handle_page, namespaces, &data);
	
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