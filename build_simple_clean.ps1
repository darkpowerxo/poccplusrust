# Simple C-only build for testing
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vsWhere) {
    $vsPath = & $vsWhere -latest -property installationPath
    Write-Host "Building C-only version..."
    Write-Host "Using Visual Studio at: $vsPath"
    
    # Create stub for Rust functions
    @'
// Temporary stub for Rust functions
#include <stdio.h>
void rust_module_init(void) {
    printf("MODULE=RUST INFO stub implementation\n");
}
void rust_module_shutdown(void) {
    printf("MODULE=RUST INFO stub shutdown\n");
}
'@ | Out-File -FilePath "c\rust_stub.c" -Encoding ASCII
    
    # Create build batch file
    @"
@echo off
call "$vsPath\VC\Auxiliary\Build\vcvarsall.bat" x64
if not exist bin mkdir bin
cl /std:c11 /experimental:c11atomics /O2 /W3 /Ic /D_CRT_SECURE_NO_WARNINGS c\globals.c c\bus.c c\mod1.c c\mod2.c c\main.c c\rust_stub.c /Fe:bin\poc-c-only.exe
"@ | Out-File -FilePath "build_c_only.bat" -Encoding ASCII
    
    # Execute build
    cmd /c build_c_only.bat
    
    if (Test-Path "bin\poc-c-only.exe") {
        Write-Host "Build successful!"
        Write-Host "Run: .\bin\poc-c-only.exe"
    } else {
        Write-Host "Build failed"
    }
} else {
    Write-Host "Visual Studio not found"
}
