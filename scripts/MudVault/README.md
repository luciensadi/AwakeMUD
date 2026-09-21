# MudVault Integration

Python tooling for the [MudVault](https://mudvault.org) voting rewards API, wired
into AwakeMUD through the shared game database (MySQL db `AwakeMUD`). The game's
pfiles table, `mudvault_votes`, and `mudvault_character_linking` all live in that
game DB; this stack and the C++ game read/write the same tables.

> **Deploying or upgrading? Start with [DEPLOYMENT.md](DEPLOYMENT.md)** — it is
> the complete, step-by-step Ubuntu guide (migrations, scoped MySQL users,
> secrets, the gunicorn systemd service, lighttpd proxy config, cron, and an
> end-to-end test). This README is the conceptual overview.
>
> The C++ side is gated behind the `-DMUDVAULT_VOTING` compiler flag
> (`src/Makefile`); without it the voting commands respond with a friendly
> "not enabled" message and the game never touches the mudvault tables.

## Layout

- `webhooks/`: a gunicorn-served Flask receiver for the incoming reward webhook,
  fronted by lighttpd `mod_proxy`. Deployment and lighttpd config notes are in
  the comment block at the top of `webhooks/webhooks.py`. It validates the
  `X-MudVault-Signature` HMAC and INSERTs rewards into `mudvault_votes`.
- `monoscript/`: a cron-driven worker. Run it every 5 minutes via the wrapper
  script (`run_monoscript.sh`, which sources `mudvault.env` since cron provides
  no environment) — exact steps in DEPLOYMENT.md.
- `mudvault.env.example`: template for the credentials/config above. Copy it to
  `mudvault.env` (outside the repo in production), fill in real values, `chmod
  600`, and own it by the user that runs the stack.
- `utils/database.py`: shared DB access layer used by both halves.

## Configuration

See `mudvault.env.example`. It covers `MUDVAULT_WEBHOOK_SECRET` (from the Edit
MUD page when configuring the webhook URL), `MUDVAULT_API_KEY` (issued when
enabling rewards; the worker composes the `X-API-Key` header as
`mv_<MUD_ID>_<API_KEY>`), `MUDVAULT_MUD_ID` (17 for this deployment), and the
MySQL credentials. Scope your MySQL users: the web service's user should be
INSERT-only on `mudvault_votes` (plus SELECT on pfiles for the trigger), while
the cron/monoscript user needs read/write on the `mudvault_*` tables.

## Reward lifecycle

1. A player votes on mudvault.org; MudVault POSTs the reward webhook to
   `webhooks/`, which stores the row in `mudvault_votes` (undelivered). The
   `polling` module also polls `GET /api/rewards/pending/<mud_id>` as a
   catch-up path and stores anything it finds the same way.
2. The C++ game reads undelivered rows from `mudvault_votes` and hands out the
   reward in-game, then sets `redeemed_at` on the row to mark delivery.
3. `claim` (in `monoscript`) selects only delivered-and-unclaimed rewards
   (`redeemed_at` set), POSTs `https://mudvault.org/api/rewards/claim/<reward_id>`
   for each, and marks them claimed locally on success. Rewards the game has not
   yet delivered are never claimed upstream.

## Character linking lifecycle

- **Link**: the game INSERTs `mudvault_character_linking` rows with status
  `needs_linking`. `link` POSTs
  `https://mudvault.org/api/rewards/verify/confirm` with
  `{"verification_code", "character_name"}` (character_name is CASE-SENSITIVE —
  always sent exactly as stored in the DB). On HTTP 200 it sets
  `linking_succeeded`; on HTTP 400 (invalid/expired code) it sets
  `linking_failed` so the game lets the player retry with a fresh code. The game
  later reads those statuses and acks processed rows as `confirmed`.
- **Unlink**: the game sets a row to `needs_unlinking`; `unlink` issues
  `DELETE https://mudvault.org/api/rewards/characters/unlink?character_name=<name>`
  and, on success, marks the row `unlinking_succeeded`.

`monoscript/main.py` runs one pass per invocation in the order
unlink → link → claim → poll; schedule it with cron or a systemd timer.
