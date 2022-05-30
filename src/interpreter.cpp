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

#include "structs.hpp"
#include "awake.hpp"
#include "comm.hpp"
#include "interpreter.hpp"
#include "db.hpp"
#include "newdb.hpp"
#include "utils.hpp"
#include "handler.hpp"
#include "screen.hpp"
#include "memory.hpp"
#include "newmagic.hpp"
#include "newshop.hpp"
#include "quest.hpp"
#include "constants.hpp"
#include "config.hpp"
#include "newmatrix.hpp"
#include "security.hpp"
#include "protocol.hpp"
#include "newdb.hpp"
#include "helpedit.hpp"
#include "archetypes.hpp"
#include "ignore_system.hpp"

#if defined(__CYGWIN__)
#include <crypt.h>
#endif

#define CH d->character

extern class memoryClass *Mem;

extern const char *MENU;
extern const char *QMENU;
extern const char *WELC_MESSG;
extern const char *START_MESSG;
extern int restrict_mud;

/* external functions */
void echo_on(struct descriptor_data * d);
void echo_off(struct descriptor_data * d);
void do_start(struct char_data * ch, bool wipe_skills);
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
int fix_common_command_fuckups(const char *arg, struct command_info *cmd_info);

void verify_data(struct char_data *ch, const char *line, int cmd, int subcmd, const char *section);

#ifdef LOG_COMMANDS
void log_command(struct char_data *ch, const char *argument, const char *tcname);
#endif

// for spell creation
void cedit_parse(struct descriptor_data *d, char *arg);

extern void affect_total(struct char_data * ch);
extern void mag_menu_system(struct descriptor_data * d, char *arg);
extern void ccr_pronoun_menu(struct descriptor_data *d);

/* prototypes for all do_x functions. */
ACMD_DECLARE(do_olcon);
ACMD_DECLARE(do_abilityset);
ACMD_DECLARE(do_accept);
ACMD_DECLARE(do_action);
ACMD_DECLARE(do_activate);
ACMD_DECLARE(do_advance);
ACMD_DECLARE(do_afk);
ACMD_DECLARE(do_alias);
ACMD_DECLARE(do_ammo);
ACMD_DECLARE(do_assist);
ACMD_DECLARE(do_ask);
ACMD_DECLARE(do_astral);
ACMD_DECLARE(do_assense);
ACMD_DECLARE(do_at);
ACMD_DECLARE(do_attach);
ACMD_DECLARE(do_award);
ACMD_DECLARE(do_availoffset);
ACMD_DECLARE(do_audit);
ACMD_DECLARE(do_backstab);
ACMD_DECLARE(do_ban);
ACMD_DECLARE(do_banish);
ACMD_DECLARE(do_bash);
ACMD_DECLARE(do_bioware);
ACMD_DECLARE(do_bond);
ACMD_DECLARE(do_boost);
ACMD_DECLARE(do_break);
ACMD_DECLARE(do_broadcast);
ACMD_DECLARE(do_build);
ACMD_DECLARE(do_cast);
ACMD_DECLARE(do_chipload);
ACMD_DECLARE(do_cleanup);
ACMD_DECLARE(do_cleanse);
ACMD_DECLARE(do_closecombat);
ACMD_DECLARE(do_commands);
ACMD_DECLARE(do_compact);
ACMD_DECLARE(do_conjure);
ACMD_DECLARE(do_consider);
ACMD_DECLARE(do_contest);
ACMD_DECLARE(do_control);
ACMD_DECLARE(do_copy);
ACMD_DECLARE(do_cook);
ACMD_DECLARE(do_costtime);
ACMD_DECLARE(do_cpool);
ACMD_DECLARE(do_crack);
ACMD_DECLARE(do_crash_mud);
ACMD_DECLARE(do_create);
ACMD_DECLARE(do_credits);
ACMD_DECLARE(do_customize);
ACMD_DECLARE(do_cyberware);
ACMD_DECLARE(do_coredump);
ACMD_DECLARE(do_date);
ACMD_DECLARE(do_dc);
ACMD_DECLARE(do_deactivate);
ACMD_DECLARE(do_decline);
ACMD_DECLARE(do_decorate);
ACMD_DECLARE(do_default);
ACMD_DECLARE(do_delete);
ACMD_DECLARE(do_destroy);
ACMD_DECLARE(do_destring);
ACMD_DECLARE(do_dice);
ACMD_DECLARE(do_dig);
ACMD_DECLARE(do_diagnose);
ACMD_DECLARE(do_die);
ACMD_DECLARE(do_discord);
ACMD_DECLARE(do_dispell);
ACMD_DECLARE(do_display);
ACMD_DECLARE(do_domain);
ACMD_DECLARE(do_drag);
ACMD_DECLARE(do_drink);
ACMD_DECLARE(do_drive);
ACMD_DECLARE(do_drop);
ACMD_DECLARE(do_eat);
ACMD_DECLARE(do_echo);
ACMD_DECLARE(do_new_echo);
ACMD_DECLARE(do_eject);
ACMD_DECLARE(do_elemental);
ACMD_DECLARE(do_enter);
ACMD_DECLARE(do_endrun);
ACMD_DECLARE(do_equipment);
ACMD_DECLARE(do_examine);
ACMD_DECLARE(do_exit);
ACMD_DECLARE(do_exits);
ACMD_DECLARE(do_flee);
ACMD_DECLARE(do_flip);
ACMD_DECLARE(do_focus);
ACMD_DECLARE(do_follow);
ACMD_DECLARE(do_force);
ACMD_DECLARE(do_forceget);
ACMD_DECLARE(do_forceput);
ACMD_DECLARE(do_forget);
ACMD_DECLARE(do_fuckups);
ACMD_DECLARE(do_gecho);
ACMD_DECLARE(do_gen_comm);
ACMD_DECLARE(do_gen_door);
ACMD_DECLARE(do_gen_ps);
ACMD_DECLARE(do_gen_write);
ACMD_DECLARE(do_get);
ACMD_DECLARE(do_give);
ACMD_DECLARE(do_gold);
ACMD_DECLARE(do_goto);
ACMD_DECLARE(do_grab);
ACMD_DECLARE(do_group);
ACMD_DECLARE(do_hail);
ACMD_DECLARE(do_hcontrol);
ACMD_DECLARE(do_heal);
ACMD_DECLARE(do_help);
ACMD_DECLARE(do_hide);
ACMD_DECLARE(do_hit);
ACMD_DECLARE(do_highlight);
ACMD_DECLARE(do_house);
ACMD_DECLARE(do_hp);
ACMD_DECLARE(do_iclist);
ACMD_DECLARE(do_ignore);
ACMD_DECLARE(do_ilist);
ACMD_DECLARE(do_vlist);
ACMD_DECLARE(do_iload);
ACMD_DECLARE(do_imagelink);
ACMD_DECLARE(do_info);
ACMD_DECLARE(do_initiate);
ACMD_DECLARE(do_insult);
ACMD_DECLARE(do_inventory);
ACMD_DECLARE(do_invis);
ACMD_DECLARE(do_jack);
ACMD_DECLARE(do_keep);
ACMD_DECLARE(do_keepalive);
ACMD_DECLARE(do_kick);
ACMD_DECLARE(do_kil);
ACMD_DECLARE(do_kill);
ACMD_DECLARE(do_last);
ACMD_DECLARE(do_leaderboard);
ACMD_DECLARE(do_learn);
ACMD_DECLARE(do_leave);
ACMD_DECLARE(do_lay);
ACMD_DECLARE(do_link);
ACMD_DECLARE(do_look);
ACMD_DECLARE(do_logwatch);
ACMD_DECLARE(do_manifest);
ACMD_DECLARE(do_map);
ACMD_DECLARE(do_masking);
ACMD_DECLARE(do_makenerps);
ACMD_DECLARE(do_memory);
ACMD_DECLARE(do_message_history);
ACMD_DECLARE(do_metamagic);
ACMD_DECLARE(do_mode);
ACMD_DECLARE(do_move);
ACMD_DECLARE(do_mlist);
ACMD_DECLARE(do_echo);
ACMD_DECLARE(do_nervestrike);
ACMD_DECLARE(do_not_here);
ACMD_DECLARE(do_oocdisable);
ACMD_DECLARE(do_order);
ACMD_DECLARE(do_packup);
ACMD_DECLARE(do_page);
ACMD_DECLARE(do_patch);
ACMD_DECLARE(do_perfmon);
ACMD_DECLARE(do_pgroup);
ACMD_DECLARE(do_photo);
ACMD_DECLARE(do_playerrolls);
ACMD_DECLARE(do_pockets);
ACMD_DECLARE(do_poofset);
ACMD_DECLARE(do_pour);
ACMD_DECLARE(do_pool);
ACMD_DECLARE(do_pop);
ACMD_DECLARE(do_position);
ACMD_DECLARE(do_powerdown);
ACMD_DECLARE(do_practice);
ACMD_DECLARE(do_program);
ACMD_DECLARE(do_progress);
ACMD_DECLARE(do_prone);
ACMD_DECLARE(do_purge);
ACMD_DECLARE(do_push);
ACMD_DECLARE(do_put);
ACMD_DECLARE(do_qedit);
ACMD_DECLARE(do_qlist);
ACMD_DECLARE(do_quit);
ACMD_DECLARE(do_radio);
ACMD_DECLARE(do_ram);
ACMD_DECLARE(do_recap);
ACMD_DECLARE(do_ready);
ACMD_DECLARE(do_reboot);
ACMD_DECLARE(do_reflex);
ACMD_DECLARE(do_register);
ACMD_DECLARE(do_release);
ACMD_DECLARE(do_reload);
ACMD_DECLARE(do_remember);
ACMD_DECLARE(do_remove);
ACMD_DECLARE(do_repair);
ACMD_DECLARE(do_relieve);
ACMD_DECLARE(do_rent);
ACMD_DECLARE(do_reply);
ACMD_DECLARE(do_report);
ACMD_DECLARE(do_rescue);
ACMD_DECLARE(do_rest);
ACMD_DECLARE(do_restring);
ACMD_DECLARE(do_restore);
ACMD_DECLARE(do_retract);
ACMD_DECLARE(do_return);
ACMD_DECLARE(do_rewrite_world);
ACMD_DECLARE(do_rlist);
ACMD_DECLARE(do_rig);
ACMD_DECLARE(do_room);
ACMD_DECLARE(do_say);
ACMD_DECLARE(do_save);
ACMD_DECLARE(do_scan);
ACMD_DECLARE(do_score);
ACMD_DECLARE(do_self_advance);
ACMD_DECLARE(do_search);
ACMD_DECLARE(do_send);
ACMD_DECLARE(do_set);
ACMD_DECLARE(do_settime);
ACMD_DECLARE(do_setfind);
ACMD_DECLARE(do_shedit);
ACMD_DECLARE(do_shopfind);
ACMD_DECLARE(do_shoot);
ACMD_DECLARE(do_show);
ACMD_DECLARE(do_shutdown);
ACMD_DECLARE(do_sit);
ACMD_DECLARE(do_skills);
ACMD_DECLARE(do_skillset);
ACMD_DECLARE(do_sleep);
ACMD_DECLARE(do_slist);
ACMD_DECLARE(do_slowns);
ACMD_DECLARE(do_sneak);
ACMD_DECLARE(do_snoop);
ACMD_DECLARE(do_spec_comm);
ACMD_DECLARE(do_spells);
ACMD_DECLARE(do_spellset);
ACMD_DECLARE(do_spray);
ACMD_DECLARE(do_spool);
ACMD_DECLARE(do_stand);
ACMD_DECLARE(do_stat);
ACMD_DECLARE(do_status);
ACMD_DECLARE(do_steal);
ACMD_DECLARE(do_stop);
ACMD_DECLARE(do_stuck);
ACMD_DECLARE(do_survey);
ACMD_DECLARE(do_switch);
ACMD_DECLARE(do_switched_message_history);
ACMD_DECLARE(do_syspoints);
ACMD_DECLARE(do_tail);
ACMD_DECLARE(do_teleport);
ACMD_DECLARE(do_tell);
ACMD_DECLARE(do_think);
ACMD_DECLARE(do_throw);
ACMD_DECLARE(do_time);
ACMD_DECLARE(do_title);
ACMD_DECLARE(do_karma);
ACMD_DECLARE(do_toggle);
ACMD_DECLARE(do_tow);
ACMD_DECLARE(do_track);
ACMD_DECLARE(do_trade);
ACMD_DECLARE(do_train);
ACMD_DECLARE(do_trans);
ACMD_DECLARE(do_treat);
ACMD_DECLARE(do_tridlog);
ACMD_DECLARE(do_type);
ACMD_DECLARE(do_unattach);
ACMD_DECLARE(do_unban);
ACMD_DECLARE(do_unbond);
ACMD_DECLARE(do_ungroup);
ACMD_DECLARE(do_unpack);
ACMD_DECLARE(do_upgrade);
ACMD_DECLARE(do_use);
ACMD_DECLARE(do_users);
ACMD_DECLARE(do_vset);
ACMD_DECLARE(do_vemote);
ACMD_DECLARE(do_visible);
ACMD_DECLARE(do_vfind);
ACMD_DECLARE(do_vnum);
ACMD_DECLARE(do_vstat);
ACMD_DECLARE(do_wake);
ACMD_DECLARE(do_watch);
ACMD_DECLARE(do_wear);
ACMD_DECLARE(do_weather);
ACMD_DECLARE(do_where);
ACMD_DECLARE(do_wheresmycar);
ACMD_DECLARE(do_who);
ACMD_DECLARE(do_wield);
ACMD_DECLARE(do_wimpy);
ACMD_DECLARE(do_wizhelp);
ACMD_DECLARE(do_wizload);
ACMD_DECLARE(do_wizlock);
ACMD_DECLARE(do_wiztell);
ACMD_DECLARE(do_wizfeel);
ACMD_DECLARE(do_wizpossess);
ACMD_DECLARE(do_wiztitle);
ACMD_DECLARE(do_wizwho);
ACMD_DECLARE(do_wizutil);
ACMD_DECLARE(do_zreset);
ACMD_DECLARE(do_zone);
ACMD_DECLARE(do_vedit);
ACMD_DECLARE(do_iedit);
ACMD_DECLARE(do_iclone);
ACMD_DECLARE(do_idelete);
ACMD_DECLARE(do_redit);
ACMD_DECLARE(do_rclone);
ACMD_DECLARE(do_rdelete);
ACMD_DECLARE(do_medit);
ACMD_DECLARE(do_mclone);
ACMD_DECLARE(do_mdelete);
ACMD_DECLARE(do_exclaim);
ACMD_DECLARE(do_zecho);
ACMD_DECLARE(do_zedit);
ACMD_DECLARE(do_zdelete);
ACMD_DECLARE(do_zlist);
ACMD_DECLARE(do_index);
ACMD_DECLARE(do_zswitch);
ACMD_DECLARE(do_penalize);
ACMD_DECLARE(do_holster);
ACMD_DECLARE(do_draw);
ACMD_DECLARE(do_copyover);
ACMD_DECLARE(do_language);
ACMD_DECLARE(do_subscribe);
ACMD_DECLARE(do_subpoint);
ACMD_DECLARE(do_driveby);
ACMD_DECLARE(do_speed);
ACMD_DECLARE(do_incognito);
ACMD_DECLARE(do_chase);
ACMD_DECLARE(do_phone);
ACMD_DECLARE(do_phonelist);
ACMD_DECLARE(do_mount);
ACMD_DECLARE(do_man);
ACMD_DECLARE(do_target);
ACMD_DECLARE(do_vteleport);
ACMD_DECLARE(do_gridguide);
ACMD_DECLARE(do_quickwho);
ACMD_DECLARE(do_hedit);
ACMD_DECLARE(do_icedit);
ACMD_DECLARE(do_connect);
ACMD_DECLARE(do_hlist);
ACMD_DECLARE(do_software);
ACMD_DECLARE(do_design);
ACMD_DECLARE(do_invitations);
ACMD_DECLARE(do_debug);
ACMD_DECLARE(do_helpedit);

