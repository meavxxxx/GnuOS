# GNU OS Public Task Tracker

GNU OS uses a public issue tracker on GitHub:

- https://github.com/meavxxxx/TempOS/issues

## What goes into the tracker

- Bugs (kernel, userspace, build, CI)
- Feature requests
- Roadmap tasks and sub-tasks
- Refactoring and technical debt items

## Issue taxonomy

Use labels to keep the queue searchable:

- Type: `bug`, `enhancement`, `docs`, `test`, `chore`
- Area: `kernel`, `userspace`, `toolchain`, `ci`, `docs`
- Priority: `P0`, `P1`, `P2`
- Status: `triage`, `blocked`, `in-progress`, `ready-for-review`

## Workflow

1. Open issue from the public template.
2. Triage and assign labels.
3. Link issue to roadmap phase/milestone.
4. Reference issue in branch and PR (`Fixes #<id>` when closing).
5. Close issue only after merge and verification.
