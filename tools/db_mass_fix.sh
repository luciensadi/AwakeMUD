#!/usr/bin/env bash
# tools/db_mass_fix.sh -- Apply base schema, migrations, and integer-width compatible ALTERs across many DBs.
# Usage:
#   tools/db_mass_fix.sh [-h HOST] [-u USER] [-p] [--fresh] db1 [db2 ...]
# Behavior:
#   - Creates DBs if missing (utf8mb4).
#   - Loads SQL/awakemud.sql if pfiles missing.
#   - Runs SQL/bullet_pants.sql if pfiles_ammo missing.
#   - Applies all SQL/Migrations/*.sql sorted.
#   - Runs targeted ALTERs to set integer columns to expected types:
#       pfiles_inv.Attempt      -> MEDIUMINT(6)
#       pfiles_worn.Attempt     -> MEDIUMINT(6)
#       pfiles_drugs.LastFix    -> BIGINT(12) UNSIGNED
#   Note: On MySQL 8+, display widths are ignored; SHOW COLUMNS will still show widthless types.
#         Compile server with -DUSE_MYSQL_8 so startup checks accept normalized integer types.

set -euo pipefail

HOST="127.0.0.1"
USER="awake"
ASKPASS=0
FRESH=0

usage(){ echo "Usage: $0 [-h HOST] [-u USER] [-p] [--fresh] db1 [db2 ...]"; exit 1; }

# Parse options
while getopts ":h:u:p-" opt; do
  case "$opt" in
    h) HOST="$OPTARG" ;;
    u) USER="$OPTARG" ;;
    p) ASKPASS=1 ;;
    -) case "${OPTARG}" in fresh) FRESH=1 ;; *) usage ;; esac ;;
    *) usage ;;
  esac
done
shift $((OPTIND-1))

[[ $# -lt 1 ]] && usage
[[ ! -d SQL ]] && { echo "Run from repo root (SQL/ missing)."; exit 2; }

# Password handling
if [[ $ASKPASS -eq 1 ]]; then
  read -rsp "Password for $USER@$HOST: " DBPW; echo
  export MYSQL_PWD="$DBPW"
elif [[ -n "${DB_PASS:-}" ]]; then
  export MYSQL_PWD="$DB_PASS"
fi

MYSQL=(mysql -h "$HOST" -u "$USER" --protocol=tcp --force --batch)

apply(){ local db="$1" file="$2"; echo "==> $db < $file"; "${MYSQL[@]}" "$db" < "$file" || echo "!! (non-fatal) mysql error in $file"; }

alter_sql(){
  local db="$1"
  # Attempt columns (safe if already correct)
  "${MYSQL[@]}" "$db" -e "ALTER TABLE pfiles_inv  MODIFY Attempt MEDIUMINT(6) DEFAULT 0;" || true
  "${MYSQL[@]}" "$db" -e "ALTER TABLE pfiles_worn MODIFY Attempt MEDIUMINT(6) DEFAULT 0;" || true
  # LastFix bigint width (will be normalized on MySQL 8, but harmless)
  "${MYSQL[@]}" "$db" -e "ALTER TABLE pfiles_drugs MODIFY LastFix BIGINT(12) UNSIGNED DEFAULT 0;" || true
}

for DB in "$@"; do
  echo "=== DB: $DB ==="
  if [[ $FRESH -eq 1 ]]; then
    "${MYSQL[@]}" -e "DROP DATABASE IF EXISTS \`$DB\`;"
  fi
  "${MYSQL[@]}" -e "CREATE DATABASE IF NOT EXISTS \`$DB\` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;"
  # Base schema check
  if ! "${MYSQL[@]}" "$DB" -e "SHOW TABLES LIKE 'pfiles';" | grep -q pfiles; then
    apply "$DB" "SQL/awakemud.sql"
  fi
  # bullet_pants
  if ! "${MYSQL[@]}" "$DB" -e "SHOW TABLES LIKE 'pfiles_ammo';" | grep -q pfiles_ammo; then
    [[ -f SQL/bullet_pants.sql ]] && apply "$DB" "SQL/bullet_pants.sql"
  fi
  # Migrations
  if compgen -G "SQL/Migrations/*.sql" > /dev/null; then
    while IFS= read -r -d '' f; do apply "$DB" "$f"; done < <(LC_ALL=C find SQL/Migrations -maxdepth 1 -type f -name "*.sql" -print0 | sort -z)
  fi
  # Targeted ALTERs
  alter_sql "$DB"
  echo "=== Done: $DB ==="
done

echo "All done. If startup still complains about integer widths on MySQL 8, ensure the server was built with -DUSE_MYSQL_8."
