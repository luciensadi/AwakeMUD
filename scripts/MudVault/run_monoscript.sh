#!/usr/bin/env bash
# Cron wrapper for the MudVault monoscript (poll / link / unlink / claim).
#
# The monoscript reads all of its credentials from environment variables, and
# cron provides none -- so this wrapper sources mudvault.env first. See
# DEPLOYMENT.md section 5.
#
# Example crontab entry (every 5 minutes, as the mudvault OS user):
#   */5 * * * * /opt/AwakeMUD/scripts/MudVault/run_monoscript.sh >> /opt/AwakeMUD/log/mudvault_cron.log 2>&1
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Which env file to load: $1 if given, otherwise mudvault.env next to this
# script. To run the cron pass under the least-privileged DB user, copy
# mudvault.env to mudvault-cron.env, swap MYSQL_USER/MYSQL_PASSWORD for the
# cron user's, and invoke: run_monoscript.sh mudvault-cron.env
ENV_FILE="${1:-$SCRIPT_DIR/mudvault.env}"

# Load secrets/credentials. The env file must be owned by the user this runs
# as, mode 600. Never log or echo its contents.
if [ ! -r "$ENV_FILE" ]; then
  echo "run_monoscript.sh: cannot read env file: $ENV_FILE" >&2
  exit 1
fi
set -a
# shellcheck disable=SC1090
source "$ENV_FILE"
set +a

# Prefer the monoscript virtualenv if it exists (it has mysql-connector +
# requests pinned); fall back to system python3.
if [ -x "$SCRIPT_DIR/monoscript/venv/bin/python3" ]; then
  PY="$SCRIPT_DIR/monoscript/venv/bin/python3"
else
  PY=python3
fi

cd "$SCRIPT_DIR"
exec "$PY" -m monoscript.main
