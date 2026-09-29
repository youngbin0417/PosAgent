#!/usr/bin/env bash
set -eu

cd /mnt/c/Projects/posagent

echo "[1/3] Building PosAgent demo on WSL..."
gcc -std=c11 -Wall -Wextra -O2 posagent_demo.c -o posagent_demo

if [ $? -ne 0 ]; then
  echo "Build failed"
  exit 1
fi

echo "[2/3] Running PosAgent demo on WSL..."
./posagent_demo

if [ $? -ne 0 ]; then
  echo "Execution failed"
  exit 1
fi

echo "[3/3] Success"
