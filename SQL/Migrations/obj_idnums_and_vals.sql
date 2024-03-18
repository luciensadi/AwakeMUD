ALTER TABLE `pfiles_bioware`   ADD `Value12`   int(2) default '0' AFTER `Value11`;
ALTER TABLE `pfiles_bioware`   ADD `Value13`   int(2) default '0' AFTER `Value12`;
ALTER TABLE `pfiles_bioware`   ADD `Value14`   int(2) default '0' AFTER `Value13`;
ALTER TABLE `pfiles_bioware`   ADD `obj_idnum` bigint(5) unsigned default '0';

ALTER TABLE `pfiles_cyberware` ADD `Value12`   int(2) default '0' AFTER `Value11`;
ALTER TABLE `pfiles_cyberware` ADD `Value13`   int(2) default '0' AFTER `Value12`;
ALTER TABLE `pfiles_cyberware` ADD `Value14`   int(2) default '0' AFTER `Value13`;
ALTER TABLE `pfiles_cyberware` ADD `obj_idnum` bigint(5) unsigned default '0';

ALTER TABLE `pfiles_inv`       ADD `Value12`   int(2) default '0' AFTER `Value11`;
ALTER TABLE `pfiles_inv`       ADD `Value13`   int(2) default '0' AFTER `Value12`;
ALTER TABLE `pfiles_inv`       ADD `Value14`   int(2) default '0' AFTER `Value13`;
ALTER TABLE `pfiles_inv`       ADD `obj_idnum` bigint(5) unsigned default '0';

ALTER TABLE `pfiles_worn`      ADD `Value12`   int(2) default '0' AFTER `Value11`;
ALTER TABLE `pfiles_worn`      ADD `Value13`   int(2) default '0' AFTER `Value12`;
ALTER TABLE `pfiles_worn`      ADD `Value14`   int(2) default '0' AFTER `Value13`;
ALTER TABLE `pfiles_worn`      ADD `obj_idnum` bigint(5) unsigned default '0';
