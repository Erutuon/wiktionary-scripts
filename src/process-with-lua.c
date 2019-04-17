#include <stdio.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "utils/parser.h"
#include "utils/string_slice.h"
#include "utils/buffer.h"
#include "commander.h"

typedef struct {
	FILE * input_file;
	Wiktionary_namespace_t * namespaces;
} option_t;

static void print_Lua_error(lua_State * L, const char * msg) {
	EPRINTF("%s", msg);
	
	if (lua_type(L, -1) == LUA_TSTRING) {
		const char * err = lua_tostring(L, -1);
		EPRINTF(": %s", err);
	}
	
	EPRINTF("\n");
}

static bool process_page (parse_info * info) {
	lua_State * L = info->additional_data;
	const page_info page = info->page;
	const buffer_t content = page.content;
	bool res;
	const char * namespace_name = get_namespace_name(page.namespace);
	
	lua_pushvalue(L, -1); // push function
	lua_pushlstring(L, content.str, content.len);
	lua_pushstring(L, page.title);
	lua_pushstring(L, namespace_name);
	
	if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
		print_Lua_error(L, "error while calling function");
		exit(EXIT_FAILURE);
	}
	
	if (lua_type(L, -1) != LUA_TBOOLEAN)
		CRASH_WITH_MSG("function did not return a boolean\n");
		
	// whether to continue processing pages
	res = lua_toboolean(L, -1);
	
	lua_pop(L, 1);
	
	return res;
}

#define CHECK_FILE(file) \
	if ((file) == NULL) \
		perror("could not open file"), exit(-1)

static inline void get_input_file (command_t * commands) {
	option_t * options = commands->data;
	FILE * input_file = fopen(commands->arg, "rb");
	CHECK_FILE(input_file);
	options->input_file = input_file;
}

#define MAX_NAMESPACES 16

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
		
	while (sscanf(arg + total_characters_read,
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

static inline char * process_args (option_t * options,
                                   int argc,
                                   char * * argv) {
	command_t commands;
	command_init(&commands, argv[0], "0");
	commands.data = options;
	char * script_file;
	
	command_option(&commands, "-i", "--input <file>",
	               "XML page dump file",
	               get_input_file);
	               
	command_option(&commands, "-n", "--namespaces <numbers>",
	               "list of namespace numbers",
	               add_namespaces);
	               
	command_parse(&commands, argc, argv);
	
	if (commands.argc == 1)
		script_file = strdup(commands.argv[0]);
	else
		CRASH_WITH_MSG("expected 1 script file, got %d\n", commands.argc);
	
	if (options->input_file == NULL)
		options->input_file = stdin;
		
	if (options->namespaces[0] == NAMESPACE_NONE) {
		static Wiktionary_namespace_t default_namespaces[] = {
			NAMESPACE_MAIN,
			NAMESPACE_RECONSTRUCTION,
			NAMESPACE_APPENDIX,
			NAMESPACE_NONE
		};
		options->namespaces = default_namespaces;
	}
	
	EPRINTF("searching in namespaces ");
	
	for (int i = 0; options->namespaces[i] != NAMESPACE_NONE; ++i) {
		if (i > 0)
			EPRINTF(", ");
			
		const char * name = get_namespace_name(options->namespaces[i]);
		EPRINTF("%s", strlen(name) == 0 ?  "main" : name);
	}
	
	putc('\n', stderr);
	
	command_free(&commands);
	
	return script_file;
}

int main (int argc, const char * * argv) {
	option_t options = {0};
	Wiktionary_namespace_t namespaces[MAX_NAMESPACES] = { NAMESPACE_NONE };
	lua_State * L;
	char * script_file;
	
	options.namespaces = namespaces;
	
	script_file = process_args(&options, argc, (char * *) argv);
	
	L = luaL_newstate();
	luaL_openlibs(L);
	
	if (luaL_dofile(L, script_file) != LUA_OK) {
		print_Lua_error(L, "error while running script");
		exit(EXIT_FAILURE);
	}
	
	if (lua_type(L, -1) != LUA_TFUNCTION)
		CRASH_WITH_MSG("script did not return a function");
		
	parse_Wiktionary_page_dump(options.input_file,
	                           process_page,
	                           options.namespaces,
	                           L);
	                           
	lua_close(L);
	
	free(script_file);
	
	return 0;
}