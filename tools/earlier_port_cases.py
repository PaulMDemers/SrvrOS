"""Inputs and host checks for the earlier CLI guest workflow."""

from hashlib import sha256
from io import BytesIO
from pathlib import Path
import subprocess
from zipfile import ZIP_DEFLATED, ZIP_STORED, ZipFile, ZipInfo



def lua_cases():
    guest = '/fat/earlier-cli'
    long_string = b'x' * 1025
    multiline = (
        b'local function sum(a, b)\n'
        b'    return a + b\n'
        b'end\n'
        b'local payload = "' + long_string + b'"\n'
        b'print("lua-stdin " .. sum(19, 23) .. " " .. #payload)\n'
    )
    files = {
        'lua-stdin.lua': multiline,
        'lua-script.lua': (
            b'local f = assert(io.open(arg[1], "wb"))\n'
            b'assert(f:write("lua-state\\n"))\n'
            b'assert(f:close())\n'
            b'print("lua-file " .. (21 + 21))\n'
        ),
        'lua-invalid.lua': b'local function missing_end()\n',
        'lua-runtime.lua': b'error({message = "table error"})\n',
        'lua-empty.lua': b'',
    }
    commands = []
    expected = {'lua-state.txt': b'lua-state\n'}
    nonempty = []

    def invoke(name, command, output, status=0):
        commands.append(f'{command} > {guest}/{name}.out 2> {guest}/{name}.err')
        commands.append(f'echo $? > {guest}/{name}.status')
        expected[f'{name}.out'] = output
        expected[f'{name}.status'] = f'{status}\n'.encode()
        if status:
            nonempty.append(f'{name}.err')
        else:
            expected[f'{name}.err'] = b''

    stdin_output = b'lua-stdin 42 1025\n'
    invoke('lua-check-file', f'lua {guest}/lua-script.lua {guest}/lua-state.txt', b'lua-file 42\n')
    invoke('lua-check-stdin', f'lua < {guest}/lua-stdin.lua', stdin_output)
    invoke('lua-check-pipe', f'cat {guest}/lua-stdin.lua | lua', stdin_output)
    invoke('lua-check-empty', f'lua < {guest}/lua-empty.lua', b'')
    invoke('lua-check-syntax', f'lua < {guest}/lua-invalid.lua', b'', 1)
    invoke('lua-check-runtime', f'lua {guest}/lua-runtime.lua', b'', 1)
    invoke('lua-check-missing', f'lua {guest}/lua-absent.lua', b'', 1)
    invoke('lua-check-recovery', "lua -e 'print(3 / 2)'", b'1.5\n')
    return files, commands, expected, nonempty


BYACC_GRAMMAR = br"""%{
#include <stdio.h>

int yylex(void);
void yyerror(const char *message);
static int result;
%}
%token NUMBER
%left '+'
%left '*'
%%
line:
    expression              { result = $1; }
    ;
expression:
    NUMBER
    | expression '+' expression { $$ = $1 + $3; }
    | expression '*' expression { $$ = $1 * $3; }
    | '(' expression ')'    { $$ = $2; }
    ;
%%
int yylex(void)
{
    int c;

    do {
        c = getchar();
    } while (c == ' ' || c == '\t');
    if (c >= '0' && c <= '9') {
        yylval = c - '0';
        return NUMBER;
    }
    return c == '\n' || c == EOF ? 0 : c;
}

void yyerror(const char *message)
{
    (void)message;
    fputs("parse rejected\n", stderr);
}

int main(void)
{
    if (yyparse() != 0)
        return 1;
    printf("%d\n", result);
    return 0;
}
"""

BYACC_FILES = {
    "byacc-expression.y": BYACC_GRAMMAR,
    "byacc-invalid.y": b"%%\nline: 'a' {\n",
}

# Use the same working directory-relative input and output names in the
# native reference, since those names can enter generated C even with -l.
BYACC_REFERENCE_CASES = (
    (
        ("-d", "-l", "-b", "byacc-file", "byacc-expression.y"),
        None,
        ("byacc-file.tab.c", "byacc-file.tab.h"),
    ),
    (
        ("-d", "-l", "-o", "byacc-stdin.c", "-"),
        "byacc-expression.y",
        ("byacc-stdin.c", "byacc-stdin.h"),
    ),
    (
        ("-d", "-l", "-o", "byacc-retry.c", "byacc-expression.y"),
        None,
        ("byacc-retry.c", "byacc-retry.h"),
    ),
)