struct command_info cmd_info[] =
  {
    { "RESERVED", 0, 0, 0, 0, 0
    }
    ,   /* this must be first -- for specprocs */
    /* directions must come before other commands but after RESERVED */
    { "north"      , POS_SITTING, do_move     , 0, SCMD_NORTH, FALSE },
    { "east"       , POS_SITTING, do_move     , 0, SCMD_EAST, FALSE },
    { "south"      , POS_SITTING, do_move     , 0, SCMD_SOUTH, FALSE },
    { "west"       , POS_SITTING, do_move     , 0, SCMD_WEST, FALSE },
    { "up"         , POS_SITTING, do_move     , 0, SCMD_UP, FALSE },
    { "down"       , POS_SITTING, do_move     , 0, SCMD_DOWN, FALSE },
    { "ne"         , POS_SITTING, do_move     , 0, SCMD_NORTHEAST, FALSE },
    { "se"         , POS_SITTING, do_move     , 0, SCMD_SOUTHEAST, FALSE },
    { "sw"         , POS_SITTING, do_move     , 0, SCMD_SOUTHWEST, FALSE },
    { "nw"         , POS_SITTING, do_move     , 0, SCMD_NORTHWEST, FALSE },
    { "northeast"  , POS_SITTING, do_move     , 0, SCMD_NORTHEAST, FALSE },
    { "southeast"  , POS_SITTING, do_move     , 0, SCMD_SOUTHEAST, FALSE },
    { "southwest"  , POS_SITTING, do_move     , 0, SCMD_SOUTHWEST, FALSE },
    { "northwest"  , POS_SITTING, do_move     , 0, SCMD_NORTHWEST, FALSE },

    /* now, the main list -- note that spec-proc commands and socials come after this list. */
    { "abilities"  , POS_SLEEPING, do_skills   , 0, SCMD_ABILITIES, TRUE },
    { "abilityset" , POS_SLEEPING, do_abilityset , LVL_DEVELOPER, 0, FALSE },
    { "activate"   , POS_LYING   , do_activate , 0, 0, FALSE },
    { "aecho"      , POS_SLEEPING, do_new_echo , LVL_ARCHITECT, SCMD_AECHO, FALSE },
    { "accept"     , POS_LYING   , do_accept   , 0, 0, FALSE },

#ifdef DIES_IRAE
    /* The power point for Karma rule was specifically included for players who do not use the advanced magic (initiation) rules.
       It is recommended that this rule be ignored if the initation rules in Magic in the Shadows are also being used.
          -- https://www.shadowrunrpg.com/resources/sr3faq.html#6

      Per the above, addpoint has been disabled.
    */
    // { "addpoint"   , POS_DEAD    , do_initiate , 0, SCMD_POWERPOINT, FALSE },
#else
    { "addpoint"   , POS_DEAD    , do_initiate , 0, SCMD_POWERPOINT, FALSE },
#endif
    { "affects"    , POS_MORTALLYW, do_status   , 0, 0, TRUE },
    { "afk"        , POS_DEAD    , do_afk      , 0, 0, TRUE },
    { "ammo"       , POS_LYING   , do_ammo     , 0, 0, TRUE },
    { "assense"    , POS_LYING   , do_assense  , 0, 0, FALSE },
    { "at"         , POS_DEAD    , do_at       , LVL_EXECUTIVE, 0, FALSE },
    { "attach"     , POS_RESTING , do_attach   , 0, 0, FALSE },
#ifdef SELFADVANCE
    // Allows running an unattended test port where anyone can bump themselves up to level 9.
    { "advance"    , POS_DEAD    , do_self_advance, 0, 0, FALSE },
#else
    { "advance"    , POS_DEAD    , do_advance  , LVL_VICEPRES, 0, FALSE },
#endif
    { "alias"      , POS_DEAD    , do_alias    , 0, 0, TRUE },
    { "answer"     , POS_LYING   , do_phone    , 0, SCMD_ANSWER, FALSE },
    { "assist"     , POS_FIGHTING, do_assist   , 1, 0, FALSE },
    { "ask"        , POS_LYING   , do_spec_comm, 0, SCMD_ASK, FALSE },
    { "award"      , POS_DEAD    , do_award    , LVL_FIXER, 0, FALSE },
    { "audit"      , POS_DEAD    , do_audit    , LVL_BUILDER, 0, FALSE },
    { "authorize"  , POS_DEAD    , do_wizutil  , LVL_ADMIN, SCMD_AUTHORIZE, FALSE },
    { "availoffset", POS_DEAD  , do_availoffset, 0, 0, TRUE },

    { "bond"       , POS_RESTING , do_bond     , 0, 0, FALSE },
    { "ban"        , POS_DEAD    , do_ban      , LVL_EXECUTIVE, 0, FALSE },
    { "banish"     , POS_STANDING, do_banish   , 0, 0, FALSE },
    { "balance"    , POS_LYING   , do_gold     , 0, 0, TRUE },
    { "bioware"    , POS_DEAD    , do_bioware  , 0, 0, TRUE },
    { "block"      , POS_DEAD    , do_ignore   , 0, 0, TRUE },
    { "boost"      , POS_LYING   , do_boost    , 0, 0, FALSE },
    { "break"      , POS_LYING   , do_break    , 0, 0, FALSE },
    { "broadcast"  , POS_LYING   , do_broadcast, 0, 0, TRUE },
    { ","          , POS_LYING   , do_broadcast, 0, 0, TRUE },
    { "build"      , POS_RESTING , do_build    , 0, 0, FALSE },
    { "bug"        , POS_DEAD    , do_gen_write, 0, SCMD_BUG, TRUE },
    { "bypass"     , POS_STANDING, do_gen_door , 0, SCMD_PICK, FALSE },

    { "cast"       , POS_SITTING , do_cast     , 1, 0, FALSE },
    { "call"       , POS_LYING   , do_phone    , 0, SCMD_RING, FALSE },
    { "chase"      , POS_SITTING , do_chase    , 0, 0, FALSE },
    { "cleanse"    , POS_LYING   , do_cleanse  , 0, 0, FALSE },
    { "cleanup"    , POS_SITTING , do_cleanup  , 0, 0, FALSE },
    { "clear"      , POS_DEAD    , do_gen_ps   , 0, SCMD_CLEAR, TRUE },
    { "close"      , POS_SITTING , do_gen_door , 0, SCMD_CLOSE, FALSE },
    { "closecombat", POS_LYING   , do_closecombat, 0, 0, FALSE },
    { "cls"        , POS_DEAD    , do_gen_ps   , 0, SCMD_CLEAR, TRUE },
    { "consider"   , POS_LYING   , do_consider , 0, 0, FALSE },
    { "configure"  , POS_DEAD    , do_toggle   , 0, 0, TRUE },
    { "conjure"    , POS_RESTING , do_conjure  , 0, 0, FALSE },
    { "connect"    , POS_RESTING , do_connect  , 0, 0, FALSE },
    { "contest"    , POS_SITTING , do_contest  , 0, 0, FALSE },
    { "control"    , POS_SITTING , do_control  , 0, 0, FALSE },
    { "combine"    , POS_RESTING , do_put      , 0, 0, FALSE },
    { "complete"   , POS_LYING   , do_recap    , 0, 0, FALSE },
    { "copy"       , POS_SITTING , do_copy     , 0, 0, FALSE },
    { "copyover"   , POS_DEAD    , do_copyover , LVL_ADMIN, 0, FALSE },
    { "commands"   , POS_DEAD    , do_commands , 0, SCMD_COMMANDS, TRUE },
    { "compress"   , POS_LYING   , do_compact  , 0, 0, FALSE },
    { "cook"       , POS_SITTING , do_cook     , 0, 0, FALSE },
    { "costtime"   , POS_DEAD    , do_costtime , 0, 0, TRUE },
    { "coredump"   , POS_DEAD    , do_coredump , LVL_PRESIDENT, 0, FALSE },
    { "cpool"      , POS_DEAD    , do_cpool    , 0, 0, FALSE },
    { "crack"      , POS_RESTING , do_crack    , 0, 0, FALSE },
    { "crashmu"    , POS_STANDING, do_crash_mud, LVL_PRESIDENT, SCMD_NOOP, FALSE },
    { "crashmud"   , POS_STANDING, do_crash_mud, LVL_PRESIDENT, SCMD_BOOM, FALSE },
    { "create"     , POS_LYING   , do_create   , 0, 0, FALSE },
    { "credits"    , POS_DEAD    , do_gen_ps   , 0, SCMD_CREDITS, TRUE },
    { "customize"  , POS_SLEEPING, do_customize, 0, 0, TRUE },
    { "cyberware"  , POS_DEAD    , do_cyberware, 0, 0, TRUE },

    { "date"       , POS_DEAD    , do_date     , 0, SCMD_DATE, TRUE },
    { "dc"         , POS_DEAD    , do_dc       , LVL_EXECUTIVE, 0, FALSE },
    { "deactivate" , POS_RESTING, do_deactivate, 0, 0, FALSE },
    { "debug"      , POS_DEAD    , do_debug    , LVL_PRESIDENT, 0, FALSE },
    { "decline"    , POS_LYING   , do_decline  , 0, 0, FALSE },
    { "decompress" , POS_LYING  , do_compact  , 0, 1, FALSE },
    { "decorate"   , POS_DEAD    , do_decorate , 0, 0, TRUE },
    { "delete"     , POS_SLEEPING, do_delete   , 0, 0, FALSE },
    { "default"    , POS_RESTING , do_default  , 0, 0, FALSE },
//  { "dennis"     , POS_SITTING, do_move     , 0, SCMD_DOWN, FALSE },
    { "design"     , POS_RESTING , do_design   , 0, 0, FALSE },
    { "destroy"    , POS_STANDING, do_destroy  , 0, 0, FALSE },
//    { "destring" , POS_DEAD    , do_destring , 0, 0, FALSE },
    { "diagnose"   , POS_RESTING , do_diagnose , 0, 0, FALSE },
    { "dice"       , POS_DEAD    , do_dice     , 0, 0, FALSE },
    { "die"        , POS_DEAD    , do_die      , 0, 0, FALSE },
    { "dig"        , POS_RESTING , do_dig      , LVL_BUILDER, SCMD_DIG, FALSE },
    { "dispell"    , POS_SITTING , do_dispell  , 0, 0, FALSE },
    { "display"    , POS_DEAD    , do_display  , 0, 0, TRUE },
    { "discord"    , POS_DEAD    , do_discord  , 0, 0, TRUE },
    { "domain"     , POS_LYING   , do_domain   , 0, 0, FALSE },
    { "donate"     , POS_RESTING , do_drop     , 0, SCMD_DONATE, FALSE },
    { "drag"       , POS_STANDING, do_drag     , 0, 0, FALSE },
    { "drink"      , POS_RESTING , do_drink    , 0, SCMD_DRINK, FALSE },
    { "drive"      , POS_SITTING , do_drive    , 0, 0, FALSE },
    { "drop"       , POS_LYING   , do_drop     , 0, SCMD_DROP, FALSE },
    { "draw"       , POS_RESTING , do_draw     , 0, 0, FALSE },
    { "driveby"    , POS_SITTING , do_driveby  , 0, 0, FALSE },

    { "eat"        , POS_RESTING , do_eat      , 0, SCMD_EAT, FALSE },
    { "echo"       , POS_SLEEPING, do_new_echo , 0, SCMD_ECHO, FALSE },
    { "eject"      , POS_RESTING , do_eject    , 0, 0, FALSE },
    { "elementals" , POS_DEAD    , do_elemental, 0, 0, FALSE },
    { "emote"      , POS_LYING   , do_new_echo , 0, SCMD_EMOTE, FALSE },
    { ":"          , POS_LYING   , do_new_echo , 0, SCMD_EMOTE, FALSE },
    { "enter"      , POS_SITTING , do_enter    , 0, 0, FALSE },
    { "endrun"     , POS_RESTING , do_endrun   , 0, 0, FALSE },
    { "equipment"  , POS_SLEEPING, do_equipment, 0, 0, TRUE },
    { "exits"      , POS_LYING   , do_exits    , 0, SCMD_LONGEXITS, TRUE },
    { "examine"    , POS_RESTING , do_examine  , 0, SCMD_EXAMINE, TRUE },
    { "exclaim"    , POS_LYING   , do_exclaim  , 0, 0, FALSE },
    { "extend"     , POS_SITTING , do_retract  , 0, 0, FALSE },

    { "force"      , POS_SLEEPING, do_force    , LVL_CONSPIRATOR, 0, FALSE },
    { "forceget"   , POS_SLEEPING, do_forceget , LVL_PRESIDENT, 0, FALSE },
    { "forceput"   , POS_SLEEPING, do_forceput , LVL_PRESIDENT, 0, FALSE },
    { "forget"     , POS_DEAD    , do_forget   , 0, 0, FALSE },
    { "fill"       , POS_SITTING , do_pour     , 0, SCMD_FILL, FALSE },
    { "finger"     , POS_DEAD    , do_last     , 0, SCMD_FINGER, TRUE },
    { "fix"        , POS_SITTING , do_repair   , 0, 0, FALSE },
    { "flee"       , POS_FIGHTING, do_flee     , 0, 0, FALSE },
    { "flip"       , POS_SITTING , do_flip     , 0, 0, FALSE },
    { "focus"      , POS_RESTING , do_focus    , 0, 0, FALSE },
    { "follow"     , POS_RESTING , do_follow   , 0, 0, FALSE },
    { "freeze"     , POS_DEAD    , do_wizutil  , LVL_FREEZE, SCMD_FREEZE, FALSE },
    { "fuckups"    , POS_DEAD    , do_fuckups  , LVL_ADMIN, 0, FALSE },

    { "get"        , POS_RESTING , do_get      , 0, 0, FALSE },
    { "gaecho"     , POS_DEAD    , do_gecho    , LVL_CONSPIRATOR, SCMD_AECHO, FALSE },
    { "gecho"      , POS_DEAD    , do_gecho    , LVL_CONSPIRATOR, 0, FALSE },
    { "give"       , POS_RESTING , do_give     , 0, 0, FALSE },
    { "goto"       , POS_MORTALLYW, do_goto     , LVL_BUILDER, 0, FALSE },
    { "group"      , POS_RESTING , do_group    , 1, 0, FALSE },
    { "grab"       , POS_RESTING , do_grab     , 0, 0, FALSE },
    { "gridguide"  , POS_RESTING , do_gridguide, 0, 0, FALSE },

    { "help"       , POS_DEAD    , do_help     , 0, 0, TRUE },
    { "hail"       , POS_STANDING, do_hail     , 0, 0, FALSE },
    { "hangup"     , POS_LYING   , do_phone    , 0, SCMD_HANGUP, FALSE },
    { "handbook"   , POS_DEAD    , do_gen_ps   , LVL_BUILDER, SCMD_HANDBOOK, FALSE },
    { "hcontrol"   , POS_DEAD    , do_hcontrol , LVL_CONSPIRATOR, 0, FALSE },
    { "heal"       , POS_STANDING, do_heal     , 0, 0, FALSE },
    { "hedit"      , POS_DEAD    , do_hedit    , LVL_BUILDER, 0, FALSE },
    { "helpedit"   , POS_DEAD    , do_helpedit , LVL_FIXER, 0, FALSE },
    { "helpexport" , POS_DEAD    , do_helpexport, LVL_PRESIDENT, 0, FALSE },
    { "hit"        , POS_FIGHTING, do_hit      , 0, SCMD_HIT, FALSE },
    { "highlight"  , POS_DEAD    , do_highlight, 0, 0, TRUE },
    { "history"    , POS_DEAD    , do_message_history, 0, 0, TRUE },
    { "hlist"      , POS_DEAD    , do_hlist    , LVL_BUILDER, 0, FALSE },
    { "hold"       , POS_RESTING , do_grab     , 1, 0, FALSE },
    { "holster"    , POS_RESTING , do_holster  , 0, 0, FALSE },
    { "house"      , POS_LYING   , do_house    , 0, 0, FALSE },
    { "ht"         , POS_DEAD    , do_gen_comm , 0, SCMD_HIREDTALK, FALSE },
    { "hts"        , POS_DEAD    , do_switched_message_history, 0, COMM_CHANNEL_HIRED, TRUE },
    { "hp"         , POS_DEAD    , do_hp       , 0, 0, TRUE },

    { "inventory"  , POS_DEAD    , do_inventory, 0, 0, TRUE },
    { "install"    , POS_RESTING , do_put      , 0, SCMD_INSTALL, FALSE },
    { "icedit"     , POS_DEAD    , do_icedit   , LVL_BUILDER, 0, FALSE },
    { "iclist"     , POS_DEAD    , do_iclist   , LVL_BUILDER, 0, FALSE },
    { "iclone"     , POS_DEAD    , do_iclone   , LVL_BUILDER, 0, FALSE },
    { "identify"   , POS_RESTING , do_examine  , 0, SCMD_PROBE, TRUE }, // deliberately out of order to guard idea
    { "idea"       , POS_DEAD    , do_gen_write, 0, SCMD_IDEA, TRUE },
    //{ "idelete"  , POS_DEAD    , do_idelete  , LVL_PRESIDENT, 0, FALSE },
    { "iedit"      , POS_DEAD    , do_iedit    , LVL_BUILDER, 0, FALSE },
    { "ignore"     , POS_DEAD    , do_ignore   , 0, 0, TRUE },
    { "ilist"      , POS_DEAD    , do_ilist    , LVL_BUILDER, 0, FALSE },
    { "iload"      , POS_DEAD    , do_iload    , LVL_BUILDER, 0, FALSE },
    { "imotd"      , POS_DEAD    , do_gen_ps   , LVL_BUILDER, SCMD_IMOTD, FALSE },
    { "immlist"    , POS_DEAD    , do_gen_ps   , 0, SCMD_IMMLIST, TRUE },
    { "incognito"  , POS_DEAD    , do_incognito, LVL_BUILDER, 0, FALSE },
    { "index"      , POS_SLEEPING, do_index    , 0, 0, TRUE },
    { "info"       , POS_SLEEPING, do_gen_ps   , 0, SCMD_INFO, TRUE },
    { "initiate"   , POS_DEAD    , do_initiate , 0, SCMD_INITIATE, FALSE },
    { "insult"     , POS_LYING   , do_insult   , 0, 0, FALSE },
    { "invis"      , POS_DEAD    , do_invis    , LVL_BUILDER, 0, FALSE },
    { "invitations", POS_LYING , do_invitations, 0, 0, FALSE },

    { "jack"       , POS_SITTING , do_jack     , 0, 0, FALSE },
    { "jobs"       , POS_DEAD    , do_recap    , 0, 0, FALSE },
    { "junk"       , POS_RESTING , do_drop     , 0, SCMD_JUNK, FALSE },

    { "keep"       , POS_LYING   , do_keep     , 0, 0, FALSE },
    { "keepalive"  , POS_DEAD    , do_keepalive, 0, 0, FALSE },
    { "kil"        , POS_FIGHTING, do_kil      , 0, 0, FALSE },
    { "kill"       , POS_FIGHTING, do_kill     , 0, SCMD_KILL, FALSE },
    { "kick"       , POS_STANDING, do_kick     , 0, 0, FALSE },
    { "knock"      , POS_STANDING, do_gen_door , 0, SCMD_KNOCK, FALSE },

    { "look"       , POS_LYING   , do_look     , 0, SCMD_LOOK, TRUE },
    { "lay"        , POS_RESTING , do_lay      , 0, 0, FALSE },
    { "language"   , POS_DEAD    , do_language , 0, 0, TRUE },
    { "last"       , POS_DEAD    , do_last     , LVL_BUILDER, 0, FALSE },
    { "leaderboards", POS_DEAD  , do_leaderboard, LVL_BUILDER, 0, FALSE },
    { "learn"      , POS_RESTING , do_learn    , 0, 0, FALSE },
    { "leave"      , POS_SITTING , do_leave    , 0, 0, FALSE },
    { "link"       , POS_SLEEPING, do_link     , 0, 0, FALSE },
    { "lock"       , POS_SITTING , do_gen_door , 0, SCMD_LOCK, FALSE },
    { "load"       , POS_RESTING , do_chipload , 0, 0, FALSE },
    { "logwatch"   , POS_DEAD    , do_logwatch , LVL_BUILDER, 0, FALSE },

    { "man"        , POS_SITTING , do_man      , 0, 0, FALSE },
    { "manifest"   , POS_RESTING , do_manifest , 0, 0, FALSE },
    { "map"        , POS_DEAD    , do_map      , 0, 0, FALSE },
    { "memory"     , POS_SLEEPING, do_memory   , 0, 0, FALSE },
    { "metamagic"  , POS_DEAD    , do_metamagic, 0, 0, FALSE },
    { "mclone"     , POS_DEAD    , do_mclone   , LVL_BUILDER, 0, FALSE },
    { "mdelete"    , POS_DEAD    , do_mdelete  , LVL_PRESIDENT, 0, FALSE },
    { "medit"      , POS_DEAD    , do_medit    , LVL_BUILDER, 0, FALSE },
    { "mlist"      , POS_DEAD    , do_mlist    , LVL_BUILDER, 0, FALSE },
    { "mode"       , POS_LYING   , do_mode     , 0, 0, FALSE },
    { "motd"       , POS_DEAD    , do_gen_ps   , 0, SCMD_MOTD, FALSE },
    { "mount"      , POS_RESTING , do_mount    , 0, 0, FALSE },
    { "makenerps"  , POS_SLEEPING, do_makenerps, LVL_FIXER, 0, FALSE },
    { "mask"       , POS_RESTING , do_masking  , 0, 0, FALSE },
    { "mute"       , POS_DEAD    , do_wizutil  , LVL_FREEZE, SCMD_SQUELCH, FALSE },
    { "muteooc"    , POS_DEAD    , do_wizutil  , LVL_FREEZE, SCMD_SQUELCHOOC, FALSE },
    { "mutetells"  , POS_DEAD    , do_wizutil  , LVL_FREEZE, SCMD_SQUELCHTELLS, FALSE },
    { "mutenewbie" , POS_DEAD    , do_wizutil  , LVL_FREEZE, SCMD_MUTE_NEWBIE, FALSE },
    { "murder"     , POS_FIGHTING, do_hit      , 0, SCMD_MURDER, FALSE },

    { "newbie"     , POS_DEAD    , do_gen_comm , 0, SCMD_NEWBIE, FALSE },
    { "newbies"    , POS_DEAD    , do_switched_message_history, 0, COMM_CHANNEL_NEWBIE, TRUE },
    { "news"       , POS_SLEEPING, do_gen_ps   , 0, SCMD_NEWS, TRUE },
    { "nervestrike", POS_DEAD    , do_nervestrike, 0, 0, FALSE },
    { "notitle"    , POS_DEAD    , do_wizutil  , LVL_FREEZE, SCMD_NOTITLE, FALSE },

    { "order"      , POS_LYING   , do_order    , 1, 0, FALSE },
    { "olc"        , POS_DEAD    , do_olcon    , LVL_DEVELOPER, 0, FALSE },
    { "ooc"        , POS_DEAD    , do_gen_comm , 0, SCMD_OOC, TRUE },
    { "oocs"       , POS_DEAD    , do_switched_message_history, 0, COMM_CHANNEL_OOC, TRUE },
    { "oocdisable" , POS_DEAD    , do_oocdisable, LVL_DEVELOPER, 0, FALSE },
    { "open"       , POS_SITTING , do_gen_door , 0, SCMD_OPEN, FALSE },
    { "osay"       , POS_LYING   , do_say      , 0, SCMD_OSAY, TRUE },
    { "osays"      , POS_DEAD    , do_switched_message_history, 0, COMM_CHANNEL_OSAYS, TRUE },
    { "out"        , POS_SITTING , do_leave    , 0, 0, FALSE },
    { "."          , POS_LYING   , do_say      , 0, SCMD_OSAY, TRUE },

    { "put"        , POS_RESTING , do_put      , 0, 0, FALSE },
    { "packup"     , POS_SITTING , do_packup   , 0, 0, FALSE },
    { "patch"      , POS_LYING   , do_patch    , 0, 0, FALSE },
    { "page"       , POS_DEAD    , do_page     , LVL_ARCHITECT, 0, FALSE },
    { "pages"      , POS_DEAD    , do_switched_message_history, LVL_ARCHITECT, COMM_CHANNEL_PAGES, TRUE },
    { "pardon"     , POS_DEAD    , do_wizutil  , LVL_CONSPIRATOR, SCMD_PARDON, FALSE },
    { "penalize"   , POS_DEAD    , do_penalize , LVL_FIXER, 0, FALSE },
    { "perceive"   , POS_LYING   , do_astral   , 0, SCMD_PERCEIVE, FALSE },
    { "perfmon"    , POS_DEAD    , do_perfmon  , LVL_ADMIN, 0, FALSE },
    { "pgroup"     , POS_LYING   , do_pgroup   , 0, 0, FALSE },
    { "phone"      , POS_LYING   , do_phone    , 0, 0, FALSE },
    { "phonelist"  , POS_DEAD    , do_phonelist, LVL_BUILDER, 0, FALSE },
    { "photo"      , POS_RESTING , do_photo    , 0, 0, FALSE },
    { "pockets"    , POS_RESTING , do_pockets  , 0, 0, TRUE },
    { "pop"        , POS_SITTING , do_pop      , 0, 0, FALSE },
    { "policy"     , POS_DEAD    , do_gen_ps   , 0, SCMD_POLICIES, TRUE },
    { "poofin"     , POS_DEAD    , do_poofset  , LVL_BUILDER, SCMD_POOFIN, FALSE },
    { "poofout"    , POS_DEAD    , do_poofset  , LVL_BUILDER, SCMD_POOFOUT, FALSE },
    { "pools"      , POS_DEAD    , do_pool     , 0, 0, FALSE },
    { "pour"       , POS_SITTING , do_pour     , 0, SCMD_POUR, FALSE },
    { "position"   , POS_DEAD    , do_position , 0, 0, TRUE },
    { "possess"    , POS_DEAD    , do_wizpossess, LVL_FIXER, 0, FALSE },
    { "powerdown"  , POS_DEAD    , do_powerdown, 0, 0, FALSE },
    { "press"      , POS_SITTING , do_push     , 0, 0, FALSE },
    { "prompt"     , POS_DEAD    , do_display  , 0, 0, TRUE },
    { "project"    , POS_LYING   , do_astral   , 0, SCMD_PROJECT, FALSE },
    { "pretitle"   , POS_DEAD    , do_wiztitle , 0, SCMD_PRETITLE, TRUE },
    { "practice"   , POS_RESTING , do_practice , 1, 0, FALSE },
    { "probe"      , POS_RESTING , do_examine  , 0, SCMD_PROBE, TRUE },
    { "program"    , POS_RESTING , do_program  , 0, 0, FALSE },
    { "progress"   , POS_RESTING , do_progress , 0, 0, TRUE },
    { "prone"      , POS_FIGHTING, do_prone    , 0, 0, FALSE },
    { "praise"     , POS_DEAD    , do_gen_write, 0, SCMD_PRAISE, TRUE },
    { "push"       , POS_SITTING , do_push     , 0, 0, FALSE },
    { "playerrolls", POS_DEAD    , do_playerrolls, LVL_PRESIDENT, 0, FALSE },
  #ifdef IS_BUILDPORT
    { "purge"      , POS_DEAD    , do_purge    , LVL_BUILDER, 0, FALSE },
  #else
    { "purge"      , POS_DEAD    , do_purge    , LVL_ARCHITECT, 0, FALSE },
  #endif

    { "quests"     , POS_DEAD    , do_recap    , 0, 0, TRUE },
    { "ql"         , POS_LYING   , do_look     , 0, SCMD_QUICKLOOK, TRUE },
    { "qui"        , POS_DEAD    , do_quit     , 0, 0, FALSE },
    { "quit"       , POS_SLEEPING, do_quit     , 0, SCMD_QUIT, FALSE },
    { "quicklook"  , POS_LYING   , do_look     , 0, SCMD_QUICKLOOK, TRUE },
    { "quickwho"   , POS_DEAD    , do_quickwho , 0, 0, TRUE },
    { "qwho"       , POS_DEAD    , do_quickwho , 0, 0, TRUE },
    { "qlist"      , POS_DEAD    , do_qlist    , LVL_FIXER, 0, FALSE },
    { "qedit"      , POS_DEAD    , do_qedit    , LVL_FIXER, 0, FALSE },

    { "reply"      , POS_LYING   , do_reply    , 0, 0, TRUE },
    { "ram"        , POS_SITTING , do_ram      , 0, 0, FALSE },
    { "radio"      , POS_LYING   , do_radio    , 0, 0, FALSE },
    { "rig"        , POS_SITTING , do_rig      , 0, 0, FALSE },
    { "rt"         , POS_DEAD    , do_gen_comm , 0, SCMD_RPETALK, FALSE },
    { "rts"        , POS_DEAD    , do_switched_message_history, 0, COMM_CHANNEL_RPE, TRUE },
    { "rclone"     , POS_DEAD    , do_rclone   , LVL_BUILDER, 0, FALSE },
    { "rdelete"    , POS_DEAD    , do_rdelete  , LVL_PRESIDENT, 0, FALSE },
    { "reflex"     , POS_RESTING , do_reflex   , 0, 0, FALSE },
    { "register"   , POS_DEAD    , do_register , 0, 0, TRUE },
    { "rest"       , POS_LYING   , do_rest     , 0, 0, FALSE },
    { "read"       , POS_LYING   , do_look     , 0, SCMD_READ, TRUE },
    { "ready"      , POS_LYING   , do_ready    , 0, 0, FALSE },
    { "reboot"     , POS_DEAD    , do_reboot   , LVL_DEVELOPER, 0, FALSE },
    { "recap"      , POS_DEAD    , do_recap    , 0, 0, TRUE },
    { "reload"     , POS_RESTING , do_reload   , 0, 0, FALSE },
    { "release"    , POS_LYING   , do_release  , 0, 0, FALSE },
    { "relieve"    , POS_STANDING, do_relieve  , 0, 0, FALSE },
    { "remove"     , POS_RESTING , do_remove   , 0, 0, FALSE },
    { "remember"   , POS_LYING   , do_remember , 0, 0, FALSE },
    { "report"     , POS_LYING   , do_report   , 0, 0, FALSE },
    { "repair"     , POS_SITTING , do_repair   , 0, 0, FALSE },
    { "restore"    , POS_DEAD    , do_restore  , 1, 0, FALSE },
    { "restring"   , POS_DEAD    , do_restring , 0, 0, FALSE },
    { "retract"    , POS_SITTING , do_retract  , 0, 0, FALSE },
    { "return"     , POS_DEAD    , do_return   , 0, 0, FALSE },
    { "rlist"      , POS_DEAD    , do_rlist    , LVL_BUILDER, 0, FALSE },
    { "room"       , POS_DEAD    , do_room     , LVL_BUILDER, 0, FALSE },
    { "roll"       , POS_DEAD    , do_dice     , 0, 0, FALSE },
    { "rpe"        , POS_DEAD    , do_wizutil  , LVL_ADMIN, SCMD_RPE, FALSE },
    { "rpetalk"    , POS_DEAD    , do_gen_comm , 0, SCMD_RPETALK, FALSE },
    { "redit"      , POS_DEAD    , do_redit    , LVL_BUILDER, 0, FALSE },
    { "rewrite_worl",  POS_DEAD, do_rewrite_world, LVL_PRESIDENT, 0, FALSE },
    { "rewrite_world", POS_DEAD, do_rewrite_world, LVL_PRESIDENT, 1, FALSE },

    { "say"        , POS_LYING   , do_say      , 0, SCMD_SAY, FALSE },
    { "says"       , POS_DEAD    , do_switched_message_history, 0, COMM_CHANNEL_SAYS, TRUE },
    { "'"          , POS_LYING   , do_say      , 0, SCMD_SAY, FALSE },
    { "sayto"      , POS_LYING   , do_say      , 0, SCMD_SAYTO, FALSE },
    { "\""         , POS_LYING   , do_say      , 0, SCMD_SAYTO, FALSE },
    { "save"       , POS_DEAD    , do_save     , 0, 0, TRUE },
    { "score"      , POS_DEAD    , do_score    , 0, 0, TRUE },
    { "scan"       , POS_RESTING , do_scan     , 0, 0, FALSE },
    { "search"     , POS_STANDING, do_search   , 0, 0, FALSE },
    { "send"       , POS_SLEEPING, do_send     , LVL_FIXER, 0, FALSE },
    { "sedit"      , POS_DEAD    , do_shedit   , LVL_BUILDER, 0, FALSE },
    { "set"        , POS_DEAD    , do_set      , LVL_CONSPIRATOR, 0, FALSE },
    { "setfind"    , POS_DEAD    , do_setfind , LVL_VICEPRES, 0, FALSE },
    { "settime"    , POS_DEAD    , do_settime  , LVL_DEVELOPER, 0, FALSE },
    { "sheathe"    , POS_RESTING , do_holster  , 0, 0, FALSE },
    { "shortexits" , POS_LYING   , do_exits    , 0, SCMD_SHORTEXITS, FALSE },
    { "shout"      , POS_LYING   , do_gen_comm , 0, SCMD_SHOUT, FALSE },
    { "shouts"     , POS_DEAD    , do_switched_message_history, 0, COMM_CHANNEL_SHOUTS, TRUE },
    { "shoot"      , POS_FIGHTING, do_shoot    , 0, 0, FALSE },
    { "show"       , POS_DEAD    , do_show     , 0, 0, FALSE },
    { "shopfind"   , POS_DEAD    , do_shopfind , LVL_VICEPRES, 0, FALSE },
    { "shutdown"   , POS_RESTING , do_shutdown , 0, SCMD_SHUTDOWN, FALSE },
    { "sip"        , POS_RESTING , do_drink    , 0, SCMD_SIP, FALSE },
    { "sit"        , POS_LYING   , do_sit      , 0, 0, FALSE },
    { "skills"     , POS_SLEEPING, do_skills   , 0, 0, TRUE },
    { "skillset"   , POS_SLEEPING, do_skillset , LVL_DEVELOPER, 0, FALSE },
    { "slist"      , POS_DEAD    , do_slist    , LVL_BUILDER, 0, FALSE },
    { "sleep"      , POS_SLEEPING, do_sleep    , 0, 0, FALSE },
    { "slowns"     , POS_DEAD    , do_slowns   , LVL_DEVELOPER, 0, FALSE },
    { "sneak"      , POS_STANDING, do_sneak    , 1, 0, FALSE },
    { "snoop"      , POS_DEAD    , do_snoop    , LVL_EXECUTIVE, 0, FALSE },
    { "socials"    , POS_DEAD    , do_commands , 0, SCMD_SOCIALS, TRUE },
    { "software"   , POS_LYING   , do_software , 0, 0, FALSE },
    { "spool"      , POS_DEAD    , do_spool    , 0, 0, FALSE },
    { "speed"      , POS_RESTING , do_speed    , 0, 0, FALSE },
    { "spells"     , POS_SLEEPING, do_spells   , 0, 0, TRUE },
    { "spellset"   , POS_SLEEPING, do_spellset , LVL_DEVELOPER, 0, FALSE },
    { "spirits"    , POS_LYING   , do_elemental, 0, 0, TRUE },
    { "spray"      , POS_STANDING, do_spray    , 0, 0, FALSE },
    { "stand"      , POS_LYING   , do_stand    , 0, 0, FALSE },
    { "stat"       , POS_DEAD    , do_stat     , LVL_BUILDER, 0, FALSE },
    { "status"     , POS_LYING   , do_status   , 0, 0, TRUE },
    { "steal"      , POS_LYING   , do_steal    , 0, 0, FALSE },
    { "stop"       , POS_LYING   , do_stop     , 0, 0, FALSE },
    { "stuck"      , POS_LYING   , do_stuck    , 0, 0, FALSE },
    { "subscribe"  , POS_SITTING , do_subscribe, 0, 0, FALSE },
    { "subpoint"   , POS_DEAD    , do_subpoint , LVL_ARCHITECT, 0, FALSE },
    { "survey"     , POS_LYING   , do_survey   , 0, 0, TRUE },
    { "switch"     , POS_SITTING , do_switch   , 0, 0, FALSE },
    { "syspoints"  , POS_DEAD    , do_syspoints, 1, 0, TRUE },

    { "talk"       , POS_LYING   , do_phone    , 0, SCMD_TALK, FALSE },
    { "tell"       , POS_DEAD    , do_tell     , 0, 0, TRUE },
    { "tells"      , POS_DEAD    , do_switched_message_history, 0, COMM_CHANNEL_TELLS, TRUE },
    { "take"       , POS_RESTING , do_get      , 0, 0, FALSE },
    { "target"     , POS_SITTING , do_target   , 0, 0, FALSE },
    { "taste"      , POS_RESTING , do_eat      , 0, SCMD_TASTE, FALSE },
    { "tail"       , POS_DEAD    , do_tail     , LVL_DEVELOPER, 0, FALSE },
    { "teleport"   , POS_DEAD    , do_teleport , LVL_CONSPIRATOR, 0, FALSE },
    { "think"      , POS_LYING   , do_think    , 0, 0, FALSE },
    { "throw"      , POS_FIGHTING, do_throw    , 0, 0, FALSE },
    { "thaw"       , POS_DEAD    , do_wizutil  , LVL_FREEZE, SCMD_THAW, FALSE },
    { "title"      , POS_DEAD    , do_title    , 0, 0, TRUE },
    { "time"       , POS_DEAD    , do_time     , 0, SCMD_NORMAL, TRUE },
    { "tke"        , POS_DEAD    , do_karma    , 0, 0, TRUE },
    { "toggle"     , POS_DEAD    , do_toggle   , 0, 0, FALSE },
    { "tow"        , POS_SITTING , do_tow      , 0, 0, FALSE },
    { "track"      , POS_STANDING, do_track    , 0, 0, FALSE },
    { "tracker"    , POS_RESTING , do_not_here , 0, 0, FALSE },
    { "treat"      , POS_SITTING , do_treat    , 0, 0, FALSE },
    { "trade"      , POS_DEAD    , do_trade    , 0, 0, FALSE },
    { "train"      , POS_STANDING, do_train    , 0, 0, FALSE },
    { "transfer"   , POS_SLEEPING, do_trans    , 0, 0, FALSE },
    { "tridlog"    , POS_DEAD    , do_tridlog  , LVL_FIXER, 0, FALSE },
    { "type"       , POS_STANDING, do_type     , 0, 0, FALSE },
    { "typo"       , POS_DEAD    , do_gen_write, 0, SCMD_TYPO, TRUE },

    { "unban"      , POS_DEAD    , do_unban    , LVL_EXECUTIVE, 0, FALSE },
    { "undig"      , POS_RESTING , do_dig      , LVL_BUILDER, SCMD_UNDIG, FALSE },
    { "ungroup"    , POS_DEAD    , do_ungroup  , 0, 0, FALSE },
    { "uninstall"  , POS_SITTING , do_get      , 0, SCMD_UNINSTALL, FALSE },
    { "unjack"     , POS_SITTING , do_jack     , 0, 1, FALSE },
    { "unkeep"     , POS_LYING   , do_keep     , 0, 0, FALSE },
    { "unlock"     , POS_SITTING , do_gen_door , 0, SCMD_UNLOCK, FALSE },
    { "unlink"     , POS_SLEEPING, do_link     , 0, 1, FALSE },
    { "unbond"     , POS_RESTING , do_unbond   , 0, 0, FALSE },
    { "unaffect"   , POS_DEAD    , do_wizutil  , LVL_EXECUTIVE, SCMD_UNAFFECT, FALSE },
    { "unattach"   , POS_RESTING , do_unattach , 0, 0, FALSE },
    { "unpack"     , POS_SITTING , do_unpack   , 0, 0, FALSE },
    { "unpractice" , POS_RESTING , do_practice, 1, SCMD_UNPRACTICE, FALSE },
    { "unsubscribe",POS_RESTING, do_subscribe, 0, SCMD_UNSUB, FALSE },
    { "untrain"    , POS_RESTING , do_train    , 1, SCMD_UNTRAIN, FALSE },
    { "unlearn"    , POS_DEAD    , do_forget   , 0, 0, FALSE },
    { "upgrade"    , POS_SITTING , do_upgrade  , 0 , 0, FALSE },
    { "uptime"     , POS_DEAD    , do_date     , 0, SCMD_UPTIME, TRUE },
    { "use"        , POS_SITTING , do_use      , 1, SCMD_USE, FALSE },
    { "users"      , POS_DEAD    , do_users    , LVL_BUILDER, 0, FALSE },

    { "version"    , POS_DEAD    , do_gen_ps   , 0, SCMD_VERSION, TRUE },
    { "vemote"     , POS_SLEEPING, do_vemote   , 0 , 0, FALSE },
    { "visible"    , POS_RESTING , do_visible  , LVL_BUILDER, 0, FALSE },
    { "view"       , POS_LYING   , do_imagelink, 0, 0, FALSE },
    { "vfind"      , POS_DEAD    , do_vfind    , LVL_BUILDER, 0, FALSE },
    { "vlist"      , POS_DEAD    , do_vlist    , LVL_BUILDER, 0, FALSE },
    { "vnum"       , POS_DEAD    , do_vnum     , LVL_BUILDER, 0, FALSE },
    { "vset"       , POS_DEAD    , do_vset     , LVL_DEVELOPER, 0, FALSE },
    { "vstat"      , POS_DEAD    , do_vstat    , LVL_BUILDER, 0, FALSE },

    { "wake"       , POS_SLEEPING, do_wake     , 0, 0, FALSE },
    { "watch"      , POS_SITTING , do_watch    , 0, 0, FALSE },
    { "wear"       , POS_RESTING , do_wear     , 0, 0, FALSE },
    { "weather"    , POS_LYING   , do_weather  , 0, 0, TRUE },
    { "who"        , POS_DEAD    , do_who      , 0, 0, TRUE },
    { "whoami"     , POS_DEAD    , do_gen_ps   , 0, SCMD_WHOAMI, TRUE },
    { "whotitle"   , POS_DEAD    , do_wiztitle , LVL_BUILDER, SCMD_WHOTITLE, TRUE },
    { "where"      , POS_DEAD    , do_where    , 1, LVL_PRESIDENT, TRUE }, // todo: why is lvl_president in the scmd slot?
    { "wheresmycar", POS_RESTING , do_wheresmycar, 1, 0, FALSE },
    { "whisper"    , POS_LYING   , do_spec_comm, 0, SCMD_WHISPER, FALSE },
    { "wield"      , POS_RESTING , do_wield    , 0, 0, FALSE },
    { "wimpy"      , POS_DEAD    , do_wimpy    , 0, 0, TRUE },
#ifdef IS_BUILDPORT
    { "wizload"    , POS_RESTING , do_wizload  , LVL_BUILDER, 0, FALSE },
#else
    { "wizload"    , POS_RESTING , do_wizload  , LVL_ADMIN, 0, FALSE },
#endif
    { "wtell"      , POS_DEAD    , do_wiztell  , LVL_BUILDER, 0, FALSE },
    { "wtells"     , POS_DEAD    , do_switched_message_history, LVL_BUILDER, COMM_CHANNEL_WTELLS, TRUE },
    { "wts"        , POS_DEAD    , do_switched_message_history, LVL_BUILDER, COMM_CHANNEL_WTELLS, TRUE },
    { "wf"         , POS_DEAD    , do_wizfeel  , LVL_BUILDER, 0, FALSE },
    { "wizhelp"    , POS_SLEEPING, do_wizhelp  , LVL_BUILDER, 0, FALSE },
    { "wizlist"    , POS_DEAD    , do_gen_ps   , 0, SCMD_IMMLIST, FALSE },
    { "wizlock"    , POS_DEAD    , do_wizlock  , LVL_DEVELOPER, 0, FALSE },
    { "wwho"       , POS_DEAD    , do_wizwho   , LVL_BUILDER, 0, FALSE },

    { "vedit"      , POS_DEAD    , do_vedit    , LVL_BUILDER, 0, FALSE },
    { "vteleport"  , POS_DEAD   , do_vteleport, LVL_CONSPIRATOR, 0, FALSE },

    // The mysterious back door command! (protip: it does nothing)
//  { "xyz"        , POS_STANDING, do_action   , LVL_PRESIDENT, 0, FALSE },

    { "yell"       , POS_LYING   , do_gen_comm , 0, SCMD_SHOUT, FALSE },

    { "zdelete"    , POS_DEAD    , do_zdelete  , LVL_PRESIDENT, 0, FALSE },
    { "zaecho"     , POS_SLEEPING, do_zecho    , LVL_FIXER, SCMD_AECHO, FALSE },
    { "zecho"      , POS_SLEEPING, do_zecho    , LVL_FIXER, 0, FALSE },
    { "zedit"      , POS_DEAD    , do_zedit    , LVL_BUILDER, 0, FALSE },
    { "zlist"      , POS_DEAD    , do_zlist    , LVL_BUILDER, 0, FALSE },
    { "zone"       , POS_DEAD    , do_zone     , LVL_BUILDER, 0, FALSE },
    { "zreset"     , POS_DEAD    , do_zreset   , LVL_BUILDER, 0, FALSE },
    { "zswitch"    , POS_DEAD    , do_zswitch  , LVL_BUILDER, 0, FALSE },
    /* End of functional commands with game-wide implementation.*/

    /* Commands that will only function in the presence of a spec_proc. */
    { "burn"       , POS_STANDING, do_not_here , 0, 0, FALSE },
    { "buy"        , POS_SITTING , do_not_here , 0, 0, FALSE },
    { "check"      , POS_RESTING , do_not_here , 0, 0, FALSE },
    { "collect"    , POS_RESTING , do_not_here , 0, 0, FALSE },
    { "blastoff"   , POS_RESTING , do_not_here , 0, 0, FALSE },
    { "cancel"     , POS_RESTING , do_not_here , 0, 0, FALSE },
    { "climb"      , POS_STANDING, do_not_here , 0, 0, FALSE },
    { "deposit"    , POS_STANDING, do_not_here , 1, 0, FALSE },
    { "hours"      , POS_LYING   , do_not_here , 0, 0, FALSE },
    { "land"       , POS_RESTING , do_not_here , 0, 0, FALSE },
    { "lease"      , POS_RESTING , do_not_here , 1, 0, FALSE },
    { "light"      , POS_STANDING, do_not_here , 0, 0, FALSE },
    { "list"       , POS_RESTING , do_not_here , 0, 0, TRUE },
    { "mail"       , POS_STANDING, do_not_here , 0, 0, FALSE },
    { "offer"      , POS_RESTING , do_not_here , 0, 0, FALSE },
    { "paint"      , POS_RESTING , do_not_here , 0, 0, FALSE },
    { "pay"        , POS_RESTING , do_not_here , 0, 0, FALSE },
    { "pull"       , POS_STANDING, do_not_here , 0, 0, FALSE },
    { "receive"    , POS_STANDING, do_not_here , 1, 0, FALSE },
    { "recharge"   , POS_DEAD    , do_not_here , 0, 0, FALSE },
    { "rent"       , POS_STANDING, do_not_here , 1, 0, FALSE },
    { "retrieve"   , POS_RESTING , do_not_here , 1, 0, FALSE },
    { "sell"       , POS_STANDING, do_not_here , 0, 0, FALSE },
    { "withdraw"   , POS_STANDING, do_not_here , 1, 0, FALSE },
    { "wire"       , POS_STANDING, do_not_here , 1, 0, FALSE },
    { "write"      , POS_RESTING , do_not_here , 0, 0, FALSE },
    { "value"      , POS_STANDING, do_not_here , 0, 0, FALSE },
    /* End of spec-proc commands. */

    /* Socials and other fluff commands. */
    { "agree"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "agree"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "accuse"   , POS_SITTING , do_action   , 0, 0, FALSE },
    { "apologize", POS_LYING   , do_action   , 0, 0, FALSE },
    { "applaud"  , POS_LYING   , do_action   , 0, 0, FALSE },
    // Socials B
    { "bounce"   , POS_STANDING, do_action   , 0, 0, FALSE },
    { "bat"      , POS_RESTING , do_action   , 0, 0, FALSE },
    { "beam"     , POS_LYING   , do_action   , 0, 0, FALSE },
    { "bearhug"  , POS_STANDING, do_action   , 0, 0, FALSE },
    { "beg"      , POS_RESTING , do_action   , 0, 0, FALSE },
    { "bite"     , POS_RESTING , do_action   , 0, 0, FALSE },
    { "blink"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "bleed"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "blush"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "boggle"   , POS_LYING   , do_action   , 0, 0, FALSE },
    { "bonk"     , POS_STANDING, do_action   , 0, 0, FALSE },
    { "bow"      , POS_STANDING, do_action   , 0, 0, FALSE },
    { "brb"      , POS_LYING   , do_action   , 0, 0, FALSE },
    { "brick"    , POS_STANDING, do_action   , 0, 0, FALSE },
    { "burp"     , POS_LYING   , do_action   , 0, 0, FALSE },
    // Socials C
    { "cackle"   , POS_LYING   , do_action   , 0, 0, FALSE },
    { "chuckle"  , POS_LYING   , do_action   , 0, 0, FALSE },
    { "chillout" , POS_RESTING , do_action   , 0, 0, FALSE },
    { "cheer"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "clap"     , POS_LYING   , do_action   , 0, 0, FALSE },
    { "cockeye"  , POS_RESTING , do_action   , 0, 0, FALSE },
    { "collapse" , POS_STANDING, do_action   , 0, 0, FALSE },
    { "comb"     , POS_RESTING , do_action   , 0, 0, FALSE },
    { "comfort"  , POS_RESTING , do_action   , 0, 0, FALSE },
    { "confused" , POS_LYING   , do_action   , 0, 0, FALSE },
    { "congrat"  , POS_LYING   , do_action   , 0, 0, FALSE },
    { "convince" , POS_LYING   , do_action   , 0, 0, FALSE },
    { "cough"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "cringe"   , POS_LYING   , do_action   , 0, 0, FALSE },
    { "cry"      , POS_LYING   , do_action   , 0, 0, FALSE },
    { "cuddle"   , POS_LYING   , do_action   , 0, 0, FALSE },
    { "curse"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "curtsey"  , POS_STANDING, do_action   , 0, 0, FALSE },
    // Socials D
    { "dance"    , POS_STANDING, do_action   , 0, 0, FALSE },
    { "daydream" , POS_SLEEPING, do_action   , 0, 0, FALSE },
    { "dis"      , POS_LYING   , do_action   , 0, 0, FALSE },
    { "disagree" , POS_LYING   , do_action   , 0, 0, FALSE },
    { "disco"    , POS_RESTING , do_action   , 0, 0, FALSE },
    { "disregard", POS_LYING   , do_action   , 0, 0, FALSE },
    { "doh"      , POS_LYING   , do_action   , 0, 0, FALSE },
    { "dribble"  , POS_LYING   , do_action   , 0, 0, FALSE },
    { "drool"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "dunce"    , POS_LYING   , do_action   , 0, 0, FALSE },
    // Socials E
    { "embrace"  , POS_LYING   , do_action   , 0, 0, FALSE },
    { "envy"     , POS_LYING   , do_action   , 0, 0, FALSE },
    { "eyebrow"  , POS_LYING   , do_action   , 0, 0, FALSE },
    // Socials F
    { "facepalm" , POS_SITTING , do_action   , 0, 0, FALSE },
    { "forgive"  , POS_LYING   , do_action   , 0, 0, FALSE },
    { "fart"     , POS_LYING   , do_action   , 0, 0, FALSE },
    { "flex"     , POS_STANDING, do_action   , 0, 0, FALSE },
    { "flirt"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "fondle"   , POS_LYING   , do_action   , 0, 0, FALSE },
    { "french"   , POS_LYING   , do_action   , 0, 0, FALSE },
    { "frown"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "fume"     , POS_LYING   , do_action   , 0, 0, FALSE },
    // Socials G
    { "gasp"     , POS_LYING   , do_action   , 0, 0, FALSE },
    { "giggle"   , POS_LYING   , do_action   , 0, 0, FALSE },
    { "glare"    , POS_RESTING , do_action   , 0, 0, FALSE },
    { "greet"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "grin"     , POS_LYING   , do_action   , 0, 0, FALSE },
    { "grimace"  , POS_LYING   , do_action   , 0, 0, FALSE },
    { "groan"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "grope"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "grovel"   , POS_LYING   , do_action   , 0, 0, FALSE },
    { "growl"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "grumble"  , POS_LYING   , do_action   , 0, 0, FALSE },
    { "grunt"    , POS_LYING   , do_action   , 0, 0, FALSE },
    // Socials H
    { "hair"     , POS_RESTING , do_action   , 0, 0, FALSE },
    { "happy"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "hand"     , POS_LYING   , do_action   , 0, 0, FALSE },
    { "hate"     , POS_LYING   , do_action   , 0, 0, FALSE },
    { "hhold"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "hiccup"   , POS_LYING   , do_action   , 0, 0, FALSE },
    { "hifive"   , POS_STANDING, do_action   , 0, 0, FALSE },
    { "hoi"      , POS_LYING   , do_action   , 0, 0, FALSE },
    { "hop"      , POS_LYING   , do_action   , 0, 0, FALSE },
    { "howl"     , POS_LYING   , do_action   , 0, 0, FALSE },
    { "hkiss"    , POS_STANDING, do_action   , 0, 0, FALSE },
    { "hug"      , POS_RESTING , do_action   , 0, 0, FALSE },
    // Socials I
    { "innocent" , POS_LYING   , do_action   , 0, 0, FALSE },
    // Socials J
    { "jig"      , POS_STANDING, do_action   , 0, 0, FALSE },
    { "jeer"     , POS_LYING   , do_action   , 0, 0, FALSE },
    { "jump"     , POS_RESTING , do_action   , 0, 0, FALSE },
    // Socials K
    { "kiss"     , POS_LYING   , do_action   , 0, 0, FALSE },
    // Socials L
    { "lap"      , POS_STANDING, do_action   , 0, 0, FALSE },
    { "laugh"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "listen"   , POS_LYING   , do_action   , 0, 0, FALSE },
    { "lick"     , POS_LYING   , do_action   , 0, 0, FALSE },
    { "lol"      , POS_LYING   , do_action   , 0, 0, FALSE },
    { "love"     , POS_LYING   , do_action   , 0, 0, FALSE },
    // Socials M
    { "mellow"   , POS_RESTING , do_action   , 0, 0, FALSE },
    { "moan"     , POS_LYING   , do_action   , 0, 0, FALSE },
    { "mosh"     , POS_STANDING, do_action   , 0, 0, FALSE },
    { "moon"     , POS_STANDING, do_action   , 0, 0, FALSE },
    { "massage"  , POS_RESTING , do_action   , 0, 0, FALSE },
    { "muthafucka",POS_RESTING , do_action   , 0, 0, FALSE },
    // Socials N
    { "nibble"   , POS_RESTING , do_action   , 0, 0, FALSE },
    { "nod"      , POS_LYING   , do_action   , 0, 0, FALSE },
    { "noogie"   , POS_STANDING, do_action   , 0, 0, FALSE },
    { "nudge"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "nuzzle"   , POS_LYING   , do_action   , 0, 0, FALSE },
    // Socials O
    // Socials P
    { "pant"     , POS_LYING   , do_action   , 0, 0, FALSE },
    { "pat"      , POS_LYING   , do_action   , 0, 0, FALSE },
    { "peck"     , POS_LYING   , do_action   , 0, 0, FALSE },
    { "peer"     , POS_LYING   , do_action   , 0, 0, FALSE },
    { "point"    , POS_RESTING , do_action   , 0, 0, FALSE },
    { "poke"     , POS_RESTING , do_action   , 0, 0, FALSE },
    { "ponder"   , POS_LYING   , do_action   , 0, 0, FALSE },
    { "pout"     , POS_RESTING , do_action   , 0, 0, FALSE },
    { "prance"   , POS_STANDING, do_action   , 0, 0, FALSE },
    { "pray"     , POS_SITTING , do_action   , 0, 0, FALSE },
    { "propose"  , POS_STANDING, do_action   , 0, 0, FALSE },
    { "psychoanalyze", POS_RESTING, do_action, 0, 0, FALSE },
    { "pucker"   , POS_RESTING , do_action   , 0, 0, FALSE },
    { "puke"     , POS_LYING   , do_action   , 0, 0, FALSE },
    { "punch"    , POS_RESTING , do_action   , 0, 0, FALSE },
    { "puppy"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "purr"     , POS_LYING   , do_action   , 0, 0, FALSE },
    // Socials Q
    // Socials R
    { "raspberry", POS_SITTING , do_action   , 0, 0, FALSE },
    { "rtfm"     , POS_STANDING, do_action   , 0, 0, FALSE },
    { "roar"     , POS_RESTING , do_action   , 0, 0, FALSE },
    { "rofl"     , POS_RESTING , do_action   , 0, 0, FALSE },
    { "rose"     , POS_STANDING, do_action   , 0, 0, FALSE },
    { "rub"      , POS_RESTING , do_action   , 0, 0, FALSE },
    { "ruffle"   , POS_STANDING, do_action   , 0, 0, FALSE },
    // Socials S
    { "sage"     , POS_LYING   , do_action   , 0, 0, FALSE },
    { "scratch"  , POS_LYING   , do_action   , 0, 0, FALSE },
    { "scream"   , POS_LYING   , do_action   , 0, 0, FALSE },
    { "shake"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "shiver"   , POS_LYING   , do_action   , 0, 0, FALSE },
    { "shrug"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "sigh"     , POS_LYING   , do_action   , 0, 0, FALSE },
    { "signal"   , POS_LYING   , do_action   , 0, 0, FALSE },
    { "sing"     , POS_LYING   , do_action   , 0, 0, FALSE },
    { "slap"     , POS_RESTING , do_action   , 0, 0, FALSE },
    { "slobber"  , POS_RESTING , do_action   , 0, 0, FALSE },
    { "slurp"    , POS_STANDING, do_action   , 0, 0, FALSE },
    { "smile"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "smirk"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "snicker"  , POS_LYING   , do_action   , 0, 0, FALSE },
    { "snap"     , POS_LYING   , do_action   , 0, 0, FALSE },
    { "snarl"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "sneeze"   , POS_LYING   , do_action   , 0, 0, FALSE },
    { "sniff"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "snort"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "snore"    , POS_SLEEPING, do_action   , 0, 0, FALSE },
    { "snowball" , POS_STANDING, do_action   , LVL_BUILDER, 0, FALSE },
    { "snuggle"  , POS_LYING   , do_action   , 0, 0, FALSE },
    { "sob"      , POS_LYING   , do_action   , 0, 0, FALSE },
    { "spank"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "spit"     , POS_STANDING, do_action   , 0, 0, FALSE },
    { "squeeze"  , POS_LYING   , do_action   , 0, 0, FALSE },
    { "stare"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "stamp"    , POS_RESTING , do_action   , 0, 0, FALSE },
    { "steam"    , POS_RESTING , do_action   , 0, 0, FALSE },
    { "striptease", POS_STANDING , do_action  , 0, 0, FALSE },
    { "stroke"   , POS_RESTING , do_action   , 0, 0, FALSE },
    { "strut"    , POS_STANDING, do_action   , 0, 0, FALSE },
    { "strangle" , POS_STANDING, do_action   , 0, 0, FALSE },
    { "sulk"     , POS_LYING   , do_action   , 0, 0, FALSE },
    { "swat"     , POS_RESTING , do_action   , 0, 0, FALSE },
    { "swear"    , POS_LYING   , do_action   , 0, 0, FALSE },
    // Socials T
    { "tackle"   , POS_RESTING , do_action   , 0, 0, FALSE },
    { "tango"    , POS_STANDING, do_action   , 0, 0, FALSE },
    { "tap"      , POS_LYING   , do_action   , 0, 0, FALSE },
    { "taunt"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "thank"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "thwap"    , POS_SITTING , do_action   , 0, 0, FALSE },
    { "tickle"   , POS_RESTING , do_action   , 0, 0, FALSE },
    { "tiptoe"   , POS_RESTING , do_action   , 0, 0, FALSE },
    { "tongue"   , POS_LYING   , do_action   , 0, 0, FALSE },
    { "torture"  , POS_RESTING , do_action   , 0, 0, FALSE },
    { "touch"    , POS_STANDING, do_action   , 0, 0, FALSE },
    { "toss"     , POS_STANDING, do_action   , 0, 0, FALSE },
    { "trip"     , POS_STANDING, do_action   , 0, 0, FALSE },
    { "twitch"   , POS_LYING   , do_action   , 0, 0, FALSE },
    { "twiddle"  , POS_LYING   , do_action   , 0, 0, FALSE },
    { "twirl"    , POS_STANDING, do_action   , 0, 0, FALSE },
    // Socials U
    // Socials V
    { "volunteer", POS_RESTING , do_action   , 0, 0, FALSE },
    // Socials W
    { "wave"     , POS_LYING   , do_action   , 0, 0, FALSE },
    { "whimper"  , POS_LYING   , do_action   , 0, 0, FALSE },
    { "whine"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "whistle"  , POS_LYING   , do_action   , 0, 0, FALSE },
    { "whiz"     , POS_SITTING , do_action   , 0, 0, FALSE },
    { "wibble"   , POS_STANDING, do_action   , 0, 0, FALSE },
    { "wiggle"   , POS_STANDING, do_action   , 0, 0, FALSE },
    { "wince"    , POS_LYING   , do_action   , 0, 0, FALSE },
    { "wink"     , POS_LYING   , do_action   , 0, 0, FALSE },
    { "wooha"    , POS_SITTING , do_action   , 0, 0, FALSE },
    { "worship"  , POS_RESTING , do_action   , 0, 0, FALSE },
    { "wrestle"  , POS_STANDING, do_action   , 0, 0, FALSE },
    // Socials X
    // Socials Y
    { "yawn"     , POS_LYING   , do_action   , 0, 0, FALSE },
    { "yodel"    , POS_LYING   , do_action   , 0, 0, FALSE },
    // Socials Z

 // { "zyx"      , POS_STANDING , do_action  , LVL_PRESIDENT, 0, FALSE },
    /* End of socials. */

  /* this must be last; do not touch anything below this line unless you know what you're doing */
    { "\n", 0, 0, 0, 0, FALSE }
  };
  /* this must be last; do not touch anything above this line unless you know what you're doing */

