DROP TABLE IF EXISTS `pfiles_ammo`;

CREATE TABLE `pfiles_ammo` (
  `idnum` mediumint(5) unsigned default '0',
  `weapon` smallint unsigned NOT NULL,
  `normal` smallint unsigned NOT NULL,
  `apds` smallint unsigned NOT NULL,
  `explosive` smallint unsigned NOT NULL,
  `ex` smallint unsigned NOT NULL,
  `flechette` smallint unsigned NOT NULL,
  `gel` smallint unsigned NOT NULL,
  `harmless` smallint unsigned NOT NULL,
  `anti-vehicle` smallint unsigned NOT NULL
);

INSERT INTO help_topic (name, body) VALUES ('RELOAD', 'Usage:\r\n^g  RELOAD <weapon> [ammotype]^n\r\n^g  RELOAD [ammotype]^n\r\n^g  RELOAD^n\r\n^n\r\nReloads the specified weapon with the specified ammo type, or with whatever it was loaded with last if you omit the ammotype argument. On its own, reloads your current weapon with whatever is in it  already. Draws ammo from your ^Wpockets^n, doesn\'t work with ammo boxes!\r\n^n\r\nSee also: ^WPOCKETS, AMMO^n\r\n') ON DUPLICATE KEY UPDATE `name` = VALUES(`name`), `body` = VALUES(`body`);
INSERT INTO help_topic (name, body) VALUES ('POCKETS BULLETPANTS', 'Usage:\r\n  ^gPOCKETS^n -- show your current ammo loadout.\r\n^n\r\n  ^gPOCKETS GET ALL^n -- get all ammo from pockets.\r\n  ^gPOCKETS GET <amount> <ammotype> <weapontype>^n -- get ammo from pockets.\r\n^n\r\n  ^gPOCKETS PUT ALL^n -- empty all carried ammo boxes into pockets.\r\n  ^gPOCKETS PUT <box>^n -- empty a specific carried ammobox into pockets.\r\n  ^gPOCKETS PUT <amount> <ammotype> <weapontype>^n -- load ammo from boxes into pockets. Works with boxes on the ground, too.\r\n^n\r\nThe pockets / ammopants system replaces the previous ammunition system. Now, you reload directly from the contents of your pockets. No pants or special objects are required to use pockets; you are free to keister rounds and otherwise "PAIGE NO" yourself as you please.\r\n^n\r\nIn the unlikely(?) event of your character\'s untimely demise, the contents of their pockets will be disgorged into their belongings as ammo boxes.\r\n^n\r\n^WSee Also: RELOAD, AMMO^n') ON DUPLICATE KEY UPDATE `name` = VALUES(`name`), `body` = VALUES(`body`);
INSERT INTO help_topic (name, body) VALUES ('AMMUNITION AMMO', 'There are 6 major types of ammunition in Shadowrun. Normal\r\nammunition is just that, it has no special effects. The others\r\nare:\r\n\r\n o APDS (Armour Piercing Discarding Sabot)\r\n   APDS is used to blast through heavily armoured enemies. It\r\n   halves their ballistic armour rating.\r\n \r\n o Explosive\r\n   Explosive rounds are solid slugs that are designed to fragment\r\n   once they hit their target. This increases the Power by 1.\r\n\r\n o EX (Enhanced eXplosive)\r\n   EX is a more damaging version of the Explosive ammunition. It\r\n   increases the Power by 2.\r\n\r\n o Flechette\r\n   Flechette ammunition fragments as it leaves the barrel, causing\r\n   massive damage to unarmoured opponents, but is easily stopped by\r\n   most armour. It increases the damage level by 1, but the Power\r\n   is lowered by either the ballistic armour of the target, or twice\r\n   their impact, whichever is higher.\r\n\r\n o Gel\r\n   Gel ammunition is non-lethal. They are designed to take down targets\r\n   without killing them. The Power of all rounds fired is reduced by 2\r\n   and do stun damage.\r\n\r\n^WSee Also: RELOAD^n\r\n') ON DUPLICATE KEY UPDATE `name` = VALUES(`name`), `body` = VALUES(`body`);
