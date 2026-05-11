# Fuzz Tests

Current harnesses:

- `fuzz_ldso_dlfcn.c` - libFuzzer smoke target for stage0 `ldso_dlfcn`.

Run locally from repo root:

```bash
bash scripts/ci/fuzz.sh
```
