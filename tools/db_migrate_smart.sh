\
#!/usr/bin/env bash
# db_migrate_smart.sh — Smarter AwakeMUD migrations (Linux/macOS).
# - Creates DB if missing (utf8mb4).
# - Loads base schema if pfiles is missing.
# - Applies bullet_pants.sql only if pfiles_ammo missing.
# - Applies SQL/Migrations/*.sql but suppresses expected duplicate errors.
# Usage: ./tools/db_migrate_smart.sh [-h HOST] [-u USER] [-p] db1 [db2 ...]
set -euo pipefail

HOST="${DB_HOST:-127.0.0.1}"
USER="${DB_USER:-awake}"
ASKPASS=0

usage(){ echo "Usage: $0 [-h HOST] [-u USER] [-p] db1 [db2 ...]"; exit 1; }
while getopts ":h:u:p" opt; do
  case "$opt" in
    h) HOST="$OPTARG";;
    u) USER="$OPTARG";;
    p) ASKPASS=1;;
    *) usage;;
  esac
done; shift $((OPTIND-1))
[[ $# -lt 1 ]] && usage

if [[ $ASKPASS -eq 1 ]]; then read -rsp "Password for $USER@$HOST: " DBPW; echo; export MYSQL_PWD="$DBPW"; fi
if [[ ! -d SQL ]]; then echo "ERROR: Run from repo root (SQL/ not found)."; exit 2; fi
MYSQL=(mysql -h "$HOST" -u "$USER" --protocol=tcp --batch --raw)

for DB in "$@"; do
  echo "=== DB: $DB ==="
  "${MYSQL[@]}" -e "CREATE DATABASE IF NOT EXISTS \`$DB\` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;"
  # base schema
  if ! "${MYSQL[@]}" "$DB" -e "SHOW TABLES LIKE 'pfiles';" | grep -q pfiles; then
    echo "-> Loading SQL/awakemud.sql (base schema)"
    "${MYSQL[@]}" "$DB" < SQL/awakemud.sql
  else
    echo "-> pfiles present; skipping base schema"
  fi
  # bullet_pants
  if ! "${MYSQL[@]}" "$DB" -e "SHOW TABLES LIKE 'pfiles_ammo';" | grep -q pfiles_ammo; then
    if [[ -f SQL/bullet_pants.sql ]]; then
      echo "-> Applying SQL/bullet_pants.sql (pfiles_ammo)"
      "${MYSQL[@]}" "$DB" < SQL/bullet_pants.sql
    fi
  else
    echo "-> pfiles_ammo present; skipping bullet_pants.sql"
  fi
  # migrations bulk apply, but filter expected duplicate messages for readability
  if compgen -G "SQL/Migrations/*.sql" >/dev/null; then
    echo "-> Applying SQL/Migrations/*.sql"
    # Use LC_ALL=C for stable ordering
    while IFS= read -r -d '' f; do
      echo "   - ${f}"
      # run with --force to continue on duplicate errors
      "${MYSQL[@]}" --force "$DB" < "$f" 2> >(grep -Eiv 'already exists|duplicate column|duplicate entry|doesn'\''t exist' >&2 || true)
    done < <(LC_ALL=C find SQL/Migrations -maxdepth 1 -type f -name "*.sql" -print0 | sort -z)
  else
    echo "-> No migrations found."
  fi
  echo "=== Done: $DB ==="
done
