"""Independent retained-file checks for the v3 userspace workspace workload."""
import hashlib

EDITED = b'Edited AaZz\nKeep me\nAppended 123\n'
EXPECTED = {
    'document.txt': EDITED,
    'reopened.txt': EDITED,
    'settings.txt': b'persisted-settings\n',
    '.profile': b'WORKSPACE_CHECK=persisted-settings\nHISTSIZE=32\n',
    '.history': b'echo workspace-history-sentinel\nexit\nhistory\nexit\n',
    'first.done': b'shell-first-pass\n',
    'verified.done': b'shell-remount-pass\n',
    'load.bin': bytes(512 * 1024),
}


def verify(read):
    result = {}
    for name, expected in EXPECTED.items():
        data = read(name)
        if data != expected: raise ValueError(f'USB shell contents mismatch: {name}')
        result[name] = {'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}
    proof = read('history-proof.txt')
    if b'echo workspace-history-sentinel' not in proof:
        raise ValueError('USB shell history recall missing')
    result['history-proof.txt'] = {'bytes':len(proof),'sha256':hashlib.sha256(proof).hexdigest()}
    return result


def verify_mounted(root):
    if root.is_symlink() or not root.is_dir(): raise ValueError('Invalid USB shell directory')
    def read(name):
        path=root/name
        bound=len(EXPECTED[name]) if name in EXPECTED else 4096
        if path.is_symlink() or not path.is_file() or path.stat().st_size>bound:
            raise ValueError(f'Invalid USB shell file: {name}')
        return path.read_bytes()
    return verify(read)
