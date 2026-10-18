# Travlr Getaways (Astro)

The CS 465 Travlr Getaways app, rebuilt on AstroJS for the CS 499 capstone.
Migration status: **all phases written** — public pages, REST API, authentication,
route guard and admin pages. Express, Handlebars and Angular are replaced by one Astro app.

## Setup

```bash
npm install
cp .env.example .env     # then set DB_HOST and JWT_SECRET (16+ characters)
npm run seed             # loads data/trips.json into MongoDB (database: travlr)
npm run dev              # http://localhost:4321
```

Checks: `npm test` (unit tests), `npm run check` (types), `npm run build && npm start` (production).

## First administrator

If the `users` collection is empty, create the first account once:

```bash
curl -X POST http://localhost:4321/api/register -H "Content-Type: application/json" \
  -d '{"name":"Your Name","email":"you@example.com","password":"at-least-12-characters"}'
```

After that, registering requires a signed-in admin. Then sign in at `/admin/login`.
Accounts from the old Express app still work; their passwords are upgraded to scrypt on first login.

## API

| Method and path | Auth | Result |
| --- | --- | --- |
| `GET /api/trips` | public | array of trips |
| `GET /api/trips/:code` | public | one trip, or 404 (was a one-item array) |
| `POST /api/trips` | admin | 201 created, 400 invalid, 409 duplicate code |
| `PUT /api/trips/:code` | admin | 200 with the updated trip (was 201) |
| `DELETE /api/trips/:code` | admin | 200, or 404 |
| `POST /api/login` | public | sets the session cookie, returns `{ token }` |
| `POST /api/logout` | any | clears the cookie, redirects to `/admin/login` |
| `POST /api/register` | admin, or first user | 201 `{ _id, name, email }` |

Send the token as the `travlr-token` cookie (browsers do this automatically) or as `Authorization: Bearer <token>`.

## Layout

| Path | Purpose |
| --- | --- |
| `src/middleware.ts` | One guard for `/admin/*` and `/api/*` (rules in `src/lib/guard.ts`) |
| `src/layouts/`, `src/components/` | BaseLayout, AdminLayout, Header, Footer, TripCard, TripForm |
| `src/pages/` | Public pages, `admin/` pages, `api/` endpoints |
| `src/lib/` | `db`, `trips`, `users`, `session`, `auth` (hash + tokens), `guard`, `validate`, `sanitize`, `ratelimit`, `config`, `http`, `nav` |
| `src/models/` | Mongoose schemas for trips and users |
| `test/` | `node:test` unit tests for auth, guard, validation, sanitizing, rate limiting |
| `public/` | CSS, images, and the template pages not yet rebuilt |
| `scripts/seed.mjs`, `data/trips.json` | Sample data loader |

## Security changes from the original

- Rejected requests end in the middleware; the original called the next handler after a 401.
- Token lives in an httpOnly, SameSite=strict cookie instead of localStorage.
- scrypt for new passwords; JWT algorithm pinned to HS256 with constant-time comparison.
- Registration is no longer open to the public.
- Sign-in is rate limited (5 failures per 15 minutes per address and email) with one generic error message.
- Trip input is validated, and descriptions are reduced to an allow-list of formatting tags.
- Tokens signed by the old app with the same `JWT_SECRET` remain valid until they expire (1 hour). Rotate the secret when cutting over.
