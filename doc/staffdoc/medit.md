# Mob Editing (MEDIT)

The MEDIT command is used to create and edit NPCs within the game world. Like the other OLC commands, you can use "MEDIT #" to edit a specific NPC; if no NPC exists at that vnum, you will have the option of creating one there.

## The Main MEDIT Menu

1) **Keywords**  
These are the aliases / keywords that the NPC can be targeted with.

2) **Name**  
This is the name of the NPC, used when they speak and when looking at them. Unless it is a proper name (e.g. "Alice" "Claire"), this should always start with a lower-case letter. This should never end in punctuation.

3) **Room Description**
This is the line that's printed to the room when you LOOK. This should always start with a capital letter and end with punctuation.

4) **Look Description**  
This is the text that's printed when you LOOK at the NPC.

5) **Mob Flags**  
Accessing this menu allows you to modify the behavioral flags that the NPC has set.  More on these flags can be found here (this window) or here (new window).

6) **Affected Flags**  
 Affected Flags are used to add extra abilities or change the state of the NPC. They are detailed HERE.

7) **Nothing**  
For reasons unknown, there is no option listed at 7.

8) **Average Nuyen / Credstick Value**
This allows you to set the average nuyen value of loose-cash and nuyen contents of the credstick that spawn on the NPC when it is killed. Set to 0 to have nothing of the type appear.

9) **Bonus Karma Points**  
 Karma Points are set based on the attributes, skills, etc of the NPC.  Bonus Karma Points add to this pre-determined total; karma points are awarded on killing.

a) **Attributes**  
Select this to enter a menu where you can set up the NPC's various attributes.

b) **Level**  
This is roughly equivalent to Immortal level- if you have an NPC that you don't want low-level Immortals forcing or setting, set the NPC's level higher than theirs to make it immune to their tampering.

c-d) **Ballistic / Impact**  
This allows you to set the NPC's inherent (naked) armor.

e-f) **Max Physical Points / Max Mental Points**
This allows you to set the maximums for the NPC's health and mental status. Unless you have a good reason to change it, this should be left at 10/10.

g-h) **Position / Default Position**  
The Position field is the position that the NPC spawns in- it's not recommended to set this to Mortally Wounded, Stunned or Fighting.  The Default Position is the position in which the NPC will print its room desc to the room; if the NPC is not in the Default Position, a custom string will be printed instead (<name> is <sleeping/sitting/standing/whatever> here).

i-k) **Gender / Weight / Height**  
These fields allow you to set the vital statistics of the NPC.

l) **Race**  
This field allows you to set the race of the NPC.  Obviously, races such as 'Dryad' and 'Dragon' should only be selected if you have a good reason to do so.

m) **Attack Type**  
This field allows you to set the unarmed attack type of the NPC.  Explosive effects such as 'rocket' and 'grenade' may or may not have splash damage, depending on your MUD's build.  If they do, it is extremely advisable to NOT set explosive attack types.

n) **Skill Menu**  
Here you can give the NPC up to six skills that it will use when relevant. These skills are usually used to make the NPC better at combat or magic.

o-p) **Arrive Text / Leave Text**  
This allows you to set the movement text of the NPC. This functions much like player-movement text.

r) **Faction Information**  
This field allows you to set the allegiance of the NPC. Characters who help or harm this NPC will have their reputation within the faction network modified accordingly.


## Mob Flags

**!BLIND**  
This legacy flag has no effect.

**!EXPLODE**  
Explosive attacks do not damage this NPC.

**!KILL**  
Attacks do not damage this NPC.

**!RAM**  
It is not possible to ram this NPC.

**AGGR**  
NPC attacks PCs and all vehicles in the room.

**AGGR_DWARF**  
NPC attacks Dwarf, Gnome, Menehune, Koborokuru PCs and all vehicles in the room.

**AGGR_ELF**  
NPC attacks Elf, Wakyambi, Night-One, Dryad PCs and all vehicles in the room.

**AGGR_HUMAN**  
NPC attacks Human PCs and all vehicles in the room.

**AGGR_ORK**  
NPC attacks Ork, Hobgoblin, Ogre, Satyr, Oni PCs and all vehicles in the room.

**AGGR_TROLL**  
NPC attacks Troll, Cyclops, Fomori, Giant, Minotaur PCs and all vehicles in the room.

**ASTRAL**  
The NPC is an astral form such as a spirit or elemental.

**AWARE**  
This legacy flag has no effect.

**AZTECHNOLOGY**  
This legacy flag has no effect.

**DUAL**  
This NPC is dual-natured and as such has a presence on the astral plane.

**FLAMEAURA**  
This flag causes engulfing fire attacks performed by the NPC to be stronger. This also modifies the NPC's non-default room desc to add 'surrounded by flames'.

**GUARD**  
This NPC will attack players bearing gear that violates the zone's security rating. This NPC will also attack players who are shooting nearby. This NPC will attack vehicles under the following conditions: The vehicle is a drone OR is a bike that is off-road at the moment.

**HELPER**  
This NPC will jump to the aid of NPCs fighting characters.  This NPC will also shoot at characters shooting nearby.

