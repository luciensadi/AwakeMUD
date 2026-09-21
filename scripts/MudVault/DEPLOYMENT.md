# MudVault Integration — Deployment Guide (Ubuntu)

Complete, step-by-step instructions for deploying the MudVault voting integration
on an Ubuntu box that already runs the AwakeMUD game and its MySQL database, and
has lighttpd installed.

Do the steps in order. Each ends with a check you can run to confirm it worked
before moving on.

---

## Architecture (what you are deploying)

```
Player votes on mudvault.org
        |
        v
(1) lighttpd (port 80/443, already running)
        |  proxies /mudvault-webhook to loopback:8001
        v
(2) gunicorn systemd service: mudvault-webhooks  [this repo: scripts/MudVault/webhooks/]
        |  validates the HMAC signature, INSERTs the vote into MySQL
        v
(3) MySQL (game DB "AwakeMUD")  <-- the ONLY handoff point
        ^
        |  reads undelivered votes, delivers 1 syspoint in-game (every 30s)
(4) The C++ game (src/mudvault_voting.cpp -- no network access at all)

(5) cron, every 5 min: run_monoscript.sh  [scripts/MudVault/monoscript/]
        polls MudVault for missed rewards, confirms verifications,
        claims rewards the game has already delivered
```

The game process never talks to the network. All HTTP lives in the Python stack;
the game and Python coordinate exclusively through two MySQL tables.

---

## Step 0 — Prerequisites

```bash
sudo apt update
sudo apt install -y python3-venv build-essential libmysqlclient-dev
```

- MySQL server and lighttpd are assumed installed and running (`systemctl status mysql lighttpd`).
- The game already builds (`cd src && make`). The MudVault code is part of the
  normal build; nothing extra to compile.
- Decide on a **dedicated OS user** that will own and run the Python stack
  (gunicorn + cron). The examples below use `mudvault`:

```bash
sudo adduser --system --group --home /opt/mudvault mudvault
```

- Check it works:

```bash
mysqladmin ping && systemctl is-active lighttpd && id mudvault
```

---

## Step 1 — Run the SQL migration

One migration must be applied to the game DB (as your MySQL admin user, e.g. `root`):

```bash
cd /path/to/AwakeMUD
mysql -u root -p AwakeMUD < SQL/Migrations/add_votes.sql
```

What it does:
- Appends `mudvault_verified` and `last_vote_time` to the END of `pfiles`.
  These statements use explicit `AFTER` clauses on purpose — the game reads
  pfiles columns positionally, so these two columns MUST be the last two in
  the table (chained after `RestrictedSysPoints`, the last pre-existing
  column). The game verifies the full tail order at boot and refuses to start
  if it is wrong.
- Creates `mudvault_votes` and `mudvault_character_linking` plus the insert
  trigger that resolves a vote's `idnum` from the character's name (only for
  MudVault-verified characters).

> If you ever see `ERROR: You need to run a migration to add ...` in the game's
> boot log, or `ERROR: ... must be the LAST two columns of pfiles ...`, a
> migration is missing or was applied incorrectly. The error names the exact
> file to run.

Check it worked:

```bash
mysql -u root -p AwakeMUD -e "SHOW TABLES LIKE 'mudvault%'; SHOW COLUMNS FROM pfiles LIKE 'mudvault%';"
```

Expect: `mudvault_votes`, `mudvault_character_linking`, and both new pfiles
columns.

---

## Step 2 — Create the scoped MySQL users

Three database identities are involved: the game's existing user, a new
**web** user (gunicorn), and a new **cron** user (monoscript). Least privilege
matters most for the web user because it is the internet-facing component.

Run these as your MySQL admin user. Pick strong passwords.