ACMD_DECLARE(do_abort);
ACMD_DECLARE(do_analyze);
ACMD_DECLARE(do_asist);
ACMD_DECLARE(do_comcall);
ACMD_DECLARE(do_control);
ACMD_DECLARE(do_crash);
ACMD_DECLARE(do_decrypt);
ACMD_DECLARE(do_download);
ACMD_DECLARE(do_evade);
ACMD_DECLARE(do_load);
ACMD_DECLARE(do_locate);
ACMD_DECLARE(do_logoff);
ACMD_DECLARE(do_logon);
ACMD_DECLARE(do_matrix_look);
ACMD_DECLARE(do_matrix_max);
ACMD_DECLARE(do_matrix_position);
ACMD_DECLARE(do_matrix_scan);
ACMD_DECLARE(do_matrix_score);
ACMD_DECLARE(do_parry);
ACMD_DECLARE(do_redirect);
ACMD_DECLARE(do_restrict);
ACMD_DECLARE(do_reveal);
ACMD_DECLARE(do_run);
ACMD_DECLARE(do_tap);
ACMD_DECLARE(do_talk);
ACMD_DECLARE(do_trace);
ACMD_DECLARE(do_fry_self);

struct command_info mtx_info[] =
  {
    { "RESERVED", 0, 0, 0, 0
    , FALSE },
    { "abort", 0, do_abort, 0, 0, FALSE },
    { "analyze", 0, do_analyze, 0, 0, FALSE },
    { "answer", 0, do_comcall, 0, SCMD_ANSWER, FALSE },
    { "asist", 0, do_asist, 0, 0, FALSE },
    { "bug", 0, do_gen_write, 0, SCMD_BUG, FALSE },
    { "call", 0, do_comcall, 0, SCMD_RING, FALSE },
  //{ "control", 0, do_control, 0, 0, FALSE },     // This is a rigging command?
    { "commands", 0, do_commands, 0, SCMD_COMMANDS, FALSE },
    { "crash", 0, do_crash, 0, 0, FALSE },
    { "decrypt", 0, do_decrypt, 0, 0, FALSE },
    { "disarm", 0, do_decrypt, 0, 1, FALSE },
    { "disconnect", 0, do_logoff, 0, 1, FALSE },
    { "download", 0, do_download, 0, 0, FALSE },
    { "evade", 0, do_evade, 0, 0, FALSE },
    { "emote", 0, do_echo, 0, SCMD_EMOTE , FALSE },
    { ":", 0, do_echo, 0, SCMD_EMOTE , FALSE },
    { "exit", 0, do_logoff, 0, 0, FALSE },
    { "hangup", 0, do_comcall, 0, SCMD_HANGUP, FALSE },
    { "help", 0, do_help, 0, 0, FALSE },
    { "ht", 0, do_gen_comm , 0, SCMD_HIREDTALK, FALSE },
    { "idea", 0, do_gen_write, 0, SCMD_IDEA, FALSE },
    { "index", 0, do_index, 0, 0, FALSE },
    { "look", 0, do_matrix_look, 0, 0, FALSE },
    { "list", 0, do_not_here, 0, 0, FALSE },
    { "load", 0, do_load, 0, SCMD_SWAP, FALSE },
    { "locate", 0, do_locate, 0, 0, FALSE },
    { "logoff", 0, do_logoff, 0, 0, FALSE },
    { "logout", 0, do_logoff, 0, 0, FALSE },
    { "logon", 0, do_logon, 0, 0, FALSE },
    { "max", 0, do_matrix_max, 0, 0, FALSE },
    { "newbie", 0, do_gen_comm, 0, SCMD_NEWBIE, FALSE },
    { "ooc", 0, do_gen_comm, 0, SCMD_OOC, FALSE },
    { "parry", 0, do_parry, 0, 0, FALSE },
    { "phone", 0, do_comcall, 0, 0, FALSE },
    { "position", 0, do_matrix_position, 0, 0, FALSE },
    { "prompt", 0, do_display, 0, 0 , FALSE },
    { "quit", 0, do_logoff, 0, 0, FALSE },
    { "read", 0, do_not_here, 0, 0, FALSE },
    { "recap", 0, do_recap, 0, 0, FALSE },
    { "redirect", 0, do_redirect, 0, 0, FALSE },
    { "remove", 0, do_not_here, 0, 0, FALSE },
    { "reply", 0, do_reply, 0, 0 , FALSE },
    { "restrict", 0, do_restrict, 0, 0, FALSE },
    { "reveal", 0, do_reveal, 0, 0, FALSE },
    { "run", 0, do_run, 0, 0, FALSE },
    { "say", 0, do_say, 0, 0, FALSE },
    { "'", 0, do_say, 0, 0, FALSE },
    { "score", 0, do_matrix_score, 0, 0, FALSE },
    { "scan", 0, do_matrix_scan, 0, 0, FALSE },
    // { "selffry", 0, do_fry_self, LVL_BUILDER, 0, FALSE },
    { "talk", 0, do_talk, 0, 0, FALSE },
    { "tap", 0, do_tap, 0, 0, FALSE },
    { "tell", 0, do_tell, 0, 0 , FALSE },
    { "time", 0, do_time, 0, 0, FALSE },
    { "toggle", 0, do_toggle, 0, 0 , FALSE },
    { "trace", 0, do_trace, 0, 0, FALSE },
    { "typo", 0, do_gen_write, 0, SCMD_TYPO, FALSE },
    { "unload", 0, do_load, 0, SCMD_UNLOAD, FALSE },
    { "upload", 0, do_load, 0, SCMD_UPLOAD, FALSE },
    { "recap", 0, do_recap, 0, 0 , FALSE },
    { "software", 0, do_software, 0, 0, FALSE },
    { "who", 0, do_who, 0, 0, FALSE },
    { "write", 0, do_not_here, 0, 0, FALSE },
    { "wtell", 0, do_wiztell, LVL_BUILDER, 0, FALSE },
    { "\n", 0, 0, 0, 0, FALSE  }
  };

