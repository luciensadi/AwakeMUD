CREATE TABLE `matrix_files` (
  `idnum` mediumint(5) unsigned,
  `name` varchar(256) default '',
  `file_type` tinyint(2) default '0',
  `program_type` tinyint(2) default '0',
  `rating` tinyint(2) default '0',
  `size` int(8) default '0',
  `original_size` int(8) default '0',
  `compression_factor` tinyint(2) default '0',
  `wound_category` tinyint(2) default '0',
  `is_default` tinyint(2) default '0',
  `creation_time` mediumint(5) unsigned default '0',
  `content` TEXT default '',

  `skill` int(8) default '0',
  `linked` mediumint(5) unsigned,

  `work_phase` tinyint(2) default '0',
  `work_ticks_left` tinyint(2) default '0',
  `work_original_ticks_left` tinyint(2) default '0',
  `work_successes` tinyint(2) default '0',

  `last_decay_time` mediumint(5) unsigned default '0',

  `creator_idnum` mediumint(5) unsigned default '0',
  `in_obj_vnum` mediumint(5) unsigned default '0',
  `in_obj_idnum` bigint(5) unsigned default '0',
  KEY(`idnum`)
);

ALTER TABLE `pfiles_inv`
    ADD `Matrix_Restring` varchar(256)
    DEFAULT 0 
    AFTER `obj_idnum`;

ALTER TABLE `pfiles_worn`
    ADD `Matrix_Restring` varchar(256)
    DEFAULT 0 
    AFTER `obj_idnum`;