```sql
-- The GAME user (whatever src/mysql_config.cpp already uses).
-- Needs read/write on both mudvault tables:
GRANT SELECT, INSERT, UPDATE, DELETE
  ON AwakeMUD.mudvault_votes            TO 'AwakeMUD'@'localhost';
GRANT SELECT, INSERT, UPDATE, DELETE
  ON AwakeMUD.mudvault_character_linking TO 'AwakeMUD'@'localhost';

-- The WEB user (gunicorn). Can ONLY add vote rows; the insert trigger reads
-- pfiles/linking on its behalf. It cannot read or modify anything else.
CREATE USER 'mudvault_web'@'localhost' IDENTIFIED BY '<strong-password>';
GRANT INSERT ON AwakeMUD.mudvault_votes TO 'mudvault_web'@'localhost';
GRANT SELECT (idnum, name) ON AwakeMUD.pfiles TO 'mudvault_web'@'localhost';
GRANT SELECT (idnum, character_name, linking_status)
  ON AwakeMUD.mudvault_character_linking TO 'mudvault_web'@'localhost';

-- The CRON user (monoscript: poll, link, unlink, claim).
CREATE USER 'mudvault_cron'@'localhost' IDENTIFIED BY '<strong-password>';
GRANT SELECT, INSERT, UPDATE ON AwakeMUD.mudvault_votes TO 'mudvault_cron'@'localhost';
GRANT SELECT, UPDATE, DELETE ON AwakeMUD.mudvault_character_linking TO 'mudvault_cron'@'localhost';
GRANT SELECT (idnum, name) ON AwakeMUD.pfiles TO 'mudvault_cron'@'localhost';
FLUSH PRIVILEGES;
```

Time zones: nothing to configure. DATETIMEs in the mudvault tables are stored
as naive SERVER-LOCAL wall time (the MySQL server's default time zone); the
Python stack writes local wall-clock values and the game reads them back in
the same zone. The game double-checks at boot that the DB server's clock
agrees with the game host's local time (if the check fails, boot logs
`MudVault: MySQL server's wall clock disagrees ...` and voting stays disabled
until the DB server's time zone/clock is aligned with the game host's). This
assumes the DB runs on (or in the same time zone as) the game host.

---

## Step 3 — Configure the secrets file

