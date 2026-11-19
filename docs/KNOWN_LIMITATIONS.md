# Known limitations
No PS5 compatibility, PS5 executable loader, OS ABI, GPU, physical-device input, PS5 storage engine, shader compiler, telemetry, GUI or game launch. CPU support is restricted to a small set of instruction encodings and is not fully x86-compatible; ADD, SUB and CMP flags have differential tests but coverage is not exhaustive. No sandbox, no threading guarantees, no real OS page protection or console virtual memory model, and no handling of hostile guest programs. No Windows, actual game, hardware GPU, game performance, leak endurance, fuzzing has occurred; limited GCC coverage measurements are documented but do not establish completeness. Do not use for untrusted input.

Experimental host file reading is synchronous, not a sandbox, has TOCTOU and file mutation races, and does not handle console packages. CRC-32 is not authentication.

Input mapping accepts only synthetic digital events; no physical keyboard, mouse or controllers, analog input, profiles or guest services are implemented.

The graphics experiment is CPU-only; it does not implement alpha blending, shaders, graphics APIs, GPU resources, PS5 command processing or presentation. Batch validation is not a security boundary.

The experimental logical timer queue is not a PS5 timer, guest service, host clock, scheduler or threading implementation; its ordering and clock units are project-defined.

The raw-test CLI accepts host files but is not a file sandbox or a PS5/PE loader; do not feed it untrusted files. Only a small subset of stack and memory MOV forms exists; indirect calls remain unimplemented; 32- and 64-bit arithmetic memory support is limited to selected forms. Read-modify-write is not host-atomic and LOCK is unsupported.

The restricted ELF64 loader is for original x86-64 test executables only. It lacks dynamic linking, relocations, process startup, syscalls, TLS and operating-system isolation; acceptance is not proof of program compatibility.
