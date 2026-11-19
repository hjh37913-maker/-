# Windows Debug and Release checks; requires CMake, CTest, MSVC and a multi-config generator.
# Not yet verified on Windows. Native Linux-specific oracles are omitted.
$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..')
cmake -S . -B build-verify-windows
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
foreach ($config in @('Debug', 'Release')) {
    cmake --build build-verify-windows --config $config --parallel 2
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    ctest --test-dir build-verify-windows -C $config --output-on-failure
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
