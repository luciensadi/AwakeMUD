CREATE TABLE `pfiles_favours` (
  `idnum` mediumint(5) unsigned default '0',
  `number` smallint(3) unsigned default '0',
  `questnum` mediumint(3) unsigned default '0',
  `completed` boolean default FALSE,
  KEY(`idnum`)
);