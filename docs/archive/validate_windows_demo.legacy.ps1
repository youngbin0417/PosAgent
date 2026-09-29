$source = ".\posagent_demo.c"
$output = ".\posagent_demo.exe"

Write-Host "[1/3] Building PosAgent demo..."
gcc -std=c11 -Wall -Wextra -O2 $source -o $output
if ($LASTEXITCODE -ne 0) {
    throw "Build failed"
}

Write-Host "[2/3] Running PosAgent demo..."
& $output
if ($LASTEXITCODE -ne 0) {
    throw "Execution failed"
}

Write-Host "[3/3] Success"
