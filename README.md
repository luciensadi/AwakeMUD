# AwakeMUD Community Edition
A fork of the [Awakened Worlds](awakenedworlds.net) MUD codebase. Issues and pull requests welcome!

Feel free to join us via telnet at mudtest.mooo.com port 4000 to participate in playtesting. If you have the knack for building, we're also interested in hiring volunteer builders who are willing to open-source their contributions to the game world.

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
- Install MySQL 5, including its development headers (`mysql/mysql.h`)
- Clone this repository to your machine.
- Run `SQL/gensql.sh` (or do the steps manually if it doesn't support your OS). If you plan on running this with MariaDB, use the `--skip-checks` command-line flag.
- Copy the mysql_config.cpp file to src.
- Edit `src/Makefile` and uncomment the OS that looks closest to yours (comment out the others). The default is Mac OS X; you'll probably want to switch it to Linux. You probably also want to remove the `-DGITHUB_INTEGRATION` flag from the Makefile at this time.
- From the src directory, run `make clean && make`.
- Change to the root directory and run the game (ex: `gdb bin/awake`, or `lldb bin/awake` on OS X).

### Make / Compile Troubleshooting

If you get an error like `newdb.cpp:11:10: fatal error: mysql/mysql.h: No such file or directory` while running `make`, you either haven't installed the MySQL development headers, or you haven't made them visible to your operating system. Since each OS is different, Google is your best bet for resolving this, but your goal / end state is to have the header inclusion path `mysql/mysql.h` resolve successfully.

If you get an error like `AwakeMUD/src/act.wizard.cpp:3841: undefined reference to 'crypt'`, it means that you've probably not selected the right OS in your `src/Makefile`. Make sure you comment out the OS X lines near the top by adding a `#` at their beginnings, and uncomment the Linux lines by removing their `#`.
