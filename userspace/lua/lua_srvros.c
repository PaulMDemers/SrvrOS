#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "linenoise.h"

long long lua_srvros_ipow(long long base, long long exponent) {
    if (exponent < 0) {
        return 0;
    }
    long long result = 1;
    while (exponent > 0) {
        if ((exponent & 1) != 0) {
            result *= base;
        }
        exponent >>= 1;
        if (exponent != 0) {
            base *= base;
        }
    }
    return result;
}

static void open_lib(lua_State *L, const char *name, lua_CFunction openf) {
    luaL_requiref(L, name, openf, 1);
    lua_pop(L, 1);
}

static void open_srvros_libs(lua_State *L) {
    open_lib(L, LUA_GNAME, luaopen_base);
    open_lib(L, LUA_COLIBNAME, luaopen_coroutine);
    open_lib(L, LUA_TABLIBNAME, luaopen_table);
    open_lib(L, LUA_IOLIBNAME, luaopen_io);
    open_lib(L, LUA_LOADLIBNAME, luaopen_package);
    open_lib(L, LUA_MATHLIBNAME, luaopen_math);
    open_lib(L, LUA_STRLIBNAME, luaopen_string);
    open_lib(L, LUA_UTF8LIBNAME, luaopen_utf8);
    open_lib(L, LUA_DBLIBNAME, luaopen_debug);

    lua_getglobal(L, "package");
    if (lua_istable(L, -1)) {
        lua_pushstring(L,
            "/fat/?.lua;/fat/?/init.lua;"
            "/fat/lib/lua/5.4/?.lua;/fat/lib/lua/5.4/?/init.lua");
        lua_setfield(L, -2, "path");
        lua_pushliteral(L, "");
        lua_setfield(L, -2, "cpath");
    }
    lua_pop(L, 1);
}

static int run_chunk(lua_State *L, int status) {
    if (status != LUA_OK) {
        printf("lua: load error: %s\n", lua_tostring(L, -1));
        return 1;
    }
    status = lua_pcall(L, 0, LUA_MULTRET, 0);
    if (status != LUA_OK) {
        printf("lua: runtime error: %s\n", lua_tostring(L, -1));
        return 1;
    }
    return 0;
}

static int run_repl_line(lua_State *L, const char *line) {
    int status = luaL_loadstring(L, line);
    if (status != LUA_OK) {
        char expression[640];
        snprintf(expression, sizeof(expression), "return %s", line);
        lua_pop(L, 1);
        status = luaL_loadstring(L, expression);
    }
    return run_chunk(L, status);
}

static int run_repl(lua_State *L) {
    printf("Lua %s srvros interactive profile\n", LUA_VERSION_RELEASE);
    int result = 0;
    if (!isatty(STDIN_FILENO)) {
        char line[512];
        while (fgets(line, sizeof(line), stdin) != 0) {
            result |= run_repl_line(L, line);
        }
        return result != 0 ? 1 : 0;
    }

    linenoiseHistorySetMaxLen(64);
    linenoiseHistoryLoad("/fat/lua.history");
    while (1) {
        char *line = linenoise("> ");
        if (line == 0) {
            break;
        }
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0 || strcmp(line, "os.exit()") == 0) {
            linenoiseFree(line);
            break;
        }
        if (line[0] != '\0') {
            linenoiseHistoryAdd(line);
            result |= run_repl_line(L, line);
        }
        linenoiseFree(line);
    }
    linenoiseHistorySave("/fat/lua.history");
    return result != 0 ? 1 : 0;
}

int main(int argc, char **argv) {
    lua_State *L = luaL_newstate();
    if (L == 0) {
        printf("lua: cannot create state\n");
        return 1;
    }
    open_srvros_libs(L);

    int result;
    if (argc >= 3 && strcmp(argv[1], "-e") == 0) {
        result = run_chunk(L, luaL_loadstring(L, argv[2]));
    } else if (argc >= 2) {
        result = run_chunk(L, luaL_loadfilex(L, argv[1], 0));
    } else {
        result = run_repl(L);
    }

    lua_close(L);
    return result;
}
