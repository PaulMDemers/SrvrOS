#!/usr/bin/env python3
import os
import shutil
import sys


LUA_FILES = [
    "lapi.c", "lapi.h",
    "lauxlib.c", "lauxlib.h",
    "lbaselib.c",
    "lcode.c", "lcode.h",
    "lcorolib.c",
    "lctype.c", "lctype.h",
    "ldblib.c",
    "ldebug.c", "ldebug.h",
    "ldo.c", "ldo.h",
    "ldump.c",
    "lfunc.c", "lfunc.h",
    "lgc.c", "lgc.h",
    "liolib.c",
    "ljumptab.h",
    "llex.c", "llex.h",
    "loadlib.c",
    "llimits.h",
    "lmathlib.c",
    "lmem.c", "lmem.h",
    "lobject.c", "lobject.h",
    "lopcodes.c", "lopcodes.h", "lopnames.h",
    "lparser.c", "lparser.h",
    "lprefix.h",
    "lstate.c", "lstate.h",
    "lstring.c", "lstring.h",
    "lstrlib.c",
    "ltable.c", "ltable.h",
    "ltablib.c",
    "ltm.c", "ltm.h",
    "lua.h",
    "luaconf.h",
    "lualib.h",
    "lundump.c", "lundump.h",
    "lutf8lib.c",
    "lvm.c", "lvm.h",
    "lzio.c", "lzio.h",
]


def patch_luaconf(text):
    text = text.replace(
        "#define LUA_FLOAT_LONGDOUBLE\t3\n",
        "#define LUA_FLOAT_LONGDOUBLE\t3\n#define LUA_FLOAT_SRVROS_INT\t4\n",
    )
    text = text.replace(
        "#define l_floor(x)\t\t(l_mathop(floor)(x))\n",
        "#if LUA_FLOAT_TYPE != LUA_FLOAT_SRVROS_INT\n"
        "#define l_floor(x)\t\t(l_mathop(floor)(x))\n"
        "#endif\n",
    )
    text = text.replace(
        "#define lua_numbertointeger(n,p) \\\n"
        "  ((n) >= (LUA_NUMBER)(LUA_MININTEGER) && \\\n"
        "   (n) < -(LUA_NUMBER)(LUA_MININTEGER) && \\\n"
        "      (*(p) = (LUA_INTEGER)(n), 1))\n",
        "#if LUA_FLOAT_TYPE == LUA_FLOAT_SRVROS_INT\n"
        "#define lua_numbertointeger(n,p) (*(p) = (LUA_INTEGER)(n), 1)\n"
        "#else\n"
        "#define lua_numbertointeger(n,p) \\\n"
        "  ((n) >= (LUA_NUMBER)(LUA_MININTEGER) && \\\n"
        "   (n) < -(LUA_NUMBER)(LUA_MININTEGER) && \\\n"
        "      (*(p) = (LUA_INTEGER)(n), 1))\n"
        "#endif\n",
    )
    text = text.replace(
        "#if LUA_FLOAT_TYPE == LUA_FLOAT_FLOAT\t\t/* { single float */\n",
        "#if LUA_FLOAT_TYPE == LUA_FLOAT_SRVROS_INT\n\n"
        "#define LUA_NUMBER\tlong long\n"
        "extern long long lua_srvros_ipow(long long base, long long exponent);\n"
        "#define SRVROS_LUA_MANT_DIG 63\n"
        "#define SRVROS_LUA_MAX_10_EXP 18\n"
        "#define l_floatatt(n)\t\t(SRVROS_LUA_##n)\n"
        "#define LUAI_UACNUMBER\tlong long\n"
        "#define LUA_NUMBER_FRMLEN\t\"ll\"\n"
        "#define LUA_NUMBER_FMT\t\t\"%lld\"\n"
        "#define l_mathop(op)\t\top\n"
        "#define l_floor(x)\t\t(x)\n"
        "#define lua_str2number(s,p)\tstrtoll((s), (p), 0)\n"
        "#define lua_strx2number(s,p)\tlua_str2number((s), (p))\n"
        "#define lua_number2strx(L,b,sz,f,n) \\\n"
        "\t((void)L, (void)f, l_sprintf(b,sz,LUA_NUMBER_FMT,(LUAI_UACNUMBER)(n)))\n"
        "#define lua_getlocaledecpoint()\t'.'\n"
        "#define luai_numdiv(L,a,b)\t((void)L, (a)/(b))\n"
        "#define luai_numidiv(L,a,b)\t((void)L, (a)/(b))\n"
        "#define luai_nummod(L,a,b,m)\t{ (void)L; (m) = (a) % (b); }\n"
        "#define luai_numpow(L,a,b)\t((void)L, lua_srvros_ipow((a), (b)))\n"
        "#define luai_numadd(L,a,b)\t((a)+(b))\n"
        "#define luai_numsub(L,a,b)\t((a)-(b))\n"
        "#define luai_nummul(L,a,b)\t((a)*(b))\n"
        "#define luai_numunm(L,a)\t(-(a))\n"
        "#define luai_numeq(a,b)\t((a)==(b))\n"
        "#define luai_numlt(a,b)\t((a)<(b))\n"
        "#define luai_numle(a,b)\t((a)<=(b))\n"
        "#define luai_numgt(a,b)\t((a)>(b))\n"
        "#define luai_numge(a,b)\t((a)>=(b))\n"
        "#define luai_numisnan(a)\t0\n\n"
        "#elif LUA_FLOAT_TYPE == LUA_FLOAT_FLOAT\t\t/* { single float */\n",
    )
    text = text.replace(
        "#if !defined(LUA_USE_C89)\n#define lua_strx2number(s,p)\t\tlua_str2number(s,p)\n#endif\n",
        "#if !defined(LUA_USE_C89) && LUA_FLOAT_TYPE != LUA_FLOAT_SRVROS_INT\n"
        "#define lua_strx2number(s,p)\t\tlua_str2number(s,p)\n"
        "#endif\n",
    )
    text = text.replace(
        "#if !defined(LUA_USE_C89)\n#define lua_number2strx(L,b,sz,f,n)  \\\n"
        "\t((void)L, l_sprintf(b,sz,f,(LUAI_UACNUMBER)(n)))\n#endif\n",
        "#if !defined(LUA_USE_C89) && LUA_FLOAT_TYPE != LUA_FLOAT_SRVROS_INT\n"
        "#define lua_number2strx(L,b,sz,f,n)  \\\n"
        "\t((void)L, l_sprintf(b,sz,f,(LUAI_UACNUMBER)(n)))\n"
        "#endif\n",
    )
    return text


def main():
    if len(sys.argv) != 3:
        print("usage: prepare_lua_port.py upstream_dir output_dir", file=sys.stderr)
        return 2
    upstream = sys.argv[1]
    output = sys.argv[2]
    os.makedirs(output, exist_ok=True)
    for name in LUA_FILES:
        source = os.path.join(upstream, name)
        target = os.path.join(output, name)
        shutil.copyfile(source, target)
    luaconf = os.path.join(output, "luaconf.h")
    with open(luaconf, "r", encoding="utf-8") as handle:
        text = handle.read()
    with open(luaconf, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(patch_luaconf(text))
    stamp = os.path.join(output, ".prepared")
    with open(stamp, "w", encoding="utf-8") as handle:
        handle.write("lua srvros port prepared\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
