# GNU Mailman Dev Setup

This directory provides a local GNU Mailman 3 stack for GNU OS development.

## Prerequisites

- Docker Desktop (Windows) with WSL integration enabled.
- `docker compose` plugin available.

## Quick start (PowerShell in repo root)

```powershell
Copy-Item .\scripts\dev\mailman\env.example .\scripts\dev\mailman\.env
docker compose -f .\scripts\dev\mailman\docker-compose.yml --env-file .\scripts\dev\mailman\.env up -d
```

Web UI is available at:

- http://127.0.0.1:8001

## Stop and cleanup

```powershell
docker compose -f .\scripts\dev\mailman\docker-compose.yml --env-file .\scripts\dev\mailman\.env down
```

To reset local state:

```powershell
Remove-Item -Recurse -Force .\scripts\dev\mailman\data
```

## Notes

- Default compose config is for local development only.
- For public deployment, front with HTTPS reverse proxy and real DNS domain.
- Track list/topic policy and moderation decisions in `docs/process/task-tracker.md`.
