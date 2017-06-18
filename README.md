# AwakeMUD Community Edition
A fork of the Awakened Worlds MUD codebase. Pull requests welcome!

Join us at mudtest.mooo.com port 4000 to participate in playtesting.

## Features
- Significant reductions in memory leaks
- Significantly reduced ticklag
- A slew of bugfixes to everything from combat code to sound propagation

## OS Support
Tested on:
- OSX 10.12
- Ubuntu 14.04.2 LTS
- Raspbian Jessie

## Installation
- Install MySQL 5.
- Clone this repository to your machine.
- Run `SQL/gensql.sh` (or do the steps manually if it doesn't support your OS).
- Copy the mysql_config.cpp file to src.
- Edit src/Makefile and select the OS that looks closest to yours.
- From the src directory, run `make clean && make`.
- Change to the root directory and run the game (ex: `gdb bin/awake`, or `lldb bin/awake` on OS X).
