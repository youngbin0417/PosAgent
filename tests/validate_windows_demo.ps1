$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot "build"
$runtime = @((Join-Path $repoRoot "src\posagent.c"), (Join-Path $repoRoot "src\posagent_json.c"), (Join-Path $repoRoot "src\posagent_chat.c"), (Join-Path $repoRoot "src\posagent_chat_transport.c"))
$includeDir = Join-Path $repoRoot "include"
$demoSource = Join-Path $repoRoot "examples\posagent_demo.c"
$chatDemoSource = Join-Path $repoRoot "examples\posagent_chat_demo.c"
$testSource = Join-Path $repoRoot "tests\test_posagent.c"
$chatTestSource = Join-Path $repoRoot "tests\test_posagent_chat.c"
$demo = Join-Path $buildDir "posagent_demo.exe"
$chatDemo = Join-Path $buildDir "posagent_chat_demo.exe"
$test = Join-Path $buildDir "test_posagent.exe"
$chatTest = Join-Path $buildDir "test_posagent_chat.exe"

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

Write-Host "[1/4] Building PosAgent demo..."
gcc -I $includeDir -std=c11 -Wall -Wextra -O2 $runtime $demoSource -o $demo -lwininet
if ($LASTEXITCODE -ne 0) { throw "Demo build failed" }
gcc -I $includeDir -std=c11 -Wall -Wextra -O2 $runtime $chatDemoSource -o $chatDemo -lwininet
if ($LASTEXITCODE -ne 0) { throw "Chat demo build failed" }

Write-Host "[2/4] Running PosAgent demo..."
& $demo
if ($LASTEXITCODE -ne 0) { throw "Demo execution failed" }

Write-Host "[3/4] Building and running behavior tests..."
gcc -I $includeDir -std=c11 -Wall -Wextra -O2 $runtime $testSource -o $test -lwininet
if ($LASTEXITCODE -ne 0) { throw "Test build failed" }
& $test
if ($LASTEXITCODE -ne 0) { throw "Behavior tests failed" }
gcc -I $includeDir -std=c11 -Wall -Wextra -O2 $runtime $chatTestSource -o $chatTest -lwininet
if ($LASTEXITCODE -ne 0) { throw "Chat adapter test build failed" }
& $chatTest
if ($LASTEXITCODE -ne 0) { throw "Chat adapter tests failed" }

Write-Host "[4/4] Success"