struct command_info rig_info[] =
  {
    { "RESERVED", 0, 0, 0, 0
    , FALSE },
    { "north", 0, do_move, 0, SCMD_NORTH , FALSE },
    { "east", 0, do_move, 0, SCMD_EAST , FALSE },
    { "south", 0, do_move, 0, SCMD_SOUTH , FALSE },
    { "west", 0, do_move, 0, SCMD_WEST , FALSE },
    { "up", 0, do_move, 0, SCMD_UP , FALSE },
    { "down", 0, do_move, 0, SCMD_DOWN , FALSE },
    { "ne", 0, do_move, 0, SCMD_NORTHEAST, FALSE },
    { "se", 0, do_move, 0, SCMD_SOUTHEAST, FALSE },
    { "sw", 0, do_move, 0, SCMD_SOUTHWEST, FALSE },
    { "nw", 0, do_move, 0, SCMD_NORTHWEST, FALSE },
    { "northeast", 0, do_move, 0, SCMD_NORTHEAST, FALSE },
    { "southeast", 0, do_move, 0, SCMD_SOUTHEAST, FALSE },
    { "southwest", 0, do_move, 0, SCMD_SOUTHWEST, FALSE },
    { "northwest", 0, do_move, 0, SCMD_NORTHWEST, FALSE },
    { "alias", 0, do_alias, 0, 0, FALSE },
    { "bug", 0, do_gen_write, 0, SCMD_BUG, FALSE },
    { "chase", 0, do_chase, 0, 0 , FALSE },
    { "cpool", 0, do_cpool, 0, 0 , FALSE },
    { "commands", 0, do_commands, 0, SCMD_COMMANDS, FALSE },
    { "copyover", LVL_PRESIDENT, do_copyover, 0, 0, FALSE },
    { "driveby", 0, do_driveby, 0, 0, FALSE },
    { "enter", 0, do_enter, 0, 0, FALSE },
    { "exits", 0, do_exits, 0, 0, FALSE },
    { "gridguide", 0, do_gridguide, 0, 0, FALSE },
    { "help", 0, do_help, 0, 0, FALSE },
    { "ht", 0, do_gen_comm , 0, SCMD_HIREDTALK, FALSE },
    { "idea", 0, do_gen_write, 0, SCMD_IDEA, FALSE },
    { "index", 0, do_index, 0, 0, FALSE },
    { "look", 0, do_look, 0, 0, FALSE },
    { "leave", 0, do_leave, 0 ,0 , FALSE },
    { "lock", 0, do_gen_door , 0, SCMD_LOCK , FALSE },
    { "mode", 0, do_mode, 0, 0, FALSE },
    { "mount", 0, do_mount, 0, 0, FALSE },
    { "newbie", 0, do_gen_comm, 0, SCMD_NEWBIE, FALSE },
    { "ooc", 0, do_gen_comm, 0, SCMD_OOC, FALSE },
    { "pools", 0, do_pool, 0, 0 , FALSE },
    { "ram", 0, do_ram, 0, 0, FALSE },
    { "recap", 0, do_recap, 0, 0 , FALSE },
    { "rig", POS_SITTING , do_rig, 0, 0 , FALSE },
    { "return", 0, do_return, 0, 0, FALSE },
    { "reply", 0, do_reply, 0, 0 , FALSE },
    { "rt", 0, do_gen_comm, 0, SCMD_RPETALK, FALSE },
    { "score", 0, do_score, 0, 0, FALSE },
    { "scan", 0, do_scan, 0, 0, FALSE },
    { "speed", 0, do_speed, 0, 0, FALSE },
    { "subscribe", 0, do_subscribe, 0, 0, FALSE },
    { "target", 0, do_target, 0, 0, FALSE },
    { "tell", 0, do_tell, 0, 0 , FALSE },
    { "time", 0, do_time, 0, 0, FALSE },
    { "tow", 0, do_tow , 0, 0 , FALSE },
    { "typo", 0, do_gen_write, 0, SCMD_TYPO, FALSE },
    { "unlock", 0, do_gen_door , 0, SCMD_UNLOCK , FALSE },
    { "vemote", 0, do_vemote, 0, 0, FALSE },
    { "where", 0, do_where, 0, 0, FALSE },
    { "who", 0, do_who, 0, 0, FALSE },
    { "wtell", 0, do_wiztell, LVL_BUILDER, 0, FALSE },
    { "\n", 0, 0, 0, 0, FALSE }
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
    "myself",
    "all",
    "room",
    "someone",
    "something",
    "new",
    "guest",
    CHARACTER_DELETED_NAME_FOR_SQL,
    "admin", // Added here since this is the one random bots like to try.
    "\n"
  };

