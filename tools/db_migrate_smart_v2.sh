\
#!/usr/bin/env bash
# db_migrate_smart_v2.sh — Smarter AwakeMUD migrations with extra standalone files.
# Handles:
#   - Base schema: SQL/awakemud.sql  (if pfiles missing)
#   - Standalone:  SQL/bullet_pants.sql     -> pfiles_ammo
#   - Standalone:  SQL/ignore_system_v2.sql -> pfiles_ignore_v2
#   - Directory:   SQL/Migrations/*.sql     -> in sorted order
# Usage:
#   ./tools/db_migrate_smart_v2.sh [-h HOST] [-u USER] [-p] db1 [db2 ...]
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

apply_if_missing() {
  local db="$1" table="$2" file="$3"
  if ! "${MYSQL[@]}" "$db" -e "SHOW TABLES LIKE '${table}';" | grep -q "${table}"; then
    if [[ -f "${file}" ]]; then
      echo "-> Applying ${file} (creates ${table})"
      "${MYSQL[@]}" "$db" < "${file}"
    else
      echo "!! ${file} not found; cannot create ${table}"
    fi
  else
    echo "-> ${table} present; skipping ${file}"
  fi
}

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
  # standalone migrations
  apply_if_missing "$DB" "pfiles_ammo" "SQL/bullet_pants.sql"
  apply_if_missing "$DB" "pfiles_ignore_v2" "SQL/ignore_system_v2.sql"
  # directory migrations (filtered noise)
  if compgen -G "SQL/Migrations/*.sql" >/dev/null; then
    echo "-> Applying SQL/Migrations/*.sql"
    while IFS= read -r -d '' f; do
      echo "   - ${f}"
      "${MYSQL[@]}" --force "$DB" < "$f" 2> >(grep -Eiv 'already exists|duplicate column|duplicate entry|doesn'\''t exist' >&2 || true)
    done < <(LC_ALL=C find SQL/Migrations -maxdepth 1 -type f -name "*.sql" -print0 | sort -z)
  else
    echo "-> No migrations found."
  fi
  echo "=== Done: $DB ==="
done
