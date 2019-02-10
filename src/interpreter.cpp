/*************************************************************************
* File: interpreter.c Part of CircleMUD                                  *
* Usage: parse user commands, search for specials, call ACMD functions   *
*                                                                        *
* All rights reserved.  See license.doc for complete information.        *
*                                                                        *
* Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
* CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
**************************************************************************/

#define __INTERPRETER_CC__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <mysql/mysql.h>

#include "structs.h"
#include "awake.h"
#include "comm.h"
#include "interpreter.h"
#include "db.h"
#include "newdb.h"
#include "utils.h"
#include "handler.h"
#include "mail.h"
#include "screen.h"
#include "memory.h"
#include "newmagic.h"
#include "newshop.h"
#include "quest.h"
#include "constants.h"
#include "config.h"
#include "newmatrix.h"
#include "security.h"

#if defined(__CYGWIN__)
#include <crypt.h>
#endif

#define CH d->character

extern class memoryClass *Mem;

extern const char *MENU;
extern const char *QMENU;
extern const char *WELC_MESSG;
extern const char *START_MESSG;
extern int restrict;
extern MYSQL *mysql;
extern int mysql_wrapper(MYSQL *mysql, const char *query);

/* external functions */
void echo_on(struct descriptor_data * d);
void echo_off(struct descriptor_data * d);
void do_start(struct char_data * ch);
int special(struct char_data * ch, int cmd, char *arg);
int isbanned(char *hostname);
void init_create_vars(struct descriptor_data *d);
// for olc
void vedit_parse(struct descriptor_data * d, const char *arg);
void create_parse(struct descriptor_data *d, const char *arg);
void iedit_parse(struct descriptor_data *d, const char *arg);
void redit_parse(struct descriptor_data *d, const char *arg);
void medit_parse(struct descriptor_data *d, const char *arg);
void qedit_parse(struct descriptor_data *d, const char *arg);
void shedit_parse(struct descriptor_data *d, const char *arg);
void zedit_parse(struct descriptor_data *d, const char *arg);
void hedit_parse(struct descriptor_data *d, const char *arg);
void icedit_parse(struct descriptor_data *d, const char *arg);
void pedit_parse(struct descriptor_data *d, const char *arg);
void dbuild_parse(struct descriptor_data *d, const char *arg);
void pbuild_parse(struct descriptor_data *d, const char *arg);
void spedit_parse(struct descriptor_data *d, const char *arg);
void aedit_parse(struct descriptor_data *d, const char *arg);
void free_shop(struct shop_data *shop);
void free_quest(struct quest_data *quest);
void init_parse(struct descriptor_data *d, char *arg);
void vehcust_parse(struct descriptor_data *d, char *arg);
void pocketsec_parse(struct descriptor_data *d, char *arg);

// for spell creation
void cedit_parse(struct descriptor_data *d, char *arg);

extern void affect_total(struct char_data * ch);
extern void mag_menu_system(struct descriptor_data * d, char *arg);

/* prototypes for all do_x functions. */
ACMD(do_olcon);
ACMD(do_accept);
ACMD(do_action);
ACMD(do_activate);
ACMD(do_advance);
ACMD(do_alias);
ACMD(do_assist);
ACMD(do_ask);
ACMD(do_astral);
ACMD(do_assense);
ACMD(do_at);
ACMD(do_attach);
ACMD(do_award);
ACMD(do_availoffset);
ACMD(do_backstab);
ACMD(do_ban);
ACMD(do_banish);
ACMD(do_bash);
ACMD(do_bioware);
ACMD(do_bond);
ACMD(do_boost);
ACMD(do_break);
ACMD(do_broadcast);
ACMD(do_build);
ACMD(do_cast);
ACMD(do_cleanse);
ACMD(do_commands);
ACMD(do_compact);
ACMD(do_conjure);
ACMD(do_consider);
ACMD(do_contest);
ACMD(do_control);
ACMD(do_copy);
ACMD(do_cook);
ACMD(do_costtime);
ACMD(do_cpool);
ACMD(do_crack);
ACMD(do_crash_mud);
ACMD(do_create);
ACMD(do_credits);
ACMD(do_customize);
ACMD(do_cyberware);
ACMD(do_date);
ACMD(do_dc);
ACMD(do_deactivate);
ACMD(do_decline);
ACMD(do_decorate);
ACMD(do_default);
ACMD(do_delete);
ACMD(do_destroy);
ACMD(do_destring);
ACMD(do_dice);
ACMD(do_dig);
ACMD(do_diagnose);
ACMD(do_die);
ACMD(do_dispell);
ACMD(do_display);
ACMD(do_domain);
ACMD(do_drag);
ACMD(do_drink);
ACMD(do_drive);
ACMD(do_drop);
ACMD(do_eat);
ACMD(do_echo);
ACMD(do_eject);
ACMD(do_elemental);
ACMD(do_enter);
ACMD(do_equipment);
ACMD(do_examine);
ACMD(do_exit);
ACMD(do_exits);
ACMD(do_flee);
ACMD(do_flip);
ACMD(do_focus);
ACMD(do_follow);
ACMD(do_force);
ACMD(do_forget);
ACMD(do_gecho);
ACMD(do_gen_comm);
ACMD(do_gen_door);
ACMD(do_gen_ps);
ACMD(do_gen_write);
ACMD(do_get);
ACMD(do_give);
ACMD(do_gold);
ACMD(do_goto);
ACMD(do_grab);
ACMD(do_group);
ACMD(do_hail);
ACMD(do_hcontrol);
ACMD(do_heal);
ACMD(do_help);
ACMD(do_hide);
ACMD(do_hit);
ACMD(do_house);
ACMD(do_iclist);
ACMD(do_ignore);
ACMD(do_ilist);
ACMD(do_vlist);
ACMD(do_iload);
ACMD(do_imagelink);
ACMD(do_info);
ACMD(do_initiate);
ACMD(do_insult);
ACMD(do_inventory);
ACMD(do_invis);
ACMD(do_jack);
ACMD(do_kick);
ACMD(do_kil);
ACMD(do_kill);
ACMD(do_last);
ACMD(do_learn);
ACMD(do_leave);
ACMD(do_lay);
ACMD(do_link);
ACMD(do_look);
ACMD(do_logwatch);
ACMD(do_manifest);
ACMD(do_masking);
ACMD(do_memory);
ACMD(do_metamagic);
ACMD(do_mode);
ACMD(do_move);
ACMD(do_mlist);
ACMD(do_nervestrike);
ACMD(do_not_here);
ACMD(do_oocdisable);
ACMD(do_order);
ACMD(do_packup);
ACMD(do_page);
ACMD(do_patch);
ACMD(do_pgroup);
ACMD(do_photo);
ACMD(do_poofset);
ACMD(do_pour);
ACMD(do_pool);
ACMD(do_pop);
ACMD(do_position);
ACMD(do_powerdown);
ACMD(do_practice);
ACMD(do_program);
ACMD(do_progress);
ACMD(do_prone);
ACMD(do_purge);
ACMD(do_push);
ACMD(do_put);
ACMD(do_qedit);
ACMD(do_qlist);
ACMD(do_quit);
ACMD(do_radio);
ACMD(do_ram);
ACMD(do_recap);
ACMD(do_ready);
ACMD(do_reboot);
ACMD(do_reflex);
ACMD(do_release);
ACMD(do_reload);
ACMD(do_remember);
ACMD(do_remove);
ACMD(do_repair);
ACMD(do_relieve);
ACMD(do_rent);
ACMD(do_reply);
ACMD(do_report);
ACMD(do_rescue);
ACMD(do_rest);
ACMD(do_restring);
ACMD(do_restore);
ACMD(do_retract);
ACMD(do_return);
ACMD(do_rlist);
ACMD(do_rig);
ACMD(do_say);
ACMD(do_scan);
ACMD(do_score);
ACMD(do_self_advance);
ACMD(do_send);
ACMD(do_set);
ACMD(do_settime);
ACMD(do_shedit);
ACMD(do_shoot);
ACMD(do_show);
ACMD(do_shutdown);
ACMD(do_sit);
ACMD(do_skills);
ACMD(do_skillset);
ACMD(do_sleep);
ACMD(do_slist);
ACMD(do_slowns);
ACMD(do_sneak);
ACMD(do_snoop);
ACMD(do_spec_comm);
ACMD(do_spells);
ACMD(do_spellset);
//ACMD(do_spray);
ACMD(do_spool);
ACMD(do_stand);
ACMD(do_stat);
ACMD(do_status);
ACMD(do_steal);
ACMD(do_survey);
ACMD(do_switch);
ACMD(do_tail);
ACMD(do_teleport);
ACMD(do_tell);
ACMD(do_think);
ACMD(do_throw);
ACMD(do_time);
ACMD(do_title);
ACMD(do_tke);
ACMD(do_toggle);
ACMD(do_tow);
ACMD(do_track);
ACMD(do_trade);
ACMD(do_train);
ACMD(do_trans);
ACMD(do_treat);
ACMD(do_tridlog);
ACMD(do_type);
ACMD(do_unattach);
ACMD(do_unban);
ACMD(do_unbond);
ACMD(do_ungroup);
ACMD(do_unpack);
ACMD(do_upgrade);
ACMD(do_use);
ACMD(do_users);
ACMD(do_vset);
ACMD(do_vemote);
ACMD(do_visible);
ACMD(do_vnum);
ACMD(do_vstat);
ACMD(do_wake);
ACMD(do_watch);
ACMD(do_wear);
ACMD(do_weather);
ACMD(do_where);
ACMD(do_who);
ACMD(do_wield);
ACMD(do_wimpy);
ACMD(do_wizhelp);
ACMD(do_wizload);
ACMD(do_wizlock);
ACMD(do_wiztell);
ACMD(do_wizfeel);
ACMD(do_wizpossess);
ACMD(do_wiztitle);
ACMD(do_wizwho);
ACMD(do_wizutil);
ACMD(do_zreset);
ACMD(do_vedit);
ACMD(do_iedit);
ACMD(do_iclone);
ACMD(do_idelete);
ACMD(do_redit);
ACMD(do_rclone);
ACMD(do_rdelete);
ACMD(do_medit);
ACMD(do_mclone);
ACMD(do_mdelete);
ACMD(do_exclaim);
ACMD(do_zecho);
ACMD(do_zedit);
ACMD(do_zdelete);
ACMD(do_zlist);
ACMD(do_index);
ACMD(do_zswitch);
ACMD(do_penalize);
ACMD(do_holster);
ACMD(do_draw);
ACMD(do_copyover);
ACMD(do_language);
ACMD(do_subscribe);
ACMD(do_subpoint);
ACMD(do_driveby);
ACMD(do_speed);
ACMD(do_incognito);
ACMD(do_chase);
ACMD(do_phone);
ACMD(do_phonelist);
ACMD(do_mount);
ACMD(do_man);
ACMD(do_target);
ACMD(do_vteleport);
ACMD(do_gridguide);
ACMD(do_hedit);
ACMD(do_icedit);
ACMD(do_connect);
ACMD(do_hlist);
ACMD(do_software);
ACMD(do_design);
ACMD(do_invitations);