**INANIMATE**  
This NPC is an inanimate object; used for making vending machines and the like.  This NPC cannot be hit by manabolts, is immune to stunbolts, does not bleed, and does not fight back.

**ISNPC**  
Leave this set.  This flags an NPC as being an NPC.

**MEMORY**  
This NPC remembers those who attack it and will attack them the next time it sees them.

**PRIVATE**  
This flag stops the NPC's stats from being investigated by Immortals below level 4.

**SCAVENGER**  
This NPC will pick up the most expensive item it can carry every action round.

**SENTINEL**  
This NPC will stand still and not leave its post under normal conditions.

**SNIPER**  
This NPC will perform its duties at range (guarding with a sniper rifle, aggro'ing with a sniper rifle, etc).  The exact range is determined by the NPC's natural vision and the range of its weapon.

**SPEC**  
This NPC has a special function hardcoded in spec_assign.cpp.

**SPIRITGUARD**  
Set by the code for spirits / elementals performing services. Do not set.

**SPIRITSORCERY**  
Set by the code for spirits / elementals performing services. Do not set.

**SPIRITSTUDY**  
 Set by the code for spirits / elementals performing services. Do not set.

**STAY-ZONE**  
This NPC will only move to rooms that are within the zone of its current room.

**TOTALINVIS**  
This NPC is completely invisible to mortals.

**TRACK**  
This legacy flag has no effect.

**WIMPY**  
This NPC will flee if its health gets too low.


## Affected Flags

**Acid**  
This flag is applied by acid-based spells; do not set.

**ACTION**  
This flag is used by the combat code; do not set.

**APPROACH**  
This flag is used by the combat code; do not set.

**Banishing**  
This flag is used by the banish code; do not set.

**Binding**  
This flag 'binds' a character to the ground, making them unable to move without first passing a check.

**Bonding Focus**  
This flag is used by the bonding code; do not set.

**Building Lodge**  
 This flag is used by the lodge-construction code; do not set.

**Conjuring**  
This flag is used by the conjuring code; do not set.

**COUNTERATTACK**  
This flag is used by the fight code; do not set.

**Damaged**  
This flag is used by the damage and healing code; do not set.

**Designing**  
This flag is used by the programming code; do not set.

**Det-invis**  
   This flag gives a +4 TN modifier instead of the usual +8 when fighting invisible enemies. This flag functions as ultrasound.

**Detox**  
This flag is used by the Detox spell to remove drugs; do not set.

**Drawing Circle**  
   This flag is used by the circle-creation code; do not set.

**Charm**  
This flag mimics the effect of the standard fantasy 'Charm' spell; the afflicted NPC cannot attack or leave their 'master' willingly.

**Fear**  
This flag causes the NPC to flee in fear until a test is passed.

**Group**  
This flag is used by the group code; do not set.

**Healed**  
This flag marks the character as having been healed already (and thus being immune to another healing spell until given a new set of wounds).

**Hide**  
This flag causes the affected NPC to be hidden from sight until the hide is broken.

**Infravision**  
This flag sets the NPC's vision to Thermographic.

**ImpInvis (Spell)**  
This flag makes the NPC invisible as per the Improved Invisibility spell.

**Invis (Spell)**  
This flag makes the NPC invisible as per the Invisibility spell.

**Laser-sight**  
This flag gives the NPC the benefits of a laser-sighted weapon.

**LL-eyes**  
This flag sets the NPC's vision to Low-Light.

**Manifest**  
This flag is used by the astral projection code; do not set.

**Manning**  
This flag is used by the turret-manning code; do not set.

**NOTHING**  
This legacy flag does nothing.

**Packing Workshop**  
This flag is used by the workshop-packing code; do not set.

**Part Building**  
This flag is used by the deck construction code; do not set.

**Part Designing**  
   This flag is used by the deck construction code; do not set.

**Petrify**  
   This flag locks the NPC / character down completely, blocking all commands. This causes many problems and cannot be removed easily from players on AW; don't set it without good reason.

**Pilot**  
This flag is used by the code to designate a driver; do not set.

**Programming**  
This flag is used by the code to designate a programmer; do not set.

**Prone**  
This flag is used to designate whether or not a character is in the prone position.

**ResistPain**  
This marks a character as being affected by pain resistance, removing TN penalties for damage but making the damage invisible to the character.

**Rigger**  
This is used by the code to mark an active rigger; do not set.

**Ruthenium**  
This makes the character invisible as if covered in Ruthenium.

**Sneak**  
This flags the character as sneaking.

**Spell Design**  
This flag is used by the spell-design code; do not set.

**Stabilize**  
This flag is used by the healing and damage code; do not set.

**Surprised**  
This flag is used by the combat code; do not set.

**Thermoptic**  
This makes the character invisible as if covered in thermoptic material.

**Tracked**  
This flag is used in the astral tracking code; do not set.

**Tracking**  
This flag is used in the astral tracking code; do not set.

**Vision x1**  
This marks the character as having 1x zoom vision.

**Vision x2**  
This marks the character as having 2x zoom vision.

**Vision x3**  
This marks the character as having 3x zoom vision.

**Withdrawl**  
This flag is used by the drug code; do not set.

**Withdrawl (Forced)**  
This flag is used by the drug code; do not set.


