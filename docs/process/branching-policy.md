# GNU OS Branching Policy

This repository is a single monorepo (`gnuos`) for kernel, userspace, tooling,
tests, and docs.

## Branch model

- `main` is the integration branch and release source.
- Short-lived topic branches are required for all changes.
- Future maintenance branches use `release/*` and `lts/*` naming.

## Topic branch naming

Use one of these prefixes:

- `feat/<scope>-<topic>`
- `fix/<scope>-<topic>`
- `docs/<scope>-<topic>`
- `chore/<scope>-<topic>`
- `refactor/<scope>-<topic>`
- `test/<scope>-<topic>`

Examples:

- `feat/kernel-apic-init`
- `fix/libc-errno-mmap`
- `docs/roadmap-phase2`

## Merge policy

- No direct development on `main` for regular changes.
- All regular changes go through pull requests.
- CI must pass before merge.
- Minimum two approvals are required before merge.
- Squash merge is preferred to keep history readable.
- Force-push is allowed only on your own topic branch.

## Hotfix and release flow

- Urgent fixes branch from `main` as `hotfix/<topic>`.
- After merge to `main`, hotfixes are cherry-picked to active `release/*` or
  `lts/*` branches when they exist.

## Roadmap discipline

- If a roadmap task is completed in code/docs, update the corresponding
  checkbox in `docs/ROADMAP.md` in the same pull request.

