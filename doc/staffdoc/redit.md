# Room Editing (REDIT)

The REDIT command is used to create and modify rooms in the OLC system. There are two forms of syntax for this command; 'REDIT' on its own edits the room you are currently standing in, and 'REDIT room_number' edits the room with the given identification number. If the given number has no associated room, a room will be created with that number as its identifier.


## The Main REDIT Menu

1) **Room Name**  
This is the title of the room.

2) **Room Desc** 
 This is the room's description- it's what you see when you type LOOK.  This is overridden during the night by the night description (if one is set).

3) **Night Desc**  
This description overrides the room's main description at night.  This is an optional field.

4) **Room Flags** 
 Room Flags are flags that can be toggled to control the room's behavior in code. Examples of room flags are GARAGE, STORAGE, SOUNDPROOF, etc. A full detailing of room flags can be found here.

5) **Domain Type**  
The Domain Type of a room is the Shamanic domain this room is located in.

6-f) **Exits**  
Exits are displayed as the vnum they connect to if they exist. Selecting one of these will put you into the exit editing menu (more on this below).

g) **Edit Jackpoint**  
This controls the Matrix connectivity of a room. The default is no jackpoint; consequentially, this can be safely ignored unless the room has a reason to have a Matrix jackpoint in it. More information can be found on this below.

h) **Light Level**  
Allows you to set the light-level of the room. The list is not sorted in any particular manner.

i) **Smoke Level**  
Allows you to set the smoke level. Smoke shows up when you LOOK and can obscure vision / modify target numbers for actions performed in the room.

j) **Combat Options**  
Allows you to configure various combat behaviors.  Most of these have no real effect; however, more information on this is listed below.

k) **Background Count**  
Background Count modifies TNs for spellcasting and magic use in the area. Background Count is covered in detail on Page 83 of 'Magic in the Shadows'.

l) **Extra Descriptions**  
Extra Descriptions are hidden descriptions that can be LOOKed at if you know the keyword. These keywords are generally hinted to within the room's description.

m) **Current / Fall rating**  
   This field only appears if a room is set up as a water room OR the room is flagged as a falling room. This number is the TN to swim / stop from falling in the room.
   
   
## Exit Information

1) **Exit To**  
This is the vnum that this exit connects to.

2) **Description**  
This is the description of the exit, seen when you LOOK in the exit's direction.

3) **Door Names**  
These are the aliases of the exit. The first name is important- it shows up in the interaction messages. Example: I open an exit with the namelist 'doors door double'.  The message printed is "You open the doors."

4) **Key vnum**  
 This is the vnum of the item that serves as the key. If no key vnum is entered, the default key (item number 0 on AW) will be used for door interaction.

5) **Door flag**  
This lets you select the type of door on this exit. The types are as follows:

- No Door: Exit cannot be opened/closed.
- Closeable Door: Exit can be closed/locked/bypassed.
- Pickproof: Exit can be closed and locked but not bypassed.
- Storm Drain / Manhole: Exit allows water flow if pointing downwards.
- Garage Door: This door can be interacted with from inside a vehicle.

6) **Lock Level**  
This is the TN required to bypass the door.

7) **Material Type**  
This allows you to select the material the door is made of. This is used in breakage tests.

8)**Barrier Rating**  
This allows you to select the barrier rating of the door. This is used in breakage tests.

9) **Hidden Rating**  
This is the TN to notice the exit.

a) **Purge exit.**  
Always use this to leave the menu if you do not want to keep the exit. Failing to do so will result in an exit leading to 'A Bright Light'.

## Matrix Jackpoint Information

0) **To Host**  
This is the vnum of the host that the jackpoint connects to.

1) **Access Mod**  
Not currently implemented.

2) **Trace Mod**  
Not currently implemented.

3) **I/O Speed**  
This is the transfer speed of the jackpoint. The following values can be used here:  0: Unlimited.  -1: Capped at MPCP * 50.  Anything else: Capped at that number.

4) **Base Bandwidth**  
Not currently implemented.

5) **Parent RTG**  
The vnum of the RTG this jack connects you to when you execute the 'logon RTG' command.

6) **Commlink Number**  
The phone number of the decker, should the decker place a call from here.

7) **Physical Address**  
A short string with the location of the jackpoint.


## Room Flags

**\***  
This flag is used by the code as part of the gridguide system. Do not set it.

**!BIKE**  
This flag blocks the travel of motorbikes through the room.

**!GRID**  
This flag blocks gridguide from, to, and through this room.

**!MAGIC**  
This legacy flag has no effect.

**!MOB**  
This flag blocks NPCs from entering the room of their own volition.

**!QUIT**
This flag blocks players from quitting in the room.

**!RADIO**  
 This flag causes radio transmissions in the area to become spotty and difficult to understand.

**!TRACK**  
This legacy flag has no effect.

**!USED**  
This legacy flag has no effect.

**!WALK**  
This flag marks the room as being on a highway, preventing on-foot players from walking through it.

**ARENA**  
This prevents players fighting within the room from being flagged as PKers.

**ASTRAL**  
This legacy flag has no effect.

**ATRIUM**  
This flag is used by the code in the creation of coded apartments. Do not set.

**DARK**  
This flag causes the room to be dark. This flag is overridden by the light level setting. This flag overrides LOW_LIGHT.

**DEATH**  
This flag causes instant death upon entry.

**FALL**  
 This flag introduces the possibility of falling down if there is a down exit in the room. The room's fall rating (the 'm' field that shows up if falling is enabled) is the TN for stopping yourself from falling.

**GARAGE**  
 This room allows the storage of vehicles over crash / copyover. Workshops may be unpacked here. This flag automatically invokes the flags !GRID and ROAD.

**HCRSH**  
This legacy flag has no effect.

**HOUSE**  
This flag is used by the code to make a room into a coded apartment. Do not set.

**INDOORS**  
This flag blocks weather effects and storm warning sirens in the room, amongst other things.

**LOW_LIGHT**  
This flag makes the light level in a room low enough to require low-light vision. This flag is overridden by the room's light level and by the DARK flag.

**OLC**  
This legacy flag has no effect.

**PEACEFUL**  
This flag prevents aggression within a room and also prevents attacks being directed into the room from elsewhere.  Use sparingly.

**ROAD**  
This flag allows cars and trucks to travel through the room.

**SENATE**  
This legacy flag has no effect.

**SENT**  
This flag is used by the code as part of sound tracking. Do not set.

**SOUNDPROOF**  
This flag entirely blocks shouting and radio usage within the room. This flag also serves to block out storm warning sirens.

**STORAGE**  
This flag causes items dropped within the room to save over MUD interruptions (copyovers, crashes, reboots).

**STREETLIGHTS**  
This flag has no lighting effect; rather, this flag causes the streetlight-turning-on message seen outside on roadways. Known as the LIT flag in Awake's parlance.

**TUNNEL**  
This flag makes it so that only two characters / NPCs can be in the room at a time.