BYACC_PARSER_CASES = (
    (b"2+3*4\n", 0, b"14\n", b""),
    (b"(2 + 3) * 4\n", 0, b"20\n", b""),
    (b"2+*4\n", 1, b"", b"parse rejected\n"),
)


def byacc_cases():
    command = "TMPDIR=/fat/earlier-cli /fat/bin/byacc"
    cases = (
        ("version", f"{command} -V", 0),
        ("file", f"{command} -d -l -b byacc-file byacc-expression.y", 0),
        ("stdin", f"cat byacc-expression.y | env {command} -d -l -o byacc-stdin.c -", 0),
        ("invalid", f"{command} -l -o byacc-bad.c byacc-invalid.y", 1),
        ("missing", f"{command} -l -o byacc-missing.c byacc-absent.y", 2),
        ("output", f"{command} -l -o byacc-absent/result.c byacc-expression.y", 2),
        ("retry", f"{command} -d -l -o byacc-retry.c byacc-expression.y", 0),
    )
    commands = ["cd /fat/earlier-cli"]
    expected = {}
    nonempty = []
    for name, invocation, status in cases:
        stem = f"byacc-{name}"
        commands.append(f"{invocation} > {stem}.out 2> {stem}.err")
        commands.append(f"echo $? > {stem}.status")
        expected[f"{stem}.status"] = f"{status}\n".encode("ascii")
        expected[f"{stem}.out"] = (
            b"/fat/bin/byacc - 2.0 20260126\n" if name == "version" else b""
        )
        if status:
            nonempty.append(f"{stem}.err")
        else:
            expected[f"{stem}.err"] = b""
    commands.append("cd /")
    return dict(BYACC_FILES), commands, expected, nonempty


ZIP_MEMBERS = {
    "zip-payload.bin": b"".join(sha256(i.to_bytes(4, "big")).digest() for i in range(3073)),
    "zip-empty.bin": b"",
    "zip-note.txt": b"SrvrOS ZIP interoperability\n",
}
ZIP_GUEST_ARCHIVES = {
    "zip-guest-deflate.zip": ZIP_DEFLATED,
    "zip-guest-stored.zip": ZIP_STORED,
}
ZIP_EXTRACTED = {
    f"{directory}/{name}": data
    for directory in ("zip-host-out", "zip-retry")
    for name, data in ZIP_MEMBERS.items()
}
ZIP_PRESERVED = {"zip-blocked/keep.txt": ZIP_MEMBERS["zip-note.txt"]}


def zip_fixture() -> bytes:
    target = BytesIO()
    with ZipFile(target, "w") as archive:
        for name, data in ZIP_MEMBERS.items():
            entry = ZipInfo(name, date_time=(2024, 1, 1, 0, 0, 0))
            mode = ZIP_STORED if name == "zip-empty.bin" else ZIP_DEFLATED
            archive.writestr(entry, data, compress_type=mode)
    return target.getvalue()


def zip_cases():
    files = {
        **ZIP_MEMBERS,
        "zip-host.zip": zip_fixture(),
        "zip-plain.txt": b"an ordinary text file\n",
    }
    commands = [
        "cd /fat/earlier-cli",
        "mkdir -p zip-host-out zip-retry zip-blocked/zip-payload.bin",
        "cp zip-note.txt zip-blocked/keep.txt",
    ]
    expected = {}
    nonempty = []
    inputs = " ".join(ZIP_MEMBERS)
    successful = (
        ("zip-list", "miniunz -l zip-host.zip"),
        ("zip-extract", "miniunz zip-host.zip -d zip-host-out"),
        ("zip-deflate", f"minizip -o zip-guest-deflate.zip {inputs}"),
        ("zip-stored", f"minizip -o -0 zip-guest-stored.zip {inputs}"),
    )
    refused = (
        ("zip-bad-archive", "miniunz -l zip-plain.txt"),
        ("zip-missing-archive", "miniunz -l zip-missing.zip"),
        ("zip-missing-input", "minizip -o zip-missing-source.zip zip-missing.bin"),
        ("zip-output-blocked", "miniunz -o zip-host.zip -d zip-blocked"),
    )
    for name, command in successful:
        commands.extend((f"{command} > {name}.log", f"echo $? > {name}.status"))
        expected[f"{name}.status"] = b"0\n"
        nonempty.append(f"{name}.log")
    for name, command in refused:
        commands.append(
            f"if {command} > {name}.log; then echo success > {name}.status; "
            f"else echo refused > {name}.status; fi"
        )
        expected[f"{name}.status"] = b"refused\n"
        nonempty.append(f"{name}.log")
    commands.extend((
        "miniunz zip-host.zip -d zip-retry > zip-retry.log",
        "echo $? > zip-retry.status",
        "cd /",
    ))
    expected["zip-retry.status"] = b"0\n"
    nonempty.extend(("zip-retry.log", *ZIP_GUEST_ARCHIVES))
    return files, commands, expected, tuple(nonempty)


