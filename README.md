# Broken Access Control & RBAC Implementation

A working web application built in **C++17** using the `cpp-httplib` single-header library that demonstrates **Broken Access Control (OWASP A01 — #1 web vulnerability)** and fixes it using a manually implemented **Role-Based Access Control (RBAC)** system.

## What This Project Does

The app runs two sets of routes side by side — one **vulnerable**, one **secure** — so you can see the attack and the defense in the same browser.

| Phase | Route Pattern | Behavior |
|-------|--------------|----------|
| Phase 1 — Attack | `/vulnerable/*` | No server-side checks — anyone can access anything |
| Phase 2 — Defense | `/secure/*` | RBAC middleware enforces role and ownership checks |

## Attack Types Demonstrated

**Vertical Privilege Escalation**
- Charlie (Employee) types `/vulnerable/admin` directly in the browser
- Server has no role check → serves the Admin Dashboard to everyone
- On `/secure/admin` → `require_role()` blocks with **403 Forbidden**

**Horizontal Privilege Escalation (IDOR)**
- Charlie (id=3) changes the URL from `/vulnerable/messages/3` to `/vulnerable/messages/1`
- Server has no ownership check → serves Alice's private messages
- On `/secure/messages/1` → `require_ownership_or_admin()` blocks with **403 Forbidden**

## How to Run

**Requirements:** Windows with [MinGW (g++)](https://www.mingw-w64.org/) installed and added to PATH.

```bash
# Step 1 — Compile
build.bat

# Step 2 — Run
server.exe

# Step 3 — Open browser
http://localhost:8081
```
## Users & Roles

| ID | Name | Role |
|----|------|------|
| 1 | Alice Admin | Admin |
| 2 | Bob Manager | Manager |
| 3 | Charlie Employee | Employee |
| 4 | Dave Employee | Employee |

Login is simulated — click a user's name on the homepage to set a session cookie.

## RBAC Permission Matrix

| Resource | Admin | Manager | Employee |
|----------|-------|---------|----------|
| `/vulnerable/admin` | ✅ | ✅ | ✅ |
| `/vulnerable/messages/:id` | ✅ | ✅ | ✅ |
| `/secure/admin` | ✅ | ❌ | ❌ |
| `/secure/messages/own id` | ✅ | ✅ | ✅ |
| `/secure/messages/other id` | ✅ | ❌ | ❌ |

> The first two rows are intentionally open — they represent the vulnerability.

## Key Functions

```cpp
// Protects /secure/admin — Admin role only
bool require_role(req, res, allowed_roles, out_user);

// Protects /secure/messages/:id — own data or Admin
bool require_ownership_or_admin(req, res, target_id, out_user);
```

Both functions read the `session_user_id` cookie, look up the user, and return `403 Forbidden` if the check fails. They are called at the top of every protected route handler — this is the **middleware pattern**.

## Tech Stack

| Component | Technology |
|-----------|-----------|
| Language | C++17 |
| HTTP Server | [cpp-httplib](https://github.com/yhirose/cpp-httplib) (single header) |
| Session | HTTP Cookie (`session_user_id`) |
| Database | In-memory `std::map` (mock) |
| Frontend | Plain HTML + CSS (served from `index.html`) |

## Security Concept

> **Hiding a link is not the same as locking the door.**

The vulnerable version hides the Admin link from non-admin users in the UI — but the route itself is fully accessible to anyone who types the URL. The secure version enforces access on the **server side** before any data is served.

This project demonstrates why client-side hiding is never a substitute for server-side authorization.

## Academic Context

- **Course:** Information Security — PUCIT
- **Instructor:** Huzaifa Nazir
- **Student:** Minahil Shahzadi (BITF24M009)
- **Topic:** OWASP A01:2021 — Broken Access Control