void nonsensical_reply(struct char_data *ch, const char *arg, const char *mode)
{
  send_to_char(ch, "That is not a valid command.\r\n");
  if (ch->desc && ++ch->desc->invalid_command_counter >= 5) {
    send_to_char(ch, "^GStuck? Need help? Feel free to ask on the %s channel! (%s%s <message>)^n\r\n",
                 PLR_FLAGGED(ch, PLR_NEWBIE) ? "newbie" : "OOC",
                 PRF_FLAGGED(ch, PRF_SCREENREADER) ? "type " : "",
                 PLR_FLAGGED(ch, PLR_NEWBIE) ? "NEWBIE" : "OOC");
    ch->desc->invalid_command_counter = 0;
  }
  // There must be an arg, and it must not be a number.
  if (arg && *arg && isalpha(*arg)) {
    // Write it to the in-game log.
    char log_buf[1000];
    snprintf(log_buf, sizeof(log_buf), "Invalid %s command: '%s'.", mode, arg);
    mudlog(log_buf, ch, LOG_FUCKUPLOG, TRUE);

    // Check to see if it's a staff command. We don't care to log these to the fuckups table, as they're not fuckups we can learn from.
    /*
    for (int i = 0; *cmd_info[i].command != '\n'; i++)
      if (cmd_info[i].minimum_level >= LVL_BUILDER && !str_str(cmd_info[i].command, arg))
        return;
    */

    // Log it to DB.
    snprintf(buf, sizeof(buf), "INSERT INTO command_fuckups (Name, Count) VALUES ('%s', 1) ON DUPLICATE KEY UPDATE Count = Count + 1;",
             prepare_quotes(buf3, arg, sizeof(buf3) / sizeof(buf3[0])));
    mysql_wrapper(mysql, buf);
  }
  /*  Removing the prior 'funny' messages and replacing them with something understandable by MUD newbies.
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
   */
}

