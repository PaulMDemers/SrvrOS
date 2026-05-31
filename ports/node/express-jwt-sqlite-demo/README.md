# srvros Express/JWT/SQLite Demo

This package is the Node application smoke target for the srvros port.

It installs real `express` and `jsonwebtoken` dependencies on the host side so
the dependency tree is visible and ready for the eventual full Node runtime.
The srvros runtime path currently uses a bundled dependency-light server shape
over `http.createServer()` because srvros Node is still built without
OpenSSL/`crypto`, npm, native addons, and `node:sqlite`.

The API exposes `GET /health`, `POST /token`, `POST /users`, `GET /users`,
and `GET /secure` with `Authorization: Bearer <token>`.

The database adapter is async and writes to `/fat/express-demo.sqlite`.
Once srvros Node is rebuilt with `node:sqlite`, the adapter can switch to real
SQLite statements without changing the HTTP API.

Smoke test from the repo root:

```powershell
python tools\node_express_demo_smoke.py --skip-build --skip-app-build
```
