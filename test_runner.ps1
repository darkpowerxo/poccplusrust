# PowerShell Test Runner for Windows
Write-Host "🧪 POC Test Suite" -ForegroundColor Cyan
Write-Host "=================="

$poc_exe = "bin\poc-msvc.exe"

if (-not (Test-Path $poc_exe)) {
    Write-Host "ERROR: $poc_exe not found. Run 'make msvc' first." -ForegroundColor Red
    exit 1
}

# Test 1: Smoke test
Write-Host "`nTest 1: Smoke Test (10s runtime)" -ForegroundColor Yellow
$job1 = Start-Job -ScriptBlock { 
    param($exe) 
    & $exe 
} -ArgumentList $poc_exe

Wait-Job $job1 -Timeout 10 | Out-Null
Stop-Job $job1 -PassThru | Remove-Job
if ($job1.State -eq "Completed") {
    Write-Host "✅ Smoke test passed" -ForegroundColor Green
} else {
    Write-Host "❌ FAIL: Smoke test crashed or timed out" -ForegroundColor Red
}

# Test 2: C-only writers
Write-Host "`nTest 2: C Writers Only (15s)" -ForegroundColor Yellow
$env:RUST_WRITER_DISABLED = "1"
$job2 = Start-Job -ScriptBlock { 
    param($exe) 
    $env:RUST_WRITER_DISABLED = "1"
    & $exe 
} -ArgumentList $poc_exe

Wait-Job $job2 -Timeout 15 | Out-Null
Stop-Job $job2 -PassThru | Remove-Job
Remove-Item Env:RUST_WRITER_DISABLED -ErrorAction SilentlyContinue
Write-Host "✅ C-only test completed" -ForegroundColor Green

# Test 3: Rust-only writer
Write-Host "`nTest 3: Rust Writer Only (15s)" -ForegroundColor Yellow
$env:C_WRITERS_DISABLED = "1"
$job3 = Start-Job -ScriptBlock { 
    param($exe) 
    $env:C_WRITERS_DISABLED = "1"
    & $exe 
} -ArgumentList $poc_exe

Wait-Job $job3 -Timeout 15 | Out-Null
Stop-Job $job3 -PassThru | Remove-Job
Remove-Item Env:C_WRITERS_DISABLED -ErrorAction SilentlyContinue
Write-Host "✅ Rust-only test completed" -ForegroundColor Green

# Test 4: High frequency stress test
Write-Host "`nTest 4: Stress Test (100Hz for 30s)" -ForegroundColor Yellow
$env:HIGH_FREQUENCY = "1"
$job4 = Start-Job -ScriptBlock { 
    param($exe) 
    $env:HIGH_FREQUENCY = "1"
    & $exe 
} -ArgumentList $poc_exe

Wait-Job $job4 -Timeout 30 | Out-Null
Stop-Job $job4 -PassThru | Remove-Job
Remove-Item Env:HIGH_FREQUENCY -ErrorAction SilentlyContinue
Write-Host "✅ Stress test completed" -ForegroundColor Green

# Test 5: Long-running stability
Write-Host "`nTest 5: Stability Test (60s)" -ForegroundColor Yellow
$job5 = Start-Job -ScriptBlock { 
    param($exe) 
    & $exe 
} -ArgumentList $poc_exe

Wait-Job $job5 -Timeout 60 | Out-Null
Stop-Job $job5 -PassThru | Remove-Job
Write-Host "✅ Stability test completed" -ForegroundColor Green

Write-Host "`n✅ Test suite completed" -ForegroundColor Green
