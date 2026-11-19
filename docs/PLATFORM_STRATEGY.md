# Platform strategy
Windows x64 is the primary target; the core uses portable C++20 and CMake, with no OS calls. Platform-specific Windows APIs should live behind behavior-driven interfaces when needed. The current build and tests were run only on Linux, not Windows. Cross-platform compilation is a design intention, not verified compatibility.
