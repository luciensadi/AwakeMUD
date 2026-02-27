#!/bin/bash
set -eo pipefail

SQL_SOURCE="/docker-entrypoint-initdb.d/sql-source"

# Base schema and patches must succeed
echo "Loading AwakeMUD base schema..."
mysql -u root -p"$MYSQL_ROOT_PASSWORD" "$MYSQL_DATABASE" < "$SQL_SOURCE/awakemud.sql"

# Load patch files in the same order as gensql.sh
for patch in playergroups.sql mail_fixes.sql helpfile_expansion.sql bullet_pants.sql fuckups.sql ignore_system_v2.sql; do
    if [ -f "$SQL_SOURCE/$patch" ]; then
        echo "Loading patch: $patch"
        mysql -u root -p"$MYSQL_ROOT_PASSWORD" "$MYSQL_DATABASE" < "$SQL_SOURCE/$patch"
    fi
done

# Load migration files (best-effort: on a fresh install the base schema is already
# current, so some migrations will fail with duplicate column/key errors -- that's fine)
if [ -d "$SQL_SOURCE/Migrations" ]; then
    echo "Loading migrations..."
    for migration in "$SQL_SOURCE/Migrations"/*.sql; do
        if [ -f "$migration" ]; then
            echo "Applying migration: $(basename "$migration")"
            if ! mysql -u root -p"$MYSQL_ROOT_PASSWORD" "$MYSQL_DATABASE" < "$migration" 2>&1; then
                echo "  (skipped — already applied or not applicable)"
            fi
        fi
    done
fi

echo "AwakeMUD database initialization complete."
