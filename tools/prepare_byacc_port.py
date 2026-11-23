#!/usr/bin/env python3
"""Stage the pinned Berkeley Yacc driver with SrvrOS output error handling."""

import argparse
from hashlib import sha256
from pathlib import Path


MAIN_C_SHA256 = "b33a877caf932f4620af0711aafb75a0d5f4f2df1a7f6050e6a9e1a6984c26db"

UPSTREAM_CLOSE = """static int got_intr = 0;

void
done(int k)
{
    DO_CLOSE(input_file);
    DO_CLOSE(output_file);
    if (iflag)
	DO_CLOSE(externs_file);
    if (rflag)
	DO_CLOSE(code_file);

    DO_CLOSE(action_file);
    DO_CLOSE(defines_file);
    DO_CLOSE(graph_file);
    DO_CLOSE(text_file);
    DO_CLOSE(union_file);
    DO_CLOSE(verbose_file);
"""

SRVROS_CLOSE = r"""static volatile sig_atomic_t got_intr = 0;

/* Generated files can report an error at the final buffered write or close. */
static void
close_output(FILE **stream, const char *name, int *status)
{
    FILE *use = *stream;
    int write_failed;
    int close_failed;

    if (use == NULL)
	return;

    *stream = NULL;
    write_failed = !got_intr && ferror(use);
    close_failed = fclose(use) == EOF;
    if (!got_intr && (write_failed || close_failed))
    {
	fprintf(stderr, "%s: f - cannot write \"%s\"\n", myname, name);
	if (*status == EXIT_SUCCESS)
	    *status = EXIT_FAILURE;
    }
}

void
done(int k)
{
    DO_CLOSE(input_file);
    close_output(&output_file, output_file_name, &k);
    if (iflag)
	close_output(&externs_file, externs_file_name, &k);
    if (rflag)
	close_output(&code_file, code_file_name, &k);

    DO_CLOSE(action_file);
    close_output(&defines_file, defines_file_name, &k);
    close_output(&graph_file, graph_file_name, &k);
    DO_CLOSE(text_file);
    DO_CLOSE(union_file);
    close_output(&verbose_file, verbose_file_name, &k);
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source_main_c", type=Path)
    parser.add_argument("staged_main_c", type=Path)
    args = parser.parse_args()

    if args.source_main_c.resolve() == args.staged_main_c.resolve():
        parser.error("the staged file must have a separate path")
    if args.staged_main_c.exists() and args.source_main_c.samefile(args.staged_main_c):
        parser.error("the staged file must have a separate inode")

    contents = args.source_main_c.read_bytes()
    if sha256(contents).hexdigest() != MAIN_C_SHA256:
        parser.error("unexpected vendored main.c digest")
    source = contents.decode("utf-8")
    if source.count(UPSTREAM_CLOSE) != 1:
        parser.error("expected a single vendored output cleanup block")

    args.staged_main_c.parent.mkdir(parents=True, exist_ok=True)
    args.staged_main_c.write_bytes(source.replace(UPSTREAM_CLOSE, SRVROS_CLOSE, 1).encode("utf-8"))


if __name__ == "__main__":
    main()
