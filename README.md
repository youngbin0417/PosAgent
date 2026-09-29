# PosAgent

PosAgent is a lightweight C graph runtime prototype intended to embed agent workflows in existing C/C++ hosts.

## Current status

The Windows demo verifies a mock model -> registered tool -> model round-trip, plus the minimal graph run. Tool arguments are checked against a documented JSON Schema subset before dispatch. No external model provider/network adapter or generic JSON Schema validator is included yet. See [the verification status](docs/verification-status-ko.md) and [argument validation contract](docs/tool-argument-validation-ko.md).

## Layout

- `include/`: public C API
- `src/`: runtime implementation
- `examples/`: runnable demo
- `tests/`: behavior checks and platform validation scripts
- `docs/`: design, usage, validation, and roadmap documents
- `build/`: generated binaries (ignored by Git)
- `WORKLOG.log`: shared agent handoff log; append dated work and verification entries

## Build and run

With GCC:

```sh
make
make test
make run
```

On Windows PowerShell, run:

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\validate_windows_demo.ps1
```

WSL/Linux helper:

```bash
bash tests/validate_wsl_demo.sh
```

See the [Korean documentation index](docs/README-ko.md), [usage guide](docs/posagent-user-guide-ko.md), and [PoC expansion roadmap](docs/poc-expansion-roadmap-ko.md).
