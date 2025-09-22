\
#!/usr/bin/env bash
# db_fix_all.sh — One-shot fixer for ALL AwakeMUD databases on a MySQL host.
#
# It will:
#   • Discover databases that contain the game (look for table `pfiles`) or use provided DB names.
#   • Create DB if missing; load SQL/awakemud.sql if pfiles missing.
#   • Apply standalone migrations if needed:
#       - SQL/bullet_pants.sql       -> creates pfiles_ammo
#       - SQL/ignore_system_v2.sql   -> creates pfiles_ignore_v2
#   • Apply all SQL/Migrations/*.sql (sorted), continuing past benign "already exists"/"duplicate" errors.
#   • Normalize key integer columns so startup checks won't trip on MySQL 8:
#       - pfiles_inv.Attempt      -> MEDIUMINT(6) DEFAULT 0
#       - pfiles_worn.Attempt     -> MEDIUMINT(6) DEFAULT 0
#       - pfiles_drugs.LastFix    -> BIGINT UNSIGNED DEFAULT 0
#       - pfiles.LastRoom         -> MEDIUMINT(5) DEFAULT 0
#   • Specifically fix playergroups.zone:
#       - If `playergroups` exists and missing `zone`, apply SQL/Migrations/playergroup_zone_ownership.sql
#
# Usage:
#   ./tools/db_fix_all.sh [-h HOST] [-u USER] [-p] [--all] [DB1 DB2 ...]
# Examples:
#   ./tools/db_fix_all.sh -h 127.0.0.1 -u awake -p --all
#   ./tools/db_fix_all.sh -h 127.0.0.1 -u awake -p awakemud awakemud_test
#
set -euo pipefail

HOST="${DB_HOST:-127.0.0.1}"
USER="${DB_USER:-awake}"
ASKPASS=0
ALL=0

usage(){ echo "Usage: $0 [-h HOST] [-u USER] [-p] [--all] [DB1 DB2 ...]"; exit 1; }

# Parse options
while [[ $# -gt 0 ]]; do
  case "$1" in
    -h) HOST="$2"; shift 2;;
    -u) USER="$2"; shift 2;;
    -p) ASKPASS=1; shift;;
    --all) ALL=1; shift;;
    --) shift; break;;
    -*) usage;;
    *) break;;
  esac
done

EXPLICIT_DBS=("$@")

if [[ $ASKPASS -eq 1 ]]; then
  read -rsp "Password for $USER@$HOST: " DBPW; echo; export MYSQL_PWD="$DBPW"
fi

if [[ ! -d SQL ]]; then
  echo "ERROR: Run from repo root (SQL/ not found)."; exit 2
fi

MYSQL=(mysql -h "$HOST" -u "$USER" --protocol=tcp --batch --raw)

discover_game_dbs() {
  # List databases, exclude system schemas, and keep those with a `pfiles` table.
  "${MYSQL[@]}" -e "SHOW DATABASES;" \
  | grep -Ev '^(Database|information_schema|mysql|performance_schema|sys)$' \
  | while read -r db; do
      if "${MYSQL[@]}" "$db" -e "SHOW TABLES LIKE 'pfiles';" | grep -q pfiles; then
        echo "$db"
      fi
    done
}

