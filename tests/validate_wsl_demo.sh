#!/usr/bin/env bash
set -eu

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"
mkdir -p build

printf '[1/4] Building PosAgent demo on WSL/Linux...\n'
gcc -I include -std=c11 -Wall -Wextra -O2 src/posagent.c src/posagent_json.c examples/posagent_demo.c -o build/posagent_demo

printf '[2/4] Running PosAgent demo on WSL/Linux...\n'
./build/posagent_demo

printf '[3/4] Building and running behavior tests...\n'
gcc -I include -std=c11 -Wall -Wextra -O2 src/posagent.c src/posagent_json.c tests/test_posagent.c -o build/test_posagent
./build/test_posagent

printf '[4/4] Success\n'
