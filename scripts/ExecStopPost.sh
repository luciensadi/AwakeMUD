#!/bin/bash
#
# Systemd unit ExecStopPost script

echo "" >> syslog.txt
echo "AwakeMUD stopped at $(date)" >> syslog.txt

tail -n 500 syslog.txt >> log/crashlog
echo "" >> log/crashlog

fgrep "self-delete" syslog.txt >> log/delete
fgrep "Running" syslog.txt >> log/restarts
fgrep "advanced" syslog.txt >> log/levels
fgrep "equipment lost" syslog.txt >> log/rentgone
fgrep "usage" syslog.txt >> log/usage
fgrep "CONNLOG" syslog.txt >> log/connlog
fgrep "MISCLOG" syslog.txt >> log/misclog
fgrep "SYSLOG" syslog.txt >> log/syslog
fgrep "WIZLOG" syslog.txt >> log/wizlog
fgrep "DEATHLOG" syslog.txt >> log/deathlog
fgrep "CHEATLOG" syslog.txt >> log/cheatlog
fgrep "Bad PW" syslog.txt >> log/badpws

# here we mail the syslog to whoever the admin is so they know the
# mud crashed (if it crashed at all)

# These aren't needed as the mud runs in GDB now.
#  mv -f lib/core.1 lib/core.2 >>& ~/awake.out
#  mv -f lib/core lib/core.1 >>& ~/awake.out

rm log/syslog.6
mv log/syslog.5 log/syslog.6
mv log/syslog.4 log/syslog.5
mv log/syslog.3 log/syslog.4
mv log/syslog.2 log/syslog.3
mv log/syslog.1 log/syslog.2
mv syslog.txt   log/syslog.1
touch syslog.txt

