CREATE TABLE `pfiles_exdescs` (
  `idnum` mediumint(5) unsigned NOT NULL,
  `keyword` varchar(100) NOT NULL,
  `name` varchar(200) NOT NULL,
  `desc` text NOT NULL,
  `wearslots` varchar(100) NOT NULL,
  PRIMARY KEY (`idnum`, `keyword`)
);

ALTER TABLE `pfiles` ADD `exdesc_max` smallint(5) unsigned default 0 AFTER `lifestyle_string`;