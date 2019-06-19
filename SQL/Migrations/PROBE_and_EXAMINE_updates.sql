UPDATE help_topic SET `body` = "Usage: ^gEXAMINE <object>^n

May give you some valuable information about an item. Depending on
the item type it will display extra information about the item along
with the same information as look. Lodges and circles will display
what it is dedicated to, guns will display ammo and attachments and
containers will show you their contents.

^WSee Also: LOOK, PROBE^n", category=2 WHERE `name` = "EXAMINE";


INSERT INTO help_topic (name, body, category, links) VALUES ("PROBE", "Usage: ^gPROBE <object or vehicle>^n

This command gives you insight into the out-of-character statistics
of the selected item or vehicle. While some values may be referenced
in character (example: weight, material), you should avoid mention
of values that your character would not know or understand. For
example, damage codes and target numbers are not a concept that are
understood by characters in the game.

^WSee Also: LOOK, EXAMINE^n", 2, NULL);