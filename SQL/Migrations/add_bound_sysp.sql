ALTER TABLE `pfiles` ADD `RestrictedSysPoints` smallint(5) unsigned default 0 AFTER `garnishment_notor`;

ALTER TABLE `pfiles_chargendata` ADD `restricted_sysp_spent` smallint(5) unsigned default '0' AFTER `channel_points`;