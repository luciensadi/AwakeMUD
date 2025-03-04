ALTER TABLE `pfiles`
    ADD `submersion_grade` smallint(3)
    DEFAULT 0 
    AFTER `otaku_path`;

CREATE TABLE `pfiles_echoes` (
  `idnum` mediumint(5) unsigned default '0',
  `echonum` smallint(3) unsigned default '0',
  `rank` smallint(3) unsigned default '0',
  KEY(`idnum`)
);