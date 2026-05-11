# Contributing to GNU OS

Thanks for helping build GNU OS.

## Development flow

1. Create a feature branch from `main`.
2. Keep commits focused and use Conventional Commits (`feat:`, `fix:`, `docs:`).
3. Run local checks before opening a merge request.
4. Open MR with a clear summary, risk notes, and test evidence.

## Branching policy

Branching and merge rules are defined in
[`docs/process/branching-policy.md`](docs/process/branching-policy.md).

Public task tracker rules are defined in
[`docs/process/task-tracker.md`](docs/process/task-tracker.md).

## Pull request requirements

- CI must pass.
- New behavior needs tests when possible.
- Kernel-impacting changes require security review.
- At least two approvals are required before merge.
- After completing roadmap items, update checkboxes in `README.md` in the same change set.

## Coding rules

- C code follows GNU style and project `.clang-format`.
- Public interfaces must be documented in headers.
- Line length limit: 100.
- `goto` is allowed only for structured cleanup in kernel error paths.

## Local quick start

```bash
make ARCH=x86_64 kernel
make ARCH=x86_64 image
make ARCH=x86_64 run
```