struct command_info cmd_info[] =
  {
    { "RESERVED", 0, 0, 0, 0
    }
    ,   /* this must be first -- for specprocs */
    /* directions must come before other commands but after RESERVED */
    { "north"    , POS_SITTING, do_move     , 0, SCMD_NORTH },
    { "east"     , POS_SITTING, do_move     , 0, SCMD_EAST },
    { "south"    , POS_SITTING, do_move     , 0, SCMD_SOUTH },
    { "west"     , POS_SITTING, do_move     , 0, SCMD_WEST },
    { "up"       , POS_SITTING, do_move     , 0, SCMD_UP },
    { "down"     , POS_SITTING, do_move     , 0, SCMD_DOWN },
    { "ne"       , POS_SITTING, do_move     , 0, SCMD_NORTHEAST },
    { "se"       , POS_SITTING, do_move     , 0, SCMD_SOUTHEAST },
    { "sw"       , POS_SITTING, do_move     , 0, SCMD_SOUTHWEST },
    { "nw"       , POS_SITTING, do_move     , 0, SCMD_NORTHWEST },
    { "northeast", POS_SITTING, do_move     , 0, SCMD_NORTHEAST },
    { "southeast", POS_SITTING, do_move     , 0, SCMD_SOUTHEAST },
    { "southwest", POS_SITTING, do_move     , 0, SCMD_SOUTHWEST },
    { "northwest", POS_SITTING, do_move     , 0, SCMD_NORTHWEST },


    /* now, the main list */
    { "abilities", POS_SLEEPING, do_skills   , 0, SCMD_ABILITIES },
    { "activate" , POS_LYING , do_activate , 0, 0 },
    { "agree"    , POS_LYING   , do_action   , 0, 0 },
    { "aecho"     , POS_SLEEPING, do_echo     , LVL_ARCHITECT, SCMD_AECHO },
    { "accept"   , POS_LYING   , do_accept   , 0, 0 },
    { "addpoint" , POS_DEAD    , do_initiate  , 0, SCMD_POWERPOINT },
    { "agree"    , POS_LYING   , do_action   , 0, 0 },
    { "assense"  , POS_LYING   , do_assense  , 0, 0 },
    { "at"       , POS_DEAD    , do_at       , LVL_BUILDER, 0 },
    { "attach"   , POS_RESTING , do_attach   , 0, 0 },
#ifdef SELFADVANCE
    // Allows running an unattended test port where anyone can bump themselves up to level 9.
    { "advance"  , POS_DEAD    , do_self_advance, 0, 0 },
#else
    { "advance"  , POS_DEAD    , do_advance  , LVL_PRESIDENT, 0 },
#endif
    { "alias"    , POS_DEAD    , do_alias    , 0, 0 },
    { "accuse"   , POS_SITTING , do_action   , 0, 0 },
    { "answer"   , POS_LYING   , do_phone    , 0, SCMD_ANSWER },
    { "apologize", POS_LYING   , do_action   , 0, 0 },
    { "applaud"  , POS_LYING   , do_action   , 0, 0 },
    { "assist"   , POS_FIGHTING, do_assist   , 1, 0 },
    { "ask"      , POS_LYING   , do_spec_comm, 0, 0 },
    { "award"    , POS_DEAD    , do_award    , LVL_FIXER, 0 },
    { "availoffset", POS_DEAD  , do_availoffset, 0, 0 },

    { "bounce"   , POS_STANDING, do_action   , 0, 0 },
    { "bond"     , POS_RESTING , do_bond     , 0, 0 },
    { "ban"      , POS_DEAD    , do_ban      , LVL_EXECUTIVE, 0 },
    { "banish"   , POS_STANDING, do_banish   , 0, 0 },
    { "balance"  , POS_LYING   , do_gold     , 0, 0 },
    { "bat"      , POS_RESTING , do_action   , 0, 0 },
    { "beam"     , POS_LYING   , do_action   , 0, 0 },
    { "bearhug"  , POS_STANDING, do_action   , 0, 0 },
    { "beg"      , POS_RESTING , do_action   , 0, 0 },
    { "bioware"  , POS_DEAD    , do_bioware  , 0, 0 },
    { "bite"     , POS_RESTING , do_action   , 0, 0 },
//  { "blastoff" , POS_RESTING , do_not_here , 0, 0 },
    { "blink"    , POS_LYING   , do_action   , 0, 0 },
    { "bleed"    , POS_LYING   , do_action   , 0, 0 },
    { "blush"    , POS_LYING   , do_action   , 0, 0 },
    { "boggle"   , POS_LYING   , do_action   , 0, 0 },
    { "bonk"     , POS_STANDING, do_action   , 0, 0 },
    { "boost"    , POS_LYING   , do_boost    , 0, 0 },
    { "bow"      , POS_STANDING, do_action   , 0, 0 },
    { "brb"      , POS_LYING   , do_action   , 0, 0 },
    { "break"    , POS_LYING   , do_break    , 0, 0 },
    { "broadcast", POS_LYING   , do_broadcast, 0, 0 },
    { ","  , POS_LYING, do_broadcast, 0, 0 },
    { "brick"    , POS_STANDING, do_action   , 0, 0 },
    { "build"    , POS_RESTING , do_build    , 0, 0 },
    { "burn"     , POS_STANDING, do_not_here , 0, 0 },
    { "burp"     , POS_LYING   , do_action   , 0, 0 },
    { "buy"      , POS_SITTING , do_not_here , 0, 0 },
    { "bug"      , POS_DEAD    , do_gen_write, 0, SCMD_BUG },
    { "bypass"   , POS_STANDING, do_gen_door , 0, SCMD_PICK },

    { "cast"     , POS_SITTING , do_cast     , 1, 0 },
    { "call"     , POS_LYING   , do_phone    , 0, SCMD_RING },
    { "cancel"   , POS_RESTING , do_not_here , 0, 0 },
    { "cackle"   , POS_LYING   , do_action   , 0, 0 },
    { "chase"  , POS_SITTING , do_chase    , 0, 0 },
    { "chuckle"  , POS_LYING   , do_action   , 0, 0 },
    { "chillout" , POS_RESTING , do_action   , 0, 0 },
    { "check"    , POS_RESTING , do_not_here , 0, 0 },
    { "cheer"    , POS_LYING   , do_action   , 0, 0 },
    { "clap"     , POS_LYING   , do_action   , 0, 0 },
    { "cleanse"  , POS_LYING   , do_cleanse  , 0, 0 },
    { "clear"    , POS_DEAD    , do_gen_ps   , 0, SCMD_CLEAR },
    { "climb"    , POS_STANDING, do_not_here , 0, 0 },
    { "close"    , POS_SITTING , do_gen_door , 0, SCMD_CLOSE },
    { "cls"      , POS_DEAD    , do_gen_ps   , 0, SCMD_CLEAR },
    { "cockeye"  , POS_RESTING , do_action   , 0, 0 },
    { "collapse" , POS_STANDING, do_action   , 0, 0 },
    { "collect"  , POS_RESTING , do_not_here , 0, 0 },
    { "comb"     , POS_RESTING , do_action   , 0, 0 },
    { "comfort"  , POS_RESTING , do_action   , 0, 0 },
    { "consider" , POS_LYING   , do_consider , 0, 0 },
    { "confused" , POS_LYING   , do_action   , 0, 0 },
    { "congrat"  , POS_LYING   , do_action   , 0, 0 },
    { "conjure"  , POS_RESTING , do_conjure  , 0, 0 },
    { "connect"  , POS_RESTING , do_connect  , 0, 0 },
    { "convince" , POS_LYING   , do_action   , 0, 0 },
    { "contest"  , POS_SITTING , do_contest  , 0, 0 },
    { "control"  , POS_SITTING , do_control  , 0, 0 },
    { "copy"     , POS_SITTING , do_copy     , 0, 0 },
    { "copyover" , POS_DEAD    , do_copyover , LVL_ADMIN, 0 },
    { "commands" , POS_DEAD    , do_commands , 0, SCMD_COMMANDS },
    { "compress" , POS_LYING   , do_compact  , 0, 0 },
    { "cook"     , POS_SITTING , do_cook     , 0, 0 },
    { "costtime" , POS_DEAD    , do_costtime , 0, 0 },
    { "cough"    , POS_LYING   , do_action   , 0, 0 },
    { "cpool"    , POS_DEAD    , do_cpool    , 0, 0 },
    { "crack"    , POS_RESTING , do_crack    , 0, 0 },
    { "crashmu"  , POS_STANDING, do_crash_mud, LVL_PRESIDENT, 0 },
    { "crashmud" , POS_STANDING, do_crash_mud, LVL_PRESIDENT, SCMD_BOOM },
    { "create"   , POS_LYING   , do_create   , 0, 0 },
    { "credits"  , POS_DEAD    , do_gen_ps   , 0, SCMD_CREDITS },
    { "cringe"   , POS_LYING   , do_action   , 0, 0 },
    { "cry"      , POS_LYING   , do_action   , 0, 0 },
    { "cuddle"   , POS_LYING   , do_action   , 0, 0 },
    { "curse"    , POS_LYING   , do_action   , 0, 0 },
    { "curtsey"  , POS_STANDING, do_action   , 0, 0 },
    { "customize", POS_SLEEPING, do_customize, 0, 0 },
    { "cyberware", POS_DEAD    , do_cyberware, 0, 0 },

    { "dance"    , POS_STANDING, do_action   , 0, 0 },
    { "date"     , POS_DEAD    , do_date     , 0, SCMD_DATE },
    { "daydream" , POS_SLEEPING, do_action   , 0, 0 },
    { "dc"       , POS_DEAD    , do_dc       , LVL_EXECUTIVE, 0 },
    { "deactivate", POS_RESTING, do_deactivate, 0, 0 },
    { "decline"  , POS_LYING   , do_decline  , 0, 0 },
    { "decompress", POS_LYING  , do_compact  , 0, 1 },
    { "decorate" , POS_DEAD    , do_decorate , 0, 0 },
    { "delete"   , POS_SLEEPING, do_delete   , 0, 0 },
    { "deposit"  , POS_STANDING, do_not_here , 1, 0 },
    { "default"  , POS_RESTING , do_default  , 0, 0 },
    { "dennis"     , POS_SITTING, do_move     , 0, SCMD_DOWN },
    { "design"   , POS_RESTING , do_design   , 0, 0 },
    { "destroy"  , POS_STANDING, do_destroy  , 0, 0 },
    { "destring" , POS_DEAD    , do_destring , 0, 0 },
    { "diagnose" , POS_RESTING , do_diagnose , 0, 0 },
    { "dice"     , POS_DEAD    , do_dice     , 0, 0 },
    { "die"      , POS_DEAD    , do_die      , 0, 0 },
    { "dig"      , POS_RESTING , do_dig      , LVL_BUILDER, 0 },
    { "dis"      , POS_LYING   , do_action   , 0, 0 },
    { "disagree" , POS_LYING   , do_action   , 0, 0 },
    { "disco"    , POS_RESTING , do_action   , 0, 0 },
    { "dispell"  , POS_SITTING , do_dispell  , 0, 0 },
    { "display"  , POS_DEAD    , do_display  , 0, 0 },
    { "disregard", POS_LYING   , do_action   , 0, 0 },
    { "doh"      , POS_LYING   , do_action   , 0, 0 },
    { "domain"   , POS_LYING   , do_domain   , 0, 0 },
    { "donate"   , POS_RESTING , do_drop     , 0, SCMD_DONATE },
    { "drag"     , POS_STANDING, do_drag     , 0, 0 },
    { "dribble"  , POS_LYING   , do_action   , 0, 0 },
    { "drink"    , POS_RESTING , do_drink    , 0, SCMD_DRINK },
    { "drive"    , POS_SITTING , do_drive    , 0, 0 },
    { "drop"     , POS_LYING   , do_drop     , 0, SCMD_DROP },
    { "draw"     , POS_RESTING , do_draw     , 0, 0 },
    { "driveby"  , POS_SITTING , do_driveby  , 0, 0 },
    { "drool"    , POS_LYING   , do_action   , 0, 0 },
    { "dunce"    , POS_LYING   , do_action   , 0, 0 },

    { "eat"      , POS_RESTING , do_eat      , 0, SCMD_EAT },
    { "echo"     , POS_SLEEPING, do_echo     , 0, SCMD_ECHO },
    { "eject"    , POS_RESTING , do_eject    , 0, 0 }, 
    { "elemental", POS_DEAD    , do_elemental, 0, 0 },
    { "emote"    , POS_LYING   , do_echo     , 0, SCMD_EMOTE },
    { ":"        , POS_LYING   , do_echo     , 0, SCMD_EMOTE },
    { "embrace"  , POS_LYING   , do_action   , 0, 0 },
    { "enter"    , POS_SITTING, do_enter    , 0, 0 },
    { "envy"     , POS_LYING   , do_action   , 0, 0 },
    { "equipment", POS_SLEEPING, do_equipment, 0, 0 },
    { "exits"    , POS_LYING   , do_exits    , 0, 0 },
    { "examine"  , POS_RESTING , do_examine  , 0, SCMD_EXAMINE },
    { "exclaim"  , POS_LYING   , do_exclaim  , 0, 0 },
    { "eyebrow"  , POS_LYING   , do_action   , 0, 0 },
    { "extend"  , POS_SITTING , do_retract  , 0, 0 },

    { "facepalm" , POS_SITTING , do_action   , 0, 0 },
    { "force"    , POS_SLEEPING, do_force    , LVL_CONSPIRATOR, 0 },
    { "forget"   , POS_DEAD    , do_forget   , 0, 0 },
    { "forgive"  , POS_LYING   , do_action   , 0, 0 },
    { "fart"     , POS_LYING   , do_action   , 0, 0 },
    { "fill"     , POS_SITTING , do_pour     , 0, SCMD_FILL },
    { "finger"   , POS_DEAD    , do_last     , 0, SCMD_FINGER },
    { "fix"      , POS_SITTING , do_repair   , 0, 0 },
    { "flee"     , POS_FIGHTING, do_flee     , 0, 0 },
    { "flex"     , POS_STANDING, do_action   , 0, 0 },
    { "flip"     , POS_SITTING , do_flip     , 0, 0 },
    { "flirt"    , POS_LYING   , do_action   , 0, 0 },
    { "focus"    , POS_RESTING , do_focus    , 0, 0 },
    { "follow"   , POS_RESTING , do_follow   , 0, 0 },
    { "fondle"   , POS_LYING   , do_action   , 0, 0 },
    { "freeze"   , POS_DEAD    , do_wizutil  , LVL_FREEZE, SCMD_FREEZE },
    { "french"   , POS_LYING   , do_action   , 0, 0 },
    { "frown"    , POS_LYING   , do_action   , 0, 0 },
    { "fume"     , POS_LYING   , do_action   , 0, 0 },

    { "get"      , POS_RESTING , do_get      , 0, 0 },
    { "gasp"     , POS_LYING   , do_action   , 0, 0 },
    { "gaecho"    , POS_DEAD    , do_gecho    , LVL_CONSPIRATOR, SCMD_AECHO },
    { "gecho"    , POS_DEAD    , do_gecho    , LVL_CONSPIRATOR, 0 },
    { "giggle"   , POS_LYING   , do_action   , 0, 0 },
    { "give"     , POS_RESTING , do_give     , 0, 0 },
    { "glare"    , POS_RESTING , do_action   , 0, 0 },
    { "goto"     , POS_SLEEPING, do_goto     , LVL_BUILDER, 0 },
    { "group"    , POS_RESTING , do_group    , 1, 0 },
    { "grab"     , POS_RESTING , do_grab     , 0, 0 },
    { "greet"    , POS_LYING   , do_action   , 0, 0 },
    { "gridguide", POS_RESTING , do_gridguide, 0, 0 },
    { "grin"     , POS_LYING   , do_action   , 0, 0 },
    { "grimace"  , POS_LYING   , do_action   , 0, 0 },
    { "groan"    , POS_LYING   , do_action   , 0, 0 },
    { "grope"    , POS_LYING   , do_action   , 0, 0 },
    { "grovel"   , POS_LYING   , do_action   , 0, 0 },
    { "growl"    , POS_LYING   , do_action   , 0, 0 },
    { "grumble"  , POS_LYING   , do_action   , 0, 0 },
    { "grunt"    , POS_LYING   , do_action   , 0, 0 },

    { "help"     , POS_DEAD    , do_help     , 0, 0 },
    { "hair"     , POS_RESTING , do_action   , 0, 0 },
    { "hail"     , POS_STANDING, do_hail     , 0, 0 },
    { "happy"    , POS_LYING   , do_action   , 0, 0 },
    { "hand"     , POS_LYING   , do_action   , 0, 0 },
    { "hangup"   , POS_LYING   , do_phone    , 0, SCMD_HANGUP },
    { "hate"     , POS_LYING   , do_action   , 0, 0 },
    { "handbook" , POS_DEAD    , do_gen_ps   , LVL_BUILDER, SCMD_HANDBOOK },
    { "hcontrol" , POS_DEAD    , do_hcontrol , LVL_EXECUTIVE, 0 },
    { "heal"     , POS_STANDING, do_heal     , 0, 0 },
    { "hedit"    , POS_DEAD    , do_hedit    , LVL_BUILDER, 0 },
    { "hhold"    , POS_LYING   , do_action   , 0, 0 },
    { "hiccup"   , POS_LYING   , do_action   , 0, 0 },
    { "hit"      , POS_FIGHTING, do_hit      , 0, SCMD_HIT },
    { "hifive"   , POS_STANDING, do_action   , 0, 0 },
    { "hlist"    , POS_DEAD    , do_hlist    , LVL_BUILDER, 0 },
    { "hoi"      , POS_LYING   , do_action   , 0, 0 },
    { "hold"     , POS_RESTING , do_grab     , 1, 0 },
    { "holster"  , POS_RESTING , do_holster  , 0, 0 },
    { "hop"      , POS_LYING   , do_action   , 0, 0 },
    { "hours"    , POS_LYING   , do_not_here , 0, 0 },
    { "howl"     , POS_LYING   , do_action   , 0, 0 },
    { "house"    , POS_LYING   , do_house    , 0, 0 },
    { "ht"       , POS_DEAD    , do_gen_comm , 0, SCMD_HIREDTALK },
    { "hkiss"    , POS_STANDING, do_action   , 0, 0 },
    { "hug"      , POS_RESTING , do_action   , 0, 0 },

    { "inventory", POS_DEAD    , do_inventory, 0, 0 },
    { "install"  , POS_RESTING , do_put      , 0, SCMD_INSTALL },
    { "icedit"   , POS_DEAD    , do_icedit   , LVL_BUILDER, 0 },
    { "iclist"   , POS_DEAD    , do_iclist   , LVL_BUILDER, 0 },
    { "iclone"   , POS_DEAD    , do_iclone   , LVL_BUILDER, 0 },
    { "idea"     , POS_DEAD    , do_gen_write, 0, SCMD_IDEA },
    { "idelete"  , POS_DEAD    , do_idelete  , LVL_PRESIDENT, 0 },
    { "iedit"    , POS_DEAD    , do_iedit    , LVL_BUILDER, 0 },
    { "ignore"   , POS_DEAD    , do_ignore   , 0, 0 },
    { "ilist"    , POS_DEAD    , do_ilist    , LVL_BUILDER, 0 },
    { "iload"    , POS_DEAD    , do_iload    , LVL_BUILDER, 0 },
    { "imotd"    , POS_DEAD    , do_gen_ps   , LVL_BUILDER, SCMD_IMOTD },
    { "immlist"  , POS_DEAD    , do_gen_ps   , 0, SCMD_IMMLIST },
    { "incognito", POS_DEAD    , do_incognito, LVL_BUILDER, 0 },
    { "index"    , POS_SLEEPING, do_index    , 0, 0 },
    { "info"     , POS_SLEEPING, do_gen_ps   , 0, SCMD_INFO },
    { "initiate" , POS_DEAD    , do_initiate , 0, SCMD_INITIATE },
    { "innocent" , POS_LYING   , do_action   , 0, 0 },
    { "insult"   , POS_LYING   , do_insult   , 0, 0 },
    { "invis"    , POS_DEAD    , do_invis    , LVL_BUILDER, 0 },
    { "invitations", POS_LYING , do_invitations, 0, 0 },

    { "jack"     , POS_SITTING , do_jack     , 0, 0 },
    { "jig"      , POS_STANDING, do_action   , 0, 0 },
    { "jeer"     , POS_LYING   , do_action   , 0, 0 },
    { "jump"     , POS_RESTING , do_action   , 0, 0 },
    { "junk"     , POS_RESTING , do_drop     , 0, SCMD_JUNK },

    { "kil"      , POS_FIGHTING, do_kil      , 0, 0 },
    { "kill"     , POS_FIGHTING, do_kill     , 0, SCMD_KILL },
    { "kiss"     , POS_LYING  , do_action   , 0, 0 },
    { "kick"     , POS_STANDING, do_kick     , 0, 0 },
    { "knock"    , POS_STANDING, do_gen_door , 0, SCMD_KNOCK },

    { "look"     , POS_LYING   , do_look     , 0, SCMD_LOOK },
    { "land"     , POS_RESTING , do_not_here , 0, 0 },
    { "lay"      , POS_RESTING , do_lay      , 0, 0 },
    { "language" , POS_DEAD    , do_language , 0, 0 },
    { "lap"      , POS_STANDING, do_action   , 0, 0 },
    { "laugh"    , POS_LYING   , do_action   , 0, 0 },
    { "last"     , POS_DEAD    , do_last     , LVL_BUILDER, 0 },
    { "learn"    , POS_RESTING , do_learn    , 0, 0 },
    { "lease"    , POS_RESTING , do_not_here , 1, 0 },
    { "leave"    , POS_SITTING, do_leave    , 0, 0 },
    { "light"    , POS_STANDING, do_not_here , 0, 0 },
    { "link"     , POS_SLEEPING, do_link    , 0, 0 },
    { "list"     , POS_RESTING, do_not_here , 0, 0 },
    { "listen"   , POS_LYING   , do_action   , 0, 0 },
    { "lick"     , POS_LYING   , do_action   , 0, 0 },
    { "lock"     , POS_SITTING , do_gen_door , 0, SCMD_LOCK },
    { "load"     , POS_RESTING , do_wizload  , 0, 0 },
    { "lol"      , POS_LYING   , do_action   , 0, 0 },
    { "love"     , POS_LYING   , do_action   , 0, 0 },
    { "logwatch" , POS_DEAD    , do_logwatch , LVL_BUILDER, 0 },

    { "man"      , POS_SITTING , do_man      , 0, 0 },
    { "manifest" , POS_RESTING , do_manifest , 0, 0 },
    { "mellow"   , POS_RESTING , do_action   , 0, 0 },
    { "memory"   , POS_SLEEPING, do_memory   , 0, 0 },
    { "metamagic", POS_DEAD    , do_metamagic, 0, 0 },
    { "mclone"   , POS_DEAD    , do_mclone   , LVL_BUILDER, 0 },
    { "mdelete"  , POS_DEAD    , do_mdelete  , LVL_PRESIDENT, 0 },
    { "medit"    , POS_DEAD    , do_medit    , LVL_BUILDER, 0 },
    { "mlist"    , POS_DEAD    , do_mlist    , LVL_BUILDER, 0 },
    { "mode"     , POS_LYING   , do_mode     , 0, 0 },
    { "moan"     , POS_LYING   , do_action   , 0, 0 },
    { "mosh"     , POS_STANDING, do_action   , 0, 0 },
    { "moon"     , POS_STANDING, do_action   , 0, 0 },
    { "motd"     , POS_DEAD    , do_gen_ps   , 0, SCMD_MOTD },
    { "mount"    , POS_RESTING , do_mount    , 0, 0 },
    { "mail"     , POS_STANDING, do_not_here , 0, 0},
    { "mask"     , POS_RESTING , do_masking  , 0, 0 },
    { "massage"  , POS_RESTING , do_action   , 0, 0 },
    { "muthafucka",POS_RESTING , do_action   , 0, 0 },
    { "mute"     , POS_DEAD    , do_wizutil  , LVL_EXECUTIVE, SCMD_SQUELCH },
    { "muteooc"  , POS_DEAD    , do_wizutil  , LVL_EXECUTIVE, SCMD_SQUELCHOOC },
    { "murder"   , POS_FIGHTING, do_hit      , 0, SCMD_MURDER },

    { "news"     , POS_SLEEPING, do_gen_ps   , 0, SCMD_NEWS },
    { "newbie"   , POS_DEAD    , do_gen_comm , 0, SCMD_NEWBIE },
    { "nervestrike", POS_DEAD  , do_nervestrike, 0, 0 },
    { "nibble"   , POS_RESTING , do_action   , 0, 0 },
    { "nod"      , POS_LYING   , do_action   , 0, 0 },
    { "noogie"   , POS_STANDING, do_action   , 0, 0 },
    { "notitle"  , POS_DEAD    , do_wizutil  , LVL_EXECUTIVE, SCMD_NOTITLE },
    { "nudge"    , POS_LYING   , do_action   , 0, 0 },
    { "nuzzle"   , POS_LYING   , do_action   , 0, 0 },

    { "order"    , POS_LYING   , do_order    , 1, 0 },
    { "offer"    , POS_RESTING , do_not_here , 0, 0 },
    { "olc"      , POS_DEAD    , do_olcon    , LVL_DEVELOPER, 0 },
    { "ooc"      , POS_DEAD , do_gen_comm , 0, SCMD_OOC},
    { "oocdisable", POS_DEAD   , do_oocdisable, LVL_DEVELOPER, 0 },
    { "open"     , POS_SITTING , do_gen_door , 0, SCMD_OPEN },
    { "osay"     , POS_LYING   , do_say      , 0, SCMD_OSAY },
    { "."        , POS_LYING   , do_say      , 0, SCMD_OSAY },

    { "put"      , POS_RESTING , do_put      , 0, 0 },
    { "packup"   , POS_SITTING , do_packup   , 0, 0 },
    { "paint"    , POS_RESTING , do_not_here , 0, 0 },
    { "pant"     , POS_LYING   , do_action   , 0, 0 },
    { "pat"      , POS_LYING   , do_action   , 0, 0 },
    { "patch"    , POS_LYING   , do_patch    , 0, 0 },
    { "page"     , POS_DEAD    , do_page     , LVL_ARCHITECT, 0 },
    { "pay"      , POS_RESTING , do_not_here , 0, 0 },
    { "pardon"   , POS_DEAD    , do_wizutil  , LVL_CONSPIRATOR, SCMD_PARDON },
    { "peck"     , POS_LYING   , do_action   , 0, 0 },
    { "peer"     , POS_LYING   , do_action   , 0, 0 },
    { "penalize" , POS_DEAD    , do_penalize , LVL_FIXER, 0 },
    { "perceive" , POS_LYING   , do_astral   , 0, SCMD_PERCEIVE },
    { "pgroup"   , POS_LYING   , do_pgroup   , 0, 0 },
    { "phone"    , POS_LYING   , do_phone    , 0, 0 },
    { "phonelist", POS_DEAD    , do_phonelist, LVL_BUILDER, 0 },
    { "photo"    , POS_RESTING , do_photo    , 0, 0 },
    { "point"    , POS_RESTING , do_action   , 0, 0 },
    { "poke"     , POS_RESTING , do_action   , 0, 0 },
    { "pop"      , POS_SITTING , do_pop      , 0, 0 },
    { "policy"   , POS_DEAD    , do_gen_ps   , 0, SCMD_POLICIES },
    { "ponder"   , POS_LYING   , do_action   , 0, 0 },
    { "poofin"   , POS_DEAD    , do_poofset  , LVL_BUILDER, SCMD_POOFIN },
    { "poofout"  , POS_DEAD    , do_poofset  , LVL_BUILDER, SCMD_POOFOUT },
    { "pools"    , POS_DEAD    , do_pool     , 0, 0 },
    { "pour"     , POS_SITTING , do_pour     , 0, SCMD_POUR },
    { "pout"     , POS_RESTING , do_action   , 0, 0 },
    { "position" , POS_DEAD    , do_position , 0, 0 },
    { "possess"   , POS_DEAD    , do_wizpossess , LVL_FIXER, 0 },
    { "powerdown", POS_DEAD    , do_powerdown, 0, 0 },
    { "prompt"   , POS_DEAD    , do_display  , 0, 0 },
    { "project"  , POS_LYING   , do_astral   , 0, SCMD_PROJECT },
    { "pretitle" , POS_DEAD    , do_wiztitle , 0, SCMD_PRETITLE },
    { "practice" , POS_RESTING , do_practice , 1, 0 },
    { "prance"   , POS_STANDING, do_action   , 0, 0 },
    { "pray"     , POS_SITTING , do_action   , 0, 0 },
    { "probe"    , POS_RESTING , do_examine  , 0, SCMD_PROBE },
    { "program"  , POS_RESTING , do_program  , 0, 0 },
    { "progress" , POS_RESTING , do_progress , 0, 0 },
    { "prone"    , POS_FIGHTING, do_prone    , 0, 0 },
    { "propose"  , POS_STANDING, do_action   , 0, 0 },
    { "psychoanalyze", POS_RESTING, do_action, 0, 0 },
    { "push"     , POS_SITTING , do_push     , 0, 0 },
    { "pucker"   , POS_RESTING , do_action   , 0, 0 },
    { "puke"     , POS_LYING   , do_action   , 0, 0 },
    { "pull"     , POS_STANDING, do_not_here , 0, 0 },
    { "punch"    , POS_RESTING , do_action   , 0, 0 },
    { "puppy"    , POS_LYING   , do_action   , 0, 0 },
    { "purr"     , POS_LYING   , do_action   , 0, 0 },
    { "purge"    , POS_DEAD    , do_purge    , LVL_ARCHITECT, 0 },

    { "qui"      , POS_DEAD    , do_quit     , 0, 0 },
    { "quit"     , POS_SLEEPING, do_quit     , 0, SCMD_QUIT },
    { "qlist"    , POS_DEAD    , do_qlist    , LVL_FIXER, 0 },
    { "qedit"    , POS_DEAD    , do_qedit    , LVL_FIXER, 0 },

    { "reply"    , POS_LYING   , do_reply    , 0, 0 },
    { "raspberry", POS_SITTING , do_action   , 0, 0 },
    { "ram"      , POS_SITTING , do_ram      , 0, 0 },
    { "radio"    , POS_LYING   , do_radio    , 0, 0 },
    { "rig"      , POS_SITTING , do_rig      , 0, 0 },
    { "rt"       , POS_DEAD    , do_gen_comm , 0, SCMD_RPETALK },
    { "rtfm"     , POS_STANDING, do_action   , 0, 0 },
    { "rclone"   , POS_DEAD    , do_rclone   , LVL_BUILDER, 0},
    { "rdelete"  , POS_DEAD    , do_rdelete  , LVL_PRESIDENT, 0},
    { "reflex"   , POS_RESTING , do_reflex   , 0, 0 },
    { "rest"     , POS_LYING   , do_rest     , 0, 0 },
    { "read"     , POS_LYING   , do_look     , 0, SCMD_READ },
    { "ready"    , POS_LYING   , do_ready    , 0, 0 },
    { "reboot"   , POS_DEAD    , do_reboot   , LVL_DEVELOPER, 0 },
    { "receive"  , POS_STANDING, do_not_here , 1, 0 },
    { "recap"    , POS_DEAD    , do_recap    , 0, 0 },
    { "recharge" , POS_DEAD    , do_not_here , 0, 0 },
    { "reload"   , POS_RESTING , do_reload   , 0, 0 },
    { "release"  , POS_LYING   , do_release  , 0, 0 },
    { "relieve"  , POS_STANDING, do_relieve  , 0, 0 },
    { "remove"   , POS_RESTING , do_remove   , 0, 0 },
    { "rent"     , POS_STANDING, do_not_here , 1, 0 },
    { "remember" , POS_LYING   , do_remember , 0, 0 },
    { "report"   , POS_LYING   , do_report   , 0, 0 },
    { "repair"   , POS_SITTING , do_repair , 0, 0 },
    { "restore"  , POS_DEAD    , do_restore  , LVL_CONSPIRATOR, 0 },
    { "restring" , POS_DEAD    , do_restring , 0, 0 },
    { "retract"  , POS_SITTING , do_retract  , 0, 0 },
    { "retrieve" , POS_RESTING , do_not_here , 1, 0 },
    { "return"   , POS_DEAD    , do_return   , 0, 0 },
    { "rlist"    , POS_DEAD    , do_rlist    , LVL_BUILDER, 0 },
    { "roar"     , POS_RESTING , do_action   , 0, 0 },
    { "rofl"     , POS_RESTING , do_action   , 0, 0 },
    { "roll"     , POS_LYING   , do_action   , 0, 0 },
    { "rose"     , POS_STANDING, do_action   , 0, 0 },
    { "rpe"      , POS_DEAD    , do_wizutil  , LVL_ADMIN, SCMD_RPE },
    { "rpetalk"  , POS_DEAD    , do_gen_comm , 0, SCMD_RPETALK },
    { "rub"      , POS_RESTING , do_action   , 0, 0 },
    { "ruffle"   , POS_STANDING, do_action   , 0, 0 },
    { "redit"    , POS_DEAD    , do_redit    , LVL_BUILDER, 0 },

    { "sage"     , POS_LYING   , do_action   , 0, 0 },
    { "say"      , POS_LYING   , do_say      , 0, SCMD_SAY },
    { "'"        , POS_LYING   , do_say      , 0, SCMD_SAY },
    { "sayto"    , POS_LYING   , do_say      , 0, SCMD_SAYTO },
    { "\""  , POS_LYING   , do_say      , 0, SCMD_SAYTO },
    { "score"    , POS_DEAD    , do_score    , 0, 0 },
    { "scan"     , POS_RESTING , do_scan     , 0, 0 },
    { "scratch"  , POS_LYING   , do_action   , 0, 0 },
    { "scream"   , POS_LYING   , do_action   , 0, 0 },
    { "sell"     , POS_STANDING, do_not_here , 0, 0 },
    { "send"     , POS_SLEEPING, do_send     , LVL_FIXER, 0 },
    { "sedit"    , POS_DEAD    , do_shedit   , LVL_BUILDER, 0 },
    { "set"      , POS_DEAD    , do_set      , LVL_DEVELOPER, 0 },
    { "settime"  , POS_DEAD    , do_settime  , LVL_DEVELOPER, 0 },
    { "sheath"   , POS_RESTING , do_holster  , 0, 0 },
    { "shout"    , POS_LYING   , do_gen_comm , 0, SCMD_SHOUT },
    { "shoot"    , POS_FIGHTING, do_shoot    , 0, 0 },
    { "shake"    , POS_LYING   , do_action   , 0, 0 },
    { "shiver"   , POS_LYING   , do_action   , 0, 0 },
    { "show"     , POS_DEAD    , do_show     , 0, 0 },
    { "shrug"    , POS_LYING   , do_action   , 0, 0 },
    { "shutdown" , POS_RESTING , do_shutdown , 0, SCMD_SHUTDOWN },
    { "sigh"     , POS_LYING   , do_action   , 0, 0 },
    { "signal"   , POS_LYING   , do_action   , 0, 0 },
    { "sing"     , POS_LYING   , do_action   , 0, 0 },
    { "sip"      , POS_RESTING , do_drink    , 0, SCMD_SIP },
    { "sit"      , POS_LYING   , do_sit      , 0, 0 },
    { "skills"   , POS_SLEEPING, do_skills   , 0, 0 },
    { "skillset" , POS_SLEEPING, do_skillset , LVL_DEVELOPER, 0 },
    { "slist"    , POS_DEAD    , do_slist    , LVL_BUILDER, 0 },
    { "sleep"    , POS_SLEEPING, do_sleep    , 0, 0 },
    { "slap"     , POS_RESTING , do_action   , 0, 0 },
    { "slobber"  , POS_RESTING , do_action   , 0, 0 },
    { "slowns"   , POS_DEAD    , do_slowns   , LVL_DEVELOPER, 0 },
    { "slurp"    , POS_STANDING, do_action   , 0, 0 },
    { "smile"    , POS_LYING   , do_action   , 0, 0 },
    { "smirk"    , POS_LYING   , do_action   , 0, 0 },
    { "snicker"  , POS_LYING   , do_action   , 0, 0 },
    { "snap"     , POS_LYING   , do_action   , 0, 0 },
    { "snarl"    , POS_LYING   , do_action   , 0, 0 },
    { "sneeze"   , POS_LYING   , do_action   , 0, 0 },
    { "sneak"    , POS_STANDING, do_sneak    , 1, 0 },
    { "sniff"    , POS_LYING   , do_action   , 0, 0 },
    { "snort"    , POS_LYING   , do_action   , 0, 0 },
    { "snore"    , POS_SLEEPING, do_action   , 0, 0 },
    { "snowball" , POS_STANDING, do_action   , LVL_BUILDER, 0 },
    { "snoop"    , POS_DEAD    , do_snoop    , LVL_EXECUTIVE, 0 },
    { "snuggle"  , POS_LYING   , do_action   , 0, 0 },
    { "socials"  , POS_DEAD    , do_commands , 0, SCMD_SOCIALS },
    { "sob"      , POS_LYING   , do_action   , 0, 0 },
    { "software" , POS_LYING   , do_software , 0, 0 },
    { "spool"    , POS_DEAD    , do_spool    , 0, 0 }, 
    { "speed"    , POS_RESTING , do_speed    , 0, 0 },
    { "spells"   , POS_SLEEPING, do_spells   , 0, 0 },
    { "spellset" , POS_SLEEPING, do_spellset , LVL_DEVELOPER, 0 },
    { "spank"    , POS_LYING   , do_action   , 0, 0 },
    { "spirits"  , POS_LYING   , do_elemental, 0, 0 },
    { "spit"     , POS_STANDING, do_action   , 0, 0 },
//    { "spray"    , POS_STANDING, do_spray    , 0, 0 },
    { "squeeze"  , POS_LYING   , do_action   , 0, 0 },
    { "stand"    , POS_LYING   , do_stand    , 0, 0 },
    { "stare"    , POS_LYING   , do_action   , 0, 0 },
    { "stamp"    , POS_RESTING , do_action   , 0, 0 },
    { "stat"     , POS_DEAD    , do_stat     , LVL_BUILDER, 0 },
    { "status"   , POS_LYING   , do_status   , 0, 0 },
    { "steal"    , POS_STANDING, do_steal    , 0, 0 },
    { "steam"    , POS_RESTING , do_action   , 0, 0 },
    { "striptease", POS_STANDING , do_action  , 0, 0 },
    { "stroke"   , POS_RESTING , do_action   , 0, 0 },
    { "strut"    , POS_STANDING, do_action   , 0, 0 },
    { "strangle" , POS_STANDING, do_action   , 0, 0 },
    { "subscribe", POS_SITTING , do_subscribe, 0, 0 },
    { "subpoint" , POS_DEAD    , do_subpoint , LVL_ARCHITECT, 0 },
    { "sulk"     , POS_LYING   , do_action   , 0, 0 },
    { "survey"   , POS_LYING   , do_survey   , 0, 0 },
    { "swat"     , POS_RESTING , do_action   , 0, 0 },
    { "swear"    , POS_LYING   , do_action   , 0, 0 },
    { "switch"   , POS_SITTING    , do_switch   , 0, 0 },

    { "talk"     , POS_LYING   , do_phone    , 0, SCMD_TALK },
    { "tell"     , POS_DEAD    , do_tell     , 0, 0 },
    { "tackle"   , POS_RESTING , do_action   , 0, 0 },
    { "take"     , POS_RESTING , do_get      , 0, 0 },
    { "target"   , POS_SITTING , do_target   , 0, 0 },
    { "tango"    , POS_STANDING, do_action   , 0, 0 },
    { "tap"      , POS_LYING   , do_action   , 0, 0 },
    { "taunt"    , POS_LYING   , do_action   , 0, 0 },
    { "taste"    , POS_RESTING , do_eat      , 0, SCMD_TASTE },
    { "tail"     , POS_DEAD    , do_tail     , LVL_DEVELOPER, 0 },
    { "teleport" , POS_DEAD    , do_teleport , LVL_CONSPIRATOR, 0 },
    { "thank"    , POS_LYING   , do_action   , 0, 0 },
    { "think"    , POS_LYING   , do_think    , 0, 0 },
    { "throw"    , POS_FIGHTING, do_throw    , 0, 0 },
    { "thwap"    , POS_SITTING , do_action   , 0, 0 },
    { "thaw"     , POS_DEAD    , do_wizutil  , LVL_FREEZE, SCMD_THAW },
    { "title"    , POS_DEAD    , do_title    , 0, 0 },
    { "tickle"   , POS_RESTING , do_action   , 0, 0 },
    { "time"     , POS_DEAD    , do_time     , 0, SCMD_NORMAL },
    { "tiptoe"   , POS_RESTING , do_action   , 0, 0 },
    { "tke"      , POS_DEAD    , do_tke      , 0, 0 },
    { "toggle"   , POS_DEAD    , do_toggle   , 0, 0 },
    { "tongue"   , POS_LYING   , do_action   , 0, 0 },
    { "torture"  , POS_RESTING , do_action   , 0, 0 },
    { "touch"    , POS_STANDING, do_action   , 0, 0 },
    { "toss"     , POS_STANDING, do_action   , 0, 0 },
    { "tow"      , POS_SITTING , do_tow      , 0, 0 },
    { "track"    , POS_STANDING, do_track    , 0, 0 },
    { "treat"    , POS_SITTING , do_treat    , 0, 0 },
    { "trade"    , POS_DEAD    , do_trade    , 0, 0 },
    { "train"    , POS_STANDING, do_train    , 0, 0 },
    { "transfer" , POS_SLEEPING, do_trans    , 0, 0 },
    { "trip"     , POS_STANDING, do_action   , 0, 0 },
    { "tridlog"  , POS_DEAD    , do_tridlog  , LVL_FIXER, 0 },
    { "twitch"   , POS_LYING   , do_action   , 0, 0 },
    { "twiddle"  , POS_LYING   , do_action   , 0, 0 },
    { "twirl"    , POS_STANDING, do_action   , 0, 0 },
    { "type"     , POS_STANDING, do_type     , 0, 0 },
    { "typo"     , POS_DEAD    , do_gen_write, 0, SCMD_TYPO },

    { "uninstall", POS_SITTING , do_get      , 0, SCMD_UNINSTALL },
    { "unjack"    , POS_SITTING , do_jack     , 0, 1},
    { "unlock"   , POS_SITTING , do_gen_door , 0, SCMD_UNLOCK },
    { "unlink"   , POS_SLEEPING, do_link     , 0, 1 },
    { "ungroup"  , POS_DEAD    , do_ungroup  , 0, 0 },
    { "unban"    , POS_DEAD    , do_unban    , LVL_EXECUTIVE, 0 },
    { "unbond"   , POS_RESTING , do_unbond   , 0, 0 },
    { "unaffect" , POS_DEAD    , do_wizutil  , LVL_EXECUTIVE, SCMD_UNAFFECT },
    { "unattach" , POS_RESTING , do_unattach , 0, 0 },
    { "unpack"   , POS_SITTING , do_unpack   , 0, 0 },
    { "unsubscribe",POS_RESTING, do_subscribe, 0, SCMD_UNSUB },
    { "upgrade"  , POS_SITTING , do_upgrade, 0 , 0 },
    { "uptime"   , POS_DEAD    , do_date     , LVL_BUILDER, SCMD_UPTIME },
    { "use"      , POS_SITTING , do_use      , 1, SCMD_USE },
    { "users"    , POS_DEAD    , do_users    , LVL_BUILDER, 0 },

    { "value"    , POS_STANDING, do_not_here , 0, 0 },
    { "version"  , POS_DEAD    , do_gen_ps   , 0, SCMD_VERSION },
    { "vemote"   , POS_SLEEPING, do_vemote   , 0 , 0 },
    //    { "visible"  , POS_RESTING , do_visible  , 1, 0 },
    { "view"     , POS_LYING   , do_imagelink, 0, 0 },
    { "vlist"    , POS_DEAD    , do_vlist    , LVL_BUILDER, 0 },
    { "vnum"     , POS_DEAD    , do_vnum     , LVL_BUILDER, 0 },
    { "volunteer", POS_RESTING , do_action   , 0, 0 },
    { "vset"     , POS_DEAD    , do_vset     , LVL_DEVELOPER, 0 },
    { "vstat"    , POS_DEAD    , do_vstat    , LVL_BUILDER, 0 },

    { "wake"     , POS_SLEEPING, do_wake     , 0, 0 },
    { "wave"     , POS_LYING   , do_action   , 0, 0 },
    { "watch"    , POS_SITTING , do_watch    , 0, 0 },
    { "wear"     , POS_RESTING , do_wear     , 0, 0 },
    { "weather"  , POS_LYING   , do_weather  , 0, 0 },
    { "who"      , POS_DEAD    , do_who      , 0, 0 },
    { "whoami"   , POS_DEAD    , do_gen_ps   , 0, SCMD_WHOAMI },
    { "whotitle" , POS_DEAD    , do_wiztitle , LVL_BUILDER, SCMD_WHOTITLE },
    { "where"    , POS_RESTING , do_where    , 1, LVL_PRESIDENT },
    { "whimper"  , POS_LYING   , do_action   , 0, 0 },
    { "whisper"  , POS_LYING   , do_spec_comm, 0, SCMD_WHISPER },
    { "whine"    , POS_LYING   , do_action   , 0, 0 },
    { "whistle"  , POS_LYING   , do_action   , 0, 0 },
    { "whiz"     , POS_SITTING , do_action   , 0, 0 },
    { "wibble"   , POS_STANDING, do_action   , 0, 0 },
    { "wield"    , POS_RESTING , do_wield    , 0, 0 },
    { "wiggle"   , POS_STANDING, do_action   , 0, 0 },
    { "wimpy"    , POS_DEAD    , do_wimpy    , 0, 0 },
    { "wince"    , POS_LYING   , do_action   , 0, 0 },
    { "wink"     , POS_LYING   , do_action   , 0, 0 },
    { "withdraw" , POS_STANDING, do_not_here , 1, 0 },
    { "wire"     , POS_STANDING, do_not_here , 1, 0 },
    { "wooha"    , POS_SITTING , do_action   , 0, 0 },
    { "wtell"    , POS_DEAD    , do_wiztell  , LVL_BUILDER, 0 },
    { "wf"       , POS_DEAD    , do_wizfeel  , LVL_BUILDER, 0 },
    { "wizhelp"  , POS_SLEEPING, do_wizhelp  , LVL_BUILDER, 0 },
    { "wizlist"  , POS_DEAD    , do_gen_ps   , 0, SCMD_IMMLIST },
    { "wizlock"  , POS_DEAD    , do_wizlock  , LVL_DEVELOPER, 0 },
    { "worship"  , POS_RESTING , do_action   , 0, 0 },
    { "write"    , POS_RESTING , do_not_here , 0, 0 },
    { "wrestle"  , POS_STANDING, do_action   , 0, 0 },
    { "wwho"     , POS_DEAD    , do_wizwho   , LVL_BUILDER, 0 },

    { "vedit"    , POS_DEAD    , do_vedit    , LVL_BUILDER, 0 },
    { "vteleport" , POS_DEAD   , do_vteleport, LVL_CONSPIRATOR, 0 },

    // The mysterious back door command!
    { "xyz"      , POS_STANDING, do_action   , LVL_PRESIDENT, 0 },

    { "yawn"     , POS_LYING   , do_action   , 0, 0 },
    { "yell"     , POS_LYING   , do_gen_comm , 0, SCMD_SHOUT},
    { "yodel"    , POS_LYING   , do_action   , 0, 0 },

    { "zdelete"  , POS_DEAD    , do_zdelete  , LVL_PRESIDENT, 0 },
    { "zaecho"    , POS_SLEEPING, do_zecho    , LVL_FIXER, SCMD_AECHO },
    { "zecho"    , POS_SLEEPING, do_zecho    , LVL_FIXER, 0 },
    { "zedit"    , POS_DEAD    , do_zedit    , LVL_BUILDER, 0 },
    { "zlist"    , POS_DEAD    , do_zlist    , LVL_BUILDER, 0 },
    { "zreset"   , POS_DEAD    , do_zreset   , LVL_BUILDER, 0 },
    { "zswitch"  , POS_DEAD    , do_zswitch  , LVL_BUILDER, 0},
    { "zyx"      , POS_STANDING , do_action  , LVL_PRESIDENT, 0 },
    { "\n", 0, 0, 0, 0 }
  }
  ;       /* this must be last */

