CREATE TABLE `pfiles_scriptvars` (
  `idnum` mediumint(5) unsigned NOT NULL,
  `name` varchar(100) NOT NULL,
  `context` bigint(20) NOT NULL default 0,
  `value` text NOT NULL,
  PRIMARY KEY (`idnum`, `name`, `context`)
);
