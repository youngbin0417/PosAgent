#!/usr/bin/env bash
set -eu

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"
mkdir -p build

printf '[1/4] Building PosAgent demo on WSL/Linux...\n'
gcc -I include -std=c11 -Wall -Wextra -O2 src/posagent.c src/posagent_json.c src/posagent_chat.c src/posagent_chat_transport.c examples/posagent_demo.c -o build/posagent_demo
gcc -I include -std=c11 -Wall -Wextra -O2 src/posagent.c src/posagent_json.c src/posagent_chat.c src/posagent_chat_transport.c examples/posagent_chat_demo.c -o build/posagent_chat_demo

printf '[2/4] Running PosAgent demo on WSL/Linux...\n'
./build/posagent_demo

printf '[3/4] Building and running behavior tests...\n'
gcc -I include -std=c11 -Wall -Wextra -O2 src/posagent.c src/posagent_json.c src/posagent_chat.c src/posagent_chat_transport.c tests/test_posagent.c -o build/test_posagent
./build/test_posagent
gcc -I include -std=c11 -Wall -Wextra -O2 src/posagent.c src/posagent_json.c src/posagent_chat.c src/posagent_chat_transport.c tests/test_posagent_chat.c -o build/test_posagent_chat
./build/test_posagent_chat

printf '[4/4] Success\n'