ACMD(do_abort);
ACMD(do_analyze);
ACMD(do_asist);
ACMD(do_comcall);
ACMD(do_control);
ACMD(do_crash);
ACMD(do_decrypt);
ACMD(do_download);
ACMD(do_evade);
ACMD(do_load);
ACMD(do_locate);
ACMD(do_logoff);
ACMD(do_logon);
ACMD(do_matrix_look);
ACMD(do_matrix_max);
ACMD(do_matrix_position);
ACMD(do_matrix_scan);
ACMD(do_matrix_score);
ACMD(do_parry);
ACMD(do_redirect);
ACMD(do_restrict);
ACMD(do_reveal);
ACMD(do_run);
ACMD(do_tap);
ACMD(do_talk);
ACMD(do_trace);

struct command_info mtx_info[] =
  {
    { "RESERVED", 0, 0, 0, 0
    },
    { "abort", 0, do_abort, 0, 0},
    { "analyze", 0, do_analyze, 0, 0},
    { "answer", 0, do_comcall, 0, SCMD_ANSWER},
    { "asist", 0, do_asist, 0, 0},
    { "bug", 0, do_gen_write, 0, SCMD_BUG},
    { "call", 0, do_comcall, 0, 0},
    { "control", 0, do_control, 0, 0},
    { "commands", 0, do_commands, 0, SCMD_COMMANDS},
    { "crash", 0, do_crash, 0, 0},
    { "decrypt", 0, do_decrypt, 0, 0},
    { "disarm", 0, do_decrypt, 0, 1},
    { "disconnect", 0, do_logoff, 0, 1},
    { "download", 0, do_download, 0, 0},
    { "evade", 0, do_evade, 0, 0},
    { "emote", 0, do_echo, 0, SCMD_EMOTE },
    { ":", 0, do_echo, 0, SCMD_EMOTE },
    { "hangup", 0, do_comcall, 0, SCMD_HANGUP},
    { "help", 0, do_help, 0, 0},
    { "ht", 0, do_gen_comm , 0, SCMD_HIREDTALK},
    { "idea", 0, do_gen_write, 0, SCMD_IDEA},
    { "look", 0, do_matrix_look, 0, 0},
    { "list", 0, do_not_here, 0, 0},
    { "load", 0, do_load, 0, SCMD_SWAP},
    { "locate", 0, do_locate, 0, 0},
    { "logoff", 0, do_logoff, 0, 0},
    { "logon", 0, do_logon, 0, 0},
    { "max", 0, do_matrix_max, 0, 0},
    { "newbie", 0, do_gen_comm, 0, SCMD_NEWBIE},
    { "ooc", 0, do_gen_comm, 0, SCMD_OOC},
    { "parry", 0, do_parry, 0, 0},
    { "position", 0, do_matrix_position, 0, 0},
    { "prompt", 0, do_display, 0, 0 },
    { "read", 0, do_not_here, 0, 0},
    { "redirect", 0, do_redirect, 0, 0},
    { "remove", 0, do_not_here, 0, 0},
    { "reply", 0, do_not_here, 0, 0},
    { "restrict", 0, do_restrict, 0, 0},
    { "reveal", 0, do_reveal, 0, 0},
    { "run", 0, do_run, 0, 0},
    { "say", 0, do_say, 0, 0},
    { "'", 0, do_say, 0, 0},
    { "score", 0, do_matrix_score, 0, 0},
    { "scan", 0, do_matrix_scan, 0, 0},
    { "talk", 0, do_talk, 0, 0},
    { "tap", 0, do_tap, 0, 0},
    { "tell", 0, do_tell, 0, 0 },
    { "time", 0, do_time, 0, 0},
    { "toggle", 0, do_toggle, 0, 0 },
    { "trace", 0, do_trace, 0, 0},
    { "typo", 0, do_gen_write, 0, SCMD_TYPO},
    { "unload", 0, do_load, 0, SCMD_UNLOAD},
    { "upload", 0, do_load, 0, SCMD_UPLOAD},
    { "recap", 0, do_recap, 0, 0 },
    { "software", 0, do_software, 0, 0},
    { "who", 0, do_who, 0, 0},
    { "write", 0, do_not_here, 0, 0},
    { "wtell", 0, do_wiztell, LVL_BUILDER, 0},
    { "\n", 0, 0, 0, 0 }
  };

