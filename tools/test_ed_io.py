#!/usr/bin/env python3
"""Exercise the actual editor's save and error exit paths using host stdio."""
import subprocess
import tempfile
from pathlib import Path
binary=Path('build/tests/ed-host').resolve()
def run(commands):
    return subprocess.run([str(binary),'-s'],input=commands,text=True,capture_output=True)
def main():
    with tempfile.TemporaryDirectory() as tmp:
        path=Path(tmp)/'document.txt'
        result=run(f'a\nDraft AaZz\n.\n1s/Draft/Edited/\nw {path}\nq\n')
        assert result.returncode==0,result
        assert path.read_text()=='Edited AaZz\n'
        assert run('a\nDo not lose this\n.\nw /dev/full\nq\n').returncode==1
        assert run('z\nq\n').returncode==1
        assert subprocess.run([str(binary),'-s',tmp],input='q\n',text=True,capture_output=True).returncode==1
    print('ed-io-host: PASS (edit/save, failed close and invalid-command status)')

if __name__ == '__main__': main()
