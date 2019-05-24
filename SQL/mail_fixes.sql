CREATE TABLE `pfiles_mail` (
  `idnum` INT AUTO_INCREMENT,
  `sender_id` mediumint(5) unsigned default '0' NOT NULL,
  `sender_name` varchar(150) default 'unknown',
  `recipient` mediumint(5) unsigned default '0' NOT NULL,
  `timestamp` timestamp NOT NULL,
  `text` text NOT NULL,
  PRIMARY KEY (`idnum`),
  KEY (`recipient`)
);