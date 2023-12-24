CREATE TABLE `pfiles_named_tags` (
  `idnum` mediumint(5) unsigned default '0',
  `tag_name` varchar(256) NOT NULL,
  PRIMARY KEY (`idnum`, `tag_name`)
);