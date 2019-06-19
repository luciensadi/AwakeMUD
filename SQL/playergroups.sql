
DROP TABLE IF EXISTS `pfiles_playergroups`;
DROP TABLE IF EXISTS `playergroups`;
DROP TABLE IF EXISTS `playergroup_invitations`;
DROP TABLE IF EXISTS `pgroup_logs`;

CREATE TABLE `playergroups` (
  `idnum` mediumint(5) unsigned unique default '0',
  `Name` varchar(161) default 'An Unimaginative Player Group' NOT NULL,
  `Alias` varchar(41) default 'newgroup' NOT NULL,
  `Tag` varchar(47) default '^R[^WEMPTY^R]^n' NOT NULL,
  `Settings` varchar(128) default '0' NOT NULL,
  `bank` bigint(9) unsigned default '0' NOT NULL,
  PRIMARY KEY (`idnum`),
  KEY (`Alias`)
);

CREATE TABLE `pfiles_playergroups` (
  `idnum` mediumint(5) unsigned unique default '0',
  `group` mediumint(5) unsigned default '0' NOT NULL,
  `Rank` tinyint(2) unsigned default '0' NOT NULL,
  `Privileges` varchar(128) default '0' NOT NULL,
  FOREIGN KEY (`idnum`) REFERENCES pfiles(`idnum`),
  FOREIGN KEY (`group`) REFERENCES playergroups(`idnum`)
);

CREATE TABLE `playergroup_invitations` (
  `idnum` mediumint(5) unsigned default '0' NOT NULL,
  `Group` mediumint(5) unsigned default '0' NOT NULL,
  `Expiration` bigint(32) default '0' NOT NULL,
  KEY (`idnum`),
  KEY (`Group`)
);

CREATE TABLE `pgroup_logs` (
  `idnum` mediumint(5) unsigned default '0' NOT NULL,
  `message` varchar(513) NOT NULL,
  `date` timestamp,
  `redacted` BOOL NOT NULL,
  KEY (`idnum`)
);

ALTER TABLE `pfiles` ADD `pgroup` mediumint(5) unsigned default '0';