CREATE TABLE `pocsec_phonebook` (
  `record_id` int NOT NULL AUTO_INCREMENT,
  `idnum` mediumint(5) unsigned NOT NULL,
  `phonenum` int(5) NOT NULL,
  `note` varchar(200) NOT NULL,
  PRIMARY KEY (`record_id`)
);