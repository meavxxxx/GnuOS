# Test Suites

Top-level test organization.

## Suites

- [`tests/unit/`](unit): host-side unit checks
- [`tests/integration/`](integration): integration flows (QEMU-driven targets)
- [`tests/fuzz/`](fuzz): fuzz harnesses (libFuzzer smoke flows)
- [`tests/posix/`](posix): POSIX conformance scaffolding

## Common commands

- Coverage pipeline: `bash scripts/ci/coverage.sh`
- Fuzz smoke pipeline: `bash scripts/ci/fuzz.sh`
- Static analysis pipeline: `bash scripts/ci/static-analysis.sh`
