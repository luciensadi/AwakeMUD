CREATE TABLE `pfiles_stowed` (
  `idnum` mediumint(5) unsigned NOT NULL,
  `vnum` mediumint(5) unsigned NOT NULL,
  `qty` smallint(3) unsigned NOT NULL,
  PRIMARY KEY(`idnum`, `vnum`)
);