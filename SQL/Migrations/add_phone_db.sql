CREATE TABLE `phone_numbers` (
  `phone_number` CHAR(8) NOT NULL,
  `associated_obj_idnum` bigint(5) NOT NULL,
  `associated_obj_vnum` bigint(5) NOT NULL,
  `created_on` bigint(5) NOT NULL,
  `last_player_name` VARCHAR(256),
  `load_origin` VARCHAR(1),
  PRIMARY KEY (`phone_number`)
);