On the [MudVault site](https://mudvault.org):
1. On your MUD's **Edit MUD** page, enable **rewards** and copy the **API key**
   (looks like `mv_17_...`). The whole string minus the `mv_<id>_` wrapper is
   `MUDVAULT_API_KEY`; this deployment's MUD ID is **17**.
2. Enter your **Webhook URL** (see Step 4 for the address) and save. Copy the
   **webhook secret** it gives you.

Create the env file (from the repo copy of the example):

```bash
sudo cp /path/to/AwakeMUD/scripts/MudVault/mudvault.env.example /opt/mudvault/mudvault.env
sudo nano /opt/mudvault/mudvault.env
```

Fill in:

```ini
MUDVAULT_WEBHOOK_SECRET=<secret from the Edit MUD page>
MUDVAULT_API_KEY=<the part after mv_17_>
MUDVAULT_MUD_ID=17
MYSQL_HOST=localhost
MYSQL_USER=mudvault_web
MYSQL_PASSWORD=<web user's password>
MYSQL_DATABASE=AwakeMUD
```

Lock it down — the `mudvault` OS user runs both gunicorn and cron, so it is the
only reader:

```bash
sudo chown mudvault:mudvault /opt/mudvault/mudvault.env
sudo chmod 600 /opt/mudvault/mudvault.env
```

> The cron user's DB password is used in Step 5's wrapper config below — keep
> both passwords handy.

Check it worked: `sudo -u mudvault grep -c '=' /opt/mudvault/mudvault.env` prints 7.

---

## Step 4 — Webhook receiver (gunicorn + systemd + lighttpd)

### 4a. Install the code and virtualenv

```bash
sudo mkdir -p /opt/mudvault
sudo cp -r /path/to/AwakeMUD/scripts/MudVault /opt/mudvault/app
sudo chown -R mudvault:mudvault /opt/mudvault/app
sudo -u mudvault python3 -m venv /opt/mudvault/app/webhooks/venv
sudo -u mudvault /opt/mudvault/app/webhooks/venv/bin/pip install \
  -r /opt/mudvault/app/webhooks/requirements.txt
```

### 4b. Install the systemd service

```bash
sudo cp /opt/mudvault/app/webhooks/mudvault_webhooks.service /etc/systemd/system/mudvault-webhooks.service
sudo nano /etc/systemd/system/mudvault-webhooks.service
```

Edit the template to match reality:

- `User=mudvault`, `Group=mudvault`
- `WorkingDirectory=/opt/mudvault/app`
- `ExecStart=/opt/mudvault/app/webhooks/venv/bin/gunicorn --workers 3 --bind 127.0.0.1:8001 ... webhooks.webhooks:app`
- `EnvironmentFile=/opt/mudvault/mudvault.env`

**Bind to loopback (`127.0.0.1:8001`).** Gunicorn must never be directly
reachable from the internet — only lighttpd should talk to it, or clients can
spoof `X-Forwarded-For` and bypass the rate limit. If you scale workers, note
the in-memory rate limit is per-worker (N workers = N x 5 req/sec).

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now mudvault-webhooks
systemctl status mudvault-webhooks
```

Check it worked:

```bash
curl -s -o /dev/null -w '%{http_code}\n' -X POST http://127.0.0.1:8001/mudvault-webhook
# Expect 401 (missing signature) -- proves the service is up and validating.
journalctl -u mudvault-webhooks -n 20
```

### 4c. lighttpd proxy

Add to your lighttpd config (e.g. `/etc/lighttpd/lighttpd.conf` or a conf.d
snippet — the same block is documented in the header of `webhooks.py`):

```conf
server.modules += ( "mod_proxy" )

$HTTP["url"] =~ "^/mudvault-webhook$" {
  proxy.server = ( "" => ( ( "host" => "127.0.0.1", "port" => 8001 ) ) )
}
```

If lighttpd itself sits behind another load balancer, also enable `mod_extforward`
per the comment block in `webhooks.py`. Keep port 8001 firewalled from the
outside (`sudo ufw deny 8001/tcp` if you use ufw).

```bash
sudo systemctl restart lighttpd
```

Check it worked (from outside the box):

```bash
curl -s -o /dev/null -w '%{http_code}\n' -X POST https://yourmud.example.com/mudvault-webhook
# Expect 401 again, this time through lighttpd.
```

Finally, make sure the **Webhook URL** registered on mudvault.org is
`https://yourmud.example.com/mudvault-webhook`.

### 4d. Send yourself a signed test webhook

```bash
sudo -u mudvault bash -c '
  set -a; source /opt/mudvault/mudvault.env; set +a
  python3 - <<"EOF"
import hashlib, hmac, json, os, urllib.request
body = json.dumps({
  "reward_id": "11111111-2222-3333-4444-555555555555",
  "vote_id": "66666666-7777-8888-9999-000000000000",
  "character_name": "Testchar",
  "reward_token": "de713c9befb04144910a541ed36a9dda",
  "voted_at": "2026-09-21T12:00:00+00:00",
  "mud_id": 17,
  "mud_name": "AwakeMUD",
}).encode()
sig = "sha256=" + hmac.new(os.environ["MUDVAULT_WEBHOOK_SECRET"].encode(), body, hashlib.sha256).hexdigest()
req = urllib.request.Request("http://127.0.0.1:8001/mudvault-webhook", data=body,
                             headers={"Content-Type": "application/json", "X-MudVault-Signature": sig})
print(urllib.request.urlopen(req).status)
EOF'
```

- `202` = accepted and inserted (check with
  `mysql -u root -p AwakeMUD -e "SELECT character_name, reward_id_hex FROM mudvault_votes;"`).
  The row will just sit there undelivered — `Testchar` is not a real character.
  Delete it: `DELETE FROM AwakeMUD.mudvault_votes WHERE character_name='Testchar';`
- `403` = signature wrong (env secret mismatch). `400` = payload problem.
- Tamper with one character of the body and re-send to see `403` — that proves
  forgery is rejected.

---

## Step 5 — Cron worker (monoscript, every 5 minutes)

The wrapper script loads `mudvault.env` (cron provides no environment) and runs
one pass: unlink -> link -> claim -> poll.

```bash
sudo chmod +x /opt/mudvault/app/run_monoscript.sh
sudo -u mudvault crontab -e
```

Add:

```
*/5 * * * * /opt/mudvault/app/run_monoscript.sh >> /opt/mudvault/cron.log 2>&1
```

To run the cron pass under the least-privileged DB user (`mudvault_cron`
instead of the web user), copy `mudvault.env` to `mudvault-cron.env`, swap in
the cron user's `MYSQL_USER`/`MYSQL_PASSWORD`, and change the crontab line to
`run_monoscript.sh mudvault-cron.env` (the wrapper takes the env file as an
optional argument).

Check it worked:

```bash
sudo -u mudvault /opt/mudvault/app/run_monoscript.sh
tail /opt/mudvault/cron.log
# Expect "Beginning run of MudVault monoscript." ... "run completed."
```

---

## Step 6 — The game

Nothing to install — just make sure the integration is compiled in and rebuild.
The whole feature is gated behind the `-DMUDVAULT_VOTING` compiler flag (set in
`src/Makefile` on all deployment types; remove it there to ship with voting
disabled — players then get a friendly "Voting rewards are not enabled on this
server" message from `vote`/`verify`, and the game skips the MudVault boot
checks entirely):

```bash
grep -n MUDVAULT_VOTING /path/to/AwakeMUD/src/Makefile   # must be present
cd /path/to/AwakeMUD/src && make
# restart the game per your usual procedure (autorun scripts / systemd)
```

Check it worked — the boot log must contain:

```
MudVault: voting integration enabled (mud id 17).
```

If instead the game **refuses to boot**, it is one of the startup guards doing
its job; the message tells you which migration to run (Step 1). If it logs
`MudVault: MySQL server's wall clock disagrees ...`, the DB server's time zone
or clock does not match the game host's (or something forces a different
session zone via `init_connect` in `my.cnf`); voting stays disabled until
resolved.

