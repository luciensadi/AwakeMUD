\
#!/usr/bin/env bash
# db_integer_width_normalize.sh — fix integer columns that cause startup checks.
# It conditionally ALTERs columns across DBs. Safe to re-run.
# Usage: ./tools/db_integer_width_normalize.sh [-h HOST] [-u USER] [-p] db1 [db2 ...]
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

MYSQL=(mysql -h "$HOST" -u "$USER" --protocol=tcp --batch --raw)

have_table(){ "${MYSQL[@]}" "$1" -e "SHOW TABLES LIKE '$2';" | grep -q "$2"; }
have_col(){ "${MYSQL[@]}" "$1" -e "SHOW COLUMNS FROM \`$2\` LIKE '$3';" | grep -q "$3"; }
col_type(){ "${MYSQL[@]}" "$1" -e "SHOW COLUMNS FROM \`$2\` LIKE '$3';" | awk 'NR==2{print $2}'; }
is_unsigned(){ [[ "$1" =~ unsigned$ ]]; }

for DB in "$@"; do
  echo "=== Normalize ints in: $DB ==="
  # pfiles_inv.Attempt -> MEDIUMINT(6) DEFAULT 0 (unsigned? not specified in schema)
  if have_table "$DB" pfiles_inv && have_col "$DB" pfiles_inv Attempt; then
    t=$(col_type "$DB" pfiles_inv Attempt)
    if [[ ! "$t" =~ ^mediumint ]]; then
      echo " -> pfiles_inv.Attempt is $t; ALTER to MEDIUMINT(6) DEFAULT 0"
      "${MYSQL[@]}" "$DB" -e "ALTER TABLE \`pfiles_inv\` MODIFY \`Attempt\` MEDIUMINT(6) DEFAULT 0;"
    fi
  fi
  # pfiles_worn.Attempt
  if have_table "$DB" pfiles_worn && have_col "$DB" pfiles_worn Attempt; then
    t=$(col_type "$DB" pfiles_worn Attempt)
    if [[ ! "$t" =~ ^mediumint ]]; then
      echo " -> pfiles_worn.Attempt is $t; ALTER to MEDIUMINT(6) DEFAULT 0"
      "${MYSQL[@]}" "$DB" -e "ALTER TABLE \`pfiles_worn\` MODIFY \`Attempt\` MEDIUMINT(6) DEFAULT 0;"
    fi
  fi
  # pfiles_drugs.LastFix -> BIGINT UNSIGNED DEFAULT 0
  if have_table "$DB" pfiles_drugs && have_col "$DB" pfiles_drugs LastFix; then
    t=$(col_type "$DB" pfiles_drugs LastFix)
    if [[ ! "$t" =~ ^bigint ]]; then
      echo " -> pfiles_drugs.LastFix is $t; ALTER to BIGINT UNSIGNED DEFAULT 0"
      "${MYSQL[@]}" "$DB" -e "ALTER TABLE \`pfiles_drugs\` MODIFY \`LastFix\` BIGINT UNSIGNED DEFAULT 0;"
    else
      # Ensure unsigned
      if ! is_unsigned "$t"; then
        echo " -> pfiles_drugs.LastFix is signed; make UNSIGNED"
        "${MYSQL[@]}" "$DB" -e "ALTER TABLE \`pfiles_drugs\` MODIFY \`LastFix\` BIGINT UNSIGNED DEFAULT 0;"
      fi
    fi
  fi
  # pfiles.LastRoom -> MEDIUMINT(5) DEFAULT 0 (MySQL 8 reports widthless)
  if have_table "$DB" pfiles && have_col "$DB" pfiles LastRoom; then
    t=$(col_type "$DB" pfiles LastRoom)
    if [[ ! "$t" =~ ^mediumint ]]; then
      echo " -> pfiles.LastRoom is $t; ALTER to MEDIUMINT(5) DEFAULT 0"
      "${MYSQL[@]}" "$DB" -e "ALTER TABLE \`pfiles\` MODIFY \`LastRoom\` MEDIUMINT(5) DEFAULT 0;"
    fi
  fi
  echo "=== Done: $DB ==="
done
