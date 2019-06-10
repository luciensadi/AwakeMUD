# AwakeMUD Community Edition
A fork of the [Awakened Worlds](http://awakenedworlds.net) MUD codebase. Issues and pull requests welcome!

**The current test port for AwakeMUD Community Edition can be reached at mudtest.mooo.com port 4000.** Many thanks to Jan for running the previous test port for so long.

**Join our Discord channel!** https://discord.gg/q5VCMkv

## Features
- 50k+ lines of additions including new areas, new features, and massive quality-of-life improvements
- Enhanced security that fixes several serious exploits
- Significant reductions in memory leaks
- Significantly reduced ticklag (game pauses every two minutes are a thing of the past!)
- A slew of bugfixes to everything from combat code to sound propagation

## OS Support
Tested on:
- OSX 10.12, 10.13
- Ubuntu 14 and 16 LTS
- Raspbian Jessie
- Amazon Linux
- Cygwin (beta)

## Installation
- Install clang (`apt-get install clang`)
- Install MySQL 5, including its development headers (ensure `mysql/mysql.h` exists in your path).
- Install [libsodium](https://github.com/jedisct1/libsodium/releases) (`./configure; make; (sudo) make install`). Version 1.0.16 is known to work.
- Clone this repository to your machine.
- Run `SQL/gensql.sh` (or do the steps manually if it doesn't support your OS). If you plan on running this with MariaDB, use the `--skip-checks` command-line flag.
- Copy the mysql_config.cpp file to src.
- Edit `src/Makefile` and uncomment the OS that looks closest to yours (comment out the others). The default is Mac OS X; you'll probably want to switch it to Linux. You probably also want to remove the `-DGITHUB_INTEGRATION` flag from the Makefile at this time.
- From the src directory, run `make clean && make`.
- Change to the root directory and run the game (ex: `gdb bin/awake`, or `lldb bin/awake` on OS X).

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

If you get an error like `structs.h:8:10: fatal error: sodium.h: No such file or directory`, it means you need to install [libsodium](https://github.com/jedisct1/libsodium/releases) (`./configure; make; (sudo) make install`).

## Runtime Troubleshooting

If you get an error like `MYSQLERROR: Data too long for column 'Password' at row X` when running the game, you need to update your database's pfile table to use the longer password-column capacity. Go into your database and execute the command in `SQL/migration-libsodium.sql`.

If you get an error like `error while loading shared libraries: libsodium.so.23: cannot open shared object file: No such file or directory`, your path does not include the directory libsodium is in. You can find libsodium.so's directory with `sudo find / -name libsodium.so`, then optionally symlink it with `ln -s /the/directory/it/was/in/libsodium.so.XXX /usr/lib/libsodium.so.XXX`, where XXX is the numbers indicated in the original error. This is probably not an ideal fix, so if anyone has a better suggestion, please file an issue!

If you get an error like `MYSQLERROR: Table 'awakemud.pfiles_mail' doesn't exist`, you need to run the command found in `SQL/mail_fixes.sql` in your database. This will upgrade your database to be compatible with the new mail system. This command is automatically run for new databases.
