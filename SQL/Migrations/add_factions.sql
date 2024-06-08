CREATE TABLE `pfiles_factions` (
  `idnum` mediumint(5) unsigned default '0',
  `faction` mediumint(5) unsigned NOT NULL,
  `rep` smallint(5) NOT NULL,
  PRIMARY KEY (`idnum`, `faction`)
);