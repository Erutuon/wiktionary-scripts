#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <expat.h>

#include "buffer.h"
#include "parser.h"

#define PRINT_STR(var) EPRINTF(#var ": %s\n", var)
#define CRASH exit(EXIT_FAILURE)

#define STRING_EQUALS(str1, str2) (strcmp((str1), (str2)) == 0)
#define ARRAY_SIZE(arr) (sizeof (arr) / sizeof (arr)[0])

// pages are pretty long!
#define PAGE_CONTENT_BUFFER_SIZE 1024 * 16

static const char * MediaWiki_page_tags[] = {
	"",
	"base",
	"case",
	"comment",
	"contributor",
	"dbname",
	"format",
	"generator",
	"id",
	"ip",
	"mediawiki",
	"minor",
	"model",
	"namespace",
	"namespaces",
	"ns",
	"page",
	"parentid",
	"redirect",
	"restrictions",
	"revision",
	"sha1",
	"siteinfo",
	"sitename",
	"text",
	"timestamp",
	"title",
	"username"
};

#define CUR_TAG(info) ((info)->tag_stack[(info)->level - 1])
#define ENCLOSING_TAG(info)  ((info)->tag_stack[(info)->level - 2])

void format_byte_count (size_t bytes, char * buf, size_t buf_len) {
	static const char * units[] = { "B", "KiB", "MiB", "GiB" };
	double accum = bytes;
	unsigned int magnitude = 0,
	             printed;
	             
	while (accum / 1024 >= 1 && magnitude + 1 < ARRAY_SIZE(units))
		accum = accum / 1024, ++magnitude;
		
	printed = snprintf(buf, buf_len, "%.1f %s", accum, units[magnitude]);
	
	if (printed >= buf_len)
		EPRINTF("buffer not long enough\n");
}

// name and namespace buffers must be zeroed at beginning of each new page tag
// so that copy_to_buf works properly.
static inline void reset_page (page_info * page) {
	page->id = 0;
	page->namespace = NAMESPACE_NONE;
	page->title[0] = '\0';
	page->redirect_target[0] = '\0';
	buffer_empty(&page->content);
}

static inline void push_tag(parse_info * info, MediaWiki_page_tag_t tag) {
	if (info->level + 1 == TAG_DEPTH)
		CRASH_WITH_MSG("tag depth too great\n");
		
	info->tag_stack[info->level++] = tag;
}

static inline void pop_tag(parse_info * info) {
	info->tag_stack[--info->level] = TAG_UNIDENTIFIED;
}

static inline void print_atts (const XML_Char ** atts) {
	int i = 0;
	
	while (atts[i]) {
		if (i > 0)
			printf(i % 2 == 0 ? "; " : ", ");
			
		printf("%s", atts[i]);
		++i;
	}
	
	putchar('\n');
}

static inline MediaWiki_page_tag_t get_tag_code (const XML_Char * name) {
	// Start at 1 to skip "".
	// The tags are in alphabetical order.
	for (int i = 1; i < (int) ARRAY_SIZE(MediaWiki_page_tags); ++i) {
		int cmp = strcmp(MediaWiki_page_tags[i], name);
		
		if (cmp == 0)
			return (MediaWiki_page_tag_t) i;
		else if (cmp > 0)
			break;
	}
	
	return TAG_UNIDENTIFIED;
}

static inline void copy_to_buf (char * buf, size_t max_len, const XML_Char * val, int len) {
	if (strlen(buf) + len > max_len)
		CRASH_WITH_MSG("not enough space");
		
	if (buf[0] == '\0') {
		memcpy(buf, val, len);
		buf[len] = '\0';
	} else
		strncat(buf, val, len);
}

static inline bool namespaces_includes (Wiktionary_namespace_t * namespaces,
                                        Wiktionary_namespace_t namespace) {
	if (namespaces == NULL || namespaces[0] == NAMESPACE_NONE)
		return true;
		
	for (int i = 0; namespaces[i] != NAMESPACE_NONE; ++i) {
		if (namespace == namespaces[i])
			return true;
	}
	
	return false;
}

static void XMLCALL tag_start_handler (void * userdatum, const XML_Char * name, const XML_Char ** atts) {
	parse_info * info = userdatum;
	
	push_tag(info, get_tag_code(name));
	
	switch (CUR_TAG(info)) {
		case TAG_PAGE: {
			reset_page(&info->page);
			++info->count;
			info->skip_page = false;
			break;
		}
		
		case TAG_ID:
			if (ENCLOSING_TAG(info) == TAG_PAGE)
				info->copy = true; // copy id of page
				
			info->number_buffer[0] = '\0';
			
			break;
			
		case TAG_NS:
			info->number_buffer[0] = '\0'; // fallthrough
			
		case TAG_REDIRECT:
		case TAG_TEXT:
		case TAG_TITLE:
			info->copy = !info->skip_page;
			break; // need to copy title, text, redirect target, and namespace
			
		case TAG_UNIDENTIFIED:
			EPRINTF("tag %s has no enum value\n", name);
			break;
			
		default:
			(void) 0;
	}
	
	/*
	// Print attributes
	if (atts[0])
		print_atts(atts);
	*/
}

