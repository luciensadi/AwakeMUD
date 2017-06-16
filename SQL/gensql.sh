#! /bin/bash -e

echo "This script will set up your MySQL DB with a localhost-only user and will install the AwakeMUD tables."

DBHOST="localhost"
DBNAME="AwakeMUD"
DBUSER="AwakeMUD"
DBPASS=`openssl rand -base64 32`

echo "Testing for MySQL..."
MYSQLVER=`mysql --version | grep -o -e 'Distrib [^,]*' | grep -o -e ' 5' | tr -d [:space:]`
if [ "$MYSQLVER" != "5" ]; then
  echo "Error: You must have MySQL version 5 installed."
  exit 1
fi

echo "Testing for OpenSSL..."
openssl version

echo "Testing for awakemud.sql..."
if [ ! -f awakemud.sql ]; then
  echo "Error: Cannot find awakemud.sql."
  echo "Please make sure the awakemud.sql file is in the directory you're running this command from."
  exit 2
fi

echo "WARNING: IF YOUR DATABASE ALREADY EXISTS, THIS WILL PURGE IT."
echo "If you have a DB and want to save it, use CTRL-C to abort this script."
echo "Otherwise, enter your MySQL root user's password when prompted."

echo "DROP DATABASE IF EXISTS $DBNAME;" > gen_temp.sql
echo "DROP USER IF EXISTS '$DBUSER'@'$DBHOST';" >> gen_temp.sql
echo "FLUSH PRIVILEGES;" >> gen_temp.sql

echo "CREATE DATABASE $DBNAME;" >> gen_temp.sql
echo "USE $DBNAME;" >> gen_temp.sql
echo "CREATE USER '$DBUSER'@'$DBHOST' IDENTIFIED BY '$DBPASS';" >> gen_temp.sql
echo "GRANT ALL PRIVILEGES ON $DBNAME.* TO '$DBUSER'@'$DBHOST';" >> gen_temp.sql
echo "FLUSH PRIVILEGES;" >> gen_temp.sql
echo "" >> gen_temp.sql
cat awakemud.sql >> gen_temp.sql

if [ -f "playergroups.sql" ]; then
  echo "" >> gen_temp.sql
  cat playergroups.sql >> gen_temp.sql
fi

mysql -u root -p < gen_temp.sql

rm gen_temp.sql

echo "const char *mysql_host =     \"$DBHOST\";" > mysql_config.cpp
echo "const char *mysql_password = \"$DBPASS\";" >> mysql_config.cpp
echo "const char *mysql_user =     \"$DBUSER\";" >> mysql_config.cpp
echo "const char *mysql_db =       \"$DBNAME\";" >> mysql_config.cpp

echo "DB creation script complete. Please copy the mysql_config.cpp file from this directory into your src directory."
