# FAQ

## Why is the kernel currently booting in QEMU first?

QEMU gives fast iteration for early bring-up, debugging, and CI automation
before scaling to real hardware validation.

## Why are many libc APIs still stubs?

Roadmap phases intentionally stage ABI first, then full behavior. This keeps
linking and userspace bootstrapping unblocked while kernel capabilities mature.

## Why keep roadmap checkbox updates in each change?

It makes execution status auditable and prevents roadmap drift between code and
documentation.

## Is GNU Mailman required now?

A local GNU Mailman stack is provided for development in `scripts/dev/mailman`.
Production deployment still needs DNS, HTTPS, and ops hardening.
