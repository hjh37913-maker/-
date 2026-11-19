# Experimental memory model

The default memory backend is a fixed-capacity, zero-initialized byte array with a separate byte of mapping/permission metadata per guest byte. An opt-in sparse backend is described below. It is not a PS5 address-space model or a security sandbox. `Memory(size)` retains the previous all-RWX behavior for compatibility with existing experiments; `Memory(size, false)` starts unmapped. This default is deliberately permissive and must not be used as a security policy.

`map` requires a nonempty, in-range and entirely unmapped range and valid read/write/execute bits. It zeroes the mapped bytes. `unmap` and `protect` require the entire nonempty range to be mapped; they do not require page alignment. Unmapping clears the bytes; remapping zeroes them again. Mapped `Permission::none` is distinct from unmapped. Adjacent or partial regions can have different permissions; operations can cross them if every byte has the required access. All operations validate the whole range before mutation; rejected writes, mapping and protection changes are atomic in this single-threaded model. Out-of-range and permission errors return false, not differentiated fault types. Empty read/write/fetch operations succeed for positions up to and including capacity; mapping operations reject empty ranges.

`read`, `write`, and instruction `fetch` independently check R, W, and X. CPU instruction fetch now uses X, not R. `stats()` scans attributes and counts capacity, mapped, readable, writable and executable bytes; this is not host memory or GPU usage telemetry. The default flat backend allocates the full capacity for both data and attributes; its costs are not suitable for a large console address space. Concurrent access is not synchronized. There are no host page protections, virtual-to-physical translation, memory-mapped devices, alignment rules, GPU coherence or precise hardware exceptions. Do not run untrusted inputs.

Testing includes an independent small reference model exercised over 100 deterministic seeds and 500 operations per seed. See TESTING.md for scope and limitations.

## Sparse virtual memory experiment
`Memory(size, SparsePages{max_pages})` starts unmapped and reserves no array
proportional to virtual capacity. It allocates 4 KiB guest pages on demand;
each holds 4 KiB data and 4 KiB byte-level permission metadata, plus map
overhead. Allocation is bounded by `max_pages`. `map_detailed` distinguishes
overlap, invalid ranges and resource limits; missing pages are staged before a
map commits, so allocation failure does not partially map the request. The
last unmapped byte frees a page. Ranges, permissions and empty read/write rules
match the flat backend; tests compare both implementations over 200,000
deterministic operations. This comparison is not an independent hardware oracle.

The experimental ELF loader preserves the selected memory backend and can
load an original high-address ELF with a sparse stack. `--run-elf-sparse`
uses a 16 TiB virtual capacity on 64-bit hosts, capped at 4096 allocated guest
pages (16 MiB guest bytes and approximately 32 MiB for data and byte metadata,
plus map overhead). Input files remain limited to 1 MiB. `examples/
high-address-demo.elf` exercises this path; do not execute its synthetic HLT
on the host. Virtual capacity is not committed physical memory, PS5 layout,
or game support. This model is unsynchronized, has no OS ABI, host sandbox,
canonical address checking, device mappings or hardware page faults.
