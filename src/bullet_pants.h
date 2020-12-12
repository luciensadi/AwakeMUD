#ifndef _bullet_pants_h
#define _bullet_pants_h

#define MAX_NUMBER_OF_BULLETS_IN_PANTS USHRT_MAX

// Give this a weapon type straight from the weapon-- ex WEAP_SMG. It will convert it for you.
#define GET_BULLETPANTS_AMMO_AMOUNT(ch, weapon_type, ammo_type) ((ch)->bullet_pants[(weapon_type) - START_OF_AMMO_USING_WEAPONS][(ammo_type)])

// 'does this weapon type use shells, rounds, darts...'
const char *get_weapon_ammo_name_as_string(int weapon_type);

// For pockets command.
extern const char *weapon_type_aliases[];
extern int npc_ammo_usage_preferences[];

// print vict's bullet pants to ch. ch == vict is allowed.
void display_pockets_to_char(struct char_data *ch, struct char_data *vict);

// Use this to change the ammopants quantity.
extern bool update_ammopants_ammo_quantity(struct char_data *ch, int weapon, int ammotype, int quantity);

// Various helper funcs.
bool print_one_weapontypes_ammo_to_string(struct char_data *ch, int wp, char *buf, int bufsize);
const char *get_ammo_representation(int weapon, int ammotype, int quantity);
bool is_valid_pockets_put_command(char *mode_buf);
bool is_valid_pockets_get_command(char *mode_buf);
const char *get_ammobox_default_restring(struct obj_data *ammobox);
bool have_bullet_pants_table();
bool reload_weapon_from_ammopants(struct char_data *ch, struct obj_data *weapon, int ammotype);

#endif
