# Architecture

Dependency direction (arrows mean depends on):

```text
CLI / eventual diagnostics UI -> machine lifecycle -> CPU -> guest memory
                                                   -> future runtime -> guest memory
                                                   -> future GPU -> guest memory
                                                   -> storage and input experiments
                                                   -> metrics
```

Implemented: `Machine` owns memory and CPU; bounded execution returns distinct stop reasons. `Memory` owns bytes, checks ranges without addition overflow, tracks byte-granular mapping and R/W/X permissions, and exposes counts. CPU fetch requires X. The default constructor mapping is permissive RWX for existing experiments. `Cpu` operates on guest bytes only and stops on unsupported encodings. CLI executes a fixed demo, a bounded raw test file, or an original restricted ELF64 test executable using the experimental loader; it cannot load PS5 software. Independent synchronous file-range reading, CRC-32, synthetic digital input mapping, a CPU-only graphics surface and a manually advanced logical timer queue are available; the CLI does not use them to load games. No module manager, config, events or logging exist yet. Future interfaces should be introduced only alongside testable behavior and explicit ownership and concurrency contracts. Backend choices do not confer console compatibility.