---

## Step 7 — End-to-end test

1. In game: `vote` — should explain profile linking.
2. Get a verification code at `https://mudvault.org/profile` as your character's
   name (exact capitalization), then in game: `verify <code>`.
3. Within ~5 minutes (cron), linking completes; the game tells you in-game and
   `vote` now shows the vote link.
4. Vote at `https://mudvault.org/?id=17`.
5. Watch it flow:

```bash
mysql -u root -p AwakeMUD -e \
  "SELECT character_name, reward_id_hex, voted_at, redeemed_at, claimed_at FROM mudvault_votes\G"
```

`redeemed_at` fills within ~30 seconds of voting (game heartbeat delivers the
syspoint in-game), and `claimed_at` within ~5 minutes (cron claims). In game you
will see the "vote received" message with the 1-system-point award, and `vote`
now shows your cooldown.

---

## Troubleshooting

| Symptom | Cause / fix |
|---|---|
| Game boot aborts: "You need to run a migration to add ..." | Run the migration named in the message (Step 1). |
| Game boot aborts: "... must be the LAST two columns of pfiles" | The pfiles migration was applied with an `AFTER` clause or columns were reordered. Fix the column order. |
| Boot log: "MySQL server's wall clock disagrees ..." | DB server's time zone/clock doesn't match the game host's (check `init_connect` in `my.cnf`). Voting stays disabled. |
| Boot log: "malformed API key, voting integration disabled" | `mudvault_api_key` in `src/mysql_config.cpp` (gitignored) is missing or not `mv_17_...`. |
| Webhook returns 403 | Signature mismatch: `MUDVAULT_WEBHOOK_SECRET` in `mudvault.env` must exactly match the secret MudVault issued for the webhook URL. Restart the service after changing it. |
| Webhook returns 400 "Wrong MUD ID" | The payload's `mud_id` doesn't match `MUDVAULT_MUD_ID=17`. |
| Player says they voted, nothing happened | Check the row exists in `mudvault_votes` (`voted_at`/`redeemed_at`/`claimed_at` tell you which stage stalled): no row -> webhook/poll intake failed (journalctl + cron.log); row without `redeemed_at` -> player was offline when it arrived; it delivers at their next login, or within 30s while online; row without `claimed_at` -> cron/claim issue (cron.log). |
| `polling` log shows "character_name is not a MudVault-verified character" | The trigger rejected an insert — normal during a deleted character's name transition; it retries and succeeds once the name's new owner verifies. |
| cron.log empty | Wrapper not running: check `sudo -u mudvault crontab -l` and that the log path is writable by `mudvault`. |

## Security checklist (recap)

- `mudvault.env`: owned by `mudvault`, mode 600; never committed (gitignored).
- Web MySQL user: INSERT-only on `mudvault_votes` (+ trigger reads). Cron and
  game users get only the table grants shown in Step 2.
- Gunicorn bound to loopback, port 8001 firewalled; internet only via lighttpd.
- HMAC signature is verified before any payload parsing; forgeries are logged
  and rejected with 403.
- The game itself has no network code — a MudVault outage can never lag the game.