def verify_zip_guest(read_file):
    """Read relative paths beneath the guest fixture and return evidence hashes."""
    hashes = {}
    listing = read_file("zip-list.log")
    for name in ZIP_MEMBERS:
        if name.encode() not in listing:
            raise RuntimeError(f"ZIP guest listing omitted {name}")

    for name, data in (ZIP_EXTRACTED | ZIP_PRESERVED).items():
        if read_file(name) != data:
            raise RuntimeError(f"ZIP guest readback differs for {name}")
        hashes[name] = sha256(data).hexdigest()

    for name, mode in ZIP_GUEST_ARCHIVES.items():
        data = read_file(name)
        with ZipFile(BytesIO(data)) as archive:
            if archive.namelist() != list(ZIP_MEMBERS) or archive.testzip() is not None:
                raise RuntimeError(f"ZIP member names or CRC differ for {name}")
            for member, expected in ZIP_MEMBERS.items():
                if archive.getinfo(member).compress_type != mode or archive.read(member) != expected:
                    raise RuntimeError(f"ZIP method or contents differ for {name}/{member}")
        hashes[name] = sha256(data).hexdigest()

    if b"error opening zip-payload.bin" not in read_file("zip-output-blocked.log"):
        raise RuntimeError("ZIP conflict did not reach the extracted output-open failure")
    return hashes


def verify_native(args, fixtures, read_guest, out):
    """Check portable Lua outputs, byte-compare parser source, and read ZIPs."""
    host = out/'host'
    host.mkdir()
    lua = args.native_lua.resolve()
    byacc = args.native_byacc.resolve()
    lua_version = subprocess.check_output([str(lua), '-v'], text=True).strip()
    byacc_version = subprocess.check_output([str(byacc), '-V'], text=True).strip()
    if 'Lua 5.4.8' not in lua_version or not byacc_version.endswith('2.0 20260126'):
        raise RuntimeError('Host references must match the shipped release pins')
    for name in ('lua-script.lua', *BYACC_FILES):
        (host/name).write_bytes(fixtures[name])
    lua_cases_to_compare = (
        ('file', ['lua-script.lua', 'state.txt'], None),
        ('stdin', [], fixtures['lua-stdin.lua']),
        ('pipe', [], fixtures['lua-stdin.lua']),
        ('empty', [], b''),
        ('recovery', ['-e', 'print(3 / 2)'], None),
    )
    for name, options, stdin in lua_cases_to_compare:
        ref = subprocess.run([str(lua), *options], cwd=host, input=stdin,
                             capture_output=True, check=True)
        if ref.stdout != read_guest(f'lua-check-{name}.out') or ref.stderr:
            raise RuntimeError(f'Native and guest Lua results differ for {name}')
    if (host/'state.txt').read_bytes() != read_guest('lua-state.txt'):
        raise RuntimeError('Native and guest Lua persisted bytes differ')

    for options, stdin_file, outputs in BYACC_REFERENCE_CASES:
        stdin = fixtures[stdin_file] if stdin_file else None
        subprocess.run([str(byacc), *options], cwd=host, input=stdin,
                       capture_output=True, check=True)
        for name in outputs:
            if (host/name).read_bytes() != read_guest(name):
                raise RuntimeError(f'Native and guest parser generation differ for {name}')
    parser = host/'guest-parser'
    subprocess.run(['cc', '-o', str(parser), str(out/'guest/byacc-file.tab.c')],
                   capture_output=True, check=True)
    for stdin, status, stdout, stderr in BYACC_PARSER_CASES:
        result = subprocess.run([str(parser)], input=stdin, capture_output=True)
        if (result.returncode, result.stdout, result.stderr) != (status, stdout, stderr):
            raise RuntimeError(f'Guest-generated parser handled {stdin!r} incorrectly')

    return {'lua': {'version': lua_version, 'sha256': sha256(lua.read_bytes()).hexdigest(),
                    'compared_stdout_cases': len(lua_cases_to_compare)},
            'byacc': {'version': byacc_version, 'sha256': sha256(byacc.read_bytes()).hexdigest(),
                      'compared_sources': [name for _, _, files in BYACC_REFERENCE_CASES for name in files],
                      'parser_cases': len(BYACC_PARSER_CASES)},
            'zip_readback_sha256': verify_zip_guest(read_guest)}
