
/etc directory
-----------------------------------------------------------------------------

These files are maintained by the game; you should never try to modify them
while the game is running.  If you know what you're doing, you can safely
alter them when the game is down.  


ALTERING ELEVATORS:

The first digit at the very top of the elevator file is the total number of elevators in the game. When adding an elevator, be sure to increase this by 1.

The first line of an elevator entry is as follows:
`<car room's vnum> <columns on panel> <num floor lines> <start floor>`.

The remaining lines each describe a floor. They are in the following format:
`<vnum of destination> <numerical cardinal direction of exit from car to destination>`

Numerical cardinal directions are as follows:
```
N  0
NE 1
E  2
SE 3
S  4
SW 5
W  6
NW 7
```

This is an example elevator file with the Chargen elevator (one elevator with one column of two floors, floor numbers starting at 4):
```
1
60531 1 2 4
60530 4
60532 4
```


ALTERING APARTMENTS / HOUSES:

As with elevators, the very topmost entry is the total number of apartment complexes in the file. Be sure to increment it if you add another apartment complex, otherwise your new complex won't be read.

The first line of an apartment entry is in the following format: 
`<landlord vnum> 0 <base cost to rent> <vnum of landlord's room>`

The second and subsequent lines of the apartment entry each describe individual apartments. They are in the following format:
`<room vnum> <key vnum> <numerical cardinal direction of the apartment's main exit that needs the key> <mode> <apartment name, like 3A> 0 0 0`

The cardinal direction table can be found above in the elevator section.