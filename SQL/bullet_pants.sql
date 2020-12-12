DROP TABLE IF EXISTS `pfiles_ammo`;

CREATE TABLE `pfiles_ammo` (
  `idnum` mediumint(5) unsigned unique default '0',
  `weapon` smallint unsigned NOT NULL,
  `normal` smallint unsigned NOT NULL,
  `apds` smallint unsigned NOT NULL,
  `explosive` smallint unsigned NOT NULL,
  `ex` smallint unsigned NOT NULL,
  `flechette` smallint unsigned NOT NULL,
  `gel` smallint unsigned NOT NULL,
  PRIMARY KEY (`idnum`)
);

INSERT INTO help_topic (name, body) VALUES ('RELOAD', 'Usage:\r\n^g  RELOAD <weapon> [ammotype]^n\r\n^g  RELOAD^n\r\n\r\nReloads the specified weapon with the specified ammo type, or with whatever it was loaded with last if you omit the ammotype argument. On its own, reloads your current weapon with whatever is in it  already. Draws ammo from your ^Wpockets^n, doesn\'t work with ammo boxes!\r\n\r\nSee also: ^WPOCKETS^n\r\n') ON DUPLICATE KEY UPDATE `name` = VALUES(`name`), `body` = VALUES(`body`);
