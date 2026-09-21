-- Order matters: load_char() (src/newdb.cpp) reads pfiles POSITIONALLY via
-- "SELECT * FROM pfiles", and expects the tail to be
--   ... submersion_grade, garnishment_nuyen, garnishment_rep, garnishment_notor, RestrictedSysPoints ...
-- Chaining each AFTER-clause to the previous column (rather than all three to
-- submersion_grade) is what produces that order.
ALTER TABLE pfiles ADD `garnishment_nuyen` mediumint(6) default '0' AFTER `submersion_grade`;
ALTER TABLE pfiles ADD `garnishment_rep` mediumint(6) default '0' AFTER `garnishment_nuyen`;
ALTER TABLE pfiles ADD `garnishment_notor` mediumint(6) default '0' AFTER `garnishment_rep`;