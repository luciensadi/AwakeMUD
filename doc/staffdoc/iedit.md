# Item Editing (IEDIT)

The IEDIT command is used to create and edit items within the game world. Like the other OLC commands, you can use "IEDIT #" to edit a specific item; if no item exists at that vnum, you will have the option of creating one there.


## The Main IEDIT Menu

1) **Item keywords**  
These are the aliases / keywords that one can use when referring to the item in commands.

2) **Item name**  
This is the name of the item as it appears in the inventory and other places; this should always begin with a lower-case letter (usually in the form of 'a/an whatever') and should never end with punctuation.

3) **Room description**  
This is the string shown to players when this item has been dropped in the room. This should be a full sentence starting with a capital and ending in punctuation; make sure there is no color bleed at the end.

4) **Look description**  
This is the text printed when one looks at the item.

5) **Item type**  
This is the item's classification in the code.

6) **Item extra flags**  
These allow you to specify various behaviors for your object. A full listing of extra flags can be found FIXME.

7) **Item affection flags**  
These allow you to add affections to the item that will be applied to characters who wear/wield the item. A full listing of affection flags can be found here.

8) **Item wear flags**
This menu allows you to set the locations that this item is wearable on.

9) **Item weight**  
This is the item's weight in kilograms, specifiable up to two decimal places.

a) **Item cost**  
This is the item's cost in nuyen.

b) **Item availability**  
Entering this menu allows you to set the availability of the item in TN and delay.

c) **Item timer**  
This field is largely used by the code and should be left at 0 unless there is a good reason to change it.

d) **Item Material**  
This sets the material the item is made of and is used in breakage tests.

e) **Item Barrier Rating**  
This dictates the durability of the item and is used in breakage tests.

f) **Item Legality**  
You can set the legality of the item here through the three-step menu.

g) **Item values**  
'Item values' is a catch-all set of data that allows you to specify the behaviors of the item. The contents of this menu are different for each item type.

h) **Item applies**  
This field is used to apply bonuses and penalties to the character when equipping the item. These options should not be used without checking with your head immortal first.

i) **Item extra descriptions**  
  You can set extra descriptions here, specifying their keywords and the desc they print.