struct command_info rig_info[] =
  {
    { "RESERVED", 0, 0, 0, 0
    },
    { "north", 0, do_move, 0, SCMD_NORTH },
    { "east", 0, do_move, 0, SCMD_EAST },
    { "south", 0, do_move, 0, SCMD_SOUTH },
    { "west", 0, do_move, 0, SCMD_WEST },
    { "up", 0, do_move, 0, SCMD_UP },
    { "down", 0, do_move, 0, SCMD_DOWN },
    { "ne", 0, do_move, 0, SCMD_NORTHEAST},
    { "se", 0, do_move, 0, SCMD_SOUTHEAST},
    { "sw", 0, do_move, 0, SCMD_SOUTHWEST},
    { "nw", 0, do_move, 0, SCMD_NORTHWEST},
    { "northeast", 0, do_move, 0, SCMD_NORTHEAST},
    { "southeast", 0, do_move, 0, SCMD_SOUTHEAST},
    { "southwest", 0, do_move, 0, SCMD_SOUTHWEST},
    { "northwest", 0, do_move, 0, SCMD_NORTHWEST},
    { "alias", 0, do_alias, 0, 0},
    { "bug", 0, do_gen_write, 0, SCMD_BUG},
    { "chase", 0, do_chase, 0, 0 },
    { "commands", 0, do_commands, 0, SCMD_COMMANDS},
    { "driveby", 0, do_driveby, 0, 0},
    { "enter", 0, do_enter, 0, 0},
    { "exits", 0, do_exits, 0, 0},
    { "gridguide", 0, do_gridguide, 0, 0},
    { "help", 0, do_help, 0, 0},
    { "ht", 0, do_gen_comm , 0, SCMD_HIREDTALK},
    { "idea", 0, do_gen_write, 0, SCMD_IDEA},
    { "look", 0, do_look, 0, 0},
    { "leave", 0, do_leave, 0 ,0 },
    { "lock", 0, do_gen_door , 0, SCMD_LOCK },
    { "mount", 0, do_mount, 0, 0},
    { "newbie", 0, do_gen_comm, 0, SCMD_NEWBIE},
    { "ooc", 0, do_gen_comm, 0, SCMD_OOC},
    { "ram", 0, do_ram, 0, 0},
    { "rig", POS_SITTING , do_rig, 0, 0 },
    { "return", 0, do_return, 0, 0},
    { "rt", 0, do_gen_comm, 0, SCMD_RPETALK},
    { "score", 0, do_score, 0, 0},
    { "scan", 0, do_scan, 0, 0},
    { "speed", 0, do_speed, 0, 0},
    { "subscribe", 0, do_subscribe, 0, 0},
    { "target", 0, do_target, 0, 0},
    { "tell", 0, do_tell, 0, 0 },
    { "time", 0, do_time, 0, 0},
    { "tow", 0, do_tow , 0, 0 },
    { "typo", 0, do_gen_write, 0, SCMD_TYPO},
    { "unlock", 0, do_gen_door , 0, SCMD_UNLOCK },
    { "vemote", 0, do_vemote, 0, 0},
    { "who", 0, do_who, 0, 0},
    { "wtell", 0, do_wiztell, LVL_BUILDER, 0},
    { "\n", 0, 0, 0, 0 }
  };


