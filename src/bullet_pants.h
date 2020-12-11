#ifndef _bullet_pants_h
#define _bullet_pants_h

// Give this a weapon type straight from the weapon-- ex WEAP_SMG. It will convert it for you.
#define GET_BULLETPANTS_AMMO_AMOUNT(ch, weapon_type, ammo_type) ((ch)->bullet_pants[(weapon_type) - START_OF_AMMO_USING_WEAPONS][(ammo_type)])

// 'does this weapon type use shells, rounds, darts...'
const char *get_weapon_ammo_name_as_string(int weapon_type);

// For pockets command.
extern const char *weapon_type_aliases[];

// print vict's bullet pants to ch. ch == vict is allowed.
void display_pockets_to_char(struct char_data *ch, struct char_data *vict);

#endif
