#ifndef _bullet_pants_h
#define _bullet_pants_h

#define MAX_NUMBER_OF_BULLETS_IN_PANTS 5000

// Set to the number of mags' worth of normal ammo you want to give to NPCs with weapons but no ammo data set. Remember that the first mag goes into the weapon.
#define NUMBER_OF_MAGAZINES_TO_GIVE_TO_UNEQUIPPED_MOBS 4

#define NPC_AMMO_USAGE_PREFERENCES_AMMO_NORMAL_INDEX 3

// Give this a weapon type straight from the weapon-- ex WEAP_SMG. It will convert it for you.
#define GET_BULLETPANTS_AMMO_AMOUNT(ch, weapon_type, ammo_type) ((ch)->bullet_pants[(weapon_type) - START_OF_AMMO_USING_WEAPONS][(ammo_type)])

// 'does this weapon type use shells, rounds, darts...'
const char *get_weapon_ammo_name_as_string(int weapon_type);

// For pockets command.
extern const char *weapon_type_aliases[];
extern int npc_ammo_usage_preferences[];

// print vict's bullet pants to ch. ch == vict is allowed.
void display_pockets_to_char(struct char_data *ch, struct char_data *vict);

// Use this to change the bullet pants quantity.
extern bool update_bulletpants_ammo_quantity(struct char_data *ch, int weapon, int ammotype, int quantity);

// Various helper funcs.
bool print_one_weapontypes_ammo_to_string(struct char_data *ch, int wp, char *buf, int bufsize);
const char *get_ammo_representation(int weapon, int ammotype, int quantity);
bool is_valid_pockets_put_command(char *mode_buf);
bool is_valid_pockets_get_command(char *mode_buf);
const char *get_ammobox_default_restring(struct obj_data *ammobox);
bool reload_weapon_from_bulletpants(struct char_data *ch, struct obj_data *weapon, int ammotype);

float get_ammo_weight(int weapontype, int ammotype, int qty);
int get_ammo_cost(int weapontype, int ammotype, int qty);

#endif
