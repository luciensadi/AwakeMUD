DROP TABLE IF EXISTS `pfiles_ignore_v2`;

CREATE TABLE `pfiles_ignore_v2` (
  `idnum` mediumint(5) unsigned,
  `vict_idnum` mediumint(5) unsigned,
  `bitfield_str` varchar(128) default '0' NOT NULL,
  PRIMARY KEY (`idnum`, `vict_idnum`)
);
