extern struct char_data *create_elemental(struct char_data *ch, int type, int force, int idnum, int tradition);
extern void circle_build(struct char_data *ch, char *type, int force);
extern void lodge_build(struct char_data *ch, int force);
extern bool conjuring_drain(struct char_data *ch, int force);
extern void end_spirit_existance(struct char_data *ch, bool message);
extern bool check_spirit_sector(struct room_data *room, int type);
extern bool spell_drain(struct char_data *ch, int type, int force, int damage);
extern void totem_bonus(struct char_data *ch, int action, int type, int &target, int &skill);
extern void mob_cast(struct char_data *ch, struct char_data *tch, struct obj_data *tobj, int spellnum, int level);
extern void end_sustained_spell(struct char_data *ch, struct sustain_data *sust);
extern void stop_spirit_power(struct char_data *spirit, int type);
extern char_data *find_spirit_by_id(int spiritid, long playerid);
extern void elemental_fulfilled_services(struct char_data *ch, struct char_data *mob, struct spirit_data *spirit);

#define DAMOBJ_NONE                     0
#define DAMOBJ_ACID                     1
#define DAMOBJ_AIR                      2
#define DAMOBJ_EARTH            3
#define DAMOBJ_FIRE                     4
#define DAMOBJ_ICE                      5
#define DAMOBJ_LIGHTNING        6
#define DAMOBJ_WATER            7
#define DAMOBJ_EXPLODE          8
#define DAMOBJ_PROJECTILE       9
#define DAMOBJ_CRUSH            10
#define DAMOBJ_SLASH            11
#define DAMOBJ_PIERCE           12
#define DAMOBJ_MANIPULATION     32

#define CONFUSION	0
#define ENGULF		1
#define CONCEAL		2
#define MOVEMENTUP	3
#define MOVEMENTDOWN	4

#define INIT_MAIN	0
#define INIT_META	1
#define INIT_GEAS	2
