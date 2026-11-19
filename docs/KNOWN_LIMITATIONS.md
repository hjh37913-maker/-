# Known limitations
No PS5 compatibility, PS5 executable loader, OS ABI, GPU, physical-device input, PS5 storage engine, shader compiler, performance telemetry, or game launch. The browser research dashboard is not a game launcher. CPU support is restricted to a small set of instruction encodings and is not fully x86-compatible; ADD, SUB and CMP flags have differential tests but coverage is not exhaustive. No sandbox, no threading guarantees, no real OS page protection or console virtual memory model, and no handling of hostile guest programs. No Windows runtime, actual game, hardware GPU, game performance or leak-endurance validation has occurred; limited GCC coverage measurements are documented but do not establish completeness. Do not use for untrusted input.

Experimental host file reading is synchronous, not a sandbox, has TOCTOU and file mutation races, and does not handle console packages. CRC-32 is not authentication.

The browser dashboard can display local keyboard and mouse events, but does not forward them to the guest. The core input mapper accepts only synthetic digital events; no native keyboard/mouse integration, controllers, analog input, profiles or guest services are implemented.

The graphics experiment is CPU-only; it does not implement alpha blending, shaders, graphics APIs, GPU resources, PS5 command processing or presentation. Batch validation is not a security boundary.

The experimental logical timer queue is not a PS5 timer, guest service, host clock, scheduler or threading implementation; its ordering and clock units are project-defined.

The raw-test CLI accepts host files but is not a file sandbox or a PS5/PE loader; do not feed it untrusted files. Only a small subset of stack and memory MOV forms exists; far calls and other indirect control-flow forms remain unimplemented; 32- and 64-bit arithmetic memory support is limited to selected forms. Read-modify-write is not host-atomic and LOCK is unsupported.

The restricted ELF64 loader is for original x86-64 test executables only. It lacks dynamic linking, relocations, process startup, syscalls, TLS and operating-system isolation; acceptance is not proof of program compatibility.

The optional Python dashboard server binds to loopback by default and has no authentication; it is not intended for untrusted networks. Interpreter uploads are original trusted test files only and are not sandboxed. Browser library data consists of metadata and optional user-provided HTTPS links in local storage, not installed or downloaded games. Source links open externally; this is not a downloader or store integration. Browser events do not control guest software. The UI has DOM-level tests but has not been visually verified in a real browser or on Windows.

Sparse memory and CPU-only triangle/depth rendering are generic experiments,
not PS5 virtual memory or a GPU implementation. Sparse allocation failures
are handled but have not been injected experimentally. Metadata inspection
does not imply compatibility and does not decrypt or execute protected files.