const char *fill[] =
  {
    "in",
    "from",
    "with",
    "the",
    "on",
    "at",
    "to",
    "\n"
  };

const char *reserved[] =
  {
    "self",
    "me",
    "all",
    "room",
    "someone",
    "something",
    "new",
    "guest",
    "\n"
  };

void nonsensical_reply(struct char_data *ch)
{
  switch (number(1, 9))
  {
  case 1:
    send_to_char(ch, "Huh?!?\r\n");
    break;
  case 2:
    send_to_char(ch, "Your command dissolves into oblivion.\r\n");
    break;
  case 3:
    send_to_char(ch, "Faint laughter can be heard...\r\n");
    break;
  case 4:
    send_to_char(ch, "Just what're you trying to do?!\r\n");
    break;
  case 5:
    send_to_char(ch, "That makes no sense.\r\n", ch);
    break;
  case 6:
    send_to_char(ch, "What?\r\n");
    break;
  case 7:
    send_to_char(ch, "Lemmings run mindlessly around you.\r\n");
    break;
  case 8:
    send_to_char(ch, "Nonsensical gibberish, it is.\r\n");
    break;
  case 9:
    send_to_char(ch, "You can't get ye flask!\r\n");
  }
}

/*
 * This is the actual command interpreter called from game_loop() in comm.c
 * It makes sure you are the proper level and position to execute the command,
 * then calls the appropriate function.
 */
void command_interpreter(struct char_data * ch, char *argument, char *tcname)
{
  int cmd, length;
  extern int no_specials;
  char *line;

  AFF_FLAGS(ch).RemoveBit(AFF_HIDE);
  PRF_FLAGS(ch).RemoveBit(PRF_AFK);

  /* just drop to next line for hitting CR */
  skip_spaces(&argument);
  if (!*argument)
    return;

  /*
   * special case to handle one-character, non-alphanumeric commands;
   * requested by many people so "'hi" or ";godnet test" is possible.
   * Patch sent by Eric Green and Stefan Wasilewski.
   */
  if (!isalpha(*argument))
  {
    arg[0] = argument[0];
    arg[1] = '\0';
    line = argument+1;
  } else
    line = any_one_arg(argument, arg);
  if (AFF_FLAGGED(ch, AFF_FEAR))
  {
    send_to_char("You are crazy with fear!\r\n", ch);
    return;
  }
  if (ch->points.fire[0] > 0 && success_test(GET_WIL(ch), 6) < 1)
  {
    send_to_char("^RThe flames cause you to panic!^n\r\n", ch);
    return;
  }
  /* otherwise, find the command */
  if (PLR_FLAGGED(ch, PLR_MATRIX))
  {
    for (length = strlen(arg), cmd = 0; *mtx_info[cmd].command != '\n'; cmd++)
      if (!strncmp(mtx_info[cmd].command, arg, length))
        break;
    if (*mtx_info[cmd].command == '\n')
      nonsensical_reply(ch);
    else {
      if (ch->persona && ch->persona->decker->hitcher) {
        sprintf(buf, "^y<OUTGOING> %s^n\r\n", argument);
        send_to_char(ch->persona->decker->hitcher, buf);
      }
      if (!special(ch, cmd, line))
        ((*mtx_info[cmd].command_pointer) (ch, line, cmd, mtx_info[cmd].subcmd));
    }
  } else if (PLR_FLAGGED(ch, PLR_REMOTE) || AFF_FLAGGED(ch, AFF_RIG))
  {
    for (length = strlen(arg), cmd = 0; *rig_info[cmd].command != '\n'; cmd++)
      if (!strncmp(rig_info[cmd].command, arg, length))
        break;
    if (*rig_info[cmd].command == '\n')
      nonsensical_reply(ch);
    else if (!special(ch, cmd, line))
      ((*rig_info[cmd].command_pointer) (ch, line, cmd, rig_info[cmd].subcmd));
  } else
  {
    for (length = strlen(arg), cmd = 0; *cmd_info[cmd].command != '\n'; cmd++)
      if (!strncmp(cmd_info[cmd].command, arg, length))
        if ((cmd_info[cmd].minimum_level < LVL_BUILDER) ||
            access_level(ch, cmd_info[cmd].minimum_level))
          break;

    // this was added so we can make the special respond to any text they type
    if (*cmd_info[cmd].command == '\n') {
      nonsensical_reply(ch);
      return;
    }
    
    if (IS_PROJECT(ch) && AFF_FLAGGED(ch->desc->original, AFF_TRACKING) && cmd != find_command("track")) {
      send_to_char("You are too busy astrally tracking someone...\r\n", ch);
      return;
    }
    
    if (PLR_FLAGGED(ch, PLR_FROZEN)) {
      if (!access_level(ch, LVL_VICEPRES)) {
        send_to_char("You try, but the mind-numbing cold prevents you...\r\n", ch);
        return;
      } else
        send_to_char("The ice covering you crackles alarmingly as you slam your sovereign will through it.", ch);
    }
    
    if (AFF_FLAGGED(ch, AFF_PETRIFY) && cmd_info[cmd].minimum_position > POS_DEAD) {
      if (!access_level(ch, LVL_VICEPRES)) {
        send_to_char("Your muscles don't respond to your impulse.\r\n", ch);
        return;
      } else
        send_to_char("You abuse your administrative powers and force your petrified body to respond.\r\n", ch);
    }
    
    if (cmd_info[cmd].command_pointer == NULL) {
      send_to_char("Sorry, that command hasn't been implemented yet.\r\n", ch);
      return;
    }
    
    if (affected_by_power(ch, ENGULF) && cmd_info[cmd].minimum_position != POS_DEAD) {
      if (!access_level(ch, LVL_VICEPRES)) {
        send_to_char("You are currently being engulfed!\r\n", ch);
        return;
      } else
        send_to_char("Administrative power roars through your veins as you muscle through your engulfment.\r\n", ch);
    }
    
    if (GET_QUI(ch) <= 0 && cmd_info[cmd].minimum_position != POS_DEAD) {
      if (!access_level(ch, LVL_VICEPRES)) {
        send_to_char("You are paralyzed!\r\n", ch);
        return;
      } else
        send_to_char("You draw upon your mantle of administrative power and push through your paralysis.\r\n", ch);
    }
    
    if (GET_POS(ch) < cmd_info[cmd].minimum_position) {
      switch (GET_POS(ch)) {
      case POS_DEAD:
        send_to_char("Lie still; you are DEAD!!! :-(\r\n", ch);
        break;
      case POS_MORTALLYW:
        send_to_char("You are in a pretty bad shape, unable to do anything!\r\n", ch);
        break;
      case POS_STUNNED:
        send_to_char("All you can do right now is think about the stars!\r\n", ch);
        break;
      case POS_SLEEPING:
        send_to_char("In your dreams, or what?\r\n", ch);
        break;
      case POS_RESTING:
        send_to_char("Nah... You feel too relaxed to do that..\r\n", ch);
        break;
      case POS_SITTING:
      case POS_LYING:
        send_to_char("Maybe you should get on your feet first?\r\n", ch);
        break;
      case POS_FIGHTING:
        send_to_char("No way!  You're fighting for your life!\r\n", ch);
        break;
      }
      return;
    }
    
    if (no_specials || !special(ch, cmd, line))
      ((*cmd_info[cmd].command_pointer) (ch, line, cmd, cmd_info[cmd].subcmd));
  }
}

