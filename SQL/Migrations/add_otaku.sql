ALTER TABLE `pfiles`
    ADD `otaku_path` tinyint(2) 
    DEFAULT 0 
    AFTER 'exdesc_max';