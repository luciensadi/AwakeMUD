#!/bin/bash

# Written by Daniel Lowe, 2008
# https://github.com/TempusMUD/Tempuscode/blob/master/bin/crash_report.sh
# Modified for AwakeMUD CE by Finster

LINECOUNT=`wc -l $0 | awk '{print $1}'`
CUTLINE=`grep --line-number "^\# GDB COMMAND LIST CUT HERE" $0 | awk -F: '{print $1}'`
TAILCOUNT=`echo $((LINECOUNT-CUTLINE))`


echo " >>>>"
echo " >>>> MUD CRASH REPORT <<<<"
echo " >>>> `date`"
echo " >>>>"
echo
echo " >>>>"
echo " >>>> TAIL of last syslog (log/syslog.0)"
echo " >>>>"
echo

tail $2

echo
echo " >>>>"
echo " >>>> GDB OUTPUT"
echo " >>>>"
echo

tail -${TAILCOUNT} $0 > /tmp/awake.crash.cmds
gdb bin/awake $1 </tmp/awake.crash.cmds
chmod g+r $1

exit

# gbd coms
#
# GDB COMMAND LIST CUT HERE
set prompt \n
set print null-stop on
set print pretty on
printf " >>>>\n >>>> STACK TRACE\n >>>>\n"
bt
printf " >>>>\n >>>> LAST CMD ISSUED\n >>>>\n"
print last_cmd
quit
