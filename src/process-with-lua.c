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
		print_Lua_error(L, "error while calling function\n");
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
		CRASH_WITH_MSG("cannot supply namespaces twice\n");
		
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

static void process_args (command_t * commands,
                                   option_t * options,
								   int argc,
                                   char * * argv) {
	command_init(commands, argv[0], "0");
	commands->data = options;
	
	command_option(commands, "-i", "--input <file>",
	               "XML page dump file",
	               get_input_file);
	               
	command_option(commands, "-n", "--namespaces <numbers>",
	               "list of namespace numbers",
	               add_namespaces);
	               
	command_parse(commands, argc, argv);
	
	if (commands->argc < 1)
		CRASH_WITH_MSG("expected a script file\n");
	
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
}

int main (int argc, const char * * argv) {
	option_t options = {0};
	Wiktionary_namespace_t namespaces[MAX_NAMESPACES] = { NAMESPACE_NONE };
	command_t commands;
	lua_State * L;
	
	options.namespaces = namespaces;
	
	process_args(&commands, &options, argc, (char * *) argv);
	
	L = luaL_newstate();
	luaL_openlibs(L);
	
	if (luaL_loadfile(L, commands.argv[0]) != LUA_OK) {
		print_Lua_error(L, "error while loading script");
		exit(EXIT_FAILURE);
	}
	
	for (int i = 1; i < commands.argc; ++i) {
		lua_pushstring(L, commands.argv[i]);
	}
	
	command_free(&commands);
	
	if (lua_pcall(L, commands.argc - 1, 1, 0) != LUA_OK) {
		print_Lua_error(L, "error while running script");
		exit(EXIT_FAILURE);
	}
	
	if (lua_type(L, -1) != LUA_TFUNCTION)
		CRASH_WITH_MSG("script did not return a function\n");
		
	parse_Wiktionary_page_dump(options.input_file,
	                           process_page,
	                           options.namespaces,
	                           L);
	                           
	lua_close(L);
	
	return 0;
}