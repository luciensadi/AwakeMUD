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
