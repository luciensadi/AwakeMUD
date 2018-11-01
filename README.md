# AwakeMUD Community Edition
A fork of the [Awakened Worlds](http://awakenedworlds.net) MUD codebase. Issues and pull requests welcome!

**The current test port for AwakeMUD Community Edition can be reached at mudtest.mooo.com port 4000.** Many thanks to Jan for running the previous test port for so long.

Discussion of this port, as well as Awake itself, can take place at https://www.reddit.com/r/AwakenedWorlds/.

## Features
- Significant reductions in memory leaks
- Significantly reduced ticklag
- A slew of bugfixes to everything from combat code to sound propagation

## OS Support
Tested on:
- OSX 10.12, 10.13
- Ubuntu 14.04.2 LTS
- Raspbian Jessie

## Installation
- Install MySQL 5, including its development headers (`mysql/mysql.h`).
- Install [libsodium](https://github.com/jedisct1/libsodium/releases) (`./configure; make; (sudo) make install`). Version 1.0.16 is known to work.
- Clone this repository to your machine.
- Run `SQL/gensql.sh` (or do the steps manually if it doesn't support your OS). If you plan on running this with MariaDB, use the `--skip-checks` command-line flag.
- Copy the mysql_config.cpp file to src.
- Edit `src/Makefile` and uncomment the OS that looks closest to yours (comment out the others). The default is Mac OS X; you'll probably want to switch it to Linux. You probably also want to remove the `-DGITHUB_INTEGRATION` flag from the Makefile at this time.
- From the src directory, run `make clean && make`.
- Change to the root directory and run the game (ex: `gdb bin/awake`, or `lldb bin/awake` on OS X).

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

### Runtime Troubleshooting

If you get an error like `MYSQLERROR: Data too long for column 'Password' at row X` when running the game, you need to update your database's pfile table to use the longer password-column capacity. Go into your database and execute the command in `SQL/migration-libsodium.sql`.

If you get an error like `error while loading shared libraries: libsodium.so.23: cannot open shared object file: No such file or directory`, your path does not include the directory libsodium is in. You can find libsodium.so's directory with `sudo find / -name libsodium.so`, then optionally symlink it with `ln -s /the/directory/it/was/in/libsodium.so.XXX /usr/lib/libsodium.so.XXX`, where XXX is the numbers indicated in the original error. This is probably not an ideal fix, so if anyone has a better suggestion, please file an issue!
