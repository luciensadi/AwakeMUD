
DROP TABLE IF EXISTS `command_fuckups`;

CREATE TABLE `command_fuckups` (
  `Name` varchar(161) NOT NULL,
  `Count` int(9) unsigned default '0' NOT NULL,
  PRIMARY KEY (`Name`)
);
