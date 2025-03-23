/* ************************************************************************
*   File: handler.h                                     Part of CircleMUD *
*  Usage: header file: prototypes of handling and utility functions       *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#ifndef _handler_h_
#define _handler_h_

/* handling the affected-structures */
void  affect_total_veh(struct veh_data *veh);
void    affect_total(struct char_data *ch);
void    affect_modify(struct char_data *ch,
                      byte loc,
                      sbyte mod,
                      const Bitfield &bitv,
                      bool add
                       );
int  affected_by_spell(struct char_data *ch, int type);
int   affected_by_power(struct char_data *ch, int type);
void    affect_veh(struct veh_data *veh, byte loc, sbyte mod);
void    spell_modify(struct char_data *ch, struct sustain_data *sust, bool add);
/* utility */
int isname(const char *str, const char *namelist);
char    *fname(char *namelist);
char    *fname_allchars(char *namelist);
int     get_number(char **name, size_t name_len);
bool vnum_from_non_approved_zone(vnum_t vnum);
bool vnum_from_editing_restricted_zone(vnum_t vnum);
struct remem *safe_found_mem(struct char_data *rememberer, struct char_data *ch);
struct remem *unsafe_found_mem(struct remem *mem, struct char_data *ch);
int recog(struct char_data *ch, struct char_data *i, const char *name);
void phone_check();
int find_skill_num(char *name);
int find_spell_num(char *name);
int find_ability_num(char *name);
/* ******** objects *********** */

void    obj_to_char(struct obj_data *object, struct char_data *ch);
void    obj_from_char(struct obj_data *object);

bool    equip_char(struct char_data *ch, struct obj_data *obj, int pos, bool recalc = TRUE);
struct obj_data *unequip_char(struct char_data *ch, int pos, bool focus, bool recalc = TRUE);

struct obj_data *get_obj_in_list(char *name, struct obj_data *list);
struct obj_data *get_obj_in_list_num(int num, struct obj_data *list);
struct obj_data *get_obj(char *name);
struct obj_data *get_obj_num(int nr);

void    obj_to_veh(struct obj_data *object, struct veh_data *veh);
void    obj_to_room(struct obj_data *object, struct room_data *room);
void    obj_from_room(struct obj_data *object);
void    obj_to_obj(struct obj_data *obj, struct obj_data *obj_to);
void    obj_from_obj(struct obj_data *obj);

void obj_from_cyberware(struct obj_data *cyber, bool recalc = TRUE);
void obj_from_bioware(struct obj_data *bio, bool recalc = TRUE);
void obj_to_cyberware(struct obj_data *object, struct char_data *ch, bool recalc = TRUE);
void obj_to_bioware(struct obj_data *object, struct char_data *ch, bool recalc = TRUE);

void    extract_obj(struct obj_data *obj, bool dont_warn_on_kept_items=FALSE);
void    extract_veh(struct veh_data *veh);
void    extract_icon(struct matrix_icon *icon);
/* ******* characters ********* */

struct char_data *get_char_room(const char *name, struct room_data *room);
struct char_data *get_char_in_list_vis(struct char_data *ch, const char *name, struct char_data *list);
// struct char_data *get_char(const char *name);

void    char_from_room(struct char_data *ch);
void    char_to_room(struct char_data *ch, struct room_data *room);
void    extract_char(struct char_data *ch, bool do_save=TRUE);
void    char_to_veh(struct veh_data *veh, struct char_data *ch);
void veh_from_room(struct veh_data *veh);
void veh_to_room(struct veh_data *veh, struct room_data *room);
void veh_to_veh(struct veh_data *veh, struct veh_data *dest);
int veh_skill(struct char_data *ch, struct veh_data *veh, int *tn);
void	icon_from_host(struct matrix_icon *icon);
void	icon_to_host(struct matrix_icon *icon, vnum_t to_host);

/* find if character can see */
struct char_data *get_char_room_vis(struct char_data *ch, const char *name);
struct char_data *get_player_vis(struct char_data *ch, const char *name, int inroom);
struct char_data *get_char_vis(struct char_data *ch, const char *name);
struct char_data *get_char_veh(struct char_data *ch, const char *name, struct veh_data *veh);
struct obj_data *get_obj_in_list_vis(struct char_data *ch,const  char *name, struct obj_data *list);
struct obj_data *get_obj_vis(struct char_data *ch, const char *name);
struct obj_data *get_object_in_equip_vis(struct char_data *ch,
          const char *arg, struct obj_data *equipment[], int *j);

struct veh_data *get_veh_list(char *name, struct veh_data *list, struct char_data *ch);

/* find all dots */

int     find_all_dots(char *arg, size_t arg_size);

#define FIND_INDIV      0
#define FIND_ALL        1
#define FIND_ALLDOT     2


/* Generic Find */

int     generic_find(char *arg, int bitvector, struct char_data *ch,
                     struct char_data **tar_ch, struct obj_data **tar_obj);

#define FIND_CHAR_ROOM     (1 << 0)
#define FIND_CHAR_WORLD    (1 << 1)
#define FIND_OBJ_INV       (1 << 2)
#define FIND_OBJ_ROOM      (1 << 3)
#define FIND_OBJ_WORLD     (1 << 4)
#define FIND_OBJ_EQUIP     (1 << 5)
#define FIND_OBJ_VEH_ROOM  (1 << 6)
#define FIND_CHAR_VEH_ROOM (1 << 7)
#define NUM_FIND_X_BITS     8

/* prototypes from fight.c */
void    set_fighting(struct char_data *ch, struct char_data *victim, ...);
void    set_fighting(struct char_data *ch, struct veh_data *victim);
void    stop_fighting(struct char_data *ch);
void    stop_follower(struct char_data *ch);
bool    hit(struct char_data *ch, struct char_data *victim, struct obj_data *weapon, struct obj_data *vweapon, struct obj_data *ammobox);
void    forget(struct char_data *ch, struct char_data *victim);
void    remember(struct char_data *ch, struct char_data *victim);
bool    damage(struct char_data *ch, struct char_data *victim, int dam, int attacktype,
               bool is_physical);
bool    skill_message(int dam, struct char_data *ch, struct char_data *vict,
                      int attacktype);

bool    vcombat(struct char_data *ch, struct veh_data *veh);
bool    vram(struct veh_data *veh, struct char_data *ch, struct veh_data *tveh);
void    process_vcombat(struct veh_data *veh);
void    chkdmg(struct veh_data * veh);
void    disp_mod(struct veh_data *veh, struct char_data *ch, int i);
void    stop_chase(struct veh_data *veh);
#endif
