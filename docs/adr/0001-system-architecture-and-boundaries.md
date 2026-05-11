# ADR-0001: System Architecture And Component Boundaries

- Status: Accepted
- Date: 2026-05-11
- Deciders: GNU OS maintainers
- Related roadmap item: 0.4 Архитектурный документ (ADR)

## Context

GNU OS needs an explicit architecture baseline so all roadmap phases follow the
same kernel/userspace boundary and compatibility targets.

## Decision

- Kernel architecture is microkernel-first with minimal ring0 surface.
- Core kernel responsibilities:
  - scheduling and context management,
  - memory management primitives,
  - interrupt/exception handling,
  - capability-aware IPC substrate.
- Policy-heavy services should prefer userspace servers over kernel growth.
- Userspace ABI targets POSIX compatibility with staged rollout.
- Primary architecture target is `x86_64`, with planned expansion to
  `aarch64` and `riscv64`.

## Consequences

- Kernel changes are evaluated against microkernel boundary discipline.
- New subsystems should expose narrow interfaces and prefer composable services.
- ABI and libc work can proceed incrementally while keeping long-term POSIX
  direction stable.

## Alternatives considered

- Monolithic kernel model with broad in-kernel services.
- Hybrid model with large in-kernel policy subsystems from early phases.

Both alternatives were rejected to keep isolation and long-term maintenance
costs aligned with project goals.
