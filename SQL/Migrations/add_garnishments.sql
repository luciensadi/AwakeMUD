ALTER TABLE pfiles ADD `garnishment_nuyen` mediumint(6) default '0' AFTER `submersion_grade`;
ALTER TABLE pfiles ADD `garnishment_rep` mediumint(6) default '0' AFTER `submersion_grade`;
ALTER TABLE pfiles ADD `garnishment_notor` mediumint(6) default '0' AFTER `submersion_grade`;