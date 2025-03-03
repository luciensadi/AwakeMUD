ALTER TABLE `pfiles`
    ADD `otaku_path` tinyint(2) 
    DEFAULT 0 
    AFTER `exdesc_max`;

ALTER TABLE `pfiles_chargendata`
    ADD `channel_points` tinyint(2) 
    DEFAULT 0 
    AFTER `prestige_alt`;
