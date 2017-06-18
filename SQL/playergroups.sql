
DROP TABLE IF EXISTS `pfiles_playergroups`;
DROP TABLE IF EXISTS `playergroups`;
DROP TABLE IF EXISTS `playergroup_invitations`;
DROP TABLE IF EXISTS `pgroup_logs`;

CREATE TABLE `playergroups` (
  `idnum` mediumint(5) unsigned unique default '0',
  `Name` varchar(80) default 'An Unimaginative Player Group',
  `Alias` varchar(20) default 'newgroup',
  `Tag` varchar(30) default '^R[^WEMPTY^R]^n',
  `Settings` varchar(128) default '0',
  PRIMARY KEY (`idnum`),
  KEY (`Alias`)
);

CREATE TABLE `pfiles_playergroups` (
  `idnum` mediumint(5) unsigned unique default '0',
  `group` mediumint(5) unsigned default '0',
  `Rank` tinyint(2) unsigned default '0',
  `Privileges` varchar(128) default '0',
  FOREIGN KEY (`idnum`) REFERENCES pfiles(`idnum`),
  FOREIGN KEY (`group`) REFERENCES playergroups(`idnum`)
);

CREATE TABLE `playergroup_invitations` (
  `idnum` mediumint(5) unsigned default '0',
  `Group` mediumint(5) unsigned default '0',
  `Expiration` bigint(32) default '0',
  KEY (`idnum`),
  KEY (`Group`)
);

CREATE TABLE `pgroup_logs` (
  `idnum` mediumint(5) unsigned default '0',
  `message` varchar(1000) NOT NULL,
  `date` timestamp,
  KEY (`idnum`)
);

ALTER TABLE `pfiles` ADD `pgroup` mediumint(5) unsigned default '0';