static void XMLCALL tag_data_handler (void * userdatum, const XML_Char * s, int len) {
	parse_info * info = userdatum;
	
	if (!info->copy)
		return;
		
	switch (CUR_TAG(info)) {
		case TAG_TEXT:
			if (!info->skip_page
			        && !buffer_append(&info->page.content, s, len))
				CRASH_WITH_MSG("failed to append string");
				
			break;
			
		case TAG_ID:
		case TAG_NS:
		case TAG_REDIRECT:
		case TAG_TITLE: {
			char * buffer = CUR_TAG(info) == TAG_REDIRECT
			                ? info->page.redirect_target
			                : CUR_TAG(info) == TAG_TITLE
			                ? info->page.title
			                : info->number_buffer;
			size_t max_len = CUR_TAG(info) == TAG_REDIRECT || CUR_TAG(info) == TAG_TITLE
			                 ? PAGE_NAME_LEN
			                 : NUMBER_BUFFER_LEN;
			copy_to_buf(buffer, max_len, s, len);
			break;
		}
		
		default:
			(void) 0;
	}
}

static void XMLCALL tag_end_handler (void * userdatum, const XML_Char * name) {
	parse_info * info = userdatum;
	
	switch (CUR_TAG(info)) {
		case TAG_ID:
			if (info->copy) {
				if (sscanf(info->number_buffer, "%u", &info->page.id) != 1)
					CRASH_WITH_MSG("scanf error in '%s' at %p\n",
					               info->number_buffer, info->number_buffer);
			}
			
			break;
			
		case TAG_NS:
			if (sscanf(info->number_buffer, "%d", &info->page.namespace) != 1)
				CRASH_WITH_MSG("scanf error in '%s' at %p\n",
				               info->number_buffer,
				               info->number_buffer);
				               
			info->skip_page = !namespaces_includes(info->namespaces,
			                                       info->page.namespace);
			                                       
			break;
			
		case TAG_PAGE:
			if (!info->skip_page) {
				const size_t page_len = buffer_length(&info->page.content);
				
				if (page_len > info->longest_page)
					info->longest_page = page_len;
					
				info->continue_parsing = info->handle_page(info);
			}
			
			break;
			
		default:
			(void) 0;
	}
	
	info->copy = false;
	
	pop_tag(info);
}

// If namespaces is NULL, all namespaces are searched.
static inline void info_init (parse_info * info,
                              page_callback handle_page,
                              Wiktionary_namespace_t * namespaces) {
	info->copy = false;
	info->continue_parsing = true;
	info->skip_page = false;
	info->count = 0;
	info->level = 0;
	info->longest_page = 0;
	
	if (handle_page == NULL)
		CRASH_WITH_MSG("Page-handling function is NULL!");
	else
		info->handle_page = handle_page;
		
	info->namespaces = namespaces;
	info->tag_stack[info->level] = TAG_UNIDENTIFIED;
	
	// rest of page initialization in `reset_page`
	if (!buffer_init(&info->page.content, PAGE_CONTENT_BUFFER_SIZE))
		perror("failed to initialize buffer"), exit(-1);
		
}

static inline void info_free (parse_info * info) {
	buffer_free(&info->page.content);
}

static inline void print_parser_info(XML_Parser parser,
                                     parse_info * info,
                                     time_t start_time,
                                     time_t end_time) {
	char byte_count_buf[BYTE_COUNT_BUF_SIZE];
	
	format_byte_count(XML_GetCurrentByteIndex(parser), byte_count_buf, sizeof byte_count_buf);
	EPRINTF("bytes read: %s; pages read: %d; ", byte_count_buf, info->count);
	
	format_byte_count(info->longest_page, byte_count_buf, sizeof byte_count_buf);
	EPRINTF("longest page: %s; ", byte_count_buf);
	
	format_byte_count(buffer_size(&info->page.content), byte_count_buf, sizeof byte_count_buf);
	EPRINTF("buffer size: %s; time: %f seconds\n",
	        byte_count_buf, ((double) end_time - start_time) / CLOCKS_PER_SEC);
}

void parse_Wiktionary_page_dump (FILE * XML_file,
                                 page_callback handle_page,
                                 Wiktionary_namespace_t * namespaces,
                                 void * data) {
	if (sizeof (XML_Char) != sizeof (char))
		CRASH_WITH_MSG("This function assumes XML_Char (%zu bytes) is the same size as char (%zu bytes).",
		               sizeof (XML_Char), sizeof (char));
		               
	time_t start_time, end_time;
	parse_info info;
	XML_Parser parser;
	XML_Char buf[BUFSIZ];
	
	start_time = clock();
	parser = XML_ParserCreate("UTF-8");
	
	if (!parser)
		CRASH_WITH_MSG("failed to create parser");
		
	info_init(&info, handle_page, namespaces);
	info.additional_data = data;
	
	XML_SetUserData(parser, &info);
	XML_SetElementHandler(parser, tag_start_handler, tag_end_handler);
	XML_SetCharacterDataHandler(parser, tag_data_handler);
	
	while (true) {
		size_t len = fread(buf, 1, BUFSIZ, XML_file);
		bool done = false;
		
		if (ferror(XML_file))
			perror("read error"), CRASH;
			
		done = feof(XML_file);
		
		if (XML_Parse(parser, buf, len, done) == XML_STATUS_ERROR)
			CRASH_WITH_MSG("Parse error at line %lu:\n%s\n",
			               XML_GetCurrentLineNumber(parser),
			               XML_ErrorString(XML_GetErrorCode(parser)));
			               
		if (done || !info.continue_parsing)
			break;
	}
	
	end_time = clock();
	print_parser_info(parser, &info, start_time, end_time);
	
	info_free(&info);
	
	XML_ParserFree(parser);
}
