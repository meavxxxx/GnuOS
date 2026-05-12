# GNU OS

GNU OS is a free operating system project with an incremental roadmap from
bootstrapping to userspace and POSIX-oriented tooling.

## Current status

- Kernel boots in QEMU on `x86_64`
- Early memory/IDT/serial bring-up is implemented
- Stage0 userspace ABI scaffolding is in progress
- UEFI stub scaffolding is added (ongoing integration)

## Quick start

```bash
make ARCH=x86_64 kernel
make ARCH=x86_64 image
make ARCH=x86_64 run
```

## Project docs

- Documentation hub: [`docs/README.md`](docs/README.md)
- Full roadmap and checkbox progress: [`docs/ROADMAP.md`](docs/ROADMAP.md)
- Contributor guide: [`CONTRIBUTING.md`](CONTRIBUTING.md)
- Kernel docs: [`kernel/README.md`](kernel/README.md)
- Userspace docs: [`userspace/README.md`](userspace/README.md)

## Feedback

- Discussions: <https://github.com/meavxxxx/TempOS/discussions>
- Issues: <https://github.com/meavxxxx/TempOS/issues>

## License

GNU OS is distributed under GPLv3 for project code unless a subcomponent
specifies a compatible license in its own directory.

---

<div align="center">

**GNU OS — свободная операционная система для свободных людей.**  
"Free as in freedom." — Richard M. Stallman

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)
[![POSIX Compliant](https://img.shields.io/badge/POSIX-1003.1--2017-green.svg)](https://pubs.opengroup.org/onlinepubs/9699919799/)
[![GNU Project](https://img.shields.io/badge/GNU-Project-red.svg)](https://www.gnu.org/)

</div>
