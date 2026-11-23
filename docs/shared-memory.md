# Anonymous shared memory in SrvrOS

The normal runtime exposes size-bounded, anonymous memory descriptors through
`<srvros/sys.h>`. `srv_shm_create(size)` returns a writable descriptor for a
new zeroed object. `srv_shm_export_readonly(fd)` returns a separate read-only
descriptor for the same bytes. Both return a descriptor or a negative SrvrOS
error and set close-on-exec on newly installed descriptors. Duplicating or
passing a descriptor as `SCM_RIGHTS` preserves its access rights. A read-only
recipient can observe subsequent writes through the original writable handle
or mapping.

`fstat` reports the immutable logical byte count and `F_GETFL` reports the
descriptor's access mode. To map it, use `mmap(NULL, size, protection,
MAP_SHARED, fd, 0)` with the exact logical size and either `PROT_READ` or
`PROT_READ | PROT_WRITE`. A writable mapping requires a writable descriptor.
Mappings are always non-executable. Whole-region `mprotect` can remove access
and restore it within the original descriptor's ceiling, even after the
descriptor closes. Whole-region `munmap` and `mprotect` accept the logical
length or its page-rounded length. Partial or mixed-region requests fail
before changing pages.

Open descriptors, queued rights and mappings hold independent references.
Closing a descriptor does not invalidate a mapping. Unmapping does not close
a descriptor. Process exit and successful exec retire the old mappings. Fork
with shared mappings returns an error because SrvrOS's existing fork copies
private pages. An unmapped descriptor follows the existing FD inheritance rules.

The normal runtime limits objects to 32 MiB each and 16 objects total, with a
64 MiB global quota charged in pages. The charge remains until the last
reference releases the backing pages. Final page reclamation is synchronous.
This interface provides neither named shared memory nor a disk-backed mapping.
Memory descriptors support mapping, metadata, duplication and transfer. They
have no byte I/O, resizing, or cross-process futex contract. The isolated
installed-account and experimental signal profiles do not admit this interface.
