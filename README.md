# AwakeMUD Community Edition
A fork of the [Awakened Worlds](http://awakenedworlds.net) MUD codebase. Issues and pull requests welcome!

**The current test port for AwakeMUD Community Edition can be reached at awakemud.com port 4000.** Many thanks to Finster for running the previous test port for so long.

**Join our Discord channel!** https://discord.gg/q5VCMkv

## Features
- 62k+ lines of additions including new areas, new features, and massive quality-of-life improvements
- Screenreader accessibility via TOGGLE SCREENREADER
- Enhanced security that fixes many serious exploits
- Significantly increased performance
- A slew of bugfixes to everything from combat code to sound propagation

## OS Support
Tested on:
- OSX 10.15 (actively); 10.12 through 10.14 believed to work but not actively tested
- Ubuntu 18 LTS (actively); 16 and 14 same as above
- Amazon Linux
- Raspbian Jessie (no longer actively tested)
- Cygwin (beta)

## Installation (Ubuntu commands in parentheses)
- Install [MySQL 5](https://dev.mysql.com/doc/refman/5.7/en/installing.html), including its development headers (ensure `mysql/mysql.h` exists in your path).
- Install automake, make, gcc, g++, clang, libtool, autoconf, zlib1g-dev, libcurl4-openssl-dev, and libmysqlclient-dev if they're not already present (`sudo apt-get install automake make gcc g++ clang libtool autoconf zlib1g-dev libcurl4-openssl-dev libmysqlclient-dev`)
- Install [libsodium](https://github.com/jedisct1/libsodium/releases) per their [installation instructions](https://download.libsodium.org/doc/installation). Version 1.0.16 is known to work, but higher versions should work as well.
- Clone this repository to your machine. (`git clone https://github.com/luciensadi/AwakeMUD.git`)
- Change to the repository's SQL directory (`cd AwakeMUD/SQL`)
- Run `./gensql.sh` (or do the steps manually if it doesn't support your OS). If you plan on running this with MariaDB, use the `--skip-checks` command-line flag.
- Install the houses file (`mv ../lib/etc/houses.template ../lib/etc/houses`).
- Change to the repository's SRC directory (`cd ../src`).
- Edit `Makefile` and uncomment the OS that looks closest to yours by removing the # marks in front of it. Comment out the others by ensuring they have a # in front of their lines. The default is Mac OS X; you'll probably want to switch it to Linux. You probably also want to remove the `-DGITHUB_INTEGRATION` flag from the Makefile at this time.
- From that same src directory, run `make clean && make`.
- Change to the root directory (`cd ..`) and run the game (raw invocation `bin/awake`, or use a debugger like `gdb bin/awake`, or `lldb bin/awake` on OS X to help troubleshoot issues).
- Connect to the game with telnet at `127.0.0.1:4000` and enjoy!

### Cygwin Installation Notes
- AwakeCE can run in Windows under Cygwin.
- To build it, you need Cygwin (64bit) and Cygwin apps/libraries: make, g++, libcrypt, libsodium, mysqlclient.
- Update src/Makefile and comment out/uncomment the Cygwin config.
- Build by doing `cd src;make`.
- Run by doing `./bin/awake`.
- Note: You may have to manually import the SQL changes as gensql.sh may or may not work, use 127.0.0.1 as dbhost if running local db.
- With Cygwin, you can also use Eclipse CPP IDE, just create a Cygwin-C++ project and point the directory to where your AwakeMUD is located, play around with build settings to ensure it is using your Makefile in src. Debugging/Running works.

### Make / Compile Troubleshooting

If you get an error like `newdb.cpp:11:10: fatal error: mysql/mysql.h: No such file or directory` while running `make`, you either haven't installed the MySQL development headers, or you haven't made them visible to your operating system. Since each OS is different, Google is your best bet for resolving this, but your goal / end state is to have the header inclusion path `mysql/mysql.h` resolve successfully.

```
/home/ubuntu/AwakeMUD/src/act.informative.cpp:2769: undefined reference to `mysql_num_rows'
/home/ubuntu/AwakeMUD/src/act.informative.cpp:2774: undefined reference to `mysql_fetch_row'
/home/ubuntu/AwakeMUD/src/act.informative.cpp:2779: undefined reference to `mysql_free_result'
```
If you see a wall of errors like the one above, you need to edit `src/Makefile` and uncomment the lines belonging to the OS you're running.

If you get an error like `AwakeMUD/src/act.wizard.cpp:3841: undefined reference to 'crypt'`, it means that you've probably not selected the right OS in your `src/Makefile`. Make sure you comment out the OS X lines near the top by adding a `#` at their beginnings, and uncomment the Linux lines by removing their `#`.

If you get errors like `/home/ubuntu/AwakeMUD/src/act.other.cpp:954: undefined reference to 'github_issues_url'`, you need to remove -DGITHUB_INTEGRATION from your selected OS in your Makefile, then `make clean && make` to scrub the references to the GitHub integration code.

If you get an error like `structs.h:8:10: fatal error: sodium.h: No such file or directory`, it means you need to install [libsodium](https://github.com/jedisct1/libsodium/releases) (`./configure; make; (sudo) make install`).

## Runtime Troubleshooting

If you get an error like `MYSQLERROR: Data too long for column 'Password' at row X` when running the game, you need to update your database's pfile table to use the longer password-column capacity. Go into your database and execute the command in `SQL/migration-libsodium.sql`.

If you get an error like `error while loading shared libraries: libsodium.so.23: cannot open shared object file: No such file or directory`, your path does not include the directory libsodium is in. You can find libsodium.so's directory with `sudo find / -name libsodium.so`, then optionally symlink it with `ln -s /the/directory/it/was/in/libsodium.so.XXX /usr/lib/libsodium.so.XXX`, where XXX is the numbers indicated in the original error. This is probably not an ideal fix, so if anyone has a better suggestion, please file an issue!

If you get an error like `MYSQLERROR: Table 'awakemud.pfiles_mail' doesn't exist`, you need to run the command found in `SQL/mail_fixes.sql` in your database. This will upgrade your database to be compatible with the new mail system. This command is automatically run for new databases.

If it takes an exceedingly long time between you entering your password and the MUD responding, but the MUD is responsive for all other input, the machine that's hosting the MUD is not powerful enough for the password storage algorithm used by default. You may opt to either endure the delay (more secure, less convenient) or add `-DNOCRYPT` to your Makefile (not at all secure, convenient, will break all current passwords and require you to either fix them by hand or purge and re-create your database and all characters in it).