/*
 * This is the actual command interpreter called from game_loop() in comm.c
 * It makes sure you are the proper level and position to execute the command,
 * then calls the appropriate function.
 */
ACMD_DECLARE(quit_the_matrix_first);
ACMD_DECLARE(stop_rigging_first);
void command_interpreter(struct char_data * ch, char *argument, char *tcname)
{
  int cmd, length;
  extern int no_specials;
  char *line;

  AFF_FLAGS(ch).RemoveBit(AFF_HIDE);

  if (PRF_FLAGS(ch).IsSet(PRF_AFK)) {
    send_to_char("You return from AFK.\r\n", ch);
    PRF_FLAGS(ch).RemoveBit(PRF_AFK);
  }

  /* just drop to next line for hitting CR */
  skip_spaces(&argument);
  if (!*argument)
    return;

  // They entered something? KaVir's protocol snippet says to clear their WriteOOB.
  if (ch->desc)
    ch->desc->pProtocol->WriteOOB = 0;

#ifdef LOG_COMMANDS
  log_command(ch, argument, tcname);
#endif

  /*
   * special case to handle one-character, non-alphanumeric commands;
   * requested by many people so "'hi" or ";godnet test" is possible.
   * Patch sent by Eric Green and Stefan Wasilewski.
   */
  if (!isalpha(*argument))
  {
    // Strip out the PennMUSH bullshit.
    if (*argument == '@' || *argument == '+' || *argument == '/') {
      argument[0] = ' ';
      skip_spaces(&argument);
      if (!*argument)
        return;
      line = any_one_arg(argument, arg);
    } else {
      arg[0] = argument[0];
      arg[1] = '\0';
      line = argument+1;
    }
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

    // If they have failed to enter a valid Matrix command, and we were unable to fix a typo in their command:
    if (*mtx_info[cmd].command == '\n' && (cmd = fix_common_command_fuckups(arg, mtx_info)) == -1) {
      // If the command exists outside of the Matrix, let them know that it's not an option here.
      for (length = strlen(arg), cmd = 0; *cmd_info[cmd].command != '\n'; cmd++)
        if (!strncmp(cmd_info[cmd].command, arg, length))
          if ((cmd_info[cmd].minimum_level < LVL_BUILDER) || access_level(ch, cmd_info[cmd].minimum_level))
            break;

      // Nothing was found? Give them the "wat" and bail.
      if (*cmd_info[cmd].command == '\n' && (cmd = fix_common_command_fuckups(arg, cmd_info)) == -1) {
        nonsensical_reply(ch, arg, "matrix");
        return;
      }

      if (ch->desc)
        ch->desc->invalid_command_counter = 0;

      // Their command was valid in external context. Inform them.
      quit_the_matrix_first(ch, line, 0, 0);
    }
    else {
      if (ch->persona && ch->persona->decker->hitcher) {
        send_to_char(ch->persona->decker->hitcher, "^y<OUTGOING> %s^n\r\n", argument);
      }
      verify_data(ch, line, cmd, mtx_info[cmd].subcmd, "pre-matrix");
      if (!special(ch, cmd, line)) {
        ((*mtx_info[cmd].command_pointer) (ch, line, cmd, mtx_info[cmd].subcmd));
        verify_data(ch, line, cmd, mtx_info[cmd].subcmd, "matrix");
      } else {
        verify_data(ch, line, cmd, mtx_info[cmd].subcmd, "matrix special");
      }
    }
  } else if (PLR_FLAGGED(ch, PLR_REMOTE) || AFF_FLAGGED(ch, AFF_RIG))
  {
    for (length = strlen(arg), cmd = 0; *rig_info[cmd].command != '\n'; cmd++)
      if (!strncmp(rig_info[cmd].command, arg, length))
        break;

    // If they have failed to enter a valid Rigging command, and we were unable to fix a typo in their command:
    if (*rig_info[cmd].command == '\n' && (cmd = fix_common_command_fuckups(arg, rig_info)) == -1) {
      // If the command exists outside of the Matrix, let them know that it's not an option here.
      for (length = strlen(arg), cmd = 0; *cmd_info[cmd].command != '\n'; cmd++)
        if (!strncmp(cmd_info[cmd].command, arg, length))
          if ((cmd_info[cmd].minimum_level < LVL_BUILDER) || access_level(ch, cmd_info[cmd].minimum_level))
            break;

      // Nothing was found? Give them the "wat" and bail.
      if (*cmd_info[cmd].command == '\n' && (cmd = fix_common_command_fuckups(arg, cmd_info)) == -1) {
        nonsensical_reply(ch, arg, "matrix");
        return;
      }

      if (ch->desc)
        ch->desc->invalid_command_counter = 0;

      // Their command was valid in external context. Inform them.
      stop_rigging_first(ch, line, 0, 0);
    } else {
      if (ch->desc)
        ch->desc->invalid_command_counter = 0;

      verify_data(ch, line, cmd, rig_info[cmd].subcmd, "pre-rig");
      if (!special(ch, cmd, line)) {
        ((*rig_info[cmd].command_pointer) (ch, line, cmd, rig_info[cmd].subcmd));
        verify_data(ch, line, cmd, rig_info[cmd].subcmd, "rig");
      } else {
        verify_data(ch, line, cmd, rig_info[cmd].subcmd, "rig special");
      }
    }
  } else
  {
    for (length = strlen(arg), cmd = 0; *cmd_info[cmd].command != '\n'; cmd++)
      if (!strncmp(cmd_info[cmd].command, arg, length))
        if ((cmd_info[cmd].minimum_level < LVL_BUILDER) ||
            access_level(ch, cmd_info[cmd].minimum_level))
          break;

    // this was added so we can make the special respond to any text they type
    if (*cmd_info[cmd].command == '\n'
        && ((cmd = fix_common_command_fuckups(arg, cmd_info)) == -1
            || ((cmd_info[cmd].minimum_level >= LVL_BUILDER) && !access_level(ch, cmd_info[cmd].minimum_level)))) {
      nonsensical_reply(ch, arg, "standard");
      return;
    } else {
      if (ch->desc)
        ch->desc->invalid_command_counter = 0;
    }

    if (IS_PROJECT(ch) && ch->desc && ch->desc->original && AFF_FLAGGED(ch->desc->original, AFF_TRACKING) && cmd != find_command("track")) {
      send_to_char("You are too busy astrally tracking someone...\r\n", ch);
      return;
    }

    if (PLR_FLAGGED(ch, PLR_FROZEN)) {
      if (!access_level(ch, LVL_VICEPRES)) {
        send_to_char("You try, but the mind-numbing cold prevents you...\r\n", ch);
        return;
      } else
        send_to_char("The ice covering you crackles alarmingly as you slam your sovereign will through it.\r\n", ch);
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

    // Restore the idle timer for the idle nuyen bonus.
    if (cmd_info[cmd].should_not_block_idle_reward) {
      ch->char_specials.timer = ch->char_specials.last_timer;
    }

    if (GET_POS(ch) < cmd_info[cmd].minimum_position) {
      switch (GET_POS(ch)) {
      case POS_DEAD:
        // send_to_char("Lie still; you are DEAD!!! :-(\r\n", ch);
        send_to_char("The last vestiges of your soul begin to leave your rapidly-cooling form.\r\n", ch);
        mudlog("WARNING: Dead character is still trying to perform actions. Killing them...", ch, LOG_SYSLOG, TRUE);
        do_die(ch, arg, 0, 0);
        break;
      case POS_MORTALLYW:
        send_to_char("You are in a pretty bad shape! You can either wait for help, or give up by typing ^WDIE^n.\r\n", ch);
        break;
      case POS_STUNNED:
        send_to_char("All you can do right now is think about the stars! You can either wait to recover, or give up by typing ^WDIE^n.\r\n", ch);
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

    verify_data(ch, line, cmd, cmd_info[cmd].subcmd, "pre-command");

    if (no_specials || !special(ch, cmd, line)) {
      ((*cmd_info[cmd].command_pointer) (ch, line, cmd, cmd_info[cmd].subcmd));
      verify_data(ch, line, cmd, cmd_info[cmd].subcmd, "command");
    } else {
      verify_data(ch, line, cmd, cmd_info[cmd].subcmd, "command special");
    }
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

ACMD(do_keepalive) {
  // no-op command to give you something to send without generating server load
}

/* The interface to the outside world: do_alias */
ACMD(do_alias)
{
  char *repl;
  struct alias *a, *temp;

  if (IS_NPC(ch) && !(ch->desc && ch->desc->original))
    return;

  ch->alias_dirty_bit = TRUE;

  repl = any_one_arg(argument, arg);

  if (!*arg) {  /* no argument specified -- list currently defined aliases */
    send_to_char("Currently defined aliases:\r\n", ch);
    if ((a = GET_ALIASES(ch)) == NULL)
      send_to_char(" None.\r\n", ch);
    else {
      while (a != NULL) {
        send_to_char(ch, "%-15s %s\r\n", a->command, a->replacement);
        a = a->next;
      }
    }
  } else { /* otherwise, add or remove aliases */
    GET_ALIAS_DIRTY_BIT(ch) = TRUE;

    /* is this an alias we've already defined? */
    if ((a = find_alias(GET_ALIASES(ch), arg)) != NULL) {
      REMOVE_FROM_LIST(a, GET_ALIASES(ch), next);
      delete a;
      a = NULL;

      // No replacement string was specified-- assume we want to delete.
      if (!*repl) {
        send_to_char("Alias deleted.\r\n", ch);
        return;
      }
    }

    if (!*repl) {
      // They wanted to delete an alias, but it didn't exist.
      send_to_char("No such alias.\r\n", ch);
    } else { /* otherwise, either add or redefine an alias */
      if (!str_cmp(arg, "alias")) {
        send_to_char("You can't alias 'alias'.\r\n", ch);
        return;
      }
      /* Should cover every possbile case of 'kill', 'hit', and 'murder' */
      else if ( (str_str(repl, "kill") || str_str(repl, "hit") || str_str(repl, "murder")) && strlen(arg) < 4 ) {
        send_to_char(
          "If you alias contains the 'kill', 'hit', or 'murder' commands,"
          " it must be accompanied by at least a 4 letter alias.\n\r",ch);
        return;
      } else if ( str_str(repl, "quit") ) {
        send_to_char("Aliases cannot contain the 'quit' command.\n\r",ch);
        return;
      }
      if (strlen(repl) > 255)
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
int search_block(const char *arg, const char **list, bool exact)
{
  int i, l;
  if (!strcmp(arg, "!"))
    return -1;

  char mutable_arg[strlen(arg) + 1];
  strlcpy(mutable_arg, arg, sizeof(mutable_arg));

  /* Make into lower case, and get length of string */
  for (l = 0; *(mutable_arg + l); l++)
    *(mutable_arg + l) = LOWER(*(mutable_arg + l));

  if (exact) {
    for (i = 0; **(list + i) != '\n'; i++)
      if (!strcmp(mutable_arg, *(list + i)))
        return (i);
  } else {
    if (!l)
      l = 1;                    /* Avoid "" to match the first available
                                                                                     * string */
    for (i = 0; **(list + i) != '\n'; i++)
      if (!strncmp(mutable_arg, *(list + i), l))
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
char *one_argument(char *argument, char *first_arg, bool preserve_case)
{
  char *begin = first_arg;

  do {
    skip_spaces(&argument);

    first_arg = begin;
    while (*argument && !isspace(*argument)) {
      *(first_arg++) = (preserve_case ? *argument : LOWER(*argument));
      argument++;
    }

    *first_arg = '\0';
  } while (fill_word(begin));

  skip_spaces(&argument);

  return argument;
}


/* same as one_argument except that it doesn't ignore fill words */
char *any_one_arg(char *argument, char *first_arg, bool preserve_case)
{
  *first_arg = '\0';

  if (!argument)
    return NULL;

  skip_spaces(&argument);

  while (*argument && !isspace(*argument)) {
    *(first_arg++) = (preserve_case ? *argument : LOWER(*argument));
    argument++;
  }

  *first_arg = '\0';

  return argument;
}

// Same as above, but without skip_spaces.
const char *any_one_arg_const(const char *argument, char *first_arg, bool preserve_case)
{
  *first_arg = '\0';

  if (!argument)
    return NULL;

  while (*argument && !isspace(*argument)) {
    *(first_arg++) = (preserve_case ? *argument : LOWER(*argument));
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
  if (!arg1 || !arg2) {
    char temp_buf[500];
    snprintf(temp_buf, sizeof(temp_buf), "SYSERR: Received null arg in is_abbrev (arg1 = %s, arg2 = %s)", arg1, arg2);
    mudlog(temp_buf, NULL, LOG_SYSLOG, TRUE);
    return 0;
  }

  if (!*arg1 || !*arg2)
    return 0;

  /* This is theoretically a better way to write the code below. I don't have time to test it, so I won't put it in, but I wrote it anyways. -LS.

  while (LOWER(*(arg1++)) == LOWER(*(arg2++)));

  return !*arg1;

  */

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
int find_command_in_x(const char *command, const struct command_info *cmd_info)
{
  int cmd;

  for (cmd = 0; *cmd_info[cmd].command != '\n'; cmd++)
    if (!strcmp(cmd_info[cmd].command, command))
      return cmd;

  return -1;
}

/* Used in specprocs, mostly.  (Exactly) matches "command" to cmd number */
int find_command(const char *command)
{
  return find_command_in_x(command, cmd_info);
}

int find_mcommand(const char *command)
{
  return find_command_in_x(command, mtx_info);
}

int special(struct char_data * ch, int cmd, char *arg)
{
  struct obj_data *i;
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
      if (GET_ROOM_SPEC(veh->in_room) (ch, &world[real_room(veh->in_room->number)], cmd, arg))
        return 1;
  if (GET_ROOM_SPEC(ch->in_room) != NULL)
    if (GET_ROOM_SPEC(ch->in_room) (ch, &world[real_room(ch->in_room->number)], cmd, arg))
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
  if (ch->in_room || ch->in_veh) {
    FOR_ITEMS_AROUND_CH(ch, i)
      if (GET_OBJ_SPEC(i) != NULL && !(ch->in_veh && ch->vfront != i->vfront))
        if (GET_OBJ_SPEC(i) (ch, i, cmd, arg))
          return 1;
    /* special in mobile present? */
    if (ch->in_room)
    {
      for (struct char_data *k = ch->in_room->people; k; k = k->next_in_room) {
        if (GET_MOB_SPEC(k) != NULL) {
          bool spec_returned_true = GET_MOB_SPEC(k) (ch, k, cmd, arg);
          if (IS_SENATOR(ch) && PRF_FLAGGED(ch, PRF_ROLLS) && !PRF_FLAGGED(ch, PRF_NOHASSLE))
            send_to_char(ch, "^LEvaluation of spec for %s returned %s.^n\r\n", GET_CHAR_NAME(k), spec_returned_true ? "TRUE" : "FALSE");
          if (spec_returned_true)
            return 1;
        }
        if (mob_index[GET_MOB_RNUM(k)].sfunc != NULL) {
          bool spec_returned_true = GET_MOB_SPEC(k) (ch, k, cmd, arg);
          if (IS_SENATOR(ch) && PRF_FLAGGED(ch, PRF_ROLLS) && !PRF_FLAGGED(ch, PRF_NOHASSLE))
            send_to_char(ch, "^LEvaluation of secondary spec for %s returned %s.^n\r\n", GET_CHAR_NAME(k), spec_returned_true ? "TRUE" : "FALSE");
          if ((mob_index[GET_MOB_RNUM(k)].sfunc) (ch, k, cmd, arg))
            return 1;
        }
      }
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
      // TODO: Desc is leaked?
      if (k->character)
        k->character->desc = NULL;
      // TODO: Character is leaked?
      k->character = NULL;
      // Original is not leaked since it's inherited by the new connection.
      k->original = NULL;
    } else if (k->character && (GET_IDNUM(k->character) == id)) {
      if (!target && STATE(k) == CON_PLAYING) {
        SEND_TO_Q("\r\nThis body has been usurped!\r\n", k);
        target = k->character;
        mode = USURP;
      }
      // TODO: Desc is leaked?
      k->character->desc = NULL;
      // TODO: Character is leaked?
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
    if (ch->in_room)
      char_from_room(ch);
    char_to_room(ch, &world[1]);
    extract_char(ch);
  }

  /* no target for swicthing into was found - allow login to continue */
  if (!target)
    return 0;

  /* Okay, we've found a target.  Connect d to target. */
  extract_char(d->character); /* get rid of the old char */
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
    snprintf(buf, sizeof(buf), "%s has reconnected.",
            GET_CHAR_NAME(d->character));
    mudlog(buf, d->character, LOG_CONNLOG, TRUE);
    log_vfprintf("[CONNLOG: %s has reconnected from %s]", GET_CHAR_NAME(d->character), d->host);
    break;
  case USURP:
    SEND_TO_Q("You take over your own body, already in use!\r\n", d);
    act("$n shakes $s head to clear it.",
        TRUE, d->character, 0, 0, TO_ROOM);
    snprintf(buf, sizeof(buf), "%s has re-logged in ... disconnecting old socket.",
            GET_CHAR_NAME(d->character));
    mudlog(buf, d->character, LOG_CONNLOG, TRUE);
    log_vfprintf("[CONNLOG: %s reconnecting from %s]", GET_CHAR_NAME(d->character), d->host);
    if (d->character->persona)
    {
      snprintf(buf, sizeof(buf), "%s depixelizes and vanishes from the host.\r\n", d->character->persona->name);
      send_to_host(d->character->persona->in_host, buf, d->character->persona, TRUE);
      extract_icon(d->character->persona);
      d->character->persona = NULL;
      PLR_FLAGS(d->character).RemoveBit(PLR_MATRIX);
    } else if (PLR_FLAGGED(d->character, PLR_MATRIX)) {
      for (struct char_data *temp = d->character->in_room->people; temp; temp = temp->next_in_room)
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
    snprintf(buf, sizeof(buf), "%s has reconnected.",
            GET_CHAR_NAME(d->character));
    mudlog(buf, d->character, LOG_CONNLOG, TRUE);
    log_vfprintf("[CONNLOG: %s has reconnected from %s]", GET_CHAR_NAME(d->character), d->host);
    break;
  }

  //This is required for autoconnecting clients like MUSHClient
  //in order to do telnet protocol options renegotiation on
  //reconnection states. Otherwise things like MXP and 256
  //color support break. It doesn't affect other clients.
  d->pProtocol->bRenegotiate = TRUE;

  // KaVir's protocol snippet.
  MXPSendTag( d, "<VERSION>" );

  return 1;
}

/* deal with newcomers and other non-playing sockets */
void nanny(struct descriptor_data * d, char *arg)
{
  char buf[1000 ];
  int load_result = NOWHERE;
  char tmp_name[MAX_INPUT_LENGTH];
  extern vnum_t mortal_start_room;
  extern vnum_t frozen_start_room;
  extern vnum_t newbie_start_room;
  extern int max_bad_pws;
  extern bool House_can_enter(struct char_data *ch, vnum_t vnum);
  vnum_t load_room = NOWHERE;
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
  case CON_HELPEDIT:
    helpedit_parse(d, arg);
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
        else {
          SEND_TO_Q("Names must be standard letters with no spaces, numbers, or punctuation, and cannot be a reserved word.\r\nName: ", d);
        }


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

        snprintf(buf, sizeof(buf), "Welcome back. Enter your password. Not %s? Enter 'abort' to try a different name. ", CAP(tmp_name));
        SEND_TO_Q(buf, d);
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

          // Create and zero out their player_specials.
          DELETE_IF_EXTANT(d->character->player_specials);
          d->character->player_specials = new player_special_data;

          // Initialize their ignore data structure, which all PCs have.
          GET_IGNORE_DATA(d->character) = new IgnoreData(d->character);

          d->character->desc = d;
        }

        d->character->player.char_name = new char[strlen(tmp_name) + 1];
        strcpy(d->character->player.char_name, CAP(tmp_name));

        snprintf(buf, sizeof(buf), "Did I get that right, %s (Y/N)? ", tmp_name);
        SEND_TO_Q(buf, d);
        STATE(d) = CON_NAME_CNFRM;
      }
    }
    break;
  case CON_NAME_CNFRM:          /* wait for conf. of new name    */
    if (UPPER(*arg) == 'Y') {
      if (isbanned(d->host) >= BAN_NEW) {
        snprintf(buf, sizeof(buf), "Request for new char %s denied from [%s] (siteban)", GET_CHAR_NAME(d->character), d->host);
        mudlog(buf, d->character, LOG_BANLOG, TRUE);
        SEND_TO_Q("Sorry, new characters are not allowed from your site.\r\n", d);
        STATE(d) = CON_CLOSE;
        return;
      }
      if (restrict_mud) {
        if (restrict_mud == LVL_MAX)
          SEND_TO_Q("The mud is being reconfigured.  Try again a bit later.\r\n", d);
        else {
          SEND_TO_Q(WIZLOCK_MSG, d);
        }
        snprintf(buf, sizeof(buf), "Request for new char %s denied from %s (wizlock)", GET_CHAR_NAME(d->character), d->host);
        mudlog(buf, d->character, LOG_CONNLOG, TRUE);
        STATE(d) = CON_CLOSE;
        return;
      }
      SEND_TO_Q("New character.\r\n", d);
      snprintf(buf, sizeof(buf), "Give me a password for %s: ",
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

    // Clear their idle counter so they don't get dropped mysteriously.
    d->idle_tics = 0;

    if (!*arg) {
      close_socket(d);
      return;
    }

    if (str_cmp(arg, "abort") == 0) {
      /* turn echo back on */
      echo_on(d);

      d->character->desc = NULL;
      extract_char(d->character);
      d->character = NULL;

      SEND_TO_Q("OK, let's try a different name.\r\n\r\nWhat's your handle, chummer? ", d);
      STATE(d) = CON_GET_NAME;
      return;
    } else {
      if (!validate_and_update_password(arg, GET_PASSWD(d->character))) {
        snprintf(buf, sizeof(buf), "Bad PW: %s [%s]", GET_CHAR_NAME(d->character), d->host);
        mudlog(buf, d->character, LOG_CONNLOG, TRUE);
        GET_BAD_PWS(d->character)++;
        d->character->in_room = &world[MAX(0, real_room(GET_LAST_IN(d->character)))];
        playerDB.SaveChar(d->character, GET_LOADROOM(d->character));
        if (++(d->bad_pws) >= max_bad_pws) {    /* 3 strikes and you're out. */
          SEND_TO_Q("Wrong password... disconnecting.\r\n", d);
          STATE(d) = CON_CLOSE;
        } else {
          char oopsbuf[500];
          snprintf(oopsbuf, sizeof(oopsbuf), "That's not the right password for the character '%s'.\r\nEnter your password, or type ABORT: ", GET_CHAR_NAME(d->character));
          SEND_TO_Q(oopsbuf, d);
        }
        return;
      }

      /* turn echo back on */
      echo_on(d);

      // Commit the password to DB on the assumption it's changed.
      char query_buf[2048];
#ifdef NOCRYPT
      char prepare_quotes_buf[2048];
      snprintf(query_buf, sizeof(query_buf), "UPDATE pfiles SET password='%s' WHERE idnum=%ld;",
              prepare_quotes(prepare_quotes_buf, GET_PASSWD(d->character), sizeof(prepare_quotes_buf) / sizeof(prepare_quotes_buf[0])),
              GET_IDNUM(d->character));
#else
      snprintf(query_buf, sizeof(query_buf), "UPDATE pfiles SET password='%s' WHERE idnum=%ld;", GET_PASSWD(d->character), GET_IDNUM(d->character));
#endif
      mysql_wrapper(mysql, query_buf);

      load_result = GET_BAD_PWS(d->character);
      GET_BAD_PWS(d->character) = 0;

      d->character->in_room = &world[MAX(0, real_room(GET_LAST_IN(d->character)))];
      playerDB.SaveChar(d->character, GET_LOADROOM(d->character));

      // TODO: Don't these returns leak memory by not cleaning up d->character?
      if (isbanned(d->host) == BAN_SELECT &&
          !PLR_FLAGGED(d->character, PLR_SITEOK)) {
        SEND_TO_Q("Sorry, this char has not been cleared for login from your site!\r\n", d);
        STATE(d) = CON_CLOSE;
        snprintf(buf, sizeof(buf), "Connection attempt for %s denied from %s", GET_CHAR_NAME(d->character), d->host);
        mudlog(buf, d->character, LOG_BANLOG, TRUE);
        return;
      }
      if (!access_level(d->character, restrict_mud)) {
        if (restrict_mud == LVL_MAX)
          SEND_TO_Q("The mud is about to reboot. Please try again in a few minutes.\r\n", d);
        else
          SEND_TO_Q("The game is temporarily restricted.. try again later.\r\n", d);
        STATE(d) = CON_CLOSE;
        snprintf(buf, sizeof(buf), "Request for login denied for %s [%s] (wizlock)", GET_CHAR_NAME(d->character), d->host);
        mudlog(buf, d->character, LOG_CONNLOG, TRUE);
        return;
      }
      /* check and make sure no other copies of this player are logged in */
      if (perform_dupe_check(d))
        return;

      if (PLR_FLAGGED(d->character, PLR_SITE_HIDDEN)) {
#ifdef USE_PRIVATE_CE_WORLD
        extern void rewrite_hidden_site(struct descriptor_data *d);
        SEND_TO_Q("\r\n^GFYI, you've connected to a site-hidden character. Rewriting your site now.^n\r\n", d);
        rewrite_hidden_site(d);
#else
        SEND_TO_Q("\r\n^RFYI, you've connected to a site-hidden character, but no site rewriting logic has been implemented for your game. Contact staff.^n\r\n", d);
#endif
      }

      /* undo it just in case they are set */
      PLR_FLAGS(d->character).RemoveBits(PLR_WRITING, PLR_MAILING,
                                         PLR_EDITING,
                                         PLR_SPELL_CREATE, PLR_CUSTOMIZE,
                                         PLR_PROJECT, PLR_MATRIX, ENDBIT);

      if (IS_SENATOR(d->character))
        SEND_TO_Q(imotd, d);
      else
        SEND_TO_Q(motd, d);

      if(PLR_FLAGGED(d->character,PLR_NOT_YET_AUTHED))
        snprintf(buf, sizeof(buf), "%s has connected (UNAUTHORIZED).",
                GET_CHAR_NAME(d->character));
      else
        snprintf(buf, sizeof(buf), "%s has connected.",
                GET_CHAR_NAME(d->character));
      DELETE_ARRAY_IF_EXTANT(d->character->player.host);
      d->character->player.host = str_dup(d->host);
      playerDB.SaveChar(d->character);
      mudlog(buf, d->character, LOG_CONNLOG, TRUE);
      log_vfprintf("[CONNLOG: %s connecting from %s]", GET_CHAR_NAME(d->character), d->host);
      if (load_result) {
        snprintf(buf, sizeof(buf), "\r\n\r\n"
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
      STATE(d) = CON_CCREATE;
      init_create_vars(d);
      ccr_pronoun_menu(d);
    } else {
      if (STATE(d) != CON_CHPWD_VRFY)
        d->character = playerDB.LoadChar(GET_CHAR_NAME(d->character), TRUE);
      SEND_TO_Q("\r\nDone.\r\n", d);
      if (PLR_FLAGGED(d->character,PLR_NOT_YET_AUTHED)) {
        playerDB.SaveChar(d->character);
        SEND_TO_Q(MENU, d);
        STATE(d) = CON_MENU;
      }
      if (dirty_password) { // STATE(d) is changed directly above this after all...
        char query_buf[2048];
#ifdef NOCRYPT
        char prepare_quotes_buf[2048];
        snprintf(query_buf, sizeof(query_buf), "UPDATE pfiles SET password='%s' WHERE idnum=%ld;",
                prepare_quotes(prepare_quotes_buf, GET_PASSWD(d->character), sizeof(prepare_quotes_buf) / sizeof(prepare_quotes_buf[0])),
                GET_IDNUM(d->character));
#else
        snprintf(query_buf, sizeof(query_buf), "UPDATE pfiles SET password='%s' WHERE idnum=%ld;", GET_PASSWD(d->character), GET_IDNUM(d->character));
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
      if (!GET_LEVEL(d->character)) {
        // Copypasta of char init code to prevent them from showing up with no stats, paralyzed in front of Dante's.
        if (GET_ARCHETYPAL_MODE(d->character)) {
          load_room = archetypes[GET_ARCHETYPAL_TYPE(d->character)]->start_room;
          // Correct for invalid archetype start rooms.
          if (real_room(load_room) == NOWHERE) {
            snprintf(buf, sizeof(buf), "WARNING: Start room %ld for archetype %s does not exist!",
                     load_room,
                     archetypes[GET_ARCHETYPAL_TYPE(d->character)]->name);
            mudlog(buf, NULL, LOG_SYSLOG, TRUE);
            load_room = newbie_start_room;
          }
          do_start(d->character, FALSE);
        } else {
          load_room = newbie_start_room;
          do_start(d->character, TRUE);
        }

        playerDB.SaveChar(d->character, load_room);
      }
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
        char char_name[strlen(GET_CHAR_NAME(d->character))+1];
        strcpy(char_name, GET_CHAR_NAME(d->character));
        free_char(d->character);
        delete d->character;

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
      // Wipe out various pointers related to game state and recalculate carry weight.
      reset_char(d->character);
      PLR_FLAGS(d->character).RemoveBit(PLR_CUSTOMIZE);
      d->character->next = character_list;
      character_list = d->character;
      d->character->player.time.logon = time(0);

      // Rewrote the entire janky-ass load room tree.
      // First: Frozen characters. They go to the frozen start room.
      if (PLR_FLAGGED(d->character, PLR_FROZEN))
        load_room = real_room(frozen_start_room);

      // Next: Unauthed (chargen) characters. They go to the start of their chargen areas.
      else if (PLR_FLAGGED(d->character, PLR_NOT_YET_AUTHED)) {
        if (!GET_ARCHETYPAL_MODE(d->character) || (load_room = real_room(archetypes[GET_ARCHETYPAL_TYPE(d->character)]->start_room)) == NOWHERE)
          load_room = real_room(newbie_start_room);
      }

      // Next: Characters who have GET_LAST_IN rooms load in there.
      else if ((load_room = GET_LAST_IN(d->character)) != NOWHERE)
        load_room = real_room(load_room);

      // Next: Characters who have load rooms rooms load in there.
      else if ((load_room = GET_LOADROOM(d->character)) != NOWHERE)
        load_room = real_room(load_room);

      // Fallthrough: No start room? Mortal start room. Functions like an ELSE to the above, but also catches invalid rooms from above.
      if (load_room == NOWHERE)
        load_room = real_room(mortal_start_room);

      // Post-processing: Non-newbies don't get to start in the newbie loadroom-- rewrite their loadroom value.
      if (load_room == real_room(RM_NEWBIE_LOADROOM) && !PLR_FLAGGED(d->character, PLR_NEWBIE))
        load_room = real_room(mortal_start_room);

      /*
      // Post-processing: Staff with invalid or mort-start-room loadrooms instead load in at their defined loadroom.
      if (IS_SENATOR(d->character) && (load_room <= 0 || load_room == real_room(mortal_start_room)))
        load_room = real_room(GET_LOADROOM(d->character));
      */

      // Post-processing: Characters who are trying to load into a house get rejected if they're not allowed in there.
      if (ROOM_FLAGGED(&world[load_room], ROOM_HOUSE) && !House_can_enter(d->character, world[load_room].number))
        load_room = real_room(mortal_start_room);

      // Post-processing: Invalid load room characters go to the newbie or mortal start rooms.
      if (load_room == NOWHERE) {
        if (PLR_FLAGGED(d->character, PLR_NEWBIE)) {
          load_room = real_room(RM_NEWBIE_LOADROOM);
        } else
          load_room = real_room(mortal_start_room);
      }

      // First-time login. This overrides the above, but it's for a good cause.
      if (!GET_LEVEL(d->character)) {
        if (GET_ARCHETYPAL_MODE(d->character)) {
          load_room = real_room(archetypes[GET_ARCHETYPAL_TYPE(d->character)]->start_room);
          // Correct for invalid archetype start rooms.
          if (load_room == NOWHERE) {
            snprintf(buf, sizeof(buf), "WARNING: Start room %ld for archetype %s does not exist!",
                     archetypes[GET_ARCHETYPAL_TYPE(d->character)]->start_room,
                     archetypes[GET_ARCHETYPAL_TYPE(d->character)]->name);
            mudlog(buf, NULL, LOG_SYSLOG, TRUE);
            load_room = real_room(newbie_start_room);
          }
          do_start(d->character, FALSE);
        } else {
          load_room = real_room(newbie_start_room);
          do_start(d->character, TRUE);
        }

        playerDB.SaveChar(d->character, GET_ROOM_VNUM(&world[load_room]));
        send_to_char(START_MESSG, d->character);
      } else {
        send_to_char(WELC_MESSG, d->character);
      }
      if (d->character->player_specials->saved.last_veh) {
        for (struct veh_data *veh = veh_list; veh; veh = veh->next)
          if (veh->idnum == d->character->player_specials->saved.last_veh && veh->damage < VEH_DAM_THRESHOLD_DESTROYED) {
            if (!veh->seating[CH->vfront])
              CH->vfront = !CH->vfront;
            char_to_veh(veh, d->character);
            break;
          }
      }
      if (!d->character->in_veh)
        char_to_room(d->character, &world[load_room]);
      act("$n has entered the game.", TRUE, d->character, 0, 0, TO_ROOM);
      snprintf(buf, sizeof(buf), "%s has entered the game.", GET_CHAR_NAME(d->character));
      mudlog(buf, d->character, LOG_CONNLOG, TRUE);

      STATE(d) = CON_PLAYING;

      // KaVir's protocol snippet.
      MXPSendTag( d, "<VERSION>" );

      if (!str_cmp(GET_EMAIL(d->character), "not set")) {
        send_to_char("\r\n^YNotice:^n This character hasn't been registered yet! Please see ^WHELP REGISTER^n for information.^n\r\n\r\n", d->character);
      }

      look_at_room(d->character, 0, 0);
      d->prompt_mode = 1;
      /* affect total to make cyberware update stats */
      affect_total(d->character);

      // Regenerate their subscriber list.
      for (struct veh_data *veh = veh_list; veh; veh = veh->next) {
        if (veh->sub && GET_IDNUM(d->character) == veh->owner) {
          struct veh_data *f = NULL;
          for (f = d->character->char_specials.subscribe; f; f = f->next_sub) {
            if (f == veh)
              break;
          }
          if (!f) {
            veh->next_sub = d->character->char_specials.subscribe;

            // Doubly link it into the list.
            if (d->character->char_specials.subscribe)
              d->character->char_specials.subscribe->prev_sub = veh;

            d->character->char_specials.subscribe = veh;
          }
        }
      }

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
      snprintf(buf, sizeof(buf), "\r\nYOU ARE ABOUT TO DELETE THIS CHARACTER (%s) PERMANENTLY. THIS CANNOT BE UNDONE.\r\n", GET_CHAR_NAME(d->character));
      SEND_TO_Q(buf, d);
      snprintf(buf, sizeof(buf), "\r\n\r\nIf you're ABSOLUTELY SURE, type your character's name (%s) to confirm, or anything else to abort: ", GET_CHAR_NAME(d->character));
      SEND_TO_Q(buf, d);
      if (STATE(d) == CON_DELCNF1)
        STATE(d) = CON_DELCNF2;
      else
        STATE(d) = CON_QDELCONF2;
    }
    break;

  case CON_DELCNF2:
  case CON_QDELCONF2:
    if (!str_cmp(arg, GET_CHAR_NAME(d->character))) {
      if (PLR_FLAGGED(d->character, PLR_FROZEN)) {
        SEND_TO_Q("You try to kill yourself, but the ice stops you.\r\n", d);
        SEND_TO_Q("Character not deleted.\r\n\r\n", d);
        STATE(d) = CON_CLOSE;
        return;
      }

      DeleteChar(GET_IDNUM(d->character));

      snprintf(buf, sizeof(buf), "Character '%s' deleted!\r\nGoodbye.\r\n",
              GET_CHAR_NAME(d->character));
      SEND_TO_Q(buf, d);
      snprintf(buf, sizeof(buf), "%s (lev %d) has self-deleted.",
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

#ifdef LOG_COMMANDS
void log_command(struct char_data *ch, const char *argument, const char *tcname) {
  if (!ch || IS_NPC(ch))
    return;

  // Discard directional commands and other high-noise things that can't affect other players.
  const char *discard_commands[] = {
    "north", "south", "east", "west", "up", "down",
    "northeast", "ne",
    "southeast", "se",
    "southwest", "sw",
    "northwest", "nw",
    "enter", "leave",
    "score", "equipment", "inventory", "status", "affects",
    "look", "scan", "search", "who", "probe",
    "alias", "help",
    "hail", "push",
    "radio", "phone",
    "drive", "speed",
    "stand", "sit",
    "nod", "list", "info",
    "open", "close", "give", "receive", "buy", "sell",
    "wear", "remove", "draw", "holster",
    "cast", "kill", "hit", "shoot", "kick", "get", "put",
    "\n"
  };
  for (int i = 0; *discard_commands[i] != '\n'; i++)
    if (str_str(discard_commands[i], argument))
      return;

  // Extract location.
  char location_buf[500];
  if (PLR_FLAGGED(ch, PLR_MATRIX)) {
    strlcpy(location_buf, "hitching unknown", sizeof(location_buf));

    if (ch->persona && ch->persona->in_host)
      snprintf(location_buf, sizeof(location_buf), "mtx %ld", matrix[ch->persona->in_host].vnum);
    else if (get_ch_in_room(ch)) {
      for (struct char_data *targ = get_ch_in_room(ch)->people; targ; targ = targ->next_in_room)
        if (targ != ch && PLR_FLAGGED(targ, PLR_MATRIX))
          snprintf(location_buf, sizeof(location_buf), "hitching %s", GET_CHAR_NAME(targ));
    }
  } else if (ch->in_room)
    snprintf(location_buf, sizeof(location_buf), "%ld", GET_ROOM_VNUM(ch->in_room));
  else if (ch->in_veh)
    snprintf(location_buf, sizeof(location_buf), "veh #%ld (@%ld)", ch->in_veh->idnum, GET_ROOM_VNUM(get_ch_in_room(ch)));

  // Compose name string.
  char name_buf[250];
  if (ch->desc && ch->desc->original)
    snprintf(name_buf, sizeof(name_buf), "%s (as %s)", GET_CHAR_NAME(ch->desc->original), GET_NAME(ch));
  else
    strncpy(name_buf, GET_CHAR_NAME(ch), sizeof(name_buf) - 1);

  // Write the command to the buffer.
  char cmd_buf[MAX_INPUT_LENGTH * 3];
  snprintf(cmd_buf, sizeof(cmd_buf), "COMMANDLOG: %s @ %s: %s", name_buf, location_buf, argument);

  // TODO: Save to a file based on the PC's name.
  log(cmd_buf);
}
#endif

// Attempts to map common typos to their actual commands.
#define COMMAND_ALIAS(typo, corrected)   if (strncmp(arg, (typo), strlen(arg)) == 0) { return find_command_in_x((corrected), cmd_info); }

int fix_common_command_fuckups(const char *arg, struct command_info *cmd_info) {
  // Doubled up and otherwise misspelled movement for those impatient-ass people.
  COMMAND_ALIAS("nn", "north");
  COMMAND_ALIAS("ee", "east");
  COMMAND_ALIAS("ss", "south");
  COMMAND_ALIAS("ww", "west");
  COMMAND_ALIAS("ws", "southwest");
  COMMAND_ALIAS("wn", "northwest");
  COMMAND_ALIAS("en", "northeast");
  COMMAND_ALIAS("es", "southeast");
  COMMAND_ALIAS("sse", "southeast");
  COMMAND_ALIAS("ssw", "southwest");
  COMMAND_ALIAS("norht", "north"); // this one happened 18 times
  COMMAND_ALIAS("esat", "east"); // this one only 8

  // Common typos and fuckups.
  COMMAND_ALIAS("receieve", "receive");
  COMMAND_ALIAS("recieve", "receive");
  COMMAND_ALIAS("dorp", "drop");
  COMMAND_ALIAS("weild", "wield");
  COMMAND_ALIAS("unsheathe", "draw");
  COMMAND_ALIAS("prove", "probe");
  COMMAND_ALIAS("chekc", "check");
  COMMAND_ALIAS("opend", "open");
  COMMAND_ALIAS("leaev", "leave");
  COMMAND_ALIAS("lisy", "list");
  COMMAND_ALIAS("swith", "switch");
  COMMAND_ALIAS("drie", "drive");
  COMMAND_ALIAS("but", "put");
  COMMAND_ALIAS("cyberwear", "cyberware");
  COMMAND_ALIAS("biowear", "bioware");
  COMMAND_ALIAS("lsit", "list");
  COMMAND_ALIAS("ist", "list");
  COMMAND_ALIAS("out", "put");
  COMMAND_ALIAS("ivn", "inventory");
  COMMAND_ALIAS("hoslter", "holster");
  COMMAND_ALIAS("stnad", "stand");
  COMMAND_ALIAS("saerch", "search");
  COMMAND_ALIAS("serach", "search");
  COMMAND_ALIAS("shot", "shoot");
  COMMAND_ALIAS("trian", "train");
  COMMAND_ALIAS("opne", "open");
  COMMAND_ALIAS("recpa", "recap");
  COMMAND_ALIAS("scoe", "score");
  COMMAND_ALIAS("hial", "hail");
  COMMAND_ALIAS("haul", "hail");
  COMMAND_ALIAS("clsoe", "close");

  // Combat stuff.
  COMMAND_ALIAS("attack", "kill");
  COMMAND_ALIAS("stab", "kill");
  COMMAND_ALIAS("unload", "eject");
  COMMAND_ALIAS("fire", "shoot");

  // Misc aliases.
  COMMAND_ALIAS("taxi", "hail");
  COMMAND_ALIAS("cab", "hail");
  COMMAND_ALIAS("yes", "nod");
  COMMAND_ALIAS("setup", "unpack");
  COMMAND_ALIAS("ability", "abilities");
  COMMAND_ALIAS("guest", "house");
  COMMAND_ALIAS("unready", "ready");
  COMMAND_ALIAS("strap", "holster"); // Not sure about this one.
  COMMAND_ALIAS("deck", "software");
  COMMAND_ALIAS("email", "register");
  COMMAND_ALIAS("clothing", "equipment");
  COMMAND_ALIAS("armor", "equipment");
  COMMAND_ALIAS("programs", "software");
  COMMAND_ALIAS("bank", "balance");
  COMMAND_ALIAS("recall", "recap");

  // Toggles.
  COMMAND_ALIAS("settings", "toggle");
  COMMAND_ALIAS("preferences", "toggle");
  COMMAND_ALIAS("options", "toggle");

  // Job interaction commands.
  COMMAND_ALIAS("endjob", "endrun");
  COMMAND_ALIAS("resign", "endrun");
  COMMAND_ALIAS("work",   "recap");
  COMMAND_ALIAS("recao",  "recap");

  // one of the most common commands, although people eventually learn to just use 'l'
  COMMAND_ALIAS("olok", "look");
  COMMAND_ALIAS("lok", "look");
  COMMAND_ALIAS("loko", "look");
  COMMAND_ALIAS("loook", "look");
  COMMAND_ALIAS("ook", "look");

  // equipment seems to give people a lot of trouble
  COMMAND_ALIAS("unwield", "remove");
  COMMAND_ALIAS("unwear", "remove");
  COMMAND_ALIAS("unequip", "remove");

  // Door-unlocking and manipulation commands.
  COMMAND_ALIAS("pick", "bypass");
  COMMAND_ALIAS("hack", "bypass");
  COMMAND_ALIAS("poen", "open");
  COMMAND_ALIAS("oepn", "open");

  // Must be after 'pick'
  COMMAND_ALIAS("pickup", "get");

  // Commands from other games.
  COMMAND_ALIAS("bamfin", "poofin");
  COMMAND_ALIAS("bamfout", "poofout");
  COMMAND_ALIAS("sacrifice", "junk");
  COMMAND_ALIAS("trash", "junk");
  COMMAND_ALIAS("inspect", "probe");
  COMMAND_ALIAS("worth", "balance");
  COMMAND_ALIAS("money", "balance");
  COMMAND_ALIAS("nuyen", "balance");
  COMMAND_ALIAS("cash", "balance");
  COMMAND_ALIAS("gold", "balance");
  COMMAND_ALIAS("suggest", "idea");
  COMMAND_ALIAS("chat", "ooc");
  COMMAND_ALIAS("gossip", "ooc");
  COMMAND_ALIAS("purchase", "buy");
  COMMAND_ALIAS("stats", "score");
  COMMAND_ALIAS("attributes", "score");
  COMMAND_ALIAS("make", "create");

  // Alternate spellings.
  COMMAND_ALIAS("customise", "customize");

  // Common staff goofs.
  COMMAND_ALIAS("odelete", "idelete");
  COMMAND_ALIAS("oload", "iload");
  COMMAND_ALIAS("oedit", "iedit");
  COMMAND_ALIAS("olist", "ilist");
  COMMAND_ALIAS("ostat", "vstat");
  COMMAND_ALIAS("mstat", "vstat");
  COMMAND_ALIAS("qstat", "vstat");
  COMMAND_ALIAS("sstat", "vstat");

  COMMAND_ALIAS("health", "hp");
  COMMAND_ALIAS("karma", "tke");

  COMMAND_ALIAS("stealth", "sneak");
  COMMAND_ALIAS("board", "enter");
  COMMAND_ALIAS("gps", "gridguide");
  COMMAND_ALIAS("detach", "unattach");

  COMMAND_ALIAS("powers", "abilities");

  COMMAND_ALIAS("scna", "scan");
  COMMAND_ALIAS("sacn", "scan");
  COMMAND_ALIAS("sya", "say");

  COMMAND_ALIAS("speak", "language");


  // Found nothing, return the failure code.
  return -1;
}
#undef MAP_TYPO
