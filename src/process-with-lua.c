#include <stdio.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "utils/parser.h"
#include "utils/string_slice.h"
#include "utils/buffer.h"

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
	
	lua_pushvalue(L, -1); // push function
	lua_pushlstring(L, content.str, content.len);
	lua_pushstring(L, page.title);
	
	if (lua_pcall(L, 2, 1, 0) != LUA_OK) {
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

int main (int argc, const char * * argv) {
	Wiktionary_namespace_t namespaces[] = { NAMESPACE_MAIN, NAMESPACE_NONE };
	lua_State * L;
	
	if (argc < 2)
		CRASH_WITH_MSG("not enough arguments\n");
	
	L = luaL_newstate();
	luaL_openlibs(L);
	
	if (luaL_dofile(L, argv[1]) != LUA_OK) {
		print_Lua_error(L, "error while running script");
		exit(EXIT_FAILURE);
	}
	
	if (lua_type(L, -1) != LUA_TFUNCTION)
		CRASH_WITH_MSG("script did not return a function");
	
	parse_Wiktionary_page_dump(stdin, process_page, namespaces, L);
	
	lua_close(L);
	
	return 0;
}