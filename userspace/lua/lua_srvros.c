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
    open_lib(L, LUA_OSLIBNAME, luaopen_os);
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

static int report_chunk_error(lua_State *L, const char *phase) {
    const char *message = lua_type(L, -1) == LUA_TSTRING ? lua_tostring(L, -1) : NULL;
    fprintf(stderr, "lua: %s error: %s\n", phase,
        message ? message : "non-string error object");
    lua_pop(L, 1);
    return 1;
}

static int run_chunk(lua_State *L, int status) {
    if (status != LUA_OK) {
        return report_chunk_error(L, "load");
    }
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        return report_chunk_error(L, "runtime");
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
    printf("%s (SrvrOS interactive profile)\n", LUA_RELEASE);
    int result = 0;
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
        fputs("lua: cannot create state\n", stderr);
        return 1;
    }
    open_srvros_libs(L);
    int script=argc>=2 && strcmp(argv[1],"-e")?1:0;
    lua_createtable(L,argc,script);
    for(int i=0;i<argc;i++) {
        lua_pushstring(L,argv[i]);lua_rawseti(L,-2,i-script);
    }
    lua_setglobal(L,"arg");

    int result;
    if (argc >= 3 && strcmp(argv[1], "-e") == 0) {
        result = run_chunk(L, luaL_loadstring(L, argv[2]));
    } else if (argc >= 2) {
        result = run_chunk(L, luaL_loadfilex(L, argv[1], NULL));
    } else if (isatty(STDIN_FILENO)) {
        result = run_repl(L);
    } else {
        result = run_chunk(L, luaL_loadfilex(L, NULL, NULL));
    }

    lua_close(L);
    int output_failed = ferror(stdout);
    if (fclose(stdout) == EOF || output_failed) {
        fputs("lua: standard output error\n", stderr);
        return 1;
    }
    return result;
}
