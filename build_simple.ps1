# Simple C-only build for testing (no Rust dependencies)
# This allows us to test the core C implementation before adding Rust

# Find Visual Studio installation
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vsWhere) {
    $vsPath = & $vsWhere -latest -property installationPath
    $vcvarsall = "$vsPath\VC\Auxiliary\Build\vcvarsall.bat"
    
    Write-Host "🔧 Building C-only version (without Rust module)" -ForegroundColor Yellow
    Write-Host "Using Visual Studio at: $vsPath" -ForegroundColor Green
    
    # Create a temporary stub for Rust functions
    $rustStub = @'
// Temporary stub for Rust functions to test C-only build
void rust_module_init(void) {
    printf("MODULE=RUST INFO stub implementation - Rust module disabled\n");
}

void rust_module_shutdown(void) {
    printf("MODULE=RUST INFO stub shutdown\n");
}
'@
    
    $rustStub | Out-File -FilePath "c\rust_stub.c" -Encoding ASCII
    
    # Build command for MSVC
    $buildCmd = @'
call "$vcvarsall" x64
cl /std:c11 /O2 /W3 /Ic c\globals.c c\bus.c c\mod1.c c\mod2.c c\main.c c\rust_stub.c /Fe:bin\poc-c-only.exe
'@
    
    $buildCmd | Out-File -FilePath "build_c_only.bat" -Encoding ASCII
    
    # Execute build
    cmd /c build_c_only.bat
    
    if (Test-Path "bin\poc-c-only.exe") {
        Write-Host "✅ C-only build successful!" -ForegroundColor Green
        Write-Host "Run: .\bin\poc-c-only.exe" -ForegroundColor Cyan
    } else {
        Write-Host "❌ Build failed" -ForegroundColor Red
    }
    
} else {
    Write-Host "❌ Visual Studio not found. Please install Visual Studio with C++ tools." -ForegroundColor Red
}
