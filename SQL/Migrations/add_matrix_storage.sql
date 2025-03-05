
CREATE TABLE `matrix_files` (
  `idnum` mediumint(5) unsigned default '0',
  `name` varchar(256) default '',
  `storage_idnum` mediumint(5) unsigned default '0',
  KEY(`idnum`)
);