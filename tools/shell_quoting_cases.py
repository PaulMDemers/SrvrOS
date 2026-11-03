"""Exact-byte shell quoting cases used by the real Terminal/awk guest fixture."""

CASES = [
    ('quote-single', r"printf '%s\n' 'left\nright'", b'left\\nright\n'),
    ('quote-double', r'printf "%s\n" "left\nright"', b'left\\nright\n'),
    ('quote-pairs', r'''printf '%s\n' 'two\\slashes' "one\\slash"''', b'two\\\\slashes\none\\slash\n'),
    ('quote-tail', r"printf '%s\n' 'tail\'", b'tail\\\n'),
    ('quote-adjacent', r'''printf '%s\n' 'a\'b "a\'b"''', b"a\\b\na\\'b\n"),
    ('quote-special', r'''printf '%s\n' "a\"b" "\$HOME" '\$HOME' ''', b'a"b\n$HOME\n\\$HOME\n'),
    ('quote-arguments', r'''printf '<%s>\n' '' "" 'two words' two\ words''', b'<>\n<>\n<two words>\n<two words>\n'),
    ('quote-operators', r"printf '%s\n' a\|b a\>b a\<b a\;b", b'a|b\na>b\na<b\na;b\n'),
    ('quote-pipeline', r"printf '%s\n' 'tail\' | cat", b'tail\\\n'),
    ('quote-awk', r'''awk 'BEGIN {printf "inline\n"; print "a\\b"; print ("a.b" ~ /a\.b/)}' ''', b'inline\na\\b\n1\n'),
    ('quote-assignment', r'''Q='tail\'; printf '%s\n' "$Q"''', b'tail\\\n'),
    ('quote-braced', r'''Q='tail\'; printf '%s\n' "${Q}" "${Q:-unused}"''', b'tail\\\ntail\\\n'),
    ('quote-value', r'''Q='a"b\c'; printf '%s\n' "$Q"''', b'a"b\\c\n'),
    ('quote-positional', r'''sh -c 'printf "%s\n" "$1" "$@"' q 'tail\' ''', b'tail\\\ntail\\\n'),
    ('quote-default', r'''unset Q; printf '<%s>\n' "${Q:-"plain"}" "${Q:-two words}" "${Q:-tail\q}"''', b'<plain>\n<two words>\n<tail\\q>\n'),
    ('quote-alternate', r'''Q=yes; printf '<%s>\n' "${Q:+"plain"}" "${Q:+two words}"''', b'<plain>\n<two words>\n'),
    ('quote-default-value', r'''unset Q; R='tail\'; printf '<%s>\n' "${Q:-$R}"''', b'<tail\\>\n'),
    ('quote-default-nested', r'''unset Q; R='tail\'; printf '<%s>\n' "${Q:-"a\b"}" "${Q:-"$R"}"''', b'<a\\b>\n<tail\\>\n'),
    ('quote-default-assign', r'''unset Q; R='a\b'; printf '%s\n' "${Q:=$R}" "$Q"''', b'a\\b\na\\b\n'),
    ('quote-unquoted-value', r'''Q='a"b\c'; printf '%s\n' $Q''', b'a"b\\c\n'),
    ('quote-default-words', r'''unset Q; printf '<%s>\n' ${Q:-'two words\x'}''', b'<two words\\x>\n'),
    ('quote-alternate-words', r'''Q=yes; printf '<%s>\n' ${Q:+'two words\x'}''', b'<two words\\x>\n'),
    ('quote-assign-words', r'''unset Q; printf '<%s>\n' ${Q:='two words\x'} "$Q"''', b'<two words\\x>\n<two words\\x>\n'),
    ('quote-default-apostrophe', r'''unset Q; printf '%s\n' "${Q:-"a'b\c"}"''', b"a'b\\c\n"),
    ('quote-substitution', r'''printf '%s\n' "$(printf '%s' 'tail\')"''', b'tail\\\n'),
    ('quote-if', r"if test 'tail\' = 'tail\'; then printf '%s' 'if\ok'; fi", b'if\\ok'),
    ('quote-for', r'''for p in 'tail\' 'two words'; do printf '<%s>\n' "$p"; done''', b'<tail\\>\n<two words>\n'),
    ('quote-group', r"{ printf '%s' 'group\'; }", b'group\\'),
    ('quote-function', r"qfunc() { printf '%s' 'function\'; }" + '\nqfunc', b'function\\'),
    ('quote-case', r"case 'tail\' in 'tail\') printf '%s' 'case\';; esac", b'case\\'),
    ('quote-nested', r'''sh -c 'printf "%s\n" "nested\path"' ''', b'nested\\path\n'),
]

LIMIT_CASES = [
    ('quote-limit-closing', "Q='" + 'a'*240 + "'\nR=$Q$Q\nprintf '%s' \"${R}abcdefghijklmnopqr\"\necho $?", b'2\n'),
    ('quote-limit-escaped', "Q='" + '\\'*120 + "'\nprintf '%s' \"$Q$Q$Q\"\necho $?", b'2\n'),
    ('quote-limit-braced', "Q='" + 'a'*240 + "'\nR=$Q$Q\nprintf '%s' \"${R}${R}\"\necho $?", b'2\n'),
]
# Shell diagnostics currently use stdout. Require the exact refusal and status,
# with no partial printf payload before it.
LIMIT_CASES = [(name, command,
                (f'/fat/awk-test/{name}.sh:{2 if name == "quote-limit-escaped" else 3}: '
                 'sh: expanded command too long\n2\n').encode())
               for name, command, _ in LIMIT_CASES]
ALL_CASES = CASES + LIMIT_CASES

# Collect compound-command output through an ordinary outer redirection, without
# relying on compound redirection support. The inner scripts use inline programs.
FILES = {name+'.sh': (command+'\n').encode() for name, command, _ in ALL_CASES}
EXPECTED = {name+'.txt': expected for name, _, expected in ALL_CASES}
WORKFLOW = ''.join('sh /fat/awk-test/'+name+'.sh > /fat/awk-test/'+name+'.txt\n'
                   'echo $? > /fat/awk-test/'+name+'-status.txt\n' for name, _, _ in ALL_CASES)
EXPECTED.update({name+'-status.txt': b'0\n' for name, _, _ in ALL_CASES})
WORKFLOW += r'''printf '%s' 'path-content' > '/fat/awk-test/quoted space.txt'
cat '/fat/awk-test/quoted space.txt' > /fat/awk-test/quote-filename.txt
false | true
echo $? > /fat/awk-test/quote-pipe-ok.txt
true | false
echo $? > /fat/awk-test/quote-pipe-fail.txt
'''
WORKFLOW += r"printf '%s\n' line" + "\\\n" + "continued > /fat/awk-test/quote-continuation.txt\n"
EXPECTED.update({'quote-filename.txt': b'path-content', 'quote-pipe-ok.txt': b'0\n',
                 'quote-pipe-fail.txt': b'1\n', 'quote-continuation.txt': b'linecontinued\n',
                 'quote-typed.txt': b'typed\\path\n'})
TYPED_COMMAND = r"printf '%s\n' 'typed\path' > /fat/awk-test/quote-typed.txt" + '\n'
