#include <stdio.h>
#include <unistd.h>
extern char **environ;
#ifdef LINE_MAX
#undef LINE_MAX
#endif
#define main srvros_shell_main
#include "../userspace/sh/sh.c"
#undef main

static unsigned checks, failures;
static unsigned error_writes;
long srv_write(int fd, const void *buffer, size_t length) {
    (void)fd; (void)buffer; error_writes++; return (long)length;
}
static void check(int ok, const char *name) {
    checks++;
    if (!ok) { fprintf(stderr, "FAIL %s\n", name); failures++; }
}

/* Quoted tokens must never ask the filesystem for wildcard matches. */
long srv_list(uint64_t index, char *path, size_t capacity, uint64_t *size) {
    (void)index; (void)path; (void)capacity; (void)size;
    check(0, "unexpected glob lookup"); return 0;
}

static void words(void) {
    const struct { const char *input, *expected; } cases[] = {
        {"'left\\nright'", "left\\nright"},
        {"\"left\\nright\"", "left\\nright"},
        {"'two\\\\slashes'", "two\\\\slashes"},
        {"\"one\\\\slash\"", "one\\slash"},
        {"'tail\\'", "tail\\"},
        {"'a\\'b", "a\\b"},
        {"\"a\\'b\"", "a\\'b"},
        {"'\\$HOME'", "\\$HOME"},
        {"\"\\$HOME\"", "$HOME"},
        {"\"a\\\"b\"", "a\"b"},
        {"\"\\`\"", "`"},
        {"\"a\\ b\"", "a\\ b"},
        {"two\\ words", "two words"},
        {"a\\|b", "a|b"},
        {"a\\>b", "a>b"},
        {"a\\;b", "a;b"},
        {"''", ""}, {"\"\"", ""},
        {"'a b'\"c d\"", "a bc d"},
    };
    for (size_t i=0; i<sizeof(cases)/sizeof(cases[0]); i++) {
        char text[256], *argv[4]; strcpy(text,cases[i].input);
        int count=split_words(text,argv,4);
        check(count==1 && !strcmp(argv[0],cases[i].expected),cases[i].input);
    }
    char multiple[]="'' 'tail\\' \"two words\"", *argv[3];
    check(split_words(multiple,argv,3)==3 && !*argv[0] &&
          !strcmp(argv[1],"tail\\") && !strcmp(argv[2],"two words"),"argument boundaries");
    char capacity[]="one two";
    check(split_words(capacity,argv,1)==-1,"argument capacity refusal");
}

static void scans(void) {
    char pipeline[]="printf '%s' 'tail\\' | cat", *segments[3];
    check(split_pipeline_segments(pipeline,segments,3)==2,"pipeline after literal backslash");
    char escaped_pipe[]="printf '%s' a\\|b";
    check(split_pipeline_segments(escaped_pipe,segments,3)==1,"escaped pipe stays an argument");
    char path[160];
    char source[]="'space \\name' rest";
    char *end=copy_redirection_path(source,path,sizeof(path));
    check(!strcmp(path,"space \\name") && !strcmp(end," rest"),"single quoted redirection path");
    char double_path[]="\"space \\name\" rest";
    end=copy_redirection_path(double_path,path,sizeof(path));
    check(!strcmp(path,"space \\name") && !strcmp(end," rest"),"double quoted redirection path");
    struct command_redirection redirection;
    char redirected[]=" 'tail\\' > 'space \\name'";
    split_redirections(redirected,&redirection);
    check(redirection.stdout_set && !strcmp(redirection.stdout_path,"space \\name") &&
          !strcmp(redirected," 'tail\\' "),"redirection following closing quote");
    char escaped_redirect[]=" a\\>b a\\<b";
    split_redirections(escaped_redirect,&redirection);
    check(!redirection.stdout_set && !redirection.stdin_set &&
          !strcmp(escaped_redirect," a\\>b a\\<b"),"escaped redirection operators");
    char command[]="'tail\\' argument";
    check(!strcmp(find_argument_tail(command)," argument"),"command argument boundary");
    check(!strcmp(skip_command_word(command)," argument"),"assignment word boundary");
    const char *sub="$(echo 'tail\\') rest";
    check(find_command_substitution_end(sub,0)==strlen("$(echo 'tail\\')")-1,"substitution boundary");
    const char *completion="echo 'tail\\'; cat";
    check(token_is_command_position(completion,strlen(completion)-3),"completion command boundary");
    check(text_contains_heredoc("echo 'tail\\'; cat <<END"),"heredoc boundary");
    struct if_parts ip; struct for_parts fp; struct while_parts wp; struct case_parts cp;
    check(find_if_parts("if test 'tail\\'; then echo yes; fi",&ip),"if quote boundary");
    check(find_for_parts("for p in 'tail\\'; do echo yes; done",&fp),"for quote boundary");
    check(find_while_parts("while test 'tail\\'; do echo yes; done",&wp),"while quote boundary");
    check(find_case_parts("case 'tail\\' in 'tail\\') echo yes;; esac",&cp),"case quote boundary");
    size_t begin, finish, next;
    check(find_group_parts("{ echo 'tail\\'; }",&begin,&finish,&next),"group quote boundary");
    char name[32], body[256];
    check(find_function_definition("f() { echo 'tail\\'; }",name,sizeof(name),body,sizeof(body)),"function quote boundary");
    check(find_case_arm_end("'tail\\') echo yes;;",0,&begin,&finish,&next),"case arm quote boundary");
    char expanded[256];
    expand_globs("'tail\\' '*.c'","/fat",expanded,sizeof(expanded));
    check(!strcmp(expanded,"'tail\\' '*.c'"),"glob scan preserves quoted token boundaries");
}

static void expansions(void) {
    const char *values[]={"tail\\", "two\\\\slashes", "a\"b\\c", "\\$HOME", "", "two words"};
    for (size_t i=0;i<sizeof(values)/sizeof(values[0]);i++) {
        char text[256]="\"", *argv[2];size_t length=1;
        int ok=append_expansion_text(text,sizeof(text),&length,values[i],'"',0);
        append_char(text,sizeof(text),&length,'"');
        check(ok && split_words(text,argv,2)==1 && !strcmp(argv[0],values[i]),
              "expanded quoted data survives decoding");
    }
    const char *unquoted[]={"tail\\", "a\"b\\c", "a'b", "\\$HOME"};
    for (size_t i=0;i<sizeof(unquoted)/sizeof(unquoted[0]);i++) {
        char text[256]="", *argv[2];size_t length=0;
        int ok=append_expansion_text(text,sizeof(text),&length,unquoted[i],0,0);
        check(ok && split_words(text,argv,2)==1 && !strcmp(argv[0],unquoted[i]),
              "expanded unquoted data survives decoding");
        length=0;
        check(append_expansion_text(text,sizeof(text),&length,unquoted[i],'"',1) &&
              !strcmp(text,unquoted[i]),"parameter word retains raw value for assignment");
    }
    char guarded[5]={'x','x','x','x','!'};
    size_t length=0;error_writes=0;
    check(append_expansion_text(guarded,3,&length,"\\",'"',0) &&
          length==2 && !strcmp(guarded,"\\\\"),"escaped value exact capacity");
    length=0;guarded[0]=0;
    check(!append_expansion_text(guarded,2,&length,"\\",'"',0) &&
          !length && !guarded[0] && guarded[4]=='!' && error_writes,
          "refuse incomplete escape without writing beyond capacity");
}

int main(void) {
    words(); scans(); expansions();
    printf("shell-quoting: %s %u checks, %u failures\n",failures?"FAIL":"PASS",checks,failures);
    return failures!=0;
}