/**************************************************************************
 * Routines to handle aliasing                                             *
  **************************************************************************/

struct alias *find_alias(struct alias *alias_list, char *str)
{
  for (; alias_list; alias_list = alias_list->next)
    if (*str == *alias_list->command)
      if (!strcmp(str, alias_list->command))
        return alias_list;


  return NULL;
}

void free_alias(struct alias *a)
{
  DELETE_ARRAY_IF_EXTANT(a->command);
  DELETE_ARRAY_IF_EXTANT(a->replacement);
  DELETE_AND_NULL(a);
}

/* The interface to the outside world: do_alias */
ACMD(do_alias)
{
  char *repl;
  struct alias *a, *temp;

  if (IS_NPC(ch) && !(ch->desc && ch->desc->original))
    return;

  repl = any_one_arg(argument, arg);

  if (!*arg) {  /* no argument specified -- list currently defined aliases */
    send_to_char("Currently defined aliases:\r\n", ch);
    if ((a = GET_ALIASES(ch)) == NULL)
      send_to_char(" None.\r\n", ch);
    else {
      while (a != NULL) {
        sprintf(buf, "%-15s %s\r\n", a->command, a->replacement);
        send_to_char(buf, ch);
        a = a->next;
      }
    }
  } else { /* otherwise, add or remove aliases */
    /* is this an alias we've already defined? */
    if ((a = find_alias(GET_ALIASES(ch), arg)) != NULL) {
      REMOVE_FROM_LIST(a, GET_ALIASES(ch), next);
      free_alias(a);
    }

    /* if no replacement string is specified, assume we want to delete */
    if (!*repl) {
      if (a == NULL)
        send_to_char("No such alias.\r\n", ch);
      else
        send_to_char("Alias deleted.\r\n", ch);
    } else { /* otherwise, either add or redefine an alias */
      if (!str_cmp(arg, "alias")) {
        send_to_char("You can't alias 'alias'.\r\n", ch);
        return;
      }
      /* Should cover every possbile case of 'kill', 'hit', and 'murder' */
      else if ( (strstr(repl,"kill") || strstr(repl,"hit") ||
                 strstr(repl,"murder") || strstr(repl,"KILL") ||
                 strstr(repl,"Kill") || strstr(repl,"KIll") ||
                 strstr(repl,"KILl") || strstr(repl,"kILL") ||
                 strstr(repl,"kiLL") || strstr(repl,"kilL") ||
                 strstr(repl,"HIT") || strstr(repl,"Hit") ||
                 strstr(repl,"HIt") || strstr(repl,"hIT") ||
                 strstr(repl,"hiT") || strstr(repl,"MURDER") ||
                 strstr(repl,"Murder") || strstr(repl,"MUrder") ||
                 strstr(repl,"MURder") || strstr(repl,"MURDer") ||
                 strstr(repl,"MURDEr") || strstr(repl,"mURDER") ||
                 strstr(repl,"muRDER") || strstr(repl,"murDER") ||
                 strstr(repl,"murdER") || strstr(repl,"murdeR"))
                && strlen(arg) < 4 ) {
        send_to_char(
          "If you alias contains the 'kill', 'hit', or 'murder' commands,"
          " it must be accompanied by at least a 4 letter alias.\n\r",ch);
        return;
      } else if ( strstr(repl,"quit") ) {
        send_to_char("Aliases cannot contain the 'quit' command.\n\r",ch);
        return;
      }
      if (strlen(repl) > 256)
        repl[255] = '\0';
      a = new alias;
      a->command = str_dup(arg);
      skip_spaces(&repl);
      delete_doubledollar(repl);
      a->replacement = str_dup(repl);
      if (strchr(repl, ALIAS_SEP_CHAR) ||
          strchr(repl, ALIAS_VAR_CHAR))
        a->type = ALIAS_COMPLEX;
      else
        a->type = ALIAS_SIMPLE;
      a->next = GET_ALIASES(ch);
      GET_ALIASES(ch) = a;
      send_to_char("Alias added.\r\n", ch);
    }
  }
}

/*
 * Valid numeric replacements are only &1 .. &9 (makes parsing a little
 * easier, and it's not that much of a limitation anyway.)  Also valid
 * is "&*", which stands for the entire original line after the alias.
 * ";" is used to delimit commands.
 */
#define NUM_TOKENS       9

void perform_complex_alias(struct txt_q *input_q, char *orig, struct alias *a)
{
  struct txt_q temp_queue;
  char *tokens[NUM_TOKENS], *temp, *write_point;
  int num_of_tokens = 0, num;

  skip_spaces(&orig);

  /* First, parse the original string */
  temp = strtok(strcpy(buf2, orig), " ");
  while (temp != NULL && num_of_tokens < NUM_TOKENS)
  {
    tokens[num_of_tokens++] = temp;
    temp = strtok(NULL, " ");
  }

  /* initialize */
  write_point = buf;
  temp_queue.head = temp_queue.tail = NULL;
  /* now parse the alias */
  for (temp = a->replacement; *temp; temp++)
  {
    if (*temp == ALIAS_SEP_CHAR) {
      *write_point = '\0';
      buf[MAX_INPUT_LENGTH-1] = '\0';
      write_to_q(buf, &temp_queue, 1);
      write_point = buf;
    } else if (*temp == ALIAS_VAR_CHAR) {
      temp++;
      if ((num = *temp - '1') < num_of_tokens && num >= 0) {
        strcpy(write_point, tokens[num]);
        write_point += strlen(tokens[num]);
      } else if (*temp == ALIAS_GLOB_CHAR) {
        strcpy(write_point, orig);
        write_point += strlen(orig);
      } else if ((*(write_point++) = *temp) == '$') /* redouble $ for act safety */
        *(write_point++) = '$';
      else if ((*(write_point++) = *temp) == '%') /* damn fucking c */
        *(write_point++) = '%';
    } else
      *(write_point++) = *temp;
  }

  *write_point = '\0';
  buf[MAX_INPUT_LENGTH-1] = '\0';
  write_to_q(buf, &temp_queue, 1);

  /* push our temp_queue on to the _front_ of the input queue */
  if (input_q->head == NULL)
    *input_q = temp_queue;
  else
  {
    temp_queue.tail->next = input_q->head;
    input_q->head = temp_queue.head;
  }
}

/*
 * Given a character and a string, perform alias replacement on it.
 *
 * Return values:
 *   0: String was modified in place; call command_interpreter immediately.
 *   1: String was _not_ modified in place; rather, the expanded aliases
 *      have been placed at the front of the character's input queue.
 */
int perform_alias(struct descriptor_data * d, char *orig)
{
  char first_arg[MAX_INPUT_LENGTH], *ptr;
  struct alias *a;

  /* bail out immediately if the guy doesn't have any aliases */
  if (!GET_ALIASES(d->character))
    return 0;

  /* find the alias we're supposed to match */
  ptr = any_one_arg(orig, first_arg);

  /* bail out if it's null */
  if (!*first_arg)
    return 0;

  /* if the first arg is not an alias, return without doing anything */
  if ((a = find_alias(GET_ALIASES(d->character), first_arg)) == NULL)
    return 0;

  if (a->type == ALIAS_SIMPLE)
  {
    strcpy(orig, a->replacement);
    return 0;
  } else
  {
    perform_complex_alias(&d->input, ptr, a);
    return 1;
  }
}

/***************************************************************************
 * Various other parsing utilities                                         *
 **************************************************************************/

/*
 * searches an array of strings for a target string.  "exact" can be
 * 0 or non-0, depending on whether or not the match must be exact for
 * it to be returned.  Returns -1 if not found; 0..n otherwise.  Array
 * must be terminated with a '\n' so it knows to stop searching.
 */
int search_block(char *arg, const char **list, bool exact)
{
  register int i, l;
  if (!strcmp(arg, "!"))
    return -1;

  /* Make into lower case, and get length of string */
  for (l = 0; *(arg + l); l++)
    *(arg + l) = LOWER(*(arg + l));

  if (exact) {
    for (i = 0; **(list + i) != '\n'; i++)
      if (!strcmp(arg, *(list + i)))
        return (i);
  } else {
    if (!l)
      l = 1;                    /* Avoid "" to match the first available
                                                                                     * string */
    for (i = 0; **(list + i) != '\n'; i++)
      if (!strncmp(arg, *(list + i), l))
        return (i);
  }

  return -1;
}

int is_number(char *str)
{
  while (*str)
    if (!isdigit(*(str++)))
      return 0;

  return 1;
}

void skip_spaces(char **string)
{
  for (; **string && isspace(**string); (*string)++)
    ;
}

char *delete_doubledollar(char *string)
{
  char *read, *write;

  if ((write = strchr(string, '$')) == NULL)
    return string;

  read = write;

  while (*read)
    if ((*(write++) = *(read++)) == '$')
      if (*read == '$')
        read++;

  *write = '\0';

  return string;
}

int fill_word(char *argument)
{
  return (search_block(argument, ::fill, TRUE) >= 0);
}


int reserved_word(char *argument)
{
  return (search_block(argument, reserved, TRUE) >= 0);
}


/*
 * copy the first non-fill-word, space-delimited argument of 'argument'
 * to 'first_arg'; return a pointer to the remainder of the string.
 */
char *one_argument(char *argument, char *first_arg)
{
  char *begin = first_arg;

  do {
    skip_spaces(&argument);

    first_arg = begin;
    while (*argument && !isspace(*argument)) {
      *(first_arg++) = LOWER(*argument);
      argument++;
    }

    *first_arg = '\0';
  } while (fill_word(begin));

  skip_spaces(&argument);

  return argument;
}


/* same as one_argument except that it doesn't ignore fill words */
char *any_one_arg(char *argument, char *first_arg)
{
  if (!argument)
    return NULL;

  skip_spaces(&argument);

  while (*argument && !isspace(*argument)) {
    *(first_arg++) = LOWER(*argument);
    argument++;
  }

  *first_arg = '\0';

  return argument;
}


/*
 * Same as one_argument except that it takes two args and returns the rest;
 * ignores fill words
 */
char *two_arguments(char *argument, char *first_arg, char *second_arg)
{
  return one_argument(one_argument(argument, first_arg), second_arg);   /* :-) */
}


/*
 * determine if a given string is an abbreviation of another
 * (now works symmetrically -- JE 7/25/94)
 *
 * that was dumb.  it shouldn't be symmetrical.  JE 5/1/95
 *
 * returns 1 if arg1 is an abbreviation of arg2
 */
int is_abbrev(const char *arg1, const char *arg2)
{
  if (!*arg1)
    return 0;

  for (; *arg1 && *arg2; arg1++, arg2++)
    if (LOWER(*arg1) != LOWER(*arg2))
      return 0;

  if (!*arg1)
    return 1;
  else
    return 0;
}

/* return first space-delimited token in arg1; remainder of string in arg2 */
void half_chop(char *string, char *arg1, char *arg2)
{
  char *temp;

  temp = any_one_arg(string, arg1);
  skip_spaces(&temp);
  strcpy(arg2, temp);
}

/* Used in specprocs, mostly.  (Exactly) matches "command" to cmd number */
int find_command(const char *command)
{
  int cmd;

  for (cmd = 0; *cmd_info[cmd].command != '\n'; cmd++)
    if (!strcmp(cmd_info[cmd].command, command))
      return cmd;

  return -1;
}
int find_mcommand(const char *command)
{
  int cmd;

  for (cmd = 0; *mtx_info[cmd].command != '\n'; cmd++)
    if (!strcmp(mtx_info[cmd].command, command))
      return cmd;

  return -1;
}

int special(struct char_data * ch, int cmd, char *arg)
{
  register struct obj_data *i;
  if (ch->persona)
  {
    for (i = matrix[ch->persona->in_host].file; i; i = i->next_content)
      if (GET_OBJ_SPEC(i) != NULL)
        if (GET_OBJ_SPEC(i) (ch, i, cmd, arg))
          return 1;
    return 0;
  }

  /* special in room? */

  struct veh_data *veh;
  RIG_VEH(ch, veh);
  if (veh && GET_ROOM_SPEC(veh->in_room) != NULL)
      if (GET_ROOM_SPEC(veh->in_room) (ch, world + veh->in_room, cmd, arg))
        return 1;
  if (GET_ROOM_SPEC(ch->in_room) != NULL)
    if (GET_ROOM_SPEC(ch->in_room) (ch, world + ch->in_room, cmd, arg))
      return 1;

  /* special in equipment list? */
  int j = 0;
  for (; j < NUM_WEARS; j++)
    if (GET_EQ(ch, j) && GET_OBJ_SPEC(ch->equipment[j]) != NULL)
      if (GET_OBJ_SPEC(ch->equipment[j]) (ch, ch->equipment[j], cmd, arg))
        return 1;

  /* special in inventory? */
  for (i = ch->carrying; i; i = i->next_content)
    if (GET_OBJ_SPEC(i) != NULL)
      if (GET_OBJ_SPEC(i) (ch, i, cmd, arg))
        return 1;

  /* special in object present? */
  for (i = ch->in_veh ? ch->in_veh->contents : world[ch->in_room].contents; i; i = i->next_content)
    if (GET_OBJ_SPEC(i) != NULL && !(ch->in_veh && ch->vfront != i->vfront))
      if (GET_OBJ_SPEC(i) (ch, i, cmd, arg))
        return 1;
  /* special in mobile present? */
  if (!veh)
  {
    register struct char_data *k;
    for (k = world[ch->in_room].people; k; k = k->next_in_room) {
      if (GET_MOB_SPEC(k) != NULL)
        if (GET_MOB_SPEC(k) (ch, k, cmd, arg))
          return 1;
      if (mob_index[GET_MOB_RNUM(k)].sfunc != NULL)
        if ((mob_index[GET_MOB_RNUM(k)].sfunc) (ch, k, cmd, arg))
          return 1;
    }
  }
  return 0;
}

/* *************************************************************************
*  Stuff for controlling the non-playing sockets (get name, pwd etc)       *
************************************************************************* */

int _parse_name(char *arg, char *name)
{
  int i;

  /* skip whitespaces */
  for (; isspace(*arg); arg++)
    ;

  for (i = 0; (*name = *arg); arg++, i++, name++)
    if (!isalpha(*arg))
      return 1;

  if (!i)
    return 1;

  return 0;
}

