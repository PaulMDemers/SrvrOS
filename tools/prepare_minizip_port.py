#!/usr/bin/env python3
"""Apply SrvrOS file-status and stack adaptations to a pinned MiniZip CLI."""

import argparse
from pathlib import Path


WRITER_EDITS = (
    (
        '''
                        if (size_read < size_buf)
                            if (feof(fin)==0)
                        {
                            printf("error in reading %s\\n",filenameinzip);
                            err = ZIP_ERRNO;
                        }

                        if (size_read>0)
''',
        '''
                        if (ferror(fin) || (size_read < size_buf && !feof(fin)))
                        {
                            printf("error in reading %s\\n",filenameinzip);
                            err = ZIP_ERRNO;
                        }
                        else if (size_read>0)
''',
        1,
    ),
    (
        '''
                if (fin)
                    fclose(fin);
''',
        '''
                if (fin && fclose(fin) != 0 && err == ZIP_OK)
                {
                    printf("error closing %s after reading\\n",filenameinzip);
                    err = ZIP_ERRNO;
                }
''',
        1,
    ),
    (
        '''
        if (errclose != ZIP_OK)
            printf("error in closing %s\\n",filename_try);
''',
        '''
        if (errclose != ZIP_OK)
        {
            printf("error in closing %s\\n",filename_try);
            if (err == ZIP_OK)
                err = errclose;
        }
''',
        1,
    ),
)

READER_EDITS = (
    ("char filename_inzip[65536+1];", "char filename_inzip[MAXFILENAME+1];", 2),
    (
        '''
    if (err!=UNZ_OK)
        printf("error %d with zipfile in unzGetGlobalInfo\\n",err);
    printf("  Length''',
        '''
    if (err!=UNZ_OK)
    {
        printf("error %d with zipfile in unzGetGlobalInfo\\n",err);
        return err;
    }
    printf("  Length''',
        1,
    ),
    (
        '''
    return 0;
}


static int do_extract_currentfile''',
        '''
    return err;
}


static int do_extract_currentfile''',
        1,
    ),
    (
        '''
                printf("error opening %s\\n",write_filename);
''',
        '''
                printf("error opening %s\\n",write_filename);
                err = UNZ_ERRNO;
''',
        1,
    ),
    (
        '''
            if (fout)
                    fclose(fout);
''',
        '''
            if (fclose(fout) != 0 && err == UNZ_OK)
            {
                printf("error closing %s\\n",write_filename);
                err = UNZ_ERRNO;
            }
''',
        1,
    ),
)


def prepare(source: Path, output: Path) -> None:
    edits = {"minizip.c": WRITER_EDITS, "miniunz.c": READER_EDITS}
    if source.name not in edits:
        raise ValueError(f"unexpected MiniZip CLI source: {source}")
    if source.resolve() == output.resolve():
        raise ValueError("output must be separate from the pinned source")

    text = source.read_text(encoding="utf-8")
    for old, new, count in edits[source.name]:
        matched = text.count(old)
        if matched != count:
            raise ValueError(f"{source}: expected {count} matches for {old.strip()!r}, got {matched}")
        text = text.replace(old, new, count)

    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(text, encoding="utf-8", newline="\n")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        prepare(args.source, args.output)
    except (OSError, ValueError) as error:
        parser.exit(1, f"{error}\n")


if __name__ == "__main__":
    main()
