"""Bind the applied WTF ABI-header copy to SrvrOS's authoritative contract."""
import hashlib
from pathlib import Path
import subprocess


def verify_random_contract(root, source):
    root = Path(root)
    native = root / 'shared/include/srvros/random.h'
    applied = Path(source) / 'Source/WTF/wtf/srvros/RandomContract.h'
    if native.read_bytes() != applied.read_bytes():
        raise ValueError('applied WTF random contract differs from native header')
    sdk_numbers = root / 'build/sysroot/srvros/include/srvros/syscall_numbers.h'
    # The new standalone header intentionally uses the retained SDK's existing
    # error constants. Bind those transitive bytes too; the native target probe
    # has static assertions for SYS_RANDOM92, flag1 and unavailable-11.
    return {
        'native_path': str(native.relative_to(root)),
        'native_sha256': hashlib.sha256(native.read_bytes()).hexdigest(),
        'applied_path': str(applied.relative_to(source)),
        'applied_sha256': hashlib.sha256(applied.read_bytes()).hexdigest(),
        'source_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root, text=True).strip(),
        'ports_commit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=root / 'ports', text=True).strip(),
        'retained_sdk_syscall_numbers_sha256': hashlib.sha256(sdk_numbers.read_bytes()).hexdigest(),
        'retained_sdk': 'unchanged; new header is supplied by the reviewed WTF patch',
    }