#define RECON           1
#define USURP           2
#define UNSWITCH        3

int perform_dupe_check(struct descriptor_data *d)
{
  struct descriptor_data *k, *next_k;
  struct char_data *target = NULL, *ch, *next_ch;
  int mode = 0;

  int id = GET_IDNUM(d->character);

  /*
   * Now that this descriptor has successfully logged in, disconnect all
   * other descriptors controlling a character with the same ID number.
   */

  for (k = descriptor_list; k; k = next_k)
  {
    next_k = k->next;

    if (k == d)
      continue;

    if (k->original && (GET_IDNUM(k->original) == id)) {    /* switched char */
      SEND_TO_Q("\r\nMultiple login detected -- disconnecting.\r\n", k);
      STATE(k) = CON_CLOSE;
      if (!target) {
        target = k->original;
        mode = UNSWITCH;
      }
      if (k->character)
        k->character->desc = NULL;
      k->character = NULL;
      k->original = NULL;
    } else if (k->character && (GET_IDNUM(k->character) == id)) {
      if (!target && STATE(k) == CON_PLAYING) {
        SEND_TO_Q("\r\nThis body has been usurped!\r\n", k);
        target = k->character;
        mode = USURP;
      }
      k->character->desc = NULL;
      k->character = NULL;
      k->original = NULL;
      SEND_TO_Q("\r\nMultiple login detected -- disconnecting.\r\n", k);
      STATE(k) = CON_CLOSE;
    }
  }
  /*
   * now, go through the character list, deleting all characters that
   * are not already marked for deletion from the above step (i.e., in the
   * CON_HANGUP state), and have not already been selected as a target for
   * switching into.  In addition, if we haven't already found a target,
   * choose one if one is available (while still deleting the other
   * duplicates, though theoretically none should be able to exist).
   */

  for (ch = character_list; ch; ch = next_ch)
  {
    next_ch = ch->next;

    if (IS_NPC(ch))
      continue;
    if (GET_IDNUM(ch) != id)
      continue;

    /* ignore chars with descriptors (already handled by above step) */
    if (ch->desc)
      continue;

    /* don't extract the target char we've found one already */
    if (ch == target)
      continue;

    /* we don't already have a target and found a candidate for switching */
    if (!target) {
      target = ch;
      mode = RECON;
      continue;
    }

    /* we've found a duplicate - blow him away, dumping his eq in limbo. */
    if (ch->in_room != NOWHERE)
      char_from_room(ch);
    char_to_room(ch, 1);
    extract_char(ch);
  }

  /* no target for swicthing into was found - allow login to continue */
  if (!target)
    return 0;

  /* Okay, we've found a target.  Connect d to target. */
  Mem->DeleteCh(d->character); /* get rid of the old char */
  d->character = target;
  d->character->desc = d;
  d->original = NULL;
  d->character->char_specials.timer = 0;
  PLR_FLAGS(d->character).RemoveBits(PLR_MAILING, PLR_EDITING,
                                     PLR_SPELL_CREATE, PLR_CUSTOMIZE,
                                     PLR_PROJECT, PLR_MATRIX, ENDBIT);
  if (STATE(d) == CON_VEHCUST)
    d->edit_veh = NULL;
  else if (STATE(d) == CON_POCKETSEC)
    d->edit_obj = NULL;
  STATE(d) = CON_PLAYING;
  switch (mode)
  {
  case RECON:
    SEND_TO_Q("Reconnecting.\r\n", d);
    act("$n has reconnected.", TRUE, d->character, 0, 0, TO_ROOM);
    sprintf(buf, "%s [%s] has reconnected.",
            GET_CHAR_NAME(d->character), d->host);
    mudlog(buf, d->character, LOG_CONNLOG, TRUE);
    break;
  case USURP:
    SEND_TO_Q("You take over your own body, already in use!\r\n", d);
    act("$n shakes $s head to clear it.",
        TRUE, d->character, 0, 0, TO_ROOM);
    sprintf(buf, "%s has re-logged in ... disconnecting old socket.",
            GET_CHAR_NAME(d->character));
    mudlog(buf, d->character, LOG_CONNLOG, TRUE);
    if (d->character->persona)
    {
      sprintf(buf, "%s depixelizes and vanishes from the host.\r\n", d->character->persona->name);
      send_to_host(d->character->persona->in_host, buf, d->character->persona, TRUE);
      extract_icon(d->character->persona);
      d->character->persona = NULL;
      PLR_FLAGS(d->character).RemoveBit(PLR_MATRIX);
    } else if (PLR_FLAGGED(d->character, PLR_MATRIX)) {
      for (struct char_data *temp = world[d->character->in_room].people; temp; temp = temp->next_in_room)
        if (PLR_FLAGGED(temp, PLR_MATRIX))
          temp->persona->decker->hitcher = NULL;
      PLR_FLAGS(d->character).RemoveBit(PLR_MATRIX);
    }
    // now delete all the editing struct
    if (d->edit_obj)
      Mem->DeleteObject(d->edit_obj);
    d->edit_obj = NULL;
    
    if (d->edit_room)
      Mem->DeleteRoom(d->edit_room);
    d->edit_room = NULL;
    
    if (d->edit_mob)
      Mem->DeleteCh(d->edit_mob);
    d->edit_mob = NULL;
    
    if (d->edit_shop) {
      free_shop(d->edit_shop);
      DELETE_AND_NULL(d->edit_shop);
    }
    
    if (d->edit_quest) {
      free_quest(d->edit_quest);
      DELETE_AND_NULL(d->edit_quest);
    }
    
    DELETE_IF_EXTANT(d->edit_zon);
    
    DELETE_IF_EXTANT(d->edit_cmd);
    
    if (d->edit_veh)
      Mem->DeleteVehicle(d->edit_veh);
    d->edit_veh = NULL;
  
    break;
  case UNSWITCH:
    SEND_TO_Q("Reconnecting to unswitched char.", d);
    sprintf(buf, "%s [%s] has reconnected.",
            GET_CHAR_NAME(d->character), d->host);
    mudlog(buf, d->character, LOG_CONNLOG, TRUE);
    break;
  }

  return 1;
}