apply_sql_file() {
  local db="$1" file="$2"
  echo "   -> Applying $file"
  "${MYSQL[@]}" --force "$db" < "$file" 2> >(grep -Eiv 'already exists|duplicate column|duplicate entry|doesn'\''t exist' >&2 || true)
}

have_table(){ "${MYSQL[@]}" "$1" -e "SHOW TABLES LIKE '$2';" | grep -q "$2"; }
have_col(){ "${MYSQL[@]}" "$1" -e "SHOW COLUMNS FROM \`$2\` LIKE '$3';" | grep -q "$3"; }
col_type(){ "${MYSQL[@]}" "$1" -e "SHOW COLUMNS FROM \`$2\` LIKE '$3';" | awk 'NR==2{print $2}'; }
is_unsigned(){ [[ "$1" =~ unsigned$ ]]; }

process_db() {
  local DB="$1"
  echo "=== DB: $DB ==="
  # Create DB if needed
  "${MYSQL[@]}" -e "CREATE DATABASE IF NOT EXISTS \`$DB\` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;"
  # Base schema
  if ! have_table "$DB" pfiles; then
    echo " - Loading base schema SQL/awakemud.sql (pfiles missing)"
    apply_sql_file "$DB" "SQL/awakemud.sql"
  else
    echo " - Base schema present (pfiles exists)"
  fi
  # Standalone migrations
  if ! have_table "$DB" pfiles_ammo && [[ -f SQL/bullet_pants.sql ]]; then
    echo " - Applying bullet_pants.sql (pfiles_ammo)"
    apply_sql_file "$DB" "SQL/bullet_pants.sql"
  fi
  if ! have_table "$DB" pfiles_ignore_v2 && [[ -f SQL/ignore_system_v2.sql ]]; then
    echo " - Applying ignore_system_v2.sql (pfiles_ignore_v2)"
    apply_sql_file "$DB" "SQL/ignore_system_v2.sql"
  fi
  # Directory migrations
  if compgen -G "SQL/Migrations/*.sql" >/dev/null; then
    echo " - Applying SQL/Migrations/*.sql"
    while IFS= read -r -d '' f; do
      apply_sql_file "$DB" "$f"
    done < <(LC_ALL=C find SQL/Migrations -maxdepth 1 -type f -name "*.sql" -print0 | sort -z)
  fi
  # Targeted integer fixes
  if have_table "$DB" pfiles_inv && have_col "$DB" pfiles_inv Attempt; then
    local t; t=$(col_type "$DB" pfiles_inv Attempt || true)
    if [[ -n "$t" && ! "$t" =~ ^mediumint ]]; then
      echo " - Fixing pfiles_inv.Attempt -> MEDIUMINT(6) DEFAULT 0 (was $t)"
      "${MYSQL[@]}" "$DB" -e "ALTER TABLE \`pfiles_inv\` MODIFY \`Attempt\` MEDIUMINT(6) DEFAULT 0;"
    fi
  fi
  if have_table "$DB" pfiles_worn && have_col "$DB" pfiles_worn Attempt; then
    local t; t=$(col_type "$DB" pfiles_worn Attempt || true)
    if [[ -n "$t" && ! "$t" =~ ^mediumint ]]; then
      echo " - Fixing pfiles_worn.Attempt -> MEDIUMINT(6) DEFAULT 0 (was $t)"
      "${MYSQL[@]}" "$DB" -e "ALTER TABLE \`pfiles_worn\` MODIFY \`Attempt\` MEDIUMINT(6) DEFAULT 0;"
    fi
  fi
  if have_table "$DB" pfiles_drugs && have_col "$DB" pfiles_drugs LastFix; then
    local t; t=$(col_type "$DB" pfiles_drugs LastFix || true)
    if [[ -n "$t" ]]; then
      if [[ ! "$t" =~ ^bigint ]]; then
        echo " - Fixing pfiles_drugs.LastFix -> BIGINT UNSIGNED DEFAULT 0 (was $t)"
        "${MYSQL[@]}" "$DB" -e "ALTER TABLE \`pfiles_drugs\` MODIFY \`LastFix\` BIGINT UNSIGNED DEFAULT 0;"
      elif [[ ! "$t" =~ unsigned$ ]]; then
        echo " - Ensuring pfiles_drugs.LastFix is UNSIGNED (was $t)"
        "${MYSQL[@]}" "$DB" -e "ALTER TABLE \`pfiles_drugs\` MODIFY \`LastFix\` BIGINT UNSIGNED DEFAULT 0;"
      fi
    fi
  fi
  if have_table "$DB" pfiles && have_col "$DB" pfiles LastRoom; then
    local t; t=$(col_type "$DB" pfiles LastRoom || true)
    if [[ -n "$t" && ! "$t" =~ ^mediumint ]]; then
      echo " - Fixing pfiles.LastRoom -> MEDIUMINT(5) DEFAULT 0 (was $t)"
      "${MYSQL[@]}" "$DB" -e "ALTER TABLE \`pfiles\` MODIFY \`LastRoom\` MEDIUMINT(5) DEFAULT 0;"
    fi
  fi
  # Specific: playergroups.zone
  if have_table "$DB" playergroups; then
    if ! have_col "$DB" playergroups zone; then
      if [[ -f SQL/Migrations/playergroup_zone_ownership.sql ]]; then
        echo " - Adding playergroups.zone via playergroup_zone_ownership.sql"
        apply_sql_file "$DB" "SQL/Migrations/playergroup_zone_ownership.sql"
      else
        echo " !! SQL/Migrations/playergroup_zone_ownership.sql not found"
      fi
    else
      echo " - playergroups.zone exists"
    fi
  else
    echo " - Table playergroups not present; skipping zone migration"
  fi
  echo "=== Done: $DB ==="
}

declare -a DBS=()
if [[ $ALL -eq 1 ]]; then
  mapfile -t DBS < <(discover_game_dbs)
  if [[ ${#DBS[@]} -eq 0 ]]; then
    echo "No game databases discovered (no DB with 'pfiles' table)."
    exit 0
  fi
else
  if [[ ${#EXPLICIT_DBS[@]} -eq 0 ]]; then usage; fi
  DBS=("${EXPLICIT_DBS[@]}")
fi

for db in "${DBS[@]}"; do
  process_db "$db"
done

echo "All done."
