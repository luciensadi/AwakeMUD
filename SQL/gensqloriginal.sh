#! /bin/bash -e

echo "This script will set up your MySQL DB with a localhost-only user and will install the AwakeMUD tables."

# Parameters for creating your AwakeMUD user, password, and DB.
DBHOST="localhost"
DBNAME="AwakeMUD"
DBUSER="AwakeMUD"
DBPASS=`openssl rand -base64 32`

# Set programmatically; if true, bypass MySQL version requirement check.
MYSQL_VERSION_CHECK_BYPASS=false

# Parse command-line flags.
while test $# -gt 0; do
  case "$1" in
    -h|--help)
      echo "$0 - create an empty AwakeMUD-compatible database"
      echo " "
      echo "$0 [options] application [arguments]"
      echo " "
      echo "options:"
      echo "-h, --help                show brief help"
      echo "-s, --skip-check          skip the MySQL version check"
      echo ""
      exit 0
      ;;
    -s|--skip-check)
      shift
      echo "Bypassing MySQL version check."
      MYSQL_VERSION_CHECK_BYPASS=true
      ;;
    *)
      break
      ;;
  esac
done

if [ ! "$MYSQL_VERSION_CHECK_BYPASS" = true ]; then
  echo "Testing for MySQL..."
  MYSQLVER=`mysql --version | grep -o -e 'Distrib [^,]*'`
  MYSQLMAJORVER=`echo $MYSQLVER | grep -o -e ' 5' | tr -d [:space:]`
  if [ "$MYSQLMAJORVER" != "5" ]; then
    echo "Error: You must have MySQL version 5 installed to use this script. If you're using MariaDB, re-run this script with the -s flag to skip compatibility checks."
    exit 1
  fi
  MYSQLMINORVER=`echo $MYSQLVER | grep -o -e '\.[0-9]*\.' | tr -d [:space:] | tr -d "\."`
  if [ "$MYSQLMINORVER" -ge "7" ]; then
    MYSQLSUBVERSION=`echo $MYSQLVER | grep -o -e '\.[0-9]*$' | tr -d "\."`
    if [ "$MYSQLSUBVERSION" -ge "8" ]; then
      MYSQLCANDROPUSERIFEXISTS=1
    else
      MYSQLCANDROPUSERIFEXISTS=0
    fi
  else
    MYSQLCANDROPUSERIFEXISTS=0
  fi
fi

echo "Testing for OpenSSL..."
openssl version > /dev/null

echo "Testing for awakemud.sql..."
if [ ! -f awakemud.sql ]; then
  echo "Error: Cannot find awakemud.sql."
  echo "Please make sure the awakemud.sql file is in the directory you're running this command from."
  exit 2
fi

echo "DROP DATABASE IF EXISTS $DBNAME;" > gen_temp.sql

if [ "$MYSQLCANDROPUSERIFEXISTS" == "1" ]; then
  echo "DROP USER IF EXISTS '$DBUSER'@'$DBHOST';" >> gen_temp.sql
  echo "FLUSH PRIVILEGES;" >> gen_temp.sql
else
  echo "Your database could not be confirmed compatible with 'DROP USER IF EXISTS', so you'll need to enter your DB root password twice."
  COUNT=`mysql -u root -p -e "SELECT COUNT(*) FROM mysql.user WHERE User='$DBUSER' AND Host='$DBHOST'" | grep -o -e "[0-9]*" | tr -d [:space:]`
  if [ "$COUNT" -ge "1" ]; then
    echo "DROP USER '$DBUSER'@'$DBHOST';" >> gen_temp.sql
    echo "FLUSH PRIVILEGES;" >> gen_temp.sql
  fi
fi

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

if [ -f "mail_fixes.sql" ]; then
  echo "" >> gen_temp.sql
  cat mail_fixes.sql >> gen_temp.sql
fi

if [ -f "helpfile_expansion.sql" ]; then
  echo "" >> gen_temp.sql
  cat helpfile_expansion.sql >> gen_temp.sql
fi

if [ -f "bullet_pants.sql" ]; then
  echo "" >> gen_temp.sql
  cat bullet_pants.sql >> gen_temp.sql
fi

if [ -f "fuckups.sql" ]; then
  echo "" >> gen_temp.sql
  cat fuckups.sql >> gen_temp.sql
fi

if [ -f "ignore_system_v2.sql" ]; then
  echo "" >> gen_temp.sql
  cat ignore_system_v2.sql >> gen_temp.sql
fi

echo "You may preview the actions that are about to be taken by viewing gen_temp.sql now."
echo ""
echo "WARNING: IF YOUR DATABASE ALREADY EXISTS, THIS WILL PURGE IT."
echo ""
echo "If you have a DB and want to save it, enter anything except your MySQL root user's password."
echo "Otherwise, enter your MySQL root user's password when prompted."

mysql -u root -p < gen_temp.sql

rm gen_temp.sql

echo "const char *mysql_host =     \"$DBHOST\";" > mysql_config.cpp
echo "const char *mysql_password = \"$DBPASS\";" >> mysql_config.cpp
echo "const char *mysql_user =     \"$DBUSER\";" >> mysql_config.cpp
echo "const char *mysql_db =       \"$DBNAME\";" >> mysql_config.cpp

if [ -f ../src/mysql_config.cpp ]; then
  echo "DB creation script complete, but the config file cannot be automatically installed due to a conflict."
  echo "Please copy the mysql_config.cpp file from this directory into your ../src directory."
else
  mv mysql_config.cpp ../src
  echo "DB creation script complete. Installed mysql_config.cpp file into your src directory."
fi

echo "Don't forget to MAKE CLEAN && MAKE in your ../src directory after mysql_config.cpp is updated!"
