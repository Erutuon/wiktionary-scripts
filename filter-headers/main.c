#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <hat-trie/hat-trie.h>
#include <string.h> // strndup is needed

#include "utils/buffer.h"
#include "utils/parser.h"
#include "utils/get_header.h"
#include "commander.h"

#define HEADER_COUNTS_SIZE MAX_HEADER_LEVEL * sizeof (header_count_t)

#define MALLOC_FAIL CRASH_WITH_MSG("could not allocate memory\n")

#define MAX_HEADERS_PER_PAGE 4

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
} option_t;

typedef struct {
	header_count_t counts[HEADER_COUNTS_SIZE];
	char header[1];
} header_info_t;

#define PAGE_COUNT_SCAN_FORMAT "%" SCNu32
#define PAGE_COUNT_PRINT_FORMAT "%" PRIu32

#define MAX_NAMESPACES 7

static inline size_t get_line_len (const char * const line_start,
                                   const char * const end) {
	const char * p = line_start;
	
	while (p < end && *p != '\n')
		++p;
		
	return p - line_start;
}

static inline void * add_trie_mem (hattrie_t * trie,
                                   const char * key,
                                   size_t key_len,
                                   size_t mem_size) {
	value_t * storage;
	void * mem = malloc(mem_size);
	
	if (mem == NULL)
		MALLOC_FAIL;
		
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
		char * * mem = (char * *) *storage;
		for (int i = 0; i < MAX_HEADERS_PER_PAGE; ++i) {
			if (mem[i] != NULL) {
				free(mem[i]);
				mem[i] = NULL;
			}
		}
		free(mem);
		*storage = (value_t) NULL;
	}
	
	hattrie_iter_free(iter);
}

static inline void add_to_set (hattrie_t * headers_by_title,
                               const char * title,
                               const char * str,
                               size_t len) {
	value_t * storage;
	size_t title_len;
	char * * headers;
	
	title_len = strlen(title);
	storage = hattrie_tryget(headers_by_title, title, title_len);
	
	if (storage == NULL) {
		headers = add_trie_mem(headers_by_title,
		                       title,
		                       title_len,
		                       sizeof *headers * MAX_HEADERS_PER_PAGE);
		                       
		for (int i = 0; i < MAX_HEADERS_PER_PAGE; ++i)
			headers[i] = NULL;
	} else
		headers = *(char * * *) storage;
		
	for (int i = 0; i < MAX_HEADERS_PER_PAGE; ++i) {
		// Don't add header if it's already there.
		if (headers[i] == NULL) {
			char * header = strndup(str, len);
			
			if (header == NULL)
				MALLOC_FAIL;
			
			headers[i] = header;
			
			return;
		} else if (memcmp(str, headers[i], len) == 0 && headers[i][len] == '\0')
			return;
	}
	
	EPRINTF("more than %d headers in '%s'\n",
	               MAX_HEADERS_PER_PAGE,
	               title);
}

static inline void find_headers (hattrie_t * headers_by_title,
                                 hattrie_t * headers_to_ignore,
								 const char * title,
                                 const char * buffer,
                                 size_t len) {
	const char * str = buffer;
	const char * const end = buffer + len;
	
	while (str < end) {
		if (str[0] == '=') {
			const char * line_start = str;
			size_t line_len = get_line_len(line_start, end);
			size_t header_len;
			const char * header = get_header(line_start,
			                                 line_len,
			                                 &header_len,
			                                 NULL);
			                                 
			if (header != NULL && header_len > 0
					&& hattrie_tryget(headers_to_ignore, header, header_len) == NULL)
				add_to_set(headers_by_title, title, header, header_len);
				
			str += line_len;
		} else // Skip line (if any) and one newline.
			while (str < end && *str++ != '\n');
	}
}

static bool handle_page (parse_info * info) {
	additional_parse_data * data = info->additional_data;
	hattrie_t * headers_by_title = data->trie;
	hattrie_t * headers_to_ignore = data->headers_to_ignore;
	static uint32_t page_count = 0;
	
	// Skip user talk page moved into mainspace.
	if (memcmp(info->page.title, "User-talk:", sizeof "User-talk:" - 1) == 0) {
		EPRINTF("Skipped '%s'\n", info->page.title);
		return true;
	}
	
	if (++page_count > data->pages_to_process)
		return false;
		
	buffer_t * buffer = &info->page.content;
	find_headers(headers_by_title,
	             headers_to_ignore,
				 info->page.title,
	             buffer_string(buffer),
	             buffer_length(buffer));
	             
	return true;
}

