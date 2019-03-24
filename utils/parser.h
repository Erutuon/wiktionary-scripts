#ifndef WIKTIONARY_PAGE_PARSER_H
#define WIKTIONARY_PAGE_PARSER_H

#define EPRINTF(...) fprintf(stderr, __VA_ARGS__)
#define CRASH_WITH_MSG(...) EPRINTF(__VA_ARGS__), exit(-1)

#define NAME_SIZE 16 // longest key 12?
#define PAGE_NAME_LEN 256 // https://www.mediawiki.org/wiki/Manual:Page_table#page_title
#define NUMBER_BUFFER_LEN 16 // maximum number of decimal digits ((1 << 31) - 1, 2147483647) plus one
#define TAG_DEPTH 8
#define BYTE_COUNT_BUF_SIZE 16

// Keep enum in sync with array in parser.c!
typedef enum {
	TAG_UNIDENTIFIED,
	TAG_BASE,
	TAG_CASE,
	TAG_COMMENT,
	TAG_CONTRIBUTOR,
	TAG_DBNAME,
	TAG_FORMAT,
	TAG_GENERATOR,
	TAG_ID,
	TAG_IP,
	TAG_MEDIAWIKI,
	TAG_MINOR,
	TAG_MODEL,
	TAG_NAMESPACE,
	TAG_NAMESPACES,
	TAG_NS,
	TAG_PAGE,
	TAG_PARENTID,
	TAG_REDIRECT,
	TAG_RESTRICTIONS,
	TAG_REVISION,
	TAG_SHA1,
	TAG_SITEINFO,
	TAG_SITENAME,
	TAG_TEXT,
	TAG_TIMESTAMP,
	TAG_TITLE,
	TAG_USERNAME
} MediaWiki_page_tag_t;

// https://www.mediawiki.org/wiki/Manual:Page_table#page_namespace
// https://en.wiktionary.org/w/api.php?action=query&meta=siteinfo&siprop=namespaces
/*
// JavaScript code:
const str = [];
for (const [num, name] of Object.entries(mw.config.get('wgFormattedNamespaces'))) {
	str.push("NAMESPACE_" + name.toUpperCase().replace(/ /g, "_") + " = " + num);
}
console.log(str.join(',\n'));

// then Lua code
local text = buffer:get_text()
local max_len = 0
for enum_value in text:gmatch 'NAMESPACE[_%u]+' do
		if #enum_value > max_len then
		max_len = #enum_value
	end
end
buffer:gsub('(NAMESPACE[_%u]+)%s*=%s*(%-?%d+)',
	function (enum, num)
		return enum .. (" "):rep(max_len - #enum) .. (" =%5d"):format(tonumber(num))
	end)
*/
typedef enum {
	NAMESPACE_NONE                   =   -3, // arbitrary value
	NAMESPACE_MEDIA                  =   -2,
	NAMESPACE_SPECIAL                =   -1,
	NAMESPACE_MAIN                   =    0,
	NAMESPACE_TALK                   =    1,
	NAMESPACE_USER                   =    2,
	NAMESPACE_USER_TALK              =    3,
	NAMESPACE_WIKTIONARY             =    4,
	NAMESPACE_WIKTIONARY_TALK        =    5,
	NAMESPACE_FILE                   =    6,
	NAMESPACE_FILE_TALK              =    7,
	NAMESPACE_MEDIAWIKI              =    8,
	NAMESPACE_MEDIAWIKI_TALK         =    9,
	NAMESPACE_TEMPLATE               =   10,
	NAMESPACE_TEMPLATE_TALK          =   11,
	NAMESPACE_HELP                   =   12,
	NAMESPACE_HELP_TALK              =   13,
	NAMESPACE_CATEGORY               =   14,
	NAMESPACE_CATEGORY_TALK          =   15,
	NAMESPACE_THREAD                 =   90,
	NAMESPACE_THREAD_TALK            =   91,
	NAMESPACE_SUMMARY                =   92,
	NAMESPACE_SUMMARY_TALK           =   93,
	NAMESPACE_APPENDIX               =  100,
	NAMESPACE_APPENDIX_TALK          =  101,
	NAMESPACE_CONCORDANCE            =  102,
	NAMESPACE_CONCORDANCE_TALK       =  103,
	NAMESPACE_INDEX                  =  104,
	NAMESPACE_INDEX_TALK             =  105,
	NAMESPACE_RHYMES                 =  106,
	NAMESPACE_RHYMES_TALK            =  107,
	NAMESPACE_TRANSWIKI              =  108,
	NAMESPACE_TRANSWIKI_TALK         =  109,
	NAMESPACE_THESAURUS              =  110,
	NAMESPACE_THESAURUS_TALK         =  111,
	NAMESPACE_CITATIONS              =  114,
	NAMESPACE_CITATIONS_TALK         =  115,
	NAMESPACE_SIGN_GLOSS             =  116,
	NAMESPACE_SIGN_GLOSS_TALK        =  117,
	NAMESPACE_RECONSTRUCTION         =  118,
	NAMESPACE_RECONSTRUCTION_TALK    =  119,
	NAMESPACE_MODULE                 =  828,
	NAMESPACE_MODULE_TALK            =  829,
	NAMESPACE_GADGET                 = 2300,
	NAMESPACE_GADGET_TALK            = 2301,
	NAMESPACE_GADGET_DEFINITION      = 2302,
	NAMESPACE_GADGET_DEFINITION_TALK = 2303,
} Wiktionary_namespace_t;

typedef struct {
	unsigned int id;
	Wiktionary_namespace_t namespace;
	buffer_t content;
	char title[PAGE_NAME_LEN];
	char redirect_target[PAGE_NAME_LEN];
} page_info;

typedef struct _parse_info parse_info;

typedef bool (*page_callback) (parse_info * page_content);

typedef struct _parse_info {
	bool copy;
	bool continue_parsing;
	bool skip_page;
	int level;
	unsigned int count; // page number
	size_t longest_page;
	void * additional_data;
	page_callback handle_page;
	Wiktionary_namespace_t * namespaces; // pointer to array terminated by NAMESPACE_NONE!
	MediaWiki_page_tag_t tag_stack[TAG_DEPTH];
	char number_buffer[NUMBER_BUFFER_LEN];
	page_info page;
} parse_info;

void do_parsing (FILE * XML_file,
                 page_callback handle_page,
				 Wiktionary_namespace_t * namespaces,
                 void * data);
				 
void format_byte_count (size_t bytes, char * buf, size_t buf_len);

#endif