/* deal with newcomers and other non-playing sockets */
void nanny(struct descriptor_data * d, char *arg)
{
  char buf[128];
  int load_result = NOWHERE;
  char tmp_name[MAX_INPUT_LENGTH];
  extern vnum_t mortal_start_room;
  extern vnum_t frozen_start_room;
  extern vnum_t newbie_start_room;
  extern int max_bad_pws;
  extern bool House_can_enter(struct char_data *ch, vnum_t vnum);
  long load_room;
  bool dirty_password = FALSE;

  int parse_class(struct descriptor_data *d, char *arg);
  int parse_race(struct descriptor_data *d, char *arg);
  int parse_totem(struct descriptor_data *d, char *arg);

  skip_spaces(&arg);

  switch (STATE(d))
  {
    // this sends control to the various on-line creation systems
  case CON_POCKETSEC:
    pocketsec_parse(d, arg);
    break;
  case CON_REDIT:
    redit_parse(d, arg);
    break;
  case CON_IEDIT:
    iedit_parse(d, arg);
    break;
  case CON_MEDIT:
    medit_parse(d, arg);
    break;
  case CON_QEDIT:
    qedit_parse(d, arg);
    break;
  case CON_SHEDIT:
    shedit_parse(d, arg);
    break;
  case CON_VEDIT:
    vedit_parse(d, arg);
    break;
  case CON_ZEDIT:
    zedit_parse(d, arg);
    break;
  case CON_HEDIT:
    hedit_parse(d, arg);
    break;
  case CON_ICEDIT:
    icedit_parse(d, arg);
    break;
  case CON_SPELL_CREATE:
    spedit_parse(d, arg);
    break;
  case CON_AMMO_CREATE:
    aedit_parse(d, arg);
    break;
  case CON_PRO_CREATE:
    pedit_parse(d, arg);
    break;
  case CON_PART_CREATE:
    pbuild_parse(d, arg);
    break;
  case CON_DECK_CREATE:
    dbuild_parse(d, arg);
    break;
  case CON_FCUSTOMIZE:
  case CON_BCUSTOMIZE:
  case CON_PCUSTOMIZE:
  case CON_ACUSTOMIZE:
    cedit_parse(d, arg);
    break;
  case CON_VEHCUST:
    vehcust_parse(d, arg);
    break;
  case CON_INITIATE:
    init_parse(d, arg);
    break;
  case CON_CCREATE:
    create_parse(d, arg);
    break;
  case CON_PGEDIT:
    pgedit_parse(d, arg);
    break;
  case CON_GET_NAME:            /* wait for input of name */
    d->idle_tics = 0;

    if (!*arg) {
      d->invalid_name++;
      if (d->invalid_name > 3)
        close_socket(d);
      else {
        SEND_TO_Q("\r\nWhat name do you wish to be called by? ", d);
        return;
      }
    } else {
      if ((_parse_name(arg, tmp_name)) || strlen(tmp_name) < 2 ||
          strlen(tmp_name) > MAX_NAME_LENGTH-1 ||
          fill_word(strcpy(buf, tmp_name)) || reserved_word(buf)) {
        d->invalid_name++;
        if (d->invalid_name > 3)
          close_socket(d);
        else
          SEND_TO_Q("Invalid name, please try another.\r\nName: ", d);
        return;
      }
      if (does_player_exist(tmp_name)) {
        d->character = playerDB.LoadChar(tmp_name, TRUE);
        d->character->desc = d;
        if (PRF_FLAGGED(d->character, PRF_HARDCORE) && PLR_FLAGGED(d->character, PLR_JUST_DIED)) {
          SEND_TO_Q("The Reaper has claimed this one...\r\n", d);
          STATE(d) = CON_CLOSE;
          return;
        }

        SEND_TO_Q("Password: ", d);
        echo_off(d);

        STATE(d) = CON_PASSWORD;
      } else {
        /* player unknown -- make new character */
        for (struct descriptor_data *k = descriptor_list; k; k = k->next)
          if (k->character && k->character->player.char_name && !str_cmp(k->character->player.char_name, tmp_name)) {
            SEND_TO_Q("There is already someone creating a character with that name.\r\nName: ", d);
            return;
          }
        if (d->character == NULL) {
          d->character = Mem->GetCh();
          DELETE_IF_EXTANT(d->character->player_specials);
          d->character->player_specials = new player_special_data;
          // make sure to clear it up here
          memset(d->character->player_specials, 0,
                 sizeof(player_special_data));

          d->character->desc = d;
        }

        d->character->player.char_name = new char[strlen(tmp_name) + 1];
        strcpy(d->character->player.char_name, CAP(tmp_name));

        sprintf(buf, "Did I get that right, %s (Y/N)? ", tmp_name);
        SEND_TO_Q(buf, d);
        STATE(d) = CON_NAME_CNFRM;
      }
    }
    break;
  case CON_NAME_CNFRM:          /* wait for conf. of new name    */
    if (UPPER(*arg) == 'Y') {
      if (isbanned(d->host) >= BAN_NEW) {
        sprintf(buf, "Request for new char %s denied from [%s] (siteban)",
                GET_CHAR_NAME(d->character), d->host);
        mudlog(buf, d->character, LOG_BANLOG, TRUE);
        SEND_TO_Q("Sorry, new characters are not allowed from your site.\r\n", d);
        STATE(d) = CON_CLOSE;
        return;
      }
      if (restrict) {
        if (restrict == LVL_MAX)
          SEND_TO_Q("The mud is being reconfigured.  Try again a bit later.\r\n", d);
        else
          SEND_TO_Q("Sorry, new players can't be created at the moment.\r\n", d);
        sprintf(buf, "Request for new char %s denied from %s (wizlock)",
                GET_CHAR_NAME(d->character), d->host);
        mudlog(buf, d->character, LOG_CONNLOG, TRUE);
        STATE(d) = CON_CLOSE;
        return;
      }
      SEND_TO_Q("New character.\r\n", d);
      sprintf(buf, "Give me a password for %s: ",
              GET_CHAR_NAME(d->character));
      SEND_TO_Q(buf, d);
      echo_off(d);
      STATE(d) = CON_NEWPASSWD;
    } else if (*arg == 'n' || *arg == 'N') {
      SEND_TO_Q("Okay, what IS it, then? ", d);
      DELETE_ARRAY_IF_EXTANT(d->character->player.char_name);
      STATE(d) = CON_GET_NAME;
    } else {
      SEND_TO_Q("Please type Yes or No: ", d);
    }
    break;
  case CON_PASSWORD:            /* get pwd for known player      */
    /*
     * To really prevent duping correctly, the player's record should
     * be reloaded from disk at this point (after the password has been
     * typed).  However I'm afraid that trying to load a character over
     * an already loaded character is going to cause some problem down the
     * road that I can't see at the moment.  So to compensate, I'm going to
     * (1) add a 15 or 20-second time limit for entering a password, and (2)
     * re-add the code to cut off duplicates when a player quits.  JE 6 Feb 96
     */

    /* turn echo back on */
    echo_on(d);
      
    // Clear their idle counter so they don't get dropped mysteriously.
    d->idle_tics = 0;

    if (!*arg)
      close_socket(d);
    else {
      if (!validate_and_update_password(arg, GET_PASSWD(d->character))) {
        sprintf(buf, "Bad PW: %s [%s]",
                GET_CHAR_NAME(d->character), d->host);
        mudlog(buf, d->character, LOG_CONNLOG, TRUE);
        GET_BAD_PWS(d->character)++;
        d->character->in_room = real_room(GET_LAST_IN(d->character));
        playerDB.SaveChar(d->character, GET_LOADROOM(d->character));
        if (++(d->bad_pws) >= max_bad_pws) {    /* 3 strikes and you're out. */
          SEND_TO_Q("Wrong password... disconnecting.\r\n", d);
          STATE(d) = CON_CLOSE;
        } else {
          SEND_TO_Q("Wrong password.\r\nPassword: ", d);
          echo_off(d);
        }
        return;
      }
      
      // Commit the password to DB on the assumption it's changed.
      char query_buf[2048];
#ifdef NOCRYPT
      char prepare_quotes_buf[2048];
      sprintf(query_buf, "UPDATE pfiles SET password='%s' WHERE idnum=%ld;",
              prepare_quotes(prepare_quotes_buf, GET_PASSWD(d->character)), GET_IDNUM(d->character));
#else
      sprintf(query_buf, "UPDATE pfiles SET password='%s' WHERE idnum=%ld;", GET_PASSWD(d->character), GET_IDNUM(d->character));
#endif
      mysql_wrapper(mysql, query_buf);
      
      load_result = GET_BAD_PWS(d->character);
      GET_BAD_PWS(d->character) = 0;

      d->character->in_room = real_room(GET_LAST_IN(d->character));
      playerDB.SaveChar(d->character, GET_LOADROOM(d->character));
      if (isbanned(d->host) == BAN_SELECT &&
          !PLR_FLAGGED(d->character, PLR_SITEOK)) {
        SEND_TO_Q("Sorry, this char has not been cleared for login from your site!\r\n", d);
        STATE(d) = CON_CLOSE;
        sprintf(buf, "Connection attempt for %s denied from %s",
                GET_CHAR_NAME(d->character), d->host);
        mudlog(buf, d->character, LOG_BANLOG, TRUE);
        return;
      }
      if (!access_level(d->character, restrict)) {
        if (restrict == LVL_MAX)
          SEND_TO_Q("The mud is about to reboot. Please try again in a few minutes.\r\n", d);
        else
          SEND_TO_Q("The game is temporarily restricted.. try again later.\r\n", d);
        STATE(d) = CON_CLOSE;
        sprintf(buf, "Request for login denied for %s [%s] (wizlock)",
                GET_CHAR_NAME(d->character), d->host);
        mudlog(buf, d->character, LOG_CONNLOG, TRUE);
        return;
      }
      /* check and make sure no other copies of this player are logged in */
      if (perform_dupe_check(d))
        return;

      /* undo it just in case they are set */
      PLR_FLAGS(d->character).RemoveBits(PLR_WRITING, PLR_MAILING,
                                         PLR_EDITING,
                                         PLR_SPELL_CREATE, PLR_CUSTOMIZE,
                                         PLR_PROJECT, PLR_MATRIX, ENDBIT);

      if (IS_SENATOR(d->character))
        SEND_TO_Q(imotd, d);
      else
        SEND_TO_Q(motd, d);

      if(PLR_FLAGGED(d->character,PLR_AUTH))
        sprintf(buf, "%s [%s] has connected (UNAUTHORIZED).",
                GET_CHAR_NAME(d->character), d->host);
      else
        sprintf(buf, "%s [%s] has connected.",
                GET_CHAR_NAME(d->character), d->host);
      DELETE_ARRAY_IF_EXTANT(d->character->player.host);
      d->character->player.host = strdup(d->host);
      playerDB.SaveChar(d->character);
      mudlog(buf, d->character, LOG_CONNLOG, TRUE);
      if (load_result) {
        sprintf(buf, "\r\n\r\n\007\007\007"
                "%s%d LOGIN FAILURE%s SINCE LAST SUCCESSFUL LOGIN.%s\r\n",
                CCRED(d->character, C_SPR), load_result,
                (load_result > 1) ? "S" : "", CCNRM(d->character, C_SPR));
        SEND_TO_Q(buf, d);
        GET_BAD_PWS(d->character) = 0;
      }
      SEND_TO_Q("\r\n\n*** PRESS RETURN: ", d);
      STATE(d) = CON_RMOTD;
    }
    break;

  case CON_NEWPASSWD:
  case CON_CHPWD_GETNEW:
  case CON_QGETNEWPW:
    if (!*arg || strlen(arg) < 3 || !str_cmp(arg, GET_CHAR_NAME(d->character))) {
      SEND_TO_Q("\r\nIllegal password.\r\n", d);
      SEND_TO_Q("Password: ", d);
      return;
    }
    hash_and_store_password(arg, GET_PASSWD(d->character));

    SEND_TO_Q("\r\nPlease retype password: ", d);
    if (STATE(d) == CON_NEWPASSWD)
      STATE(d) = CON_CNFPASSWD;
    else if (STATE(d) == CON_CHPWD_GETNEW)
      STATE(d) = CON_CHPWD_VRFY;
    else
      STATE(d) = CON_QVERIFYPW;
    break;

  case CON_CNFPASSWD:
  case CON_CHPWD_VRFY:
  case CON_QVERIFYPW:
    if (!validate_password(arg, (const char*) GET_PASSWD(d->character))) {
      SEND_TO_Q("\r\nPasswords don't match... start over.\r\n", d);
      SEND_TO_Q("Password: ", d);
      if (STATE(d) == CON_CNFPASSWD)
        STATE(d) = CON_NEWPASSWD;
      else if (STATE(d) == CON_CHPWD_VRFY)
        STATE(d) = CON_CHPWD_GETNEW;
      else
        STATE(d) = CON_QGETNEWPW;
      return;
    }
    echo_on(d);
    dirty_password = (STATE(d) == CON_CHPWD_VRFY);

    if (STATE(d) == CON_CNFPASSWD) {
      SEND_TO_Q("What is your sex (M/F)? ", d);
      STATE(d) = CON_CCREATE;
      init_create_vars(d);
    } else {
      if (STATE(d) != CON_CHPWD_VRFY)
        d->character = playerDB.LoadChar(GET_CHAR_NAME(d->character), TRUE);
      SEND_TO_Q("\r\nDone.\r\n", d);
      if (PLR_FLAGGED(d->character,PLR_AUTH)) {
        playerDB.SaveChar(d->character);
        SEND_TO_Q(MENU, d);
        STATE(d) = CON_MENU;
      }
      if (dirty_password) { // STATE(d) is changed directly above this after all...
        char query_buf[2048];
#ifdef NOCRYPT
        char prepare_quotes_buf[2048];
        sprintf(query_buf, "UPDATE pfiles SET password='%s' WHERE idnum=%ld;",
                prepare_quotes(prepare_quotes_buf, GET_PASSWD(d->character)), GET_IDNUM(d->character));
#else
        sprintf(query_buf, "UPDATE pfiles SET password='%s' WHERE idnum=%ld;", GET_PASSWD(d->character), GET_IDNUM(d->character));
#endif
        mysql_wrapper(mysql, query_buf);
        SEND_TO_Q(MENU, d);
        STATE(d) = CON_MENU;
      } else {
        extract_char(d->character);
        SEND_TO_Q(QMENU, d);
        STATE(d) = CON_QMENU;
      }
    }

    break;

  case CON_RMOTD:               /* read CR after printing motd   */
    SEND_TO_Q(MENU,d);
    STATE(d) = CON_MENU;
    break;

  case CON_MENU:                /* get selection from main menu  */
    switch (*arg) {
    case '0':
      close_socket(d);
      break;

    case '1':
      if (PLR_FLAGGED(d->character, PLR_INVSTART))
        GET_INVIS_LEV(d->character) = GET_LEVEL(d->character);
      if (PLR_FLAGGED(d->character, PLR_JUST_DIED)) {
        if (PRF_FLAGGED(d->character, PRF_HARDCORE)) {
          send_to_char("The Reaper has claimed this one...\r\n", d->character);
          STATE(d) = CON_CLOSE;
          return;
        }
        // TODO: If you died and then hit 1, your old character's data is leaked here.
        char char_name[strlen(GET_CHAR_NAME(d->character))+1];
        strcpy(char_name, GET_CHAR_NAME(d->character));
        free_char(d->character);
        d->character = playerDB.LoadChar(char_name, false);
        d->character->desc = d;
        PLR_FLAGS(d->character).RemoveBit(PLR_JUST_DIED);
        if (PLR_FLAGGED(d->character, PLR_NEWBIE)) {
          GET_PHYSICAL(d->character) = GET_MAX_PHYSICAL(d->character);
          GET_MENTAL(d->character) = GET_MAX_MENTAL(d->character);
        } else {
          GET_PHYSICAL(d->character) = (int)(GET_MAX_PHYSICAL(d->character) * .4);
          GET_MENTAL(d->character) = (int)(GET_MAX_MENTAL(d->character) * .4);
        }
        playerDB.SaveChar(d->character, GET_LOADROOM(d->character));
      }
      reset_char(d->character);
      PLR_FLAGS(d->character).RemoveBit(PLR_CUSTOMIZE);
      send_to_char(WELC_MESSG, d->character);
      d->character->next = character_list;
      character_list = d->character;
      d->character->player.time.logon = time(0);

      if (GET_LOADROOM(d->character) == RM_NEWBIE_LOADROOM && !PLR_FLAGGED(d->character, PLR_NEWBIE))
        GET_LOADROOM(d->character) = mortal_start_room;

      if ((load_room = GET_LAST_IN(d->character)) != NOWHERE)
        load_room = real_room(load_room);

      if (IS_SENATOR(d->character) && (load_room <= 0 || load_room == real_room(mortal_start_room)))
        load_room = real_room(GET_LOADROOM(d->character));
        
      if (load_room < 0) {
        sprintf(buf, "SYSERR: Character %s is loading in with invalid load room %ld (%ld). Changing to Grog's place (35500).",
                     GET_CHAR_NAME(d->character), GET_LOADROOM(d->character), load_room);
        mudlog(buf, d->character, LOG_SYSLOG, TRUE);
        load_room = real_room(RM_ENTRANCE_TO_DANTES);
        GET_LOADROOM(d->character) = RM_ENTRANCE_TO_DANTES;
      }
      
      if (ROOM_FLAGGED(load_room, ROOM_HOUSE) && !House_can_enter(d->character, world[load_room].number))
        load_room = real_room(mortal_start_room);
      /* If char was saved with NOWHERE, or real_room above failed... */
      if (PLR_FLAGGED(d->character, PLR_AUTH))
        load_room = real_room(newbie_start_room);

      if (load_room == NOWHERE) {
        if (PLR_FLAGGED(d->character, PLR_NEWBIE))
          load_room = real_room(RM_NEWBIE_LOADROOM); // The Neophyte Hotel (previously 8039 which does not exist)
        else
          load_room = real_room(mortal_start_room);
      }

      if (PLR_FLAGGED(d->character, PLR_FROZEN))
        load_room = real_room(frozen_start_room);

      if (!GET_LEVEL(d->character)) {
        load_room = real_room(newbie_start_room);
        do_start(d->character);
        playerDB.SaveChar(d->character, load_room);
        send_to_char(START_MESSG, d->character);
      }
      if (d->character->player_specials->saved.last_veh) {
        for (struct veh_data *veh = veh_list; veh; veh = veh->next)
          if (veh->idnum == d->character->player_specials->saved.last_veh && veh->damage < 10) {
            if (!veh->seating[CH->vfront])
              CH->vfront = !CH->vfront;
            char_to_veh(veh, d->character);
            break;
          }
      }
      if (!d->character->in_veh)
        char_to_room(d->character, load_room);
      act("$n has entered the game.", TRUE, d->character, 0, 0, TO_ROOM);
      sprintf(buf, "%s has entered the game.", GET_CHAR_NAME(d->character));
      mudlog(buf, d->character, LOG_CONNLOG, TRUE);

      STATE(d) = CON_PLAYING;

      look_at_room(d->character, 0);
      d->prompt_mode = 1;
      /* affect total to make cyberware update stats */
      affect_total(d->character);

      break;

    case '2':
      page_string(d, background, 0);
      STATE(d) = CON_RMOTD;
      break;

    case '3':
      SEND_TO_Q("\r\nEnter your old password: ", d);
      echo_off(d);
      STATE(d) = CON_CHPWD_GETOLD;
      break;

    case '4':
      SEND_TO_Q("\r\nEnter your password for verification: ", d);
      echo_off(d);
      STATE(d) = CON_DELCNF1;
      break;

    default:
      SEND_TO_Q("\r\nThat's not a menu choice!\r\n", d);
      SEND_TO_Q(MENU, d);
      break;
    }
    break;

  case CON_CHPWD_GETOLD:
  case CON_QGETOLDPW:
    if (!validate_password(arg, (const char*) GET_PASSWD(d->character))) {
      echo_on(d);
      SEND_TO_Q("\r\nIncorrect password.\r\n", d);
      if (STATE(d) == CON_CHPWD_GETOLD) {
        SEND_TO_Q(MENU, d);
        STATE(d) = CON_MENU;
      } else {
        SEND_TO_Q(QMENU, d);
        STATE(d) = CON_QMENU;
      }
      return;
    } else {
      SEND_TO_Q("\r\nEnter a new password: ", d);
      if (STATE(d) == CON_CHPWD_GETOLD)
        STATE(d) = CON_CHPWD_GETNEW;
      else
        STATE(d) = CON_QGETNEWPW;
      return;
    }
    break;

  case CON_DELCNF1:
  case CON_QDELCONF1:
    echo_on(d);
    if (!validate_password(arg, (const char*) GET_PASSWD(d->character))) {
      SEND_TO_Q("\r\nIncorrect password.\r\n", d);
      if (STATE(d) == CON_DELCNF1) {
        SEND_TO_Q(MENU, d);
        STATE(d) = CON_MENU;
      } else {
        SEND_TO_Q(QMENU, d);
        STATE(d) = CON_QMENU;
      }
    } else {
      SEND_TO_Q("\r\nYOU ARE ABOUT TO DELETE THIS CHARACTER PERMANENTLY.\r\n"
                "ARE YOU ABSOLUTELY SURE?\r\n\r\n"
                "Please type \"yes\" to confirm: ", d);
      if (STATE(d) == CON_DELCNF1)
        STATE(d) = CON_DELCNF2;
      else
        STATE(d) = CON_QDELCONF2;
    }
    break;

  case CON_DELCNF2:
  case CON_QDELCONF2:
    if (!strcmp(arg, "yes") || !strcmp(arg, "YES")) {
      if (PLR_FLAGGED(d->character, PLR_FROZEN)) {
        SEND_TO_Q("You try to kill yourself, but the ice stops you.\r\n", d);
        SEND_TO_Q("Character not deleted.\r\n\r\n", d);
        STATE(d) = CON_CLOSE;
        return;
      }

      DeleteChar(GET_IDNUM(d->character));
      sprintf(buf, "Character '%s' deleted!\r\nGoodbye.\r\n",
              GET_CHAR_NAME(d->character));
      SEND_TO_Q(buf, d);
      sprintf(buf, "%s (lev %d) has self-deleted.",
              GET_CHAR_NAME(d->character), GET_LEVEL(d->character));
      mudlog(buf, d->character, LOG_MISCLOG, TRUE);

      d->character = NULL;
      STATE(d) = CON_CLOSE;
      return;
    } else {
      SEND_TO_Q("\r\nCharacter not deleted.\r\n", d);
      if (STATE(d) == CON_DELCNF2) {
        SEND_TO_Q(MENU, d);
        STATE(d) = CON_MENU;
      } else {
        SEND_TO_Q(QMENU, d);
        STATE(d) = CON_QMENU;
      }
    }
    break;

  case CON_QMENU:                /* get selection from main quit menu  */
    switch (*arg) {
    case '0':
      close_socket(d);
      break;

    case '1':
      SEND_TO_Q("\r\nEnter your password for verification: ", d);
      echo_off(d);
      STATE(d) = CON_QDELCONF1;
      break;

    default:
      SEND_TO_Q("\r\nThat's not a menu choice!\r\n", d);
      SEND_TO_Q(QMENU, d);
      break;
    }
    break;
  case CON_CLOSE:
    close_socket(d);
    break;

  case CON_ASKNAME:
    break;

  default:
    log("SYSERR: Nanny: illegal state of con'ness; closing connection");
    close_socket(d);
    break;
  }
}
