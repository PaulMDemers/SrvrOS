"""Small in-memory fixture helper; only for disposable generated test images."""
import struct
import subprocess
from mk_exfat_image import file_entry


def check_clean_image(path):
    """exfatprogs can emit ERROR while returning zero (e.g. bitmap length)."""
    result = subprocess.run(['/usr/sbin/fsck.exfat', '-n', str(path)],
                            capture_output=True, text=True)
    output = result.stdout + result.stderr
    if result.returncode or 'ERROR' in output or 'corrupted' in output or ': clean.' not in output:
        raise RuntimeError(output)
    return output


class ExfatFixture:
    def __init__(self, data):
        self.data = bytearray(data)
        if self.data[3:11] != b'EXFAT   ': raise ValueError('Not exFAT')
        self.sector = 1 << self.data[108]
        self.cluster = self.sector << self.data[109]
        self.fat = self.u32(80) * self.sector
        self.heap = self.u32(88) * self.sector
        self.count = self.u32(92)
        self.root = self.u32(96)

    def u32(self, off): return struct.unpack_from('<I', self.data, off)[0]
    def offset(self, cluster):
        if not 2 <= cluster < self.count + 2: raise ValueError('Invalid cluster')
        return self.heap + (cluster-2)*self.cluster

    def chain(self, first, size=None, contiguous=False):
        if size == 0: return b''
        chunks, seen = [], set()
        while first < 0xfffffff8:
            if first in seen: raise ValueError('Cluster loop')
            seen.add(first)
            off = self.offset(first)
            chunks.append(bytes(self.data[off:off+self.cluster]))
            if size is not None and len(chunks)*self.cluster >= size: break
            first = first+1 if contiguous else self.u32(self.fat+first*4)
        result = b''.join(chunks)
        return result if size is None else result[:size]

    def entries(self, directory):
        off = 0
        while off+32 <= len(directory) and directory[off]:
            if directory[off] != 0x85:
                off += 32; continue
            n = directory[off+1]+1
            entry = directory[off:off+n*32]
            name_len = entry[35]
            name_bytes = b''.join(entry[i+2:i+32] for i in range(64, len(entry), 32))
            name = name_bytes[:name_len*2].decode('utf-16le')
            yield name, entry
            off += n*32

    def read(self, path):
        directory = self.chain(self.root)
        for part in path.split('/'):
            entry = next(e for name, e in self.entries(directory) if name == part)
            first = struct.unpack_from('<I', entry, 52)[0]
            size = struct.unpack_from('<Q', entry, 56)[0]
            directory = self.chain(first, size, bool(entry[33] & 2))
        return directory

    @staticmethod
    def checked_entry(name, first, data, attributes=0x20, data_length=None):
        entry = bytearray(file_entry(name, first, data, attributes, data_length))
        entry[33] |= 1  # AllocationPossible, required on stream extensions.
        value = 0
        for byte in name.upper().encode('utf-16le'):
            value = (((value & 1) << 15) | (value >> 1)) + byte
            value &= 0xffff
        struct.pack_into('<H', entry, 36, value)
        value = 0
        for i, byte in enumerate(entry):
            if i in (2, 3): continue
            value = ((((value & 1) << 15) | (value >> 1)) + byte) & 0xffff
        struct.pack_into('<H', entry, 2, value)
        return entry

    def add_request(self, run, version=1):
        self.add_directory_file('srvros-tests', f'{run}.req',
                                f'SRVROS-AUTOTEST-V{version} {run}\n'.encode())

    def add_directory_file(self, directory_name, filename, payload):
        self.add_directory_files(directory_name, {filename: payload})

    def add_directory_files(self, directory_name, files):
        if any(len(payload) > self.cluster for payload in files.values()): raise ValueError('Fixture payload too large')
        root = self.chain(self.root)
        bitmap_cluster = None
        end = 0
        while root[end]:
            if root[end] == 0x81:
                bitmap_cluster = struct.unpack_from('<I', root, end+20)[0]
            end += 32
        if bitmap_cluster is None: raise ValueError('No allocation bitmap')
        bitmap = self.offset(bitmap_cluster)
        free = [i+2 for i in range(self.count) if not self.data[bitmap+i//8] & (1 << (i%8))][:1+len(files)]
        if len(free) != 1+len(files): raise ValueError('Fixture full')
        directory_cluster = free[0]
        for cluster in free:
            bit = cluster-2
            self.data[bitmap+bit//8] |= 1 << (bit%8)
            struct.pack_into('<I', self.data, self.fat+cluster*4, 0xffffffff)
            off = self.offset(cluster)
            self.data[off:off+self.cluster] = b'\0'*self.cluster
        entries = bytearray()
        for cluster, (filename, payload) in zip(free[1:], files.items()):
            off = self.offset(cluster)
            self.data[off:off+len(payload)] = payload
            entries += self.checked_entry(filename, cluster, payload)
        if len(entries) + 32 > self.cluster: raise ValueError('Fixture directory full')
        off = self.offset(directory_cluster)
        self.data[off:off+len(entries)] = entries
        entry = self.checked_entry(directory_name, directory_cluster, b'', 0x10, self.cluster)
        # Generated root chain is contiguous; ensure this entry doesn't straddle
        # a cluster boundary and the next zero entry remains inside the root.
        if end % self.cluster + len(entry) + 32 > self.cluster: raise ValueError('Root slot straddles cluster')
        off = self.offset(self.root) + end
        self.data[off:off+len(entry)] = entry