static inline void print_header_counts (const header_count_t * const header_counts) {
	bool first = true;
	
	for (header_level_t header_level = 1;
	        header_level <= MAX_HEADER_LEVEL;
	        ++header_level) {
		const header_count_t count = header_counts[header_level - 1];
		
		if (count > 0) {
			if (!first)
				putchar(';');
				
			printf("%hd:%d", header_level, count);
			
			first = false;
		}
	}
}

static inline void print_header_info (const char * const title,
                                      const size_t title_len,
                                      const char * * const headers) {
	printf("%.*s\t", (int) title_len, title);
	for (int i = 0; i < MAX_HEADERS_PER_PAGE && headers[i] != NULL; ++i) {
		if (i > 0)
			putchar('\t');
		printf("%s", headers[i]);
	}
	putchar('\n');
}

static inline void print_headers_by_title (hattrie_t * trie) {
	hattrie_iter_t * iter;
	
	for (iter = hattrie_iter_begin(trie, true);
	        !hattrie_iter_finished(iter);
	        hattrie_iter_next(iter)) {
		size_t title_len;
		const char * title = hattrie_iter_key(iter, &title_len);
		value_t * value = hattrie_iter_val(iter);
		
		if (value == NULL)
			CRASH_WITH_MSG("!!!\n");
			
		const char * * headers = *(const char * * *) value;
		print_header_info(title, title_len, headers);
	}
	
	hattrie_iter_free(iter);
}

static inline void process_pages (page_count_t pages_to_process,
                                  hattrie_t * headers_to_ignore,
                                  Wiktionary_namespace_t * namespaces) {
	hattrie_t * headers_by_title = hattrie_create();
	additional_parse_data data;
	
	data.trie = headers_by_title;
	data.pages_to_process = pages_to_process;
	data.headers_to_ignore = headers_to_ignore;
	
	do_parsing(handle_page, namespaces, &data);
	
	size_t title_count = hattrie_size(headers_by_title);
	EPRINTF("gathered lists of headers for %zu page%s\n",
	        title_count, title_count == 1 ? "" : "s");
	        
	print_headers_by_title(headers_by_title);
	
	free_all_trie_mem(headers_by_title);
	hattrie_free(headers_by_title);
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
	
	if (header_list == NULL)
		perror("could not open file"), exit(-1);
		
	char buf[BUFSIZ];
	size_t line_number = 0;
	
	while (fgets(buf, sizeof buf, header_list) != NULL) {
		++line_number;
		const char * newline = strchr(buf, '\n');
		
		if (newline == NULL && !feof(header_list)) {
			char byte_count_buf[BYTE_COUNT_BUF_SIZE];
			format_byte_count(sizeof buf, byte_count_buf, sizeof byte_count_buf);
			CRASH_WITH_MSG("line %zu in '%s' is too long for buffer "
			               "(%s)!\n",
			               line_number, filename, byte_count_buf);
		}
		
		size_t len = newline != NULL
		             ? (size_t) (newline - buf)
		             : sizeof buf - 1;
		             
		if (len > 0)
			hattrie_get(headers, buf, len);
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
		
	if (memcmp(arg, "all", 4) == 0) // Search all namespaces.
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

#define MAX_PAGES_TO_PROCESS UINT32_MAX

static inline void process_args (option_t * options, int argc, char * * argv) {
	command_t commands;
	command_init(&commands, argv[0], "0");
	commands.data = options;
	
	char command_desc[128]; // Use this in only one command.
	snprintf(command_desc,
	         sizeof command_desc,
	         "number of pages in the given namespaces to process "
	         "(default: " PAGE_COUNT_PRINT_FORMAT ")",
	         MAX_PAGES_TO_PROCESS);
	command_option(&commands, "-p", "--pages <count>",
	               command_desc,
	               get_pages_to_process);
	               
	command_option(&commands, "-x", "--exclude-headers <file>",
	               "file containing newline-separated headers "
	               "that should not be tracked",
	               add_headers);
	               
	command_option(&commands, "-n", "--namespaces <numbers>",
	               "list of namespace numbers",
	               add_namespaces);
	               
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
}

int main (int argc, char * * argv) {
	option_t options;
	hattrie_t * headers_to_ignore = hattrie_create();
	Wiktionary_namespace_t namespaces[MAX_NAMESPACES];
	
	if (headers_to_ignore == NULL)
		CRASH_WITH_MSG("failed to create trie\n");
		
	options.pages_to_process = 0;
	options.headers = headers_to_ignore;
	options.namespaces = namespaces;
	namespaces[0] = NAMESPACE_NONE; // sentinel!
	
	process_args(&options, argc, argv);
	
	process_pages(options.pages_to_process,
	              options.headers,
	              options.namespaces);
	              
	hattrie_free(headers_to_ignore);
	
	return 0;
}