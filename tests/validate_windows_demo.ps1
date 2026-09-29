$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot "build"
$runtime = @((Join-Path $repoRoot "src\posagent.c"), (Join-Path $repoRoot "src\posagent_json.c"))
$includeDir = Join-Path $repoRoot "include"
$demoSource = Join-Path $repoRoot "examples\posagent_demo.c"
$testSource = Join-Path $repoRoot "tests\test_posagent.c"
$demo = Join-Path $buildDir "posagent_demo.exe"
$test = Join-Path $buildDir "test_posagent.exe"

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

Write-Host "[1/4] Building PosAgent demo..."
gcc -I $includeDir -std=c11 -Wall -Wextra -O2 $runtime $demoSource -o $demo
if ($LASTEXITCODE -ne 0) { throw "Demo build failed" }

Write-Host "[2/4] Running PosAgent demo..."
& $demo
if ($LASTEXITCODE -ne 0) { throw "Demo execution failed" }

Write-Host "[3/4] Building and running behavior tests..."
gcc -I $includeDir -std=c11 -Wall -Wextra -O2 $runtime $testSource -o $test
if ($LASTEXITCODE -ne 0) { throw "Test build failed" }
& $test
if ($LASTEXITCODE -ne 0) { throw "Behavior tests failed" }

Write-Host "[4/4] Success"
