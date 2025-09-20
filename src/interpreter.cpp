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
#include "newhouse.hpp"
#include "creative_works.hpp"
#include "channels.hpp"
#include "vehicles.hpp"
#include "dblist.hpp"
#include "player_exdescs.hpp"
#include "pets.hpp"
#include "gmcp.hpp"

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
void cfedit_parse(struct descriptor_data *d, const char *arg);
void dbuild_parse(struct descriptor_data *d, const char *arg);
void pbuild_parse(struct descriptor_data *d, const char *arg);
void spedit_parse(struct descriptor_data *d, const char *arg);
void aedit_parse(struct descriptor_data *d, const char *arg);
void houseedit_apartment_parse(struct descriptor_data *d, const char *arg);
void houseedit_complex_parse(struct descriptor_data *d, const char *arg);
void free_shop(struct shop_data *shop);
void free_quest(struct quest_data *quest);
void init_parse(struct descriptor_data *d, char *arg);
void submersion_parse(struct descriptor_data *d, char *arg);
void vehcust_parse(struct descriptor_data *d, char *arg);
void pocketsec_parse(struct descriptor_data *d, char *arg);
void faction_edit_parse(struct descriptor_data *d, const char *arg);
int fix_common_command_fuckups(const char *arg, struct command_info *cmd_info);
const char *get_descriptor_fingerprint(struct descriptor_data *d);

#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
extern void verify_every_pointer_we_can_think_of();
#endif

void verify_data(struct char_data *ch, const char *line, int cmd, int subcmd, const char *section);

#ifdef LOG_COMMANDS
void log_command(struct char_data *ch, const char *argument, const char *tcname);
#endif

// for spell creation
void cedit_parse(struct descriptor_data *d, char *arg);

extern void affect_total(struct char_data * ch);
extern void mag_menu_system(struct descriptor_data * d, char *arg);
extern void ccr_pronoun_menu(struct descriptor_data *d);
extern void disable_xterm_256(descriptor_t *apDescriptor);
extern void enable_xterm_256(descriptor_t *apDescriptor);
extern void recalculate_whole_game_players_in_zone();

// Some commands are not supported but are common in other games. We handle those with these SCMDs.
#define SCMD_INTRODUCE 0

/* prototypes for all do_x functions. */
ACMD_DECLARE(do_olcon);
ACMD_DECLARE(do_abilityset);
ACMD_DECLARE(do_accept);
ACMD_DECLARE(do_account);
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
ACMD_DECLARE(do_banvpn);
ACMD_DECLARE(do_bash);
ACMD_DECLARE(do_bioware);
ACMD_DECLARE(do_bond);
ACMD_DECLARE(do_boost);
ACMD_DECLARE(do_break);
ACMD_DECLARE(do_broadcast);
ACMD_DECLARE(do_build);
ACMD_DECLARE(do_brief);
ACMD_DECLARE(do_cast);
ACMD_DECLARE(do_charge);
ACMD_DECLARE(do_changelog);
ACMD_DECLARE(do_cheatlog);
ACMD_DECLARE(do_chipload);
ACMD_DECLARE(do_cleanup);
ACMD_DECLARE(do_cleanse);
ACMD_DECLARE(do_closecombat);
ACMD_DECLARE(do_commands);
ACMD_DECLARE(do_compact);
ACMD_DECLARE(do_conceal_reveal);
ACMD_DECLARE(do_consent);
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
ACMD_DECLARE(do_forms);
ACMD_DECLARE(do_credits);
ACMD_DECLARE(do_customize);
ACMD_DECLARE(do_cyberware);
ACMD_DECLARE(do_coredump);
ACMD_DECLARE(do_count);
ACMD_DECLARE(do_date);
ACMD_DECLARE(do_dc);
ACMD_DECLARE(do_deactivate);
ACMD_DECLARE(do_decline);
ACMD_DECLARE(do_decorate);
ACMD_DECLARE(do_default);
ACMD_DECLARE(do_delete);
ACMD_DECLARE(do_describe);
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
ACMD_DECLARE(do_download_headware);
ACMD_DECLARE(do_drag);
ACMD_DECLARE(do_drink);
ACMD_DECLARE(do_drive);
ACMD_DECLARE(do_drop);
ACMD_DECLARE(do_drugs);
ACMD_DECLARE(do_docwagon);
ACMD_DECLARE(do_dw_retrieve);
ACMD_DECLARE(do_eat);
ACMD_DECLARE(do_echo);
ACMD_DECLARE(do_eject);
ACMD_DECLARE(do_elemental);
ACMD_DECLARE(do_enter);
ACMD_DECLARE(do_endrun);
ACMD_DECLARE(do_equipment);
ACMD_DECLARE(do_examine);
ACMD_DECLARE(do_exdesc);
ACMD_DECLARE(do_exit);
ACMD_DECLARE(do_exits);
ACMD_DECLARE(do_factions);
ACMD_DECLARE(do_flee);
ACMD_DECLARE(do_flip);
ACMD_DECLARE(do_flyto);
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
ACMD_DECLARE(do_hit);
ACMD_DECLARE(do_highlight);
ACMD_DECLARE(do_house);
ACMD_DECLARE(do_houseedit);
ACMD_DECLARE(do_hp);
ACMD_DECLARE(do_iclist);
ACMD_DECLARE(do_ignore);
ACMD_DECLARE(do_ilist);
ACMD_DECLARE(do_iload);
ACMD_DECLARE(do_imagelink);
ACMD_DECLARE(do_info);
ACMD_DECLARE(do_initiate);
ACMD_DECLARE(do_submerse);
ACMD_DECLARE(do_insult);
ACMD_DECLARE(do_inventory);
ACMD_DECLARE(do_invis);
ACMD_DECLARE(do_items);
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
ACMD_DECLARE(do_look_while_rigging);
ACMD_DECLARE(do_logwatch);
ACMD_DECLARE(do_manifest);
ACMD_DECLARE(do_map);
ACMD_DECLARE(do_masking);
ACMD_DECLARE(do_makenerps);
ACMD_DECLARE(do_memory);
ACMD_DECLARE(do_message_history);
ACMD_DECLARE(do_metamagic);
ACMD_DECLARE(do_metrics);
ACMD_DECLARE(do_mobs);
ACMD_DECLARE(do_mode);
ACMD_DECLARE(do_move);
ACMD_DECLARE(do_mlist);
ACMD_DECLARE(do_networth);
ACMD_DECLARE(do_new_echo);
ACMD_DECLARE(do_not_here);
ACMD_DECLARE(do_oocdisable);
ACMD_DECLARE(do_order);
ACMD_DECLARE(do_packup);
ACMD_DECLARE(do_page);
ACMD_DECLARE(do_patch);
ACMD_DECLARE(do_payout);
ACMD_DECLARE(do_perfmon);
ACMD_DECLARE(do_penalties);
ACMD_DECLARE(do_pgroup);
ACMD_DECLARE(do_pgset);
ACMD_DECLARE(do_photo);
ACMD_DECLARE(do_playerrolls);
ACMD_DECLARE(do_pockets);
ACMD_DECLARE(do_policy);
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
ACMD_DECLARE(do_push_while_rigging);
ACMD_DECLARE(do_put);
ACMD_DECLARE(do_qedit);
ACMD_DECLARE(do_qlist);
ACMD_DECLARE(do_quit);
ACMD_DECLARE(do_radio);
ACMD_DECLARE(do_ram);
ACMD_DECLARE(do_recap);
ACMD_DECLARE(do_ready);
ACMD_DECLARE(do_reboot);
ACMD_DECLARE(do_redesc);
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
ACMD_DECLARE(do_stow);
ACMD_DECLARE(do_stuck);
ACMD_DECLARE(do_survey);
ACMD_DECLARE(do_switch);
ACMD_DECLARE(do_switched_message_history);
ACMD_DECLARE(do_syspoints);
ACMD_DECLARE(do_teleport);
ACMD_DECLARE(do_tell);
ACMD_DECLARE(do_tempdesc);
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
ACMD_DECLARE(do_unfollow);
ACMD_DECLARE(do_ungroup);
ACMD_DECLARE(do_unpack);
ACMD_DECLARE(do_unsupported_command);
ACMD_DECLARE(do_upgrade);
ACMD_DECLARE(do_upload_headware);
ACMD_DECLARE(do_use);
ACMD_DECLARE(do_usenerps);
ACMD_DECLARE(do_users);
ACMD_DECLARE(do_vehicle_osay);
ACMD_DECLARE(do_vset);
ACMD_DECLARE(do_vemote);
ACMD_DECLARE(do_visible);
ACMD_DECLARE(do_vfind);
ACMD_DECLARE(do_vnum);
ACMD_DECLARE(do_vstat);
ACMD_DECLARE(do_vlist);
ACMD_DECLARE(do_valset);
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
ACMD_DECLARE(do_vclone);
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
ACMD_DECLARE(do_deduct);
ACMD_DECLARE(do_holster);
ACMD_DECLARE(do_draw);
ACMD_DECLARE(do_cheatmark);
ACMD_DECLARE(do_copyover);
ACMD_DECLARE(do_land);
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

#define BLOCKS_IDLE_REWARD  FALSE
#define ALLOWS_IDLE_REWARD  TRUE

struct command_info cmd_info[] =
  {
    { "RESERVED", 0, 0, 0, 0, 0
    }
    ,   /* this must be first -- for specprocs */
    /* directions must come before other commands but after RESERVED */
    { "north"      , POS_SITTING, do_move     , 0, SCMD_NORTH, BLOCKS_IDLE_REWARD },
    { "east"       , POS_SITTING, do_move     , 0, SCMD_EAST, BLOCKS_IDLE_REWARD },
    { "south"      , POS_SITTING, do_move     , 0, SCMD_SOUTH, BLOCKS_IDLE_REWARD },
    { "west"       , POS_SITTING, do_move     , 0, SCMD_WEST, BLOCKS_IDLE_REWARD },
    { "up"         , POS_SITTING, do_move     , 0, SCMD_UP, BLOCKS_IDLE_REWARD },
    { "down"       , POS_SITTING, do_move     , 0, SCMD_DOWN, BLOCKS_IDLE_REWARD },
    { "ne"         , POS_SITTING, do_move     , 0, SCMD_NORTHEAST, BLOCKS_IDLE_REWARD },
    { "se"         , POS_SITTING, do_move     , 0, SCMD_SOUTHEAST, BLOCKS_IDLE_REWARD },
    { "sw"         , POS_SITTING, do_move     , 0, SCMD_SOUTHWEST, BLOCKS_IDLE_REWARD },
    { "nw"         , POS_SITTING, do_move     , 0, SCMD_NORTHWEST, BLOCKS_IDLE_REWARD },
    { "northeast"  , POS_SITTING, do_move     , 0, SCMD_NORTHEAST, BLOCKS_IDLE_REWARD },
    { "southeast"  , POS_SITTING, do_move     , 0, SCMD_SOUTHEAST, BLOCKS_IDLE_REWARD },
    { "southwest"  , POS_SITTING, do_move     , 0, SCMD_SOUTHWEST, BLOCKS_IDLE_REWARD },
    { "northwest"  , POS_SITTING, do_move     , 0, SCMD_NORTHWEST, BLOCKS_IDLE_REWARD },

    /* now, the main list -- note that spec-proc commands and socials come after this list. */
    { "abilities"  , POS_MORTALLYW, do_skills   , 0, SCMD_ABILITIES, ALLOWS_IDLE_REWARD },
    { "abilityset" , POS_SLEEPING, do_abilityset , LVL_DEVELOPER, 0, ALLOWS_IDLE_REWARD },
    { "activate"   , POS_LYING   , do_activate , 0, 0, BLOCKS_IDLE_REWARD },
    { "aecho"      , POS_SLEEPING, do_new_echo , LVL_ARCHITECT, SCMD_AECHO, BLOCKS_IDLE_REWARD },
    { "accept"     , POS_LYING   , do_accept   , 0, 0, BLOCKS_IDLE_REWARD },
#ifdef IS_BUILDPORT
    { "account"    , POS_DEAD    , do_account  , 0, 0, ALLOWS_IDLE_REWARD },
#endif

#ifdef DIES_IRAE
    /* The power point for Karma rule was specifically included for players who do not use the advanced magic (initiation) rules.
       It is recommended that this rule be ignored if the initation rules in Magic in the Shadows are also being used.
          -- https://www.shadowrunrpg.com/resources/sr3faq.html#6

      Per the above, addpoint has been disabled.
    */
    // { "addpoint"   , POS_DEAD    , do_initiate , 0, SCMD_POWERPOINT, BLOCKS_IDLE_REWARD },
#else
    { "addpoint"   , POS_DEAD    , do_initiate , 0, SCMD_POWERPOINT, BLOCKS_IDLE_REWARD },
#endif
    { "affects"    , POS_MORTALLYW, do_status   , 0, 0, ALLOWS_IDLE_REWARD },
    { "afk"        , POS_DEAD    , do_afk      , 0, 0, ALLOWS_IDLE_REWARD },
    { "ammo"       , POS_LYING   , do_ammo     , 0, 0, ALLOWS_IDLE_REWARD },
    { "assense"    , POS_LYING   , do_assense  , 0, 0, BLOCKS_IDLE_REWARD },
    { "at"         , POS_DEAD    , do_at       , LVL_ADMIN, 0, BLOCKS_IDLE_REWARD },
    { "attach"     , POS_RESTING , do_attach   , 0, 0, BLOCKS_IDLE_REWARD },
#ifdef SELFADVANCE
    // Allows running an unattended test port where anyone can bump themselves up to level 9.
    { "advance"    , POS_DEAD    , do_self_advance, 0, 0, BLOCKS_IDLE_REWARD },
#else
    { "advance"    , POS_DEAD    , do_advance  , LVL_VICEPRES, 0, BLOCKS_IDLE_REWARD },
#endif
    { "alias"      , POS_DEAD    , do_alias    , 0, 0, ALLOWS_IDLE_REWARD },
    { "answer"     , POS_LYING   , do_phone    , 0, SCMD_ANSWER, BLOCKS_IDLE_REWARD },
    { "assist"     , POS_FIGHTING, do_assist   , 1, 0, BLOCKS_IDLE_REWARD },
    { "ask"        , POS_LYING   , do_spec_comm, 0, SCMD_ASK, BLOCKS_IDLE_REWARD },
    { "award"      , POS_DEAD    , do_award    , LVL_FIXER, 0, BLOCKS_IDLE_REWARD },
    { "audit"      , POS_DEAD    , do_audit    , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "authorize"  , POS_DEAD    , do_wizutil  , LVL_ADMIN, SCMD_AUTHORIZE, BLOCKS_IDLE_REWARD },
    { "availoffset", POS_DEAD  , do_availoffset, 0, 0, ALLOWS_IDLE_REWARD },

    { "bond"       , POS_RESTING , do_bond     , 0, 0, BLOCKS_IDLE_REWARD },
    { "ban"        , POS_DEAD    , do_ban      , LVL_EXECUTIVE, 0, BLOCKS_IDLE_REWARD },
    { "banish"     , POS_STANDING, do_banish   , 0, 0, BLOCKS_IDLE_REWARD },
    { "banvpn"     , POS_DEAD    , do_banvpn   , LVL_EXECUTIVE, 0, BLOCKS_IDLE_REWARD },
    { "balance"    , POS_LYING   , do_gold     , 0, 0, ALLOWS_IDLE_REWARD },
    { "bioware"    , POS_DEAD    , do_bioware  , 0, 0, ALLOWS_IDLE_REWARD },
    { "block"      , POS_DEAD    , do_ignore   , 0, SCMD_IGNORE, ALLOWS_IDLE_REWARD },
    { "boost"      , POS_LYING   , do_boost    , 0, 0, BLOCKS_IDLE_REWARD },
    { "break"      , POS_LYING   , do_break    , 0, 0, BLOCKS_IDLE_REWARD },
    { "broadcast"  , POS_LYING   , do_broadcast, 0, 0, ALLOWS_IDLE_REWARD },
    { ","          , POS_LYING   , do_broadcast, 0, 0, ALLOWS_IDLE_REWARD },
    { "build"      , POS_RESTING , do_build    , 0, 0, BLOCKS_IDLE_REWARD },
    { "brief"      , POS_RESTING , do_brief    , 0, 0, BLOCKS_IDLE_REWARD },
    { "bug"        , POS_DEAD    , do_gen_write, 0, SCMD_BUG, ALLOWS_IDLE_REWARD },
    { "bypass"     , POS_SITTING, do_gen_door , 0, SCMD_PICK, BLOCKS_IDLE_REWARD },

    { "cast"       , POS_SITTING , do_cast     , 1, SCMD_STANDARD_CAST, BLOCKS_IDLE_REWARD },
    { "call"       , POS_LYING   , do_phone    , 0, SCMD_RING, ALLOWS_IDLE_REWARD },
    { "chase"      , POS_SITTING , do_chase    , 0, 0, BLOCKS_IDLE_REWARD },
    { "charge"     , POS_DEAD    , do_charge   , LVL_FIXER, 0, BLOCKS_IDLE_REWARD },
    { "changelog"  , POS_LYING   , do_changelog, 0, 0, ALLOWS_IDLE_REWARD },
    { "cheatmark"  , POS_DEAD    , do_cheatmark, LVL_VICEPRES, 0, BLOCKS_IDLE_REWARD },
    { "cheatlog"   , POS_DEAD    , do_cheatlog , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "cleanup"    , POS_SITTING , do_cleanup  , 0, 0, BLOCKS_IDLE_REWARD },
    { "cleanse"    , POS_LYING   , do_cleanse  , 0, 0, BLOCKS_IDLE_REWARD },
    { "clear"      , POS_DEAD    , do_gen_ps   , 0, SCMD_CLEAR, ALLOWS_IDLE_REWARD },
    { "close"      , POS_SITTING , do_gen_door , 0, SCMD_CLOSE, BLOCKS_IDLE_REWARD },
    { "closecombat", POS_LYING   , do_closecombat, 0, 0, BLOCKS_IDLE_REWARD },
    { "cls"        , POS_DEAD    , do_gen_ps   , 0, SCMD_CLEAR, ALLOWS_IDLE_REWARD },
    { "conceal"    , POS_LYING   , do_conceal_reveal, 0, SCMD_CONCEAL, ALLOWS_IDLE_REWARD },
    { "consider"   , POS_LYING   , do_consider , 0, 0, BLOCKS_IDLE_REWARD },
    { "consent"    , POS_MORTALLYW, do_consent , 0, 0, ALLOWS_IDLE_REWARD },
    { "configure"  , POS_DEAD    , do_toggle   , 0, 0, ALLOWS_IDLE_REWARD },
    { "conjure"    , POS_STANDING, do_conjure  , 0, 0, BLOCKS_IDLE_REWARD },
    { "connect"    , POS_SITTING , do_connect  , 0, 0, BLOCKS_IDLE_REWARD },
    { "contest"    , POS_STANDING, do_contest  , 0, 0, BLOCKS_IDLE_REWARD },
    { "control"    , POS_SITTING , do_control  , 0, 0, BLOCKS_IDLE_REWARD },
    { "combine"    , POS_RESTING , do_put      , 0, 0, BLOCKS_IDLE_REWARD },
    { "comegetme"  , POS_DEAD    , do_dw_retrieve, 0, 0, BLOCKS_IDLE_REWARD },
    { "complete"   , POS_LYING   , do_recap    , 0, 0, BLOCKS_IDLE_REWARD },
    { "copy"       , POS_SITTING , do_copy     , 0, 0, BLOCKS_IDLE_REWARD },
    { "copyover"   , POS_DEAD    , do_copyover , LVL_ADMIN, 0, BLOCKS_IDLE_REWARD },
    { "commands"   , POS_DEAD    , do_commands , 0, SCMD_COMMANDS, ALLOWS_IDLE_REWARD },
    { "compress"   , POS_LYING   , do_compact  , 0, 0, BLOCKS_IDLE_REWARD },
    { "cook"       , POS_SITTING , do_cook     , 0, 0, BLOCKS_IDLE_REWARD },
    { "costtime"   , POS_DEAD    , do_costtime , 0, 0, ALLOWS_IDLE_REWARD },
    { "coredump"   , POS_DEAD    , do_coredump , LVL_PRESIDENT, 0, BLOCKS_IDLE_REWARD },
    { "count"      , POS_DEAD    , do_count, 0, 0, ALLOWS_IDLE_REWARD },
    { "cpool"      , POS_DEAD    , do_cpool    , 0, 0, BLOCKS_IDLE_REWARD },
    { "crack"      , POS_RESTING , do_crack    , 0, 0, BLOCKS_IDLE_REWARD },
    { "crashmu"    , POS_STANDING, do_crash_mud, LVL_PRESIDENT, SCMD_NOOP, BLOCKS_IDLE_REWARD },
    { "crashmud"   , POS_STANDING, do_crash_mud, LVL_PRESIDENT, SCMD_BOOM, BLOCKS_IDLE_REWARD },
    { "create"     , POS_LYING   , do_create   , 0, 0, BLOCKS_IDLE_REWARD },
    { "credits"    , POS_DEAD    , do_gen_ps   , 0, SCMD_CREDITS, ALLOWS_IDLE_REWARD },
    { "customize"  , POS_SLEEPING, do_customize, 0, 0, ALLOWS_IDLE_REWARD },
    { "cyberware"  , POS_DEAD    , do_cyberware, 0, 0, ALLOWS_IDLE_REWARD },

    { "date"       , POS_DEAD    , do_date     , 0, SCMD_DATE, ALLOWS_IDLE_REWARD },
    { "dc"         , POS_DEAD    , do_dc       , LVL_EXECUTIVE, 0, BLOCKS_IDLE_REWARD },
    { "deactivate" , POS_LYING   , do_deactivate, 0, 0, BLOCKS_IDLE_REWARD },
#ifdef IS_BUILDPORT
    { "debug"      , POS_DEAD    , do_debug    , LVL_ADMIN, 0, BLOCKS_IDLE_REWARD },
#else
    { "debug"      , POS_DEAD    , do_debug    , LVL_PRESIDENT, 0, BLOCKS_IDLE_REWARD },
#endif
    { "decline"    , POS_LYING   , do_decline  , 0, 0, BLOCKS_IDLE_REWARD },
    { "decompress" , POS_LYING   , do_compact  , 0, 1, BLOCKS_IDLE_REWARD },
    { "decorate"   , POS_DEAD    , do_decorate , 0, 0, ALLOWS_IDLE_REWARD },
    { "deduct"     , POS_DEAD    , do_deduct   , LVL_FIXER, 0, BLOCKS_IDLE_REWARD },
    { "delete"     , POS_SLEEPING, do_delete   , 0, 0, BLOCKS_IDLE_REWARD },
    { "default"    , POS_RESTING , do_default  , 0, 0, BLOCKS_IDLE_REWARD },
    { "describe"   , POS_LYING   , do_describe , 0, 0, ALLOWS_IDLE_REWARD },
//  { "dennis"     , POS_SITTING, do_move     , 0, SCMD_DOWN, BLOCKS_IDLE_REWARD },
    { "design"     , POS_RESTING , do_design   , 0, 0, BLOCKS_IDLE_REWARD },
    { "destroy"    , POS_STANDING, do_destroy  , 0, 0, BLOCKS_IDLE_REWARD },
//    { "destring" , POS_DEAD    , do_destring , 0, 0, BLOCKS_IDLE_REWARD },
    { "diagnose"   , POS_RESTING , do_diagnose , 0, 0, BLOCKS_IDLE_REWARD },
    { "dice"       , POS_DEAD    , do_dice     , 0, SCMD_STANDARD_ROLL, BLOCKS_IDLE_REWARD },
    { "die"        , POS_DEAD    , do_die      , 0, 0, BLOCKS_IDLE_REWARD },
    { "dig"        , POS_RESTING , do_dig      , LVL_BUILDER, SCMD_DIG, BLOCKS_IDLE_REWARD },
    { "dispell"    , POS_SITTING , do_dispell  , 0, 0, BLOCKS_IDLE_REWARD },
    { "display"    , POS_DEAD    , do_display  , 0, 0, ALLOWS_IDLE_REWARD },
    { "discord"    , POS_DEAD    , do_discord  , 0, 0, ALLOWS_IDLE_REWARD },
    { "docwagon"   , POS_LYING   , do_docwagon , 0, 0, BLOCKS_IDLE_REWARD },
    { "domain"     , POS_LYING   , do_domain   , 0, 0, BLOCKS_IDLE_REWARD },
    { "download"   , POS_RESTING , do_download_headware, 0, 0, BLOCKS_IDLE_REWARD },
    { "donate"     , POS_RESTING , do_drop     , 0, SCMD_DONATE, BLOCKS_IDLE_REWARD },
    { "drag"       , POS_STANDING, do_drag     , 0, 0, BLOCKS_IDLE_REWARD },
    { "drink"      , POS_RESTING , do_drink    , 0, SCMD_DRINK, BLOCKS_IDLE_REWARD },
    { "drive"      , POS_SITTING , do_drive    , 0, 0, BLOCKS_IDLE_REWARD },
    { "drop"       , POS_LYING   , do_drop     , 0, SCMD_DROP, BLOCKS_IDLE_REWARD },
    { "draw"       , POS_RESTING , do_draw     , 0, 0, BLOCKS_IDLE_REWARD },
    { "driveby"    , POS_SITTING , do_driveby  , 0, 0, BLOCKS_IDLE_REWARD },
    { "drugs"      , POS_MORTALLYW, do_drugs   , 0, 0, ALLOWS_IDLE_REWARD },

    { "eat"        , POS_RESTING , do_eat      , 0, SCMD_EAT, BLOCKS_IDLE_REWARD },
    { "echo"       , POS_SLEEPING, do_new_echo , 0, SCMD_ECHO, BLOCKS_IDLE_REWARD },
    { "eject"      , POS_RESTING , do_eject    , 0, 0, BLOCKS_IDLE_REWARD },
    { "elementals" , POS_DEAD    , do_elemental, 0, 0, BLOCKS_IDLE_REWARD },
    { "emote"      , POS_LYING   , do_new_echo , 0, SCMD_EMOTE, BLOCKS_IDLE_REWARD },
    { ":"          , POS_LYING   , do_new_echo , 0, SCMD_EMOTE, BLOCKS_IDLE_REWARD },
    { "enter"      , POS_SITTING , do_enter    , 0, 0, BLOCKS_IDLE_REWARD },
    { "endru"      , POS_RESTING , do_endrun , 0, SCMD_QUI, ALLOWS_IDLE_REWARD }, // You must type the whole command.
    { "endrun"     , POS_RESTING , do_endrun   , 0, SCMD_QUIT, BLOCKS_IDLE_REWARD },
    { "equipment"  , POS_MORTALLYW, do_equipment, 0, 0, ALLOWS_IDLE_REWARD },
    { "exits"      , POS_LYING   , do_exits    , 0, SCMD_LONGEXITS, ALLOWS_IDLE_REWARD },
    { "examine"    , POS_RESTING , do_examine  , 0, SCMD_EXAMINE, ALLOWS_IDLE_REWARD },
    { "exclaim"    , POS_LYING   , do_exclaim  , 0, 0, BLOCKS_IDLE_REWARD },
    { "extend"     , POS_SITTING , do_retract  , 0, 0, BLOCKS_IDLE_REWARD },
    { "exdescs"    , POS_RESTING , do_exdesc   , LVL_CONSPIRATOR, SCMD_EXAMINE, ALLOWS_IDLE_REWARD },

    // TODO: Make this a rigging and matrix command too
    { "factions"   , POS_MORTALLYW, do_factions, LVL_PRESIDENT, 0, ALLOWS_IDLE_REWARD },
    { "force"      , POS_SLEEPING, do_force    , LVL_EXECUTIVE, 0, BLOCKS_IDLE_REWARD },
    { "forceget"   , POS_SLEEPING, do_forceget , LVL_PRESIDENT, 0, BLOCKS_IDLE_REWARD },
    { "forceput"   , POS_SLEEPING, do_forceput , LVL_PRESIDENT, 0, BLOCKS_IDLE_REWARD },
    { "forms"      , POS_LYING   , do_forms    , 0, 0, BLOCKS_IDLE_REWARD },
    { "forget"     , POS_DEAD    , do_forget   , 0, 0, BLOCKS_IDLE_REWARD },
    { "fill"       , POS_SITTING , do_pour     , 0, SCMD_FILL, BLOCKS_IDLE_REWARD },
    { "finger"     , POS_DEAD    , do_last     , 0, SCMD_FINGER, ALLOWS_IDLE_REWARD },
    { "fix"        , POS_SITTING , do_repair   , 0, 0, BLOCKS_IDLE_REWARD },
    { "flee"       , POS_FIGHTING, do_flee     , 0, 0, BLOCKS_IDLE_REWARD },
    { "flip"       , POS_SITTING , do_flip     , 0, 0, BLOCKS_IDLE_REWARD },
    { "flyto"      , POS_SITTING , do_flyto    , 0, 0, BLOCKS_IDLE_REWARD },
    { "focus"      , POS_RESTING , do_focus    , 0, 0, BLOCKS_IDLE_REWARD },
    { "follow"     , POS_LYING   , do_follow   , 0, 0, BLOCKS_IDLE_REWARD },
    { "freeze"     , POS_DEAD    , do_wizutil  , LVL_FREEZE, SCMD_FREEZE, BLOCKS_IDLE_REWARD },
    { "fuckups"    , POS_DEAD    , do_fuckups  , LVL_ADMIN, 0, BLOCKS_IDLE_REWARD },

    { "get"        , POS_RESTING , do_get      , 0, 0, BLOCKS_IDLE_REWARD },
    { "gaecho"     , POS_DEAD    , do_gecho    , LVL_CONSPIRATOR, SCMD_AECHO, BLOCKS_IDLE_REWARD },
    { "gecho"      , POS_DEAD    , do_gecho    , LVL_CONSPIRATOR, 0, BLOCKS_IDLE_REWARD },
    { "give"       , POS_RESTING , do_give     , 0, 0, BLOCKS_IDLE_REWARD },
    { "goto"       , POS_MORTALLYW, do_goto     , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "group"      , POS_RESTING , do_group    , 1, 0, BLOCKS_IDLE_REWARD },
    { "grab"       , POS_RESTING , do_grab     , 0, 0, BLOCKS_IDLE_REWARD },
    { "gridguide"  , POS_RESTING , do_gridguide, 0, 0, BLOCKS_IDLE_REWARD },

    { "help"       , POS_DEAD    , do_help     , 0, 0, ALLOWS_IDLE_REWARD },
    { "hail"       , POS_STANDING, do_hail     , 0, 0, BLOCKS_IDLE_REWARD },
    { "hangup"     , POS_LYING   , do_phone    , 0, SCMD_HANGUP, BLOCKS_IDLE_REWARD },
    { "handbook"   , POS_DEAD    , do_gen_ps   , LVL_BUILDER, SCMD_HANDBOOK, ALLOWS_IDLE_REWARD },
    { "hcontrol"   , POS_DEAD    , do_hcontrol , LVL_EXECUTIVE, 0, BLOCKS_IDLE_REWARD },
    { "heal"       , POS_SITTING , do_heal     , 0, 0, BLOCKS_IDLE_REWARD },
    { "hedit"      , POS_DEAD    , do_hedit    , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "helpedit"   , POS_DEAD    , do_helpedit , LVL_FIXER, 0, BLOCKS_IDLE_REWARD },
    { "helpexport" , POS_DEAD    , do_helpexport, LVL_PRESIDENT, 0, BLOCKS_IDLE_REWARD },
    { "hit"        , POS_FIGHTING, do_hit      , 0, SCMD_HIT, BLOCKS_IDLE_REWARD },
    { "highlight"  , POS_DEAD    , do_highlight, 0, 0, ALLOWS_IDLE_REWARD },
    { "history"    , POS_DEAD    , do_message_history, 0, 0, ALLOWS_IDLE_REWARD },
    { "hlist"      , POS_DEAD    , do_hlist    , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "hold"       , POS_RESTING , do_grab     , 1, 0, BLOCKS_IDLE_REWARD },
    { "holster"    , POS_RESTING , do_holster  , 0, 0, BLOCKS_IDLE_REWARD },
    { "house"      , POS_LYING   , do_house    , 0, 0, BLOCKS_IDLE_REWARD },
    { "houseedit"  , POS_SLEEPING, do_houseedit, LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "ht"         , POS_DEAD    , do_gen_comm , 0, SCMD_HIREDTALK, BLOCKS_IDLE_REWARD },
    { "hts"        , POS_DEAD    , do_switched_message_history, 0, COMM_CHANNEL_HIRED, ALLOWS_IDLE_REWARD },
    { "hp"         , POS_DEAD    , do_hp       , 0, 0, ALLOWS_IDLE_REWARD },

    { "inventory"  , POS_DEAD    , do_inventory, 0, 0, ALLOWS_IDLE_REWARD },
    { "install"    , POS_RESTING , do_put      , 0, SCMD_INSTALL, BLOCKS_IDLE_REWARD },
    { "icedit"     , POS_DEAD    , do_icedit   , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "iclist"     , POS_DEAD    , do_iclist   , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "iclone"     , POS_DEAD    , do_iclone   , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "identify"   , POS_RESTING , do_examine  , 0, SCMD_PROBE, ALLOWS_IDLE_REWARD }, // deliberately out of order to guard idea
    { "idea"       , POS_DEAD    , do_gen_write, 0, SCMD_IDEA, ALLOWS_IDLE_REWARD },
    //{ "idelete"  , POS_DEAD    , do_idelete  , LVL_PRESIDENT, 0, BLOCKS_IDLE_REWARD },
    { "iedit"      , POS_DEAD    , do_iedit    , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "ignore"     , POS_DEAD    , do_ignore   , 0, SCMD_IGNORE, ALLOWS_IDLE_REWARD },
    { "ilist"      , POS_DEAD    , do_ilist    , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "iload"      , POS_DEAD    , do_iload    , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "imotd"      , POS_DEAD    , do_gen_ps   , LVL_BUILDER, SCMD_IMOTD, BLOCKS_IDLE_REWARD },
    { "immlist"    , POS_DEAD    , do_gen_ps   , 0, SCMD_IMMLIST, ALLOWS_IDLE_REWARD },
    { "incognito"  , POS_DEAD    , do_incognito, LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "index"      , POS_SLEEPING, do_index    , 0, 0, ALLOWS_IDLE_REWARD },
    { "info"       , POS_SLEEPING, do_gen_ps   , 0, SCMD_INFO, ALLOWS_IDLE_REWARD },
    { "initiate"   , POS_DEAD    , do_initiate , 0, SCMD_INITIATE, BLOCKS_IDLE_REWARD },
    { "insult"     , POS_LYING   , do_insult   , 0, 0, BLOCKS_IDLE_REWARD },
    { "invis"      , POS_DEAD    , do_invis    , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "invitations", POS_LYING   , do_invitations, 0, 0, BLOCKS_IDLE_REWARD },
    { "introduce"  , POS_DEAD    , do_unsupported_command, 0, SCMD_INTRODUCE, BLOCKS_IDLE_REWARD },
    { "items"      , POS_LYING   , do_items    , 0, 0, ALLOWS_IDLE_REWARD },

    { "jack"       , POS_SITTING , do_jack     , 0, 0, BLOCKS_IDLE_REWARD },
    { "jobs"       , POS_DEAD    , do_recap    , 0, 0, BLOCKS_IDLE_REWARD },
    { "junk"       , POS_RESTING , do_drop     , 0, SCMD_JUNK, BLOCKS_IDLE_REWARD },

    // { "kil"        , POS_FIGHTING, do_kil      , 0, 0, BLOCKS_IDLE_REWARD },
    { "kill"       , POS_FIGHTING, do_kill     , 0, SCMD_KILL, BLOCKS_IDLE_REWARD },
    { "keep"       , POS_LYING   , do_keep     , 0, 0, BLOCKS_IDLE_REWARD },
    { "keepalive"  , POS_DEAD    , do_keepalive, 0, 0, ALLOWS_IDLE_REWARD },
    { "kick"       , POS_STANDING, do_kick     , 0, 0, BLOCKS_IDLE_REWARD },
    { "knock"      , POS_STANDING, do_gen_door , 0, SCMD_KNOCK, BLOCKS_IDLE_REWARD },

    { "look"       , POS_LYING   , do_look     , 0, SCMD_LOOK, ALLOWS_IDLE_REWARD },
    { "lay"        , POS_RESTING , do_lay      , 0, 0, BLOCKS_IDLE_REWARD },
    { "language"   , POS_DEAD    , do_language , 0, 0, ALLOWS_IDLE_REWARD },
    { "land"       , POS_RESTING , do_land     , 0, 0, BLOCKS_IDLE_REWARD },
    { "last"       , POS_DEAD    , do_last     , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "leaderboards", POS_DEAD  , do_leaderboard, LVL_MORTAL, 0, ALLOWS_IDLE_REWARD },
    { "learn"      , POS_RESTING , do_learn    , 0, 0, BLOCKS_IDLE_REWARD },
    { "leave"      , POS_SITTING , do_leave    , 0, 0, BLOCKS_IDLE_REWARD },
    { "link"       , POS_SLEEPING, do_link     , 0, 0, BLOCKS_IDLE_REWARD },
    { "lock"       , POS_SITTING , do_gen_door , 0, SCMD_LOCK, BLOCKS_IDLE_REWARD },
    { "load"       , POS_RESTING , do_chipload , 0, 0, BLOCKS_IDLE_REWARD },
    { "logwatch"   , POS_DEAD    , do_logwatch , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },

    { "man"        , POS_SITTING , do_man      , 0, 0, BLOCKS_IDLE_REWARD },
    { "manifest"   , POS_RESTING , do_manifest , 0, 0, BLOCKS_IDLE_REWARD },
    { "map"        , POS_DEAD    , do_map      , 0, 0, BLOCKS_IDLE_REWARD },
    { "memory"     , POS_SLEEPING, do_memory   , 0, 0, ALLOWS_IDLE_REWARD },
    { "metamagic"  , POS_DEAD    , do_metamagic, 0, 0, ALLOWS_IDLE_REWARD },
    { "metrics"    , POS_DEAD    , do_metrics  , LVL_PRESIDENT, 0, ALLOWS_IDLE_REWARD },
    { "mclone"     , POS_DEAD    , do_mclone   , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "mdelete"    , POS_DEAD    , do_mdelete  , LVL_PRESIDENT, 0, BLOCKS_IDLE_REWARD },
    { "medit"      , POS_DEAD    , do_medit    , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "mlist"      , POS_DEAD    , do_mlist    , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "mobs"       , POS_LYING   , do_mobs     , 0, 0, BLOCKS_IDLE_REWARD },
    { "mode"       , POS_LYING   , do_mode     , 0, 0, ALLOWS_IDLE_REWARD },
    { "motd"       , POS_DEAD    , do_gen_ps   , 0, SCMD_MOTD, ALLOWS_IDLE_REWARD },
    { "mount"      , POS_RESTING , do_mount    , 0, 0, BLOCKS_IDLE_REWARD },
    { "makenerps"  , POS_SLEEPING, do_makenerps, LVL_FIXER, 0, BLOCKS_IDLE_REWARD },
    { "mask"       , POS_RESTING , do_masking  , 0, 0, BLOCKS_IDLE_REWARD },
    { "mute"       , POS_DEAD    , do_wizutil  , LVL_FREEZE, SCMD_SQUELCH, ALLOWS_IDLE_REWARD },
    { "muteooc"    , POS_DEAD    , do_wizutil  , LVL_FREEZE, SCMD_SQUELCHOOC, ALLOWS_IDLE_REWARD },
    { "mutetells"  , POS_DEAD    , do_wizutil  , LVL_FREEZE, SCMD_SQUELCHTELLS, ALLOWS_IDLE_REWARD },
    { "mutequestions", POS_DEAD    , do_wizutil  , LVL_FREEZE, SCMD_MUTE_NEWBIE, ALLOWS_IDLE_REWARD },
    { "murder"     , POS_FIGHTING, do_hit      , 0, SCMD_MURDER, BLOCKS_IDLE_REWARD },

    { "question"     , POS_DEAD    , do_gen_comm , 0, SCMD_QUESTION, ALLOWS_IDLE_REWARD },
    { "questions"    , POS_DEAD    , do_switched_message_history, 0, COMM_CHANNEL_QUESTIONS, ALLOWS_IDLE_REWARD },
    //{ "news"       , POS_SLEEPING, do_gen_ps   , 0, SCMD_NEWS, ALLOWS_IDLE_REWARD },
    { "networth"   , POS_MORTALLYW, do_networth, LVL_ADMIN, 0, ALLOWS_IDLE_REWARD},
    { "notitle"    , POS_DEAD    , do_wizutil  , LVL_FREEZE, SCMD_NOTITLE, BLOCKS_IDLE_REWARD },

    { "order"      , POS_LYING   , do_order    , 1, 0, BLOCKS_IDLE_REWARD },
    { "olc"        , POS_DEAD    , do_olcon    , LVL_DEVELOPER, 0, BLOCKS_IDLE_REWARD },
    { "ooc"        , POS_DEAD    , do_gen_comm , 0, SCMD_OOC, ALLOWS_IDLE_REWARD },
    { "oocs"       , POS_DEAD    , do_switched_message_history, 0, COMM_CHANNEL_OOC, ALLOWS_IDLE_REWARD },
    { "oocdisable" , POS_DEAD    , do_oocdisable, LVL_DEVELOPER, 0, BLOCKS_IDLE_REWARD },
    { "open"       , POS_SITTING , do_gen_door , 0, SCMD_OPEN, BLOCKS_IDLE_REWARD },
    { "osay"       , POS_LYING   , do_say      , 0, SCMD_OSAY, ALLOWS_IDLE_REWARD },
    { "osays"      , POS_DEAD    , do_switched_message_history, 0, COMM_CHANNEL_OSAYS, ALLOWS_IDLE_REWARD },
    { "out"        , POS_SITTING , do_leave    , 0, 0, BLOCKS_IDLE_REWARD },
    { "oneway"     , POS_RESTING , do_dig      , LVL_BUILDER, SCMD_ONEWAY, BLOCKS_IDLE_REWARD },
    { "."          , POS_LYING   , do_say      , 0, SCMD_OSAY, ALLOWS_IDLE_REWARD },

    { "put"        , POS_RESTING , do_put      , 0, 0, BLOCKS_IDLE_REWARD },
    { "packup"     , POS_SITTING , do_packup   , 0, 0, BLOCKS_IDLE_REWARD },
    { "patch"      , POS_LYING   , do_patch    , 0, 0, BLOCKS_IDLE_REWARD },
    { "page"       , POS_DEAD    , do_page     , LVL_ARCHITECT, 0, ALLOWS_IDLE_REWARD },
    { "pages"      , POS_DEAD    , do_switched_message_history, LVL_ARCHITECT, COMM_CHANNEL_PAGES, ALLOWS_IDLE_REWARD },
    { "pardon"     , POS_DEAD    , do_wizutil  , LVL_BUILDER, SCMD_PARDON, BLOCKS_IDLE_REWARD },
    { "payout"     , POS_DEAD    , do_payout   , LVL_FIXER, 0, BLOCKS_IDLE_REWARD },
    { "perceive"   , POS_LYING   , do_astral   , 0, SCMD_PERCEIVE, BLOCKS_IDLE_REWARD },
    { "perfmon"    , POS_DEAD    , do_perfmon  , LVL_ADMIN, 0, BLOCKS_IDLE_REWARD },
    { "penalties"  , POS_MORTALLYW, do_penalties, 0, 0, ALLOWS_IDLE_REWARD },
    { "pgroup"     , POS_LYING   , do_pgroup   , 0, 0, BLOCKS_IDLE_REWARD },
    { "pgset"      , POS_MORTALLYW, do_pgset   , LVL_ADMIN, 0, BLOCKS_IDLE_REWARD },
    { "phone"      , POS_LYING   , do_phone    , 0, 0, ALLOWS_IDLE_REWARD },
    { "phonelist"  , POS_DEAD    , do_phonelist, LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "photo"      , POS_RESTING , do_photo    , 0, 0, BLOCKS_IDLE_REWARD },
    { "pockets"    , POS_RESTING , do_pockets  , 0, 0, ALLOWS_IDLE_REWARD },
    { "pop"        , POS_SITTING , do_pop      , 0, 0, BLOCKS_IDLE_REWARD },
   // { "policy"     , POS_DEAD    , do_gen_ps   , 0, SCMD_POLICIES, ALLOWS_IDLE_REWARD },
    { "policy"     , POS_DEAD    , do_policy   , 0, SCMD_POLICIES, ALLOWS_IDLE_REWARD },
    { "poofin"     , POS_DEAD    , do_poofset  , LVL_BUILDER, SCMD_POOFIN, BLOCKS_IDLE_REWARD },
    { "poofout"    , POS_DEAD    , do_poofset  , LVL_BUILDER, SCMD_POOFOUT, BLOCKS_IDLE_REWARD },
    { "pools"      , POS_DEAD    , do_pool     , 0, 0, ALLOWS_IDLE_REWARD },
    { "pour"       , POS_SITTING , do_pour     , 0, SCMD_POUR, BLOCKS_IDLE_REWARD },
    { "position"   , POS_DEAD    , do_position , 0, 0, ALLOWS_IDLE_REWARD },
    { "possess"    , POS_DEAD    , do_wizpossess, LVL_FIXER, 0, BLOCKS_IDLE_REWARD },
    { "powers"     , POS_DEAD    , do_skills   , 0, SCMD_ABILITIES, ALLOWS_IDLE_REWARD },
    { "powerdown"  , POS_LYING   , do_powerdown, 0, 0, BLOCKS_IDLE_REWARD },
    { "press"      , POS_SITTING , do_push     , 0, 0, BLOCKS_IDLE_REWARD },
    { "project"    , POS_LYING   , do_astral   , 0, SCMD_PROJECT, BLOCKS_IDLE_REWARD },
    { "pretitle"   , POS_DEAD    , do_wiztitle , 0, SCMD_PRETITLE, ALLOWS_IDLE_REWARD },
    { "practice"   , POS_RESTING , do_practice , 1, 0, BLOCKS_IDLE_REWARD },
    { "probe"      , POS_RESTING , do_examine  , 0, SCMD_PROBE, ALLOWS_IDLE_REWARD },
    { "program"    , POS_RESTING , do_program  , 0, 0, BLOCKS_IDLE_REWARD },
    { "progress"   , POS_RESTING , do_progress , 0, 0, ALLOWS_IDLE_REWARD },
    { "prone"      , POS_FIGHTING, do_prone    , 0, 0, BLOCKS_IDLE_REWARD },
    { "privatedice", POS_DEAD    , do_dice     , 0, SCMD_PRIVATE_ROLL, BLOCKS_IDLE_REWARD },
    { "praise"     , POS_DEAD    , do_gen_write, 0, SCMD_PRAISE, ALLOWS_IDLE_REWARD }, // Moved down to prevent accidental invocation.
    { "prompt"     , POS_DEAD    , do_display  , 0, 0, ALLOWS_IDLE_REWARD }, // Moved down to prevent accidental invocation.
    { "push"       , POS_SITTING , do_push     , 0, 0, BLOCKS_IDLE_REWARD },
    { "playerrolls", POS_DEAD    , do_playerrolls, LVL_ADMIN, 0, BLOCKS_IDLE_REWARD },
    { "pur"        , POS_RESTING , do_put      , 0, 0, BLOCKS_IDLE_REWARD }, // special case aliasing- this use case is not covered by fuckups below
  #ifdef IS_BUILDPORT
    { "purge"      , POS_DEAD    , do_purge    , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
  #else
    { "purge"      , POS_DEAD    , do_purge    , LVL_ARCHITECT, 0, BLOCKS_IDLE_REWARD },
  #endif

    { "quests"     , POS_DEAD    , do_recap    , 0, 0, ALLOWS_IDLE_REWARD },
    { "ql"         , POS_LYING   , do_look     , 0, SCMD_QUICKLOOK, ALLOWS_IDLE_REWARD },
    { "qui"        , POS_DEAD    , do_quit     , 0, 0, BLOCKS_IDLE_REWARD },
    { "quit"       , POS_SLEEPING, do_quit     , 0, SCMD_QUIT, BLOCKS_IDLE_REWARD },
    { "quicklook"  , POS_LYING   , do_look     , 0, SCMD_QUICKLOOK, ALLOWS_IDLE_REWARD },
    { "quickwho"   , POS_DEAD    , do_quickwho , 0, 0, ALLOWS_IDLE_REWARD },
    { "qwho"       , POS_DEAD    , do_quickwho , 0, 0, ALLOWS_IDLE_REWARD },
    { "qlist"      , POS_DEAD    , do_qlist    , LVL_FIXER, 0, BLOCKS_IDLE_REWARD },
    { "qedit"      , POS_DEAD    , do_qedit    , LVL_FIXER, 0, BLOCKS_IDLE_REWARD },

    { "reply"      , POS_DEAD    , do_reply    , 0, 0, ALLOWS_IDLE_REWARD },
    { "ram"        , POS_SITTING , do_ram      , 0, 0, BLOCKS_IDLE_REWARD },
    { "radio"      , POS_LYING   , do_radio    , 0, 0, ALLOWS_IDLE_REWARD },
    { "rig"        , POS_SITTING , do_rig      , 0, 0, BLOCKS_IDLE_REWARD },
    { "rt"         , POS_DEAD    , do_gen_comm , 0, SCMD_RPETALK, ALLOWS_IDLE_REWARD },
    { "rts"        , POS_DEAD    , do_switched_message_history, 0, COMM_CHANNEL_RPE, ALLOWS_IDLE_REWARD },
    { "rclone"     , POS_DEAD    , do_rclone   , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "rdelete"    , POS_DEAD    , do_rdelete  , LVL_PRESIDENT, 0, BLOCKS_IDLE_REWARD },
    { "reflex"     , POS_RESTING , do_reflex   , 0, 0, BLOCKS_IDLE_REWARD },
    { "register"   , POS_DEAD    , do_register , 0, 0, ALLOWS_IDLE_REWARD },
    { "rest"       , POS_LYING   , do_rest     , 0, 0, BLOCKS_IDLE_REWARD },
    { "read"       , POS_LYING   , do_look     , 0, SCMD_READ, ALLOWS_IDLE_REWARD },
    { "ready"      , POS_LYING   , do_ready    , 0, 0, BLOCKS_IDLE_REWARD },
    { "reboot"     , POS_DEAD    , do_reboot   , LVL_DEVELOPER, 0, BLOCKS_IDLE_REWARD },
    { "recap"      , POS_DEAD    , do_recap    , 0, 0, ALLOWS_IDLE_REWARD },
    { "reload"     , POS_RESTING , do_reload   , 0, 0, BLOCKS_IDLE_REWARD },
    { "release"    , POS_LYING   , do_release  , 0, 0, BLOCKS_IDLE_REWARD },
    { "relieve"    , POS_SITTING, do_relieve  , 0, 0, BLOCKS_IDLE_REWARD },
    { "remove"     , POS_RESTING , do_remove   , 0, 0, BLOCKS_IDLE_REWARD },
    { "remember"   , POS_LYING   , do_remember , 0, 0, BLOCKS_IDLE_REWARD },
    { "report"     , POS_LYING   , do_report   , 0, 0, BLOCKS_IDLE_REWARD },
    { "repair"     , POS_SITTING , do_repair   , 0, 0, BLOCKS_IDLE_REWARD },
    { "restore"    , POS_DEAD    , do_restore  , 1, 0, BLOCKS_IDLE_REWARD },
    { "restring"   , POS_DEAD    , do_restring , 0, 0, BLOCKS_IDLE_REWARD },
    { "retract"    , POS_SITTING , do_retract  , 0, 0, BLOCKS_IDLE_REWARD },
    { "return"     , POS_DEAD    , do_return   , 0, 0, BLOCKS_IDLE_REWARD },
    { "reveal"     , POS_LYING   , do_conceal_reveal, 0, SCMD_REVEAL, ALLOWS_IDLE_REWARD },
    { "ritualcast" , POS_SITTING , do_cast     , 1, SCMD_RITUAL_CAST, BLOCKS_IDLE_REWARD },
    { "rlist"      , POS_DEAD    , do_rlist    , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "room"       , POS_DEAD    , do_room     , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "roll"       , POS_DEAD    , do_dice     , 0, 0, BLOCKS_IDLE_REWARD },
    { "rpe"        , POS_DEAD    , do_wizutil  , LVL_FIXER, SCMD_RPE, ALLOWS_IDLE_REWARD },
    { "rpetalk"    , POS_DEAD    , do_gen_comm , 0, SCMD_RPETALK, ALLOWS_IDLE_REWARD },
    { "redit"      , POS_DEAD    , do_redit    , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "redesc"     , POS_DEAD    , do_redesc   , LVL_FIXER, 0, BLOCKS_IDLE_REWARD },
    { "rewrite_worl",  POS_DEAD, do_rewrite_world, LVL_PRESIDENT, 0, BLOCKS_IDLE_REWARD },
    { "rewrite_world", POS_DEAD, do_rewrite_world, LVL_PRESIDENT, 1, BLOCKS_IDLE_REWARD },

    { "say"        , POS_LYING   , do_say      , 0, SCMD_SAY, BLOCKS_IDLE_REWARD },
    { "says"       , POS_DEAD    , do_switched_message_history, 0, COMM_CHANNEL_SAYS, ALLOWS_IDLE_REWARD },
    { "'"          , POS_LYING   , do_say      , 0, SCMD_SAY, BLOCKS_IDLE_REWARD },
    { "sayto"      , POS_LYING   , do_say      , 0, SCMD_SAYTO, BLOCKS_IDLE_REWARD },
    { "\""         , POS_LYING   , do_say      , 0, SCMD_SAYTO, BLOCKS_IDLE_REWARD },
    { "save"       , POS_DEAD    , do_save     , 0, 0, ALLOWS_IDLE_REWARD },
    { "score"      , POS_DEAD    , do_score    , 0, 0, ALLOWS_IDLE_REWARD },
    { "scan"       , POS_RESTING , do_scan     , 0, 0, BLOCKS_IDLE_REWARD },
    { "search"     , POS_STANDING, do_search   , 0, 0, BLOCKS_IDLE_REWARD },
    { "send"       , POS_SLEEPING, do_send     , LVL_FIXER, 0, BLOCKS_IDLE_REWARD },
    { "sedit"      , POS_DEAD    , do_shedit   , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "set"        , POS_DEAD    , do_set      , LVL_CONSPIRATOR, 0, BLOCKS_IDLE_REWARD },
    { "setfind"    , POS_DEAD    , do_setfind , LVL_VICEPRES, 0, BLOCKS_IDLE_REWARD },
    { "settime"    , POS_DEAD    , do_settime  , LVL_DEVELOPER, 0, BLOCKS_IDLE_REWARD },
    { "sheathe"    , POS_RESTING , do_holster  , 0, 0, BLOCKS_IDLE_REWARD },
    { "shortexits" , POS_LYING   , do_exits    , 0, SCMD_SHORTEXITS, BLOCKS_IDLE_REWARD },
    { "shout"      , POS_LYING   , do_gen_comm , 0, SCMD_SHOUT, BLOCKS_IDLE_REWARD },
    { "shouts"     , POS_DEAD    , do_switched_message_history, 0, COMM_CHANNEL_SHOUTS, ALLOWS_IDLE_REWARD },
    { "shoot"      , POS_FIGHTING, do_shoot    , 0, 0, BLOCKS_IDLE_REWARD },
    { "show"       , POS_DEAD    , do_show     , 0, 0, BLOCKS_IDLE_REWARD },
    { "shopfind"   , POS_DEAD    , do_shopfind , LVL_VICEPRES, 0, BLOCKS_IDLE_REWARD },
    { "shutdown"   , POS_RESTING , do_shutdown , 0, SCMD_SHUTDOWN, BLOCKS_IDLE_REWARD },
    { "sip"        , POS_RESTING , do_drink    , 0, SCMD_SIP, BLOCKS_IDLE_REWARD },
    { "sit"        , POS_LYING   , do_sit      , 0, 0, BLOCKS_IDLE_REWARD },
    { "skills"     , POS_MORTALLYW, do_skills   , 0, 0, ALLOWS_IDLE_REWARD },
    { "skillset"   , POS_SLEEPING, do_skillset , LVL_DEVELOPER, 0, BLOCKS_IDLE_REWARD },
    { "slist"      , POS_DEAD    , do_slist    , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "sleep"      , POS_SLEEPING, do_sleep    , 0, 0, BLOCKS_IDLE_REWARD },
    { "slowns"     , POS_DEAD    , do_slowns   , LVL_DEVELOPER, 0, BLOCKS_IDLE_REWARD },
    { "sneak"      , POS_SITTING, do_sneak    , 1, 0, BLOCKS_IDLE_REWARD },
    { "snoop"      , POS_DEAD    , do_snoop    , LVL_CONSPIRATOR, 0, BLOCKS_IDLE_REWARD },
    { "socials"    , POS_DEAD    , do_commands , 0, SCMD_SOCIALS, ALLOWS_IDLE_REWARD },
    { "software"   , POS_LYING   , do_software , 0, 0, ALLOWS_IDLE_REWARD },
    { "spool"      , POS_DEAD    , do_spool    , 0, 0, ALLOWS_IDLE_REWARD },
    { "speed"      , POS_RESTING , do_speed    , 0, 0, BLOCKS_IDLE_REWARD },
    { "spells"     , POS_SLEEPING, do_spells   , 0, 0, ALLOWS_IDLE_REWARD },
    { "spellset"   , POS_SLEEPING, do_spellset , LVL_DEVELOPER, 0, BLOCKS_IDLE_REWARD },
    { "spirits"    , POS_LYING   , do_elemental, 0, 0, ALLOWS_IDLE_REWARD },
    { "spray"      , POS_STANDING, do_spray    , 0, 0, BLOCKS_IDLE_REWARD },
    { "stand"      , POS_LYING   , do_stand    , 0, 0, BLOCKS_IDLE_REWARD },
    { "stat"       , POS_DEAD    , do_stat     , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "status"     , POS_MORTALLYW, do_status   , 0, 0, ALLOWS_IDLE_REWARD },
    { "steal"      , POS_LYING   , do_steal    , 0, 0, BLOCKS_IDLE_REWARD },
    { "stop"       , POS_LYING   , do_stop     , 0, 0, BLOCKS_IDLE_REWARD },
#ifdef USE_HAMMERSPACE
    { "stow"       , POS_LYING   , do_stow     , LVL_BUILDER, SCMD_STOW, BLOCKS_IDLE_REWARD },
    { "stowed"     , POS_LYING   , do_stow     , 1, SCMD_LIST_STOWED, BLOCKS_IDLE_REWARD },
#endif
    { "stuck"      , POS_LYING   , do_stuck    , 0, 0, BLOCKS_IDLE_REWARD },
    { "subscribe"  , POS_SITTING , do_subscribe, 0, 0, BLOCKS_IDLE_REWARD },
    { "submerse"   , POS_DEAD    , do_submerse , 0, SCMD_INITIATE, BLOCKS_IDLE_REWARD },
    { "subpoint"   , POS_DEAD    , do_subpoint , LVL_ARCHITECT, 0, BLOCKS_IDLE_REWARD },
    { "survey"     , POS_LYING   , do_survey   , 0, 0, ALLOWS_IDLE_REWARD },
    { "switch"     , POS_SITTING , do_switch   , 0, 0, BLOCKS_IDLE_REWARD },
    { "syspoints"  , POS_DEAD    , do_syspoints, 1, 0, ALLOWS_IDLE_REWARD },

    { "talk"       , POS_LYING   , do_phone    , 0, SCMD_TALK, BLOCKS_IDLE_REWARD },
    { "tell"       , POS_DEAD    , do_tell     , 0, 0, ALLOWS_IDLE_REWARD },
    { "tells"      , POS_DEAD    , do_switched_message_history, 0, COMM_CHANNEL_TELLS, ALLOWS_IDLE_REWARD },
    { "take"       , POS_RESTING , do_get      , 0, SCMD_TAKE, BLOCKS_IDLE_REWARD },
    { "target"     , POS_SITTING , do_target   , 0, 0, BLOCKS_IDLE_REWARD },
    { "taste"      , POS_RESTING , do_eat      , 0, SCMD_TASTE, BLOCKS_IDLE_REWARD },
    { "teleport"   , POS_DEAD    , do_teleport , LVL_CONSPIRATOR, 0, BLOCKS_IDLE_REWARD },
    { "tempdesc"   , POS_LYING   , do_tempdesc , LVL_MORTAL, 0, ALLOWS_IDLE_REWARD },
    { "think"      , POS_LYING   , do_think    , 0, 0, BLOCKS_IDLE_REWARD },
    { "throw"      , POS_FIGHTING, do_throw    , 0, 0, BLOCKS_IDLE_REWARD },
    { "thaw"       , POS_DEAD    , do_wizutil  , LVL_FREEZE, SCMD_THAW, BLOCKS_IDLE_REWARD },
    { "title"      , POS_DEAD    , do_title    , 0, 0, ALLOWS_IDLE_REWARD },
    { "time"       , POS_DEAD    , do_time     , 0, SCMD_NORMAL, ALLOWS_IDLE_REWARD },
    { "tke"        , POS_DEAD    , do_karma    , 0, 0, ALLOWS_IDLE_REWARD },
    { "toggle"     , POS_DEAD    , do_toggle   , 0, 0, ALLOWS_IDLE_REWARD },
    { "tow"        , POS_SITTING , do_tow      , 0, 0, BLOCKS_IDLE_REWARD },
    { "track"      , POS_SITTING, do_track    , 0, 0, BLOCKS_IDLE_REWARD },
    { "tracker"    , POS_RESTING , do_not_here , 0, 0, BLOCKS_IDLE_REWARD },
    { "treat"      , POS_SITTING , do_treat    , 0, 0, BLOCKS_IDLE_REWARD },
    { "trade"      , POS_DEAD    , do_trade    , 0, 0, BLOCKS_IDLE_REWARD },
    { "train"      , POS_STANDING, do_train    , 0, 0, BLOCKS_IDLE_REWARD },
    { "transfer"   , POS_SLEEPING, do_trans    , 0, 0, BLOCKS_IDLE_REWARD },
    { "tridlog"    , POS_DEAD    , do_tridlog  , LVL_FIXER, 0, BLOCKS_IDLE_REWARD },
    { "type"       , POS_STANDING, do_type     , 0, 0, BLOCKS_IDLE_REWARD },
    { "typo"       , POS_DEAD    , do_gen_write, 0, SCMD_TYPO, ALLOWS_IDLE_REWARD },

    { "unban"      , POS_DEAD    , do_unban    , LVL_EXECUTIVE, 0, BLOCKS_IDLE_REWARD },
    { "undig"      , POS_RESTING , do_dig      , LVL_BUILDER, SCMD_UNDIG, BLOCKS_IDLE_REWARD },
    { "ungroup"    , POS_DEAD    , do_ungroup  , 0, 0, BLOCKS_IDLE_REWARD },
    { "unignore"   , POS_DEAD    , do_ignore   , 0, SCMD_UNIGNORE, ALLOWS_IDLE_REWARD },
    { "uninstall"  , POS_SITTING , do_get      , 0, SCMD_UNINSTALL, BLOCKS_IDLE_REWARD },
    { "unjack"     , POS_SITTING , do_jack     , 0, 1, BLOCKS_IDLE_REWARD },
    { "unkeep"     , POS_LYING   , do_keep     , 0, 0, BLOCKS_IDLE_REWARD },
    { "unlock"     , POS_SITTING , do_gen_door , 0, SCMD_UNLOCK, BLOCKS_IDLE_REWARD },
    { "unlink"     , POS_SLEEPING, do_link     , 0, 1, BLOCKS_IDLE_REWARD },
    { "unblock"    , POS_DEAD    , do_ignore   , 0, SCMD_UNIGNORE, ALLOWS_IDLE_REWARD },
    { "unbond"     , POS_RESTING , do_unbond   , 0, 0, BLOCKS_IDLE_REWARD },
    { "unaffect"   , POS_DEAD    , do_wizutil  , LVL_EXECUTIVE, SCMD_UNAFFECT, BLOCKS_IDLE_REWARD },
    { "unattach"   , POS_RESTING , do_unattach , 0, 0, BLOCKS_IDLE_REWARD },
    { "unpack"     , POS_SITTING , do_unpack   , 0, 0, BLOCKS_IDLE_REWARD },
    { "unpractice" , POS_RESTING , do_practice, 1, SCMD_UNPRACTICE, BLOCKS_IDLE_REWARD },
#ifdef USE_HAMMERSPACE
    { "unstow"     , POS_LYING   , do_stow     , 1, SCMD_UNSTOW, BLOCKS_IDLE_REWARD },
#endif
    { "unsubscribe",POS_RESTING, do_subscribe, 0, SCMD_UNSUB, BLOCKS_IDLE_REWARD },
    { "untrain"    , POS_RESTING , do_train    , 1, SCMD_UNTRAIN, BLOCKS_IDLE_REWARD },
    { "unlearn"    , POS_DEAD    , do_forget   , 0, 0, BLOCKS_IDLE_REWARD },
    { "unfollow"   , POS_LYING   , do_unfollow , 0, 0, BLOCKS_IDLE_REWARD },
    { "upgrade"    , POS_SITTING , do_upgrade  , 0 , 0, BLOCKS_IDLE_REWARD },
    { "upload"     , POS_RESTING , do_upload_headware, 0, 0, BLOCKS_IDLE_REWARD },
    { "uptime"     , POS_DEAD    , do_date     , 0, SCMD_UPTIME, ALLOWS_IDLE_REWARD },
    { "use"        , POS_LYING   , do_use      , 1, SCMD_USE, BLOCKS_IDLE_REWARD },
    { "usenerps"   , POS_LYING   , do_usenerps , 1, 0, BLOCKS_IDLE_REWARD },
    { "users"      , POS_DEAD    , do_users    , LVL_ADMIN, 0, BLOCKS_IDLE_REWARD },

    { "valset"     , POS_DEAD    , do_valset   , LVL_ADMIN, 0, BLOCKS_IDLE_REWARD },
    { "vclone"     , POS_DEAD    , do_vclone   , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "vedit"      , POS_DEAD    , do_vedit    , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "version"    , POS_DEAD    , do_gen_ps   , 0, SCMD_VERSION, ALLOWS_IDLE_REWARD },
    { "vemote"     , POS_SLEEPING, do_new_echo , 0 , SCMD_VEMOTE, BLOCKS_IDLE_REWARD }, // was do_vemote
    { "visible"    , POS_RESTING , do_visible  , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "view"       , POS_LYING   , do_imagelink, 0, 0, ALLOWS_IDLE_REWARD },
    { "vfind"      , POS_DEAD    , do_vfind    , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "vlist"      , POS_DEAD    , do_vlist    , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "vnum"       , POS_DEAD    , do_vnum     , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "vset"       , POS_DEAD    , do_vset     , LVL_DEVELOPER, 0, BLOCKS_IDLE_REWARD },
    { "vstat"      , POS_DEAD    , do_vstat    , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "vteleport"  , POS_DEAD   , do_vteleport , LVL_CONSPIRATOR, 0, BLOCKS_IDLE_REWARD },

    { "wake"       , POS_SLEEPING, do_wake     , 0, 0, BLOCKS_IDLE_REWARD },
    { "watch"      , POS_SITTING , do_watch    , 0, 0, BLOCKS_IDLE_REWARD },
    { "wear"       , POS_RESTING , do_wear     , 0, 0, BLOCKS_IDLE_REWARD },
    { "weather"    , POS_LYING   , do_weather  , 0, 0, ALLOWS_IDLE_REWARD },
    { "who"        , POS_DEAD    , do_who      , 0, 0, ALLOWS_IDLE_REWARD },
    { "whoami"     , POS_DEAD    , do_gen_ps   , 0, SCMD_WHOAMI, ALLOWS_IDLE_REWARD },
    { "whotitle"   , POS_DEAD    , do_wiztitle , 0, SCMD_WHOTITLE, ALLOWS_IDLE_REWARD },
    { "where"      , POS_MORTALLYW, do_where    , 1, 0, ALLOWS_IDLE_REWARD },
    { "wheresmycar", POS_RESTING , do_wheresmycar, 1, 0, BLOCKS_IDLE_REWARD },
    { "whisper"    , POS_LYING   , do_spec_comm, 0, SCMD_WHISPER, BLOCKS_IDLE_REWARD },
    { "wield"      , POS_RESTING , do_wield    , 0, 0, BLOCKS_IDLE_REWARD },
    { "wimpy"      , POS_DEAD    , do_wimpy    , 0, 0, ALLOWS_IDLE_REWARD },
#ifdef IS_BUILDPORT
    { "wizload"    , POS_RESTING , do_wizload  , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
#else
    { "wizload"    , POS_RESTING , do_wizload  , LVL_ADMIN, 0, BLOCKS_IDLE_REWARD },
#endif
    { "wtell"      , POS_DEAD    , do_wiztell  , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "wtells"     , POS_DEAD    , do_switched_message_history, LVL_BUILDER, COMM_CHANNEL_WTELLS, ALLOWS_IDLE_REWARD },
    { "wts"        , POS_DEAD    , do_switched_message_history, LVL_BUILDER, COMM_CHANNEL_WTELLS, ALLOWS_IDLE_REWARD },
    { "wf"         , POS_DEAD    , do_wizfeel  , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "wizhelp"    , POS_SLEEPING, do_wizhelp  , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "wizlist"    , POS_DEAD    , do_gen_ps   , 0, SCMD_IMMLIST, BLOCKS_IDLE_REWARD },
    { "wizlock"    , POS_DEAD    , do_wizlock  , LVL_DEVELOPER, 0, BLOCKS_IDLE_REWARD },
    { "wwho"       , POS_DEAD    , do_wizwho   , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },

    { "yell"       , POS_LYING   , do_gen_comm , 0, SCMD_SHOUT, BLOCKS_IDLE_REWARD },

    { "zdelete"    , POS_DEAD    , do_zdelete  , LVL_PRESIDENT, 0, BLOCKS_IDLE_REWARD },
    { "zaecho"     , POS_SLEEPING, do_zecho    , LVL_FIXER, SCMD_AECHO, BLOCKS_IDLE_REWARD },
    { "zecho"      , POS_SLEEPING, do_zecho    , LVL_FIXER, 0, BLOCKS_IDLE_REWARD },
    { "zedit"      , POS_DEAD    , do_zedit    , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "zlist"      , POS_DEAD    , do_zlist    , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "zone"       , POS_DEAD    , do_zone     , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "zreset"     , POS_DEAD    , do_zreset   , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "zswitch"    , POS_DEAD    , do_zswitch  , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    /* End of functional commands with game-wide implementation.*/

    /* Commands that will only function in the presence of a spec_proc. */
    { "burn"       , POS_STANDING, do_not_here , 0, 0, BLOCKS_IDLE_REWARD },
    { "buy"        , POS_RESTING , do_not_here , 0, 0, BLOCKS_IDLE_REWARD },
    { "check"      , POS_RESTING , do_not_here , 0, 0, BLOCKS_IDLE_REWARD },
    { "collect"    , POS_RESTING , do_not_here , 0, 0, BLOCKS_IDLE_REWARD },
    { "blastoff"   , POS_RESTING , do_not_here , 0, 0, BLOCKS_IDLE_REWARD },
    { "cancel"     , POS_RESTING , do_not_here , 0, 0, BLOCKS_IDLE_REWARD },
    { "climb"      , POS_STANDING, do_not_here , 0, 0, BLOCKS_IDLE_REWARD },
    { "deposit"    , POS_STANDING, do_not_here , 1, 0, BLOCKS_IDLE_REWARD },
    { "hours"      , POS_LYING   , do_not_here , 0, 0, BLOCKS_IDLE_REWARD },
    { "lease"      , POS_RESTING , do_not_here , 1, 0, BLOCKS_IDLE_REWARD },
    { "light"      , POS_STANDING, do_not_here , 0, 0, BLOCKS_IDLE_REWARD },
    { "list"       , POS_RESTING , do_not_here , 0, 0, ALLOWS_IDLE_REWARD },
    { "mail"       , POS_STANDING, do_not_here , 0, 0, BLOCKS_IDLE_REWARD },
    { "offer"      , POS_RESTING , do_not_here , 0, 0, BLOCKS_IDLE_REWARD },
    { "paint"      , POS_RESTING , do_not_here , 0, 0, BLOCKS_IDLE_REWARD },
    { "pay"        , POS_RESTING , do_not_here , 0, 0, BLOCKS_IDLE_REWARD },
    { "pull"       , POS_STANDING, do_not_here , 0, 0, BLOCKS_IDLE_REWARD },
    { "receive"    , POS_STANDING, do_not_here , 1, 0, BLOCKS_IDLE_REWARD },
    { "recharge"   , POS_DEAD    , do_not_here , 0, 0, BLOCKS_IDLE_REWARD },
    { "rent"       , POS_STANDING, do_not_here , 1, 0, BLOCKS_IDLE_REWARD },
    { "retrieve"   , POS_RESTING , do_not_here , 1, 0, BLOCKS_IDLE_REWARD },
    { "sell"       , POS_RESTING , do_not_here , 0, 0, BLOCKS_IDLE_REWARD },
    { "withdraw"   , POS_SITTING, do_not_here , 1, 0, BLOCKS_IDLE_REWARD },
    { "wire"       , POS_STANDING, do_not_here , 1, 0, BLOCKS_IDLE_REWARD },
    { "write"      , POS_RESTING , do_not_here , 0, 0, BLOCKS_IDLE_REWARD },
    { "value"      , POS_STANDING, do_not_here , 0, 0, BLOCKS_IDLE_REWARD },
    /* End of spec-proc commands. */

    /* Socials and other fluff commands. */
    { "agree"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "agree"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "accuse"   , POS_SITTING , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "apologize", POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "applaud"  , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    // Socials B
    { "bounce"   , POS_STANDING, do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "bat"      , POS_RESTING , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "beam"     , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "beg"      , POS_RESTING , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "bite"     , POS_RESTING , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "blink"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "bleed"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "blush"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "boggle"   , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "bow"      , POS_STANDING, do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "brb"      , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "brick"    , POS_STANDING, do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "burp"     , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    // Socials C
    { "cackle"   , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "chuckle"  , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "chillout" , POS_RESTING , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "cheer"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "clap"     , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "cockeye"  , POS_RESTING , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "collapse" , POS_STANDING, do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "comfort"  , POS_RESTING , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "confused" , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "congrat"  , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "convince" , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "cough"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "cringe"   , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "cry"      , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "curse"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "curtsey"  , POS_STANDING, do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    // Socials D
    { "dance"    , POS_STANDING, do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "daydream" , POS_SLEEPING, do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "disagree" , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "disregard", POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "doh"      , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "drool"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    // Socials E
    { "em_crack" , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "em_flip"  , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "em_roll"  , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "em_think" , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "envy"     , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "eyebrow"  , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    // Socials F
    { "facepalm" , POS_SITTING , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "forgive"  , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "fart"     , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "flex"     , POS_STANDING, do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "flirt"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "frown"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "fume"     , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    // Socials G
    { "gasp"     , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "giggle"   , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "glare"    , POS_RESTING , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "greet"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "grin"     , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "grimace"  , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "groan"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "grovel"   , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "growl"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "grumble"  , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "grunt"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    // Socials H
    { "happy"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "hand"     , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "hate"     , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "hiccup"   , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "hifive"   , POS_STANDING, do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "hoi"      , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "hop"      , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "howl"     , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "hug"      , POS_RESTING , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    // Socials I
    { "innocent" , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    // Socials J
    { "jig"      , POS_STANDING, do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "jump"     , POS_RESTING , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    // Socials L
    { "laugh"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "listen"   , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "lol"      , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "love"     , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    // Socials M
    { "mellow"   , POS_RESTING , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "moan"     , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "mosh"     , POS_STANDING, do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "moon"     , POS_STANDING, do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    // Socials N
    { "nod"      , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "nudge"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    // Socials O
    // Socials P
    { "pant"     , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "pat"      , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "peer"     , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "point"    , POS_RESTING , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "poke"     , POS_RESTING , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "ponder"   , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "pout"     , POS_RESTING , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "prance"   , POS_STANDING, do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "pray"     , POS_SITTING , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "psychoanalyze", POS_RESTING, do_action, 0, 0, BLOCKS_IDLE_REWARD },
    { "puke"     , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "punch"    , POS_RESTING , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "puppy"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "purr"     , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    // Socials Q
    // Socials R
    { "raspberry", POS_SITTING , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "roar"     , POS_RESTING , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "rofl"     , POS_RESTING , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "rose"     , POS_STANDING, do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "rub"      , POS_RESTING , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    // Socials S
    { "sage"     , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "scratch"  , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "scream"   , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "shake"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "shiver"   , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "shrug"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "sigh"     , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "signal"   , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "sing"     , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "smile"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "smirk"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "snicker"  , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "snap"     , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "snarl"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "sneeze"   , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "sniff"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "snort"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "snore"    , POS_SLEEPING, do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "snowball" , POS_STANDING, do_action   , LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "snuggle"  , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "sob"      , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "spank"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "spit"     , POS_STANDING, do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "stare"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "steam"    , POS_RESTING , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "strut"    , POS_STANDING, do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "strangle" , POS_STANDING, do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "sulk"     , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "swear"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    // Socials T
    { "tap"      , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "taunt"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "thank"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "tiptoe"   , POS_RESTING , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "tongue"   , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "touch"    , POS_STANDING, do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "trip"     , POS_STANDING, do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "twitch"   , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "twiddle"  , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "twirl"    , POS_STANDING, do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    // Socials U
    // Socials V
    { "volunteer", POS_RESTING , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    // Socials W
    { "wave"     , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "whimper"  , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "whine"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "whistle"  , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "wibble"   , POS_STANDING, do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "wiggle"   , POS_STANDING, do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "wince"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "wink"     , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "wooha"    , POS_SITTING , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "worship"  , POS_RESTING , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    // Socials X
    // Socials Y
    { "yawn"     , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    { "yodel"    , POS_LYING   , do_action   , 0, 0, BLOCKS_IDLE_REWARD },
    // Socials Z

 // { "zyx"      , POS_STANDING , do_action  , LVL_PRESIDENT, 0, BLOCKS_IDLE_REWARD },
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

struct command_info mtx_htr_info[] =
  {
    { "RESERVED", 0, 0, 0, 0
    , BLOCKS_IDLE_REWARD },
    { "afk", 0, do_afk, 0, 0, ALLOWS_IDLE_REWARD },
    { "disconnect", 0, do_logoff, 0, 1, BLOCKS_IDLE_REWARD },
    { "look", 0, do_not_here, 0, 0, BLOCKS_IDLE_REWARD }, // shadows 'l' so people reflexively looking don't get DC'd from hitch session
    { "logoff", 0, do_logoff, 0, 0, BLOCKS_IDLE_REWARD },
    { "emote", 0, do_echo, 0, SCMD_EMOTE , BLOCKS_IDLE_REWARD },
    { ":", 0, do_echo, 0, SCMD_EMOTE , BLOCKS_IDLE_REWARD },
    { "exit", 0, do_logoff, 0, 0, BLOCKS_IDLE_REWARD },
    { "help", 0, do_help, 0, 0, BLOCKS_IDLE_REWARD },
    { "ht", 0, do_gen_comm , 0, SCMD_HIREDTALK, BLOCKS_IDLE_REWARD },
    { "idea", 0, do_gen_write, 0, SCMD_IDEA, BLOCKS_IDLE_REWARD },
    { "index", 0, do_index, 0, 0, BLOCKS_IDLE_REWARD },
    { "jobs", 0, do_recap, 0, 0, BLOCKS_IDLE_REWARD },
    { "look", 0, do_look, 0, 0, BLOCKS_IDLE_REWARD },
    { "say", 0, do_say, 0, 0, BLOCKS_IDLE_REWARD },
    { "'", 0, do_say, 0, 0, BLOCKS_IDLE_REWARD },
    { "ooc", 0, do_gen_comm, 0, SCMD_OOC, BLOCKS_IDLE_REWARD },
    { "quit", 0, do_logoff, 0, 0, BLOCKS_IDLE_REWARD },
    { "praise", 0, do_gen_write, 0, SCMD_PRAISE, BLOCKS_IDLE_REWARD },
    { "reply", 0, do_reply, 0, 0 , BLOCKS_IDLE_REWARD },
    { "tell", 0, do_tell, 0, 0 , BLOCKS_IDLE_REWARD },
    { "skills", 0, do_skills   , 0, 0, BLOCKS_IDLE_REWARD },
    { "score", 0, do_score, 0, 0, BLOCKS_IDLE_REWARD },
    { "typo", 0, do_gen_write, 0, SCMD_TYPO, BLOCKS_IDLE_REWARD },
    { "who", 0, do_who, 0, 0, BLOCKS_IDLE_REWARD },
    { "wtell", 0, do_wiztell, LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },

        // Channel history commands.
    { "history"    , 0, do_message_history, 0, 0, ALLOWS_IDLE_REWARD },
    { "hts"        , 0, do_switched_message_history, 0, COMM_CHANNEL_HIRED, ALLOWS_IDLE_REWARD },
    { "questions"  , 0, do_switched_message_history, 0, COMM_CHANNEL_QUESTIONS, ALLOWS_IDLE_REWARD },
    { "oocs"       , 0, do_switched_message_history, 0, COMM_CHANNEL_OOC, ALLOWS_IDLE_REWARD },
    { "osays"      , 0, do_switched_message_history, 0, COMM_CHANNEL_OSAYS, ALLOWS_IDLE_REWARD },
    { "pages"      , 0, do_switched_message_history, LVL_ARCHITECT, COMM_CHANNEL_PAGES, ALLOWS_IDLE_REWARD },
    { "rts"        , 0, do_switched_message_history, 0, COMM_CHANNEL_RPE, ALLOWS_IDLE_REWARD },
    { "says"       , 0, do_switched_message_history, 0, COMM_CHANNEL_SAYS, ALLOWS_IDLE_REWARD },
    { "shouts"     , 0, do_switched_message_history, 0, COMM_CHANNEL_SHOUTS, ALLOWS_IDLE_REWARD },
    { "tells"      , 0, do_switched_message_history, 0, COMM_CHANNEL_TELLS, ALLOWS_IDLE_REWARD },
    { "wtells"     , 0, do_switched_message_history, LVL_BUILDER, COMM_CHANNEL_WTELLS, ALLOWS_IDLE_REWARD },
    { "wts"        , 0, do_switched_message_history, LVL_BUILDER, COMM_CHANNEL_WTELLS, ALLOWS_IDLE_REWARD },

    { "\n", 0, 0, 0, 0, FALSE  }
  };

struct command_info mtx_info[] =
  {
    { "RESERVED", 0, 0, 0, 0
    , BLOCKS_IDLE_REWARD },
    { "abort", 0, do_abort, 0, 0, BLOCKS_IDLE_REWARD },
    { "afk", 0, do_afk, 0, 0, ALLOWS_IDLE_REWARD },
    { "alias", 0, do_alias, 0, 0, BLOCKS_IDLE_REWARD },
    { "analyze", 0, do_analyze, 0, 0, BLOCKS_IDLE_REWARD },
    { "answer", 0, do_comcall, 0, SCMD_ANSWER, BLOCKS_IDLE_REWARD },
    { "asist", 0, do_asist, 0, 0, BLOCKS_IDLE_REWARD },
    { "broadcast", 0, do_broadcast, 0, 0, BLOCKS_IDLE_REWARD },
    { "bug", 0, do_gen_write, 0, SCMD_BUG, BLOCKS_IDLE_REWARD },
    { "call", 0, do_comcall, 0, SCMD_RING, BLOCKS_IDLE_REWARD },
  //{ "control", 0, do_control, 0, 0, BLOCKS_IDLE_REWARD },     // This is a rigging command?
    { "connect", 0, do_logon, 0, 0, BLOCKS_IDLE_REWARD },
    { "commands", 0, do_commands, 0, SCMD_COMMANDS, BLOCKS_IDLE_REWARD },
    { "crash", 0, do_crash, 0, 0, BLOCKS_IDLE_REWARD },
    { "decrypt", 0, do_decrypt, 0, 0, BLOCKS_IDLE_REWARD },
    { "disarm", 0, do_decrypt, 0, 1, BLOCKS_IDLE_REWARD },
    { "disconnect", 0, do_logoff, 0, 1, BLOCKS_IDLE_REWARD },
    { "download", 0, do_download, 0, 0, BLOCKS_IDLE_REWARD },
    { "evade", 0, do_evade, 0, 0, BLOCKS_IDLE_REWARD },
    { "echo", 0, do_echo, 0, SCMD_EMOTE , BLOCKS_IDLE_REWARD },
    { "emote", 0, do_echo, 0, SCMD_EMOTE , BLOCKS_IDLE_REWARD },
    { ":", 0, do_echo, 0, SCMD_EMOTE , BLOCKS_IDLE_REWARD },
    { "exit", 0, do_logoff, 0, 0, BLOCKS_IDLE_REWARD },
    { "hangup", 0, do_comcall, 0, SCMD_HANGUP, BLOCKS_IDLE_REWARD },
    { "help", 0, do_help, 0, 0, BLOCKS_IDLE_REWARD },
    { "ht", 0, do_gen_comm , 0, SCMD_HIREDTALK, BLOCKS_IDLE_REWARD },
    { "idea", 0, do_gen_write, 0, SCMD_IDEA, BLOCKS_IDLE_REWARD },
    { "index", 0, do_index, 0, 0, BLOCKS_IDLE_REWARD },
    { "jobs", 0, do_recap, 0, 0, BLOCKS_IDLE_REWARD },
    { "look", 0, do_matrix_look, 0, 0, BLOCKS_IDLE_REWARD },
    { "list", 0, do_not_here, 0, 0, BLOCKS_IDLE_REWARD },
    { "load", 0, do_load, 0, SCMD_SWAP, BLOCKS_IDLE_REWARD },
    { "locate", 0, do_locate, 0, 0, BLOCKS_IDLE_REWARD },
    { "logoff", 0, do_logoff, 0, 0, BLOCKS_IDLE_REWARD },
    { "logout", 0, do_logoff, 0, 0, BLOCKS_IDLE_REWARD },
    { "logon", 0, do_logon, 0, 0, BLOCKS_IDLE_REWARD },
    { "max", 0, do_matrix_max, 0, 0, BLOCKS_IDLE_REWARD },
    { "ooc", 0, do_gen_comm, 0, SCMD_OOC, BLOCKS_IDLE_REWARD },
    { "parry", 0, do_parry, 0, 0, BLOCKS_IDLE_REWARD },
    { "phone", 0, do_comcall, 0, 0, BLOCKS_IDLE_REWARD },
    { "position", 0, do_matrix_position, 0, 0, BLOCKS_IDLE_REWARD },
    { "praise", 0, do_gen_write, 0, SCMD_PRAISE, BLOCKS_IDLE_REWARD },
    { "prompt", 0, do_display, 0, 0 , BLOCKS_IDLE_REWARD },
    { "quit", 0, do_logoff, 0, 0, BLOCKS_IDLE_REWARD },
    { "radio", 0, do_radio, 0, 0, ALLOWS_IDLE_REWARD },
    { "read", 0, do_not_here, 0, 0, BLOCKS_IDLE_REWARD },
    { "recap", 0, do_recap, 0, 0, BLOCKS_IDLE_REWARD },
    { "redirect", 0, do_redirect, 0, 0, BLOCKS_IDLE_REWARD },
    { "remove", 0, do_not_here, 0, 0, BLOCKS_IDLE_REWARD },
    { "reply", 0, do_reply, 0, 0 , BLOCKS_IDLE_REWARD },
    { "restrict", 0, do_restrict, 0, 0, BLOCKS_IDLE_REWARD },
    { "reveal", 0, do_reveal, 0, 0, BLOCKS_IDLE_REWARD },
    { "run", 0, do_run, 0, 0, BLOCKS_IDLE_REWARD },
    { "say", 0, do_say, 0, 0, BLOCKS_IDLE_REWARD },
    { "'", 0, do_say, 0, 0, BLOCKS_IDLE_REWARD },
    { "score", 0, do_matrix_score, 0, 0, BLOCKS_IDLE_REWARD },
    { "scan", 0, do_matrix_scan, 0, 0, BLOCKS_IDLE_REWARD },
    { "skills", 0, do_skills   , 0, 0, BLOCKS_IDLE_REWARD },
    { "selffry", 0, do_fry_self, LVL_PRESIDENT, 0, BLOCKS_IDLE_REWARD },
    { "talk", 0, do_talk, 0, 0, BLOCKS_IDLE_REWARD },
    { "tap", 0, do_tap, 0, 0, BLOCKS_IDLE_REWARD },
    { "tell", 0, do_tell, 0, 0 , BLOCKS_IDLE_REWARD },
    { "time", 0, do_time, 0, 0, BLOCKS_IDLE_REWARD },
    { "toggle", 0, do_toggle, 0, 0 , BLOCKS_IDLE_REWARD },
    { "trace", 0, do_trace, 0, 0, BLOCKS_IDLE_REWARD },
    { "typo", 0, do_gen_write, 0, SCMD_TYPO, BLOCKS_IDLE_REWARD },
    { "unload", 0, do_load, 0, SCMD_UNLOAD, BLOCKS_IDLE_REWARD },
    { "upload", 0, do_load, 0, SCMD_UPLOAD, BLOCKS_IDLE_REWARD },
    { "question", 0, do_gen_comm, 0, SCMD_QUESTION, BLOCKS_IDLE_REWARD },
    { "recap", 0, do_recap, 0, 0 , BLOCKS_IDLE_REWARD },
    { "software", 0, do_software, 0, 0, BLOCKS_IDLE_REWARD },
    { "who", 0, do_who, 0, 0, BLOCKS_IDLE_REWARD },
    { "write", 0, do_not_here, 0, 0, BLOCKS_IDLE_REWARD },
    { "wtell", 0, do_wiztell, LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },
    { "memory", 0, do_memory   , 0, 0, ALLOWS_IDLE_REWARD },

    // Channel history commands.
    { "history"    , 0, do_message_history, 0, 0, ALLOWS_IDLE_REWARD },
    { "hts"        , 0, do_switched_message_history, 0, COMM_CHANNEL_HIRED, ALLOWS_IDLE_REWARD },
    { "questions"  , 0, do_switched_message_history, 0, COMM_CHANNEL_QUESTIONS, ALLOWS_IDLE_REWARD },
    { "oocs"       , 0, do_switched_message_history, 0, COMM_CHANNEL_OOC, ALLOWS_IDLE_REWARD },
    { "osays"      , 0, do_switched_message_history, 0, COMM_CHANNEL_OSAYS, ALLOWS_IDLE_REWARD },
    { "pages"      , 0, do_switched_message_history, LVL_ARCHITECT, COMM_CHANNEL_PAGES, ALLOWS_IDLE_REWARD },
    { "rts"        , 0, do_switched_message_history, 0, COMM_CHANNEL_RPE, ALLOWS_IDLE_REWARD },
    { "says"       , 0, do_switched_message_history, 0, COMM_CHANNEL_SAYS, ALLOWS_IDLE_REWARD },
    { "shouts"     , 0, do_switched_message_history, 0, COMM_CHANNEL_SHOUTS, ALLOWS_IDLE_REWARD },
    { "tells"      , 0, do_switched_message_history, 0, COMM_CHANNEL_TELLS, ALLOWS_IDLE_REWARD },
    { "wtells"     , 0, do_switched_message_history, LVL_BUILDER, COMM_CHANNEL_WTELLS, ALLOWS_IDLE_REWARD },
    { "wts"        , 0, do_switched_message_history, LVL_BUILDER, COMM_CHANNEL_WTELLS, ALLOWS_IDLE_REWARD },

    { "\n", 0, 0, 0, 0, FALSE  }
  };

struct command_info rig_info[] =
  {
    { "RESERVED", 0, 0, 0, 0
    , BLOCKS_IDLE_REWARD },
    { "north", 0, do_move, 0, SCMD_NORTH , BLOCKS_IDLE_REWARD },
    { "east", 0, do_move, 0, SCMD_EAST , BLOCKS_IDLE_REWARD },
    { "south", 0, do_move, 0, SCMD_SOUTH , BLOCKS_IDLE_REWARD },
    { "west", 0, do_move, 0, SCMD_WEST , BLOCKS_IDLE_REWARD },
    { "up", 0, do_move, 0, SCMD_UP , BLOCKS_IDLE_REWARD },
    { "down", 0, do_move, 0, SCMD_DOWN , BLOCKS_IDLE_REWARD },
    { "ne", 0, do_move, 0, SCMD_NORTHEAST, BLOCKS_IDLE_REWARD },
    { "se", 0, do_move, 0, SCMD_SOUTHEAST, BLOCKS_IDLE_REWARD },
    { "sw", 0, do_move, 0, SCMD_SOUTHWEST, BLOCKS_IDLE_REWARD },
    { "nw", 0, do_move, 0, SCMD_NORTHWEST, BLOCKS_IDLE_REWARD },
    { "northeast", 0, do_move, 0, SCMD_NORTHEAST, BLOCKS_IDLE_REWARD },
    { "southeast", 0, do_move, 0, SCMD_SOUTHEAST, BLOCKS_IDLE_REWARD },
    { "southwest", 0, do_move, 0, SCMD_SOUTHWEST, BLOCKS_IDLE_REWARD },
    { "northwest", 0, do_move, 0, SCMD_NORTHWEST, BLOCKS_IDLE_REWARD },
    { "alias", 0, do_alias, 0, 0, BLOCKS_IDLE_REWARD },
    { "bug", 0, do_gen_write, 0, SCMD_BUG, BLOCKS_IDLE_REWARD },
    { "chase", 0, do_chase, 0, 0 , BLOCKS_IDLE_REWARD },
    { "cpool", 0, do_cpool, 0, 0 , BLOCKS_IDLE_REWARD },
    { "commands", 0, do_commands, 0, SCMD_COMMANDS, BLOCKS_IDLE_REWARD },
    { "copyover", LVL_PRESIDENT, do_copyover, 0, 0, BLOCKS_IDLE_REWARD },
    { "driveby", 0, do_driveby, 0, 0, BLOCKS_IDLE_REWARD },
    { "drop", 0, do_drop, 0, SCMD_DROP, BLOCKS_IDLE_REWARD },
    { "echo", 0, do_new_echo, 0, SCMD_VEMOTE, BLOCKS_IDLE_REWARD },
    { "emote", 0, do_new_echo, 0, SCMD_VEMOTE, BLOCKS_IDLE_REWARD },
    { "enter", 0, do_enter, 0, 0, BLOCKS_IDLE_REWARD },
    { "examine", 0, do_look_while_rigging, 0, 0, BLOCKS_IDLE_REWARD },
    { "exits", 0, do_exits, 0, 0, BLOCKS_IDLE_REWARD },
    { "flyto", 0, do_flyto, 0, 0, BLOCKS_IDLE_REWARD },
    { "get", 0, do_get, 0, 0, BLOCKS_IDLE_REWARD },
    { "gridguide", 0, do_gridguide, 0, 0, BLOCKS_IDLE_REWARD },
    { "help", 0, do_help, 0, 0, BLOCKS_IDLE_REWARD },
    { "ht", 0, do_gen_comm , 0, SCMD_HIREDTALK, BLOCKS_IDLE_REWARD },
    { "i", 0, do_inventory, 0, 0, BLOCKS_IDLE_REWARD },
    { "idea", 0, do_gen_write, 0, SCMD_IDEA, BLOCKS_IDLE_REWARD },
    { "look", 0, do_look_while_rigging, 0, 0, BLOCKS_IDLE_REWARD },
    { "inventory", 0, do_inventory, 0, 0, BLOCKS_IDLE_REWARD },
    { "index", 0, do_index, 0, 0, BLOCKS_IDLE_REWARD },
    { "items", 0, do_items, 0, 0, BLOCKS_IDLE_REWARD },
    { "jobs", 0, do_recap, 0, 0, BLOCKS_IDLE_REWARD },
    { "languages", 0, do_language, 0, 0, BLOCKS_IDLE_REWARD },
    { "leave", 0, do_leave, 0 ,0 , BLOCKS_IDLE_REWARD },
    { "lock", 0, do_gen_door , 0, SCMD_LOCK , BLOCKS_IDLE_REWARD },
    { "mobs", 0, do_mobs, 0, 0, BLOCKS_IDLE_REWARD },
    { "mode", 0, do_mode, 0, 0, BLOCKS_IDLE_REWARD },
    { "mount", 0, do_mount, 0, 0, BLOCKS_IDLE_REWARD },
    { "ooc", 0, do_gen_comm, 0, SCMD_OOC, BLOCKS_IDLE_REWARD },
    { "osay", 0, do_vehicle_osay, 0, 0, BLOCKS_IDLE_REWARD },
    { "penalties", 0, do_penalties, 0, 0, ALLOWS_IDLE_REWARD },
    { "press", 0, do_push_while_rigging, 0, 0, BLOCKS_IDLE_REWARD },
    { "pools", 0, do_pool, 0, 0 , BLOCKS_IDLE_REWARD },
    { "position", 0, do_position, 0, 0, ALLOWS_IDLE_REWARD },
    { "push", 0, do_push_while_rigging, 0, 0, BLOCKS_IDLE_REWARD },
    { "question", 0, do_gen_comm, 0, SCMD_QUESTION, BLOCKS_IDLE_REWARD },
    { "ram", 0, do_ram, 0, 0, BLOCKS_IDLE_REWARD },
    { "read", 0, do_look_while_rigging, 0, 0, BLOCKS_IDLE_REWARD },
    { "recap", 0, do_recap, 0, 0 , BLOCKS_IDLE_REWARD },
    { "rig", POS_SITTING , do_rig, 0, 0 , BLOCKS_IDLE_REWARD },
    { "return", 0, do_return, 0, 0, BLOCKS_IDLE_REWARD },
    { "reply", 0, do_reply, 0, 0 , BLOCKS_IDLE_REWARD },
    { "rt", 0, do_gen_comm, 0, SCMD_RPETALK, BLOCKS_IDLE_REWARD },
    { "score", 0, do_score, 0, 0, BLOCKS_IDLE_REWARD },
    { "scan", 0, do_scan, 0, 0, BLOCKS_IDLE_REWARD },
    { "skills", 0, do_skills, 0, 0, BLOCKS_IDLE_REWARD },
    { "speed", 0, do_speed, 0, 0, BLOCKS_IDLE_REWARD },
    { "speak", 0, do_language, 0, 0, BLOCKS_IDLE_REWARD },
    { "subscribe", 0, do_subscribe, 0, 0, BLOCKS_IDLE_REWARD },
    { "syspoints", POS_MORTALLYW, do_syspoints, 1, 0, ALLOWS_IDLE_REWARD },
    { "target", 0, do_target, 0, 0, BLOCKS_IDLE_REWARD },
    { "tell", 0, do_tell, 0, 0 , BLOCKS_IDLE_REWARD },
    { "time", 0, do_time, 0, 0, BLOCKS_IDLE_REWARD },
    { "toggle", POS_MORTALLYW, do_toggle, 1, 0, ALLOWS_IDLE_REWARD },
    { "tow", 0, do_tow, 0, 0 , BLOCKS_IDLE_REWARD },
    { "typo", 0, do_gen_write, 0, SCMD_TYPO, BLOCKS_IDLE_REWARD },
    { "unlock", 0, do_gen_door , 0, SCMD_UNLOCK , BLOCKS_IDLE_REWARD },
    { "vemote", 0, do_new_echo, 0, SCMD_VEMOTE, BLOCKS_IDLE_REWARD },
    { "weather", 0, do_weather, 0, 0, ALLOWS_IDLE_REWARD },
    { "where", 0, do_where, 0, 0, BLOCKS_IDLE_REWARD },
    { "who", 0, do_who, 0, 0, BLOCKS_IDLE_REWARD },
    { "wtell", 0, do_wiztell, LVL_BUILDER, 0, BLOCKS_IDLE_REWARD },

    // Channel history commands.
    { "history"    , 0, do_message_history, 0, 0, ALLOWS_IDLE_REWARD },
    { "hts"        , 0, do_switched_message_history, 0, COMM_CHANNEL_HIRED, ALLOWS_IDLE_REWARD },
    { "questions"  , 0, do_switched_message_history, 0, COMM_CHANNEL_QUESTIONS, ALLOWS_IDLE_REWARD },
    { "oocs"       , 0, do_switched_message_history, 0, COMM_CHANNEL_OOC, ALLOWS_IDLE_REWARD },
    { "osays"      , 0, do_switched_message_history, 0, COMM_CHANNEL_OSAYS, ALLOWS_IDLE_REWARD },
    { "pages"      , 0, do_switched_message_history, LVL_ARCHITECT, COMM_CHANNEL_PAGES, ALLOWS_IDLE_REWARD },
    { "rts"        , 0, do_switched_message_history, 0, COMM_CHANNEL_RPE, ALLOWS_IDLE_REWARD },
    { "says"       , 0, do_switched_message_history, 0, COMM_CHANNEL_SAYS, ALLOWS_IDLE_REWARD },
    { "shouts"     , 0, do_switched_message_history, 0, COMM_CHANNEL_SHOUTS, ALLOWS_IDLE_REWARD },
    { "tells"      , 0, do_switched_message_history, 0, COMM_CHANNEL_TELLS, ALLOWS_IDLE_REWARD },
    { "wtells"     , 0, do_switched_message_history, LVL_BUILDER, COMM_CHANNEL_WTELLS, ALLOWS_IDLE_REWARD },
    { "wts"        , 0, do_switched_message_history, LVL_BUILDER, COMM_CHANNEL_WTELLS, ALLOWS_IDLE_REWARD },

    { "\n", 0, 0, 0, 0, BLOCKS_IDLE_REWARD }
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
  send_to_char(ch, "'%s' is not a valid command.\r\n", arg);
  if (ch->desc && ++ch->desc->invalid_command_counter >= 5) {
    send_to_char(ch, "^GStuck? Need help? Feel free to ask on the ^W%s^G channel! (%s^W%s <message>^G)^n\r\n",
                 PLR_FLAGGED(ch, PLR_NEWBIE) ? "QUESTION" : "OOC",
                 PRF_FLAGGED(ch, PRF_SCREENREADER) ? "type " : "",
                 PLR_FLAGGED(ch, PLR_NEWBIE) ? "QUESTION" : "OOC");
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
}

ACMD_DECLARE(quit_the_matrix_first);
bool matrix_interpreter(struct char_data * ch, char *argument, char *line, command_info *scmd_list) {
  int cmd, length;

  for (length = strlen(arg), cmd = 0; *scmd_list[cmd].command != '\n'; cmd++)
    if (!strncmp(scmd_list[cmd].command, arg, length))
      if ((scmd_list[cmd].minimum_level < LVL_BUILDER) || access_level(ch, scmd_list[cmd].minimum_level))
        break;

  // If they have failed to enter a valid Matrix command, and we were unable to fix a typo in their command:
  if (*scmd_list[cmd].command == '\n' && (cmd = fix_common_command_fuckups(arg, scmd_list)) == -1) {
    // If the command exists outside of the Matrix, let them know that it's not an option here.
    for (length = strlen(arg), cmd = 0; *cmd_info[cmd].command != '\n'; cmd++)
      if (!strncmp(cmd_info[cmd].command, arg, length))
        if ((cmd_info[cmd].minimum_level < LVL_BUILDER) || access_level(ch, cmd_info[cmd].minimum_level))
          break;

    // Nothing was found? Give them the "wat" and bail.
    if (*cmd_info[cmd].command == '\n' && (cmd = fix_common_command_fuckups(arg, cmd_info)) == -1) {
      // Attempt to send it as a custom channel message. If that doesn't work, nonsensical reply.
      if (!send_command_as_custom_channel_message(ch, arg)) {
        nonsensical_reply(ch, arg, "matrix");
      }
#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
      verify_every_pointer_we_can_think_of();
#endif
      return TRUE;
    }

    if (ch->desc)
      ch->desc->invalid_command_counter = 0;

    // Their command was valid in external context. Inform them.
    quit_the_matrix_first(ch, line, 0, 0);
  }
  else {
    // Sanity check: Level restriction.
    if ((scmd_list[cmd].minimum_level >= LVL_BUILDER) && !access_level(ch, scmd_list[cmd].minimum_level)) {
      send_to_char(ch, "Sorry, that's a staff-only command.\r\n", ch);
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: %s was able to trigger staff-only matrix command %s!", GET_CHAR_NAME(ch), scmd_list[cmd].command);
#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
      verify_every_pointer_we_can_think_of();
#endif
      return TRUE;
    }

    if (ch->persona && ch->persona->decker->hitcher) {
      send_to_char(ch->persona->decker->hitcher, "^y<OUTGOING> %s^n\r\n", argument);
    }
    verify_data(ch, line, cmd, scmd_list[cmd].subcmd, "pre-matrix");
    if (!special(ch, cmd, line)) {
      ((scmd_list[cmd].command_pointer) (ch, line, cmd, scmd_list[cmd].subcmd));
      verify_data(ch, line, cmd, scmd_list[cmd].subcmd, "matrix");
    } else {
      verify_data(ch, line, cmd, scmd_list[cmd].subcmd, "matrix special");
    }
    return TRUE;
  }
  #ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
  verify_every_pointer_we_can_think_of();
  #endif
  return FALSE;
}

char *condense_repeated_characters(char *argument) {
  char last_seen_char = 1;
  int last_seen_count = 0;

  if (!argument || !*argument)
    return argument;

  char *reader = argument;
  char *writer = argument;

  while (true) {
    if (*reader == last_seen_char) {
      // Skip anything that's been repeated more than 3 times, unless it's a digit.
      if (++last_seen_count > 3 && !isdigit(*reader)) {
        reader++;
        continue;
      } else {
        *writer++ = *reader;
      }
    } else {
      *writer++ = *reader;
      last_seen_char = *reader;
      last_seen_count = 1;
    }

    if (*(reader++) == '\0')
      break;
  }

  return argument;
}

/*
 * This is the actual command interpreter called from game_loop() in comm.c
 * It makes sure you are the proper level and position to execute the command,
 * then calls the appropriate function.
 */
ACMD_DECLARE(stop_rigging_first);
void command_interpreter(struct char_data * ch, char *argument, const char *tcname)
{
  int cmd, length;
  extern int no_specials;
  char *line;

  if (get_ch_in_room(ch)) {
    zone_table[get_ch_in_room(ch)->zone].last_player_action = time(0);
  }

  if (PRF_FLAGS(ch).IsSet(PRF_AFK)) {
    send_to_char("You return from AFK.\r\n", ch);
    PRF_FLAGS(ch).RemoveBit(PRF_AFK);
  }

  /* just drop to next line for hitting CR */
  skip_spaces(&argument);
  if (!*argument) {
#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
    verify_every_pointer_we_can_think_of();
#endif
    return;
  }

  // They entered something? KaVir's protocol snippet says to clear their WriteOOB.
  if (ch->desc)
    ch->desc->pProtocol->WriteOOB = 0;

  // Strip out massively repeated characters.
  argument = condense_repeated_characters(argument);

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
    if (*argument == '@' || *argument == '+' || *argument == '/' || *argument == '#') {
      argument[0] = ' ';
      skip_spaces(&argument);
      if (!*argument) {
#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
        verify_every_pointer_we_can_think_of();
#endif
        return;
      }
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
#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
    verify_every_pointer_we_can_think_of();
#endif
    return;
  }
  if (ch->points.fire[0] > 0 && success_test(GET_WIL(ch), 6) < 1)
  {
    send_to_char("^RThe flames cause you to panic!^n\r\n", ch);
    WAIT_STATE(ch, 1 RL_SEC);
#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
    verify_every_pointer_we_can_think_of();
#endif
    return;
  }

  /* otherwise, find the command */
  if (PLR_FLAGGED(ch, PLR_MATRIX)) {
    if (!ch->persona) {
      // This block exists for the hitcher-exclusive matrix commandset.
      if (matrix_interpreter(ch, argument, line, mtx_htr_info)) return;
    } else {
      // This is the proper command code for matrix/deckers
      if (matrix_interpreter(ch, argument, line, mtx_info)) return;
    }
  } else if (PLR_FLAGGED(ch, PLR_REMOTE) || AFF_FLAGGED(ch, AFF_RIG))
  {
    for (length = strlen(arg), cmd = 0; *rig_info[cmd].command != '\n'; cmd++)
      if (!strncmp(rig_info[cmd].command, arg, length))
        if ((rig_info[cmd].minimum_level < LVL_BUILDER) || access_level(ch, rig_info[cmd].minimum_level))
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
        // Attempt to send it as a custom channel message. If that doesn't work, nonsensical reply.
        if (!send_command_as_custom_channel_message(ch, arg)) {
          nonsensical_reply(ch, arg, "rigging");
        }
#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
        verify_every_pointer_we_can_think_of();
#endif
        return;
      }

      if (ch->desc)
        ch->desc->invalid_command_counter = 0;

      // Their command was valid in external context. Inform them.
      stop_rigging_first(ch, line, 0, 0);
    } else {
      // Sanity check: Level restriction.
      if ((rig_info[cmd].minimum_level >= LVL_BUILDER) && !access_level(ch, rig_info[cmd].minimum_level)) {
        send_to_char(ch, "Sorry, that's a staff-only command.\r\n", ch);
        mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: %s was able to trigger staff-only rigging command %s!", GET_CHAR_NAME(ch), rig_info[cmd].command);
#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
        verify_every_pointer_we_can_think_of();
#endif
        return;
      }

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
#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
    verify_every_pointer_we_can_think_of();
#endif
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
      // Attempt to send it as a custom channel message. If that doesn't work, nonsensical reply.
      if (!send_command_as_custom_channel_message(ch, arg)) {
        nonsensical_reply(ch, arg, "standard");
      }
#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
      verify_every_pointer_we_can_think_of();
#endif
      return;
    } else {
      if (ch->desc)
        ch->desc->invalid_command_counter = 0;
    }

    if (IS_PROJECT(ch) && ch->desc && ch->desc->original && AFF_FLAGGED(ch->desc->original, AFF_TRACKING) && cmd != find_command("track")) {
      send_to_char("You are too busy astrally tracking someone...\r\n", ch);
#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
      verify_every_pointer_we_can_think_of();
#endif
      return;
    }

    if (PLR_FLAGGED(ch, PLR_FROZEN)) {
      if (!access_level(ch, LVL_PRESIDENT)) {
        send_to_char(ch, "Sorry, this character has been frozen by staff and is unable to take any input. If you're seeing this message, it usually means that you've connected to a character that is pending review or has been banned. If you believe that this has been done in error, reach out to %s for next steps.\r\n", STAFF_CONTACT_EMAIL);
#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
        verify_every_pointer_we_can_think_of();
#endif
        return;
      } else
        send_to_char("The ice covering you crackles alarmingly as you slam your sovereign will through it.\r\n", ch);
    }

    if (cmd_info[cmd].command_pointer == NULL) {
      send_to_char("Sorry, that command hasn't been implemented yet.\r\n", ch);
#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
      verify_every_pointer_we_can_think_of();
#endif
      return;
    }

    if (affected_by_power(ch, ENGULF) && cmd_info[cmd].minimum_position != POS_DEAD) {
      if (!access_level(ch, LVL_VICEPRES)) {
        send_to_char("You are currently being engulfed!\r\n", ch);
#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
        verify_every_pointer_we_can_think_of();
#endif
        return;
      } else
        send_to_char("Administrative power roars through your veins as you muscle through your engulfment.\r\n", ch);
    }

    if (GET_QUI(ch) <= 0 && cmd_info[cmd].minimum_position != POS_DEAD) {
      if (!access_level(ch, LVL_VICEPRES)) {
        send_to_char("You are paralyzed!\r\n", ch);
#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
        verify_every_pointer_we_can_think_of();
#endif
        return;
      } else
        send_to_char("You draw upon your mantle of administrative power and push through your paralysis.\r\n", ch);
    }

    // Restore the idle timer for the idle nuyen bonus.
    if (cmd_info[cmd].should_not_block_idle_reward) {
      ch->char_specials.timer = ch->char_specials.last_timer;
    }
    // Otherwise, notify any staff waiting for them that they've returned.
    else {
      if (ch->desc && ch->desc->watcher) {
        char msg_buf[1000];
        snprintf(msg_buf, sizeof(msg_buf), "%s is active again. You will receive this message with every command until you type ##^WWATCH %s^n again.\r\n", CAP(GET_CHAR_NAME(ch)), GET_CHAR_NAME(ch));
        SEND_TO_Q(msg_buf, ch->desc->watcher);
      }
    }

    // Make sure they're conscious / not morted / etc. Restore chargen chars if that's where they're at right now.
    if (GET_POS(ch) < cmd_info[cmd].minimum_position && (restore_to_full_health_if_still_in_chargen(ch) ? GET_POS(ch) < cmd_info[cmd].minimum_position : TRUE)) {
      switch (GET_POS(ch)) {
      case POS_DEAD:
        // send_to_char("Lie still; you are DEAD!!! :-(\r\n", ch);
        send_to_char("The last vestiges of your soul begin to leave your rapidly-cooling form.\r\n", ch);
        mudlog("WARNING: Dead character is still trying to perform actions. Killing them...", ch, LOG_SYSLOG, TRUE);
        do_die(ch, arg, 0, 0);
        break;
      case POS_MORTALLYW:
        send_to_char("You are in a pretty bad shape! You can either wait for help, or give up by typing ^WDIE^n.\r\n", ch);
        if (PLR_FLAGGED(ch, PLR_NEWBIE)) {
          send_to_char(ch, "^L[OOC: While your TKE is less than %d, your only penalty for dying is losing your current job. See ^wHELP NEWBIE^L for more details.]\r\n", NEWBIE_KARMA_THRESHOLD);
        }
        break;
      case POS_STUNNED:
        send_to_char("All you can do right now is think about the stars! You can either wait to recover, or give up by typing ^WDIE^n.\r\n", ch);
        if (PLR_FLAGGED(ch, PLR_NEWBIE)) {
          send_to_char(ch, "^L[OOC: While your TKE is less than %d, your only penalty for dying is losing your current job. See ^wHELP NEWBIE^L for more details.]\r\n", NEWBIE_KARMA_THRESHOLD);
        }
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
#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
      verify_every_pointer_we_can_think_of();
#endif
      return;
    }

    // Sanity check: Level restriction.
    if ((cmd_info[cmd].minimum_level >= LVL_BUILDER) && !access_level(ch, cmd_info[cmd].minimum_level)) {
      send_to_char(ch, "Sorry, that's a staff-only command.\r\n", ch);
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: %s was able to trigger staff-only command %s!", GET_CHAR_NAME(ch), cmd_info[cmd].command);
#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
      verify_every_pointer_we_can_think_of();
#endif
      return;
    }

    verify_data(ch, line, cmd, cmd_info[cmd].subcmd, "pre-command");

    if (no_specials || !special(ch, cmd, line)) {
      ((*cmd_info[cmd].command_pointer) (ch, line, cmd, cmd_info[cmd].subcmd));
      verify_data(ch, line, cmd, cmd_info[cmd].subcmd, "command");
    } else {
      verify_data(ch, line, cmd, cmd_info[cmd].subcmd, "command special");
    }
#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
    verify_every_pointer_we_can_think_of();
#endif
    return;
  }

#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
  verify_every_pointer_we_can_think_of();
#endif
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

      // TODO: No overwriting any command in any command set. (iterate through each command set looking for lower-case matches, deny if found)

      else if ( str_str(repl, "quit") ) {
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
  strlcpy(buf2, orig, sizeof(buf2));
  temp = strtok(buf2, " ");
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

  char mutable_arg[MAX_INPUT_LENGTH + 1];
  strlcpy(mutable_arg, arg, sizeof(mutable_arg));

  /* Make into lower case, and get length of string */
  for (l = 0; *(mutable_arg + l); l++)
    *(mutable_arg + l) = LOWER(*(mutable_arg + l));

  // For non-exact matching, avoid "" to match the first available * string.
  if (!exact && !l)
    l = 1;

  char lowercase_item[500];

  // Iterate over the list, converting to lower case.
  for (i = 0; **(list + i) != '\n'; i++) {
    strlcpy(lowercase_item, *(list + i), sizeof(lowercase_item));
    for (int l = 0; l < (int) strlen(lowercase_item); l++)
      lowercase_item[l] = LOWER(lowercase_item[l]);

    // Compare.
    if (exact) {
      if (!strcmp(mutable_arg, *(list + i))) {
        return (i);
      }
    } else {
      if (!strncmp(mutable_arg, lowercase_item, l)) {
        return (i);
      }
    }
  }

  // Failed to find anything.
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

void replace_word(const char *input, char *output, size_t output_size, const char *replace_from, const char *replace_to) {
  char *output_anchor = output;

  // Loop over input.
  while (TRUE) {
    // Skim until we see the first instance of a replace_from character.
    while (*input && *input != *replace_from && (output - output_anchor < (long) (output_size - 1)))
      *(output++) = *(input++);

    // Break if no more input or if over length.
    if (!*input || output - output_anchor >= (long) (output_size - 1))
      break;

    // Found a first-character match. We'll want to read until we find the end of it.
    const char *read_ptr = input;
    const char *from_ptr = replace_from;

    // Read until we run out of input, run out of replace_from, or no longer match.
    while (*read_ptr && *from_ptr && *read_ptr == *from_ptr) {
      read_ptr++;
      from_ptr++;
    }

    // We hit the end of from_ptr? Perfect, write in the replacement.
    if (!*from_ptr) {
      const char *to_ptr = replace_to;
      while (*to_ptr && (output - output_anchor < (long) (output_size - 1)))
        *(output++) = *(to_ptr++);

      // Break if over length
      if (output - output_anchor >= (long) (output_size - 1))
        break;

      // Clear the replacement word from the input.
      from_ptr = replace_from;
      while (*input && *input == *from_ptr) {
        input++;
        from_ptr++;
      }
    }
    // Otherwise, we didn't have a complete match. Carry on.
    else {
      *(output++) = *(input++);
    }
  }
  // Null-terminate output.
  *output = '\0';
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
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Received null arg in is_abbrev (arg1 = %s, arg2 = %s)", arg1 ? arg1 : "<NULL>", arg2 ? arg2 : "<NULL>");
    return FALSE;
  }

  if (!*arg1 || !*arg2)
    return FALSE;

  for (; *arg1 && *arg2; arg1++, arg2++)
    if (LOWER(*arg1) != LOWER(*arg2))
      return FALSE;

  return !*arg1;
}

/* return first space-delimited token in arg1; remainder of string in arg2 */
void half_chop(char *string, char *arg1, char *arg2, size_t arg2_sz)
{
  char *temp;
  char errbuf[50];

  if (arg2_sz == sizeof(char *)) {
    log("ERROR: half_chop received an arg2_sz equal to sizeof(char *): You fucked up!");

    log_traceback("half_chop(%s, arg1, arg2, %ld) w/ arg2_sz equal to sizeof(char *)", string, arg2_sz);

    strlcpy(errbuf, "Error", sizeof(errbuf));
    arg2 = errbuf;
    arg2_sz = sizeof(errbuf);
  }

  temp = any_one_arg(string, arg1);
  skip_spaces(&temp);

  char memory_overlap_prevention_buf[MAX_INPUT_LENGTH];
  strlcpy(memory_overlap_prevention_buf, temp, sizeof(memory_overlap_prevention_buf));
  strlcpy(arg2, memory_overlap_prevention_buf, arg2_sz);
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
      SEND_TO_Q("\r\nMultiple login detected (type 1) -- disconnecting.\r\n", k);
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
      SEND_TO_Q("\r\nMultiple login detected (type 2) -- disconnecting.\r\n", k);
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
    next_ch = ch->next_in_character_list;

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
  PLR_FLAGS(d->character).RemoveBits(PLR_MAILING, PLR_EDITING, PLR_WRITING,
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
    {
      struct room_data *in_room = (d->character ? get_ch_in_room(d->character) : NULL);
      if (in_room) {
        zone_table[in_room->zone].last_player_action = time(0);
        // Counts can get fucky when players extract, so we take our best guess at what it should be, then recalc it for the whole game.
        zone_table[in_room->zone].players_in_zone++;
        recalculate_whole_game_players_in_zone();
      }
    }
    break;
  case USURP:
    SEND_TO_Q("You take over your own body, already in use!\r\n", d);
    act("$n shakes $s head to clear it.",
        TRUE, d->character, 0, 0, TO_ROOM);
    snprintf(buf, sizeof(buf), "%s has re-logged in ... disconnecting old socket.",
            GET_CHAR_NAME(d->character));
    mudlog(buf, d->character, LOG_CONNLOG, TRUE);
    log_vfprintf("[CONNLOG: %s reconnecting from %s with fingerprint %s and JSON '''%s''']", GET_CHAR_NAME(d->character), d->host, get_descriptor_fingerprint(d), d->pProtocol ? d->pProtocol->new_environ_info.dump().c_str() : "{}");
    if (d->character->persona)
    {
      snprintf(buf, sizeof(buf), "%s depixelizes and vanishes from the host.\r\n", d->character->persona->name);
      send_to_host(d->character->persona->in_host, buf, d->character->persona, TRUE);
      extract_icon(d->character->persona);
      d->character->persona = NULL;
      PLR_FLAGS(d->character).RemoveBit(PLR_MATRIX);
    } else if (PLR_FLAGGED(d->character, PLR_MATRIX)) {
      clear_hitcher(d->character, TRUE);
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

  // Additional gmcp hooks
  SendGMCPCoreSupports(d);
  SendGMCPCharInfo(d->character);
  SendGMCPCharVitals(d->character);
  SendGMCPCharPools ( d->character );

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
  vnum_t load_room_vnum = RM_ENTRANCE_TO_DANTES;
  rnum_t load_room_rnum = 0;
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
  case CON_FACTION_EDIT:
    faction_edit_parse(d, arg);
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
  case CON_CF_CREATE:
    cfedit_parse(d, arg);
    break;
  case CON_PART_CREATE:
    pbuild_parse(d, arg);
    break;
  case CON_DECK_CREATE:
    dbuild_parse(d, arg);
    break;
  case CON_ART_CREATE:
    art_edit_parse(d, arg);
    break;
  case CON_PET_CREATE:
    create_pet_parse(d, arg);
    break;
  case CON_FCUSTOMIZE:
  case CON_BCUSTOMIZE:
  case CON_PCUSTOMIZE:
  case CON_ACUSTOMIZE:
    cedit_parse(d, arg);
    break;
  case CON_PC_EXDESC_EDIT:
    pc_exdesc_edit_parse(d, arg);
    break;
  case CON_VEHCUST:
    vehcust_parse(d, arg);
    break;
  case CON_INITIATE:
    init_parse(d, arg);
    break;
  case CON_SUBMERSION:
    submersion_parse(d, arg);
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
  case CON_HOUSEEDIT_COMPLEX:
    houseedit_complex_parse(d, arg);
    break;
  case CON_HOUSEEDIT_APARTMENT:
    houseedit_apartment_parse(d, arg);
    break;
  case CON_GET_NAME:            /* wait for input of name */
    d->idle_ticks = 0;

    if (!*arg) {
      d->invalid_name++;
      if (d->invalid_name > 3)
        close_socket(d);
      else {
        SEND_TO_Q("\r\nWhat name do you wish to be called by? ", d);
#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
        verify_every_pointer_we_can_think_of();
#endif
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

#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
        verify_every_pointer_we_can_think_of();
#endif
        return;
      }
      // Drop the HELP crawler.
      if (strcmp(buf, "HELP") == 0) {
        close_socket(d);
#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
        verify_every_pointer_we_can_think_of();
#endif
        return;
      }
      if (does_player_exist(tmp_name)) {
        d->character = playerDB.LoadChar(tmp_name, TRUE, PC_LOAD_REASON_ENTER_PASSWORD);
        d->character->desc = d;

        snprintf(buf, sizeof(buf), "Welcome back. Enter your password. Not %s? Enter 'abort' to try a different name. ", CAP(tmp_name));
        SEND_TO_Q(buf, d);
        echo_off(d);

        STATE(d) = CON_PASSWORD;
      } else {
        /* player unknown -- make new character */
        for (struct descriptor_data *k = descriptor_list; k; k = k->next)
          if (k->character && k->character->player.char_name && !str_cmp(k->character->player.char_name, tmp_name)) {
            SEND_TO_Q("There is already someone creating a character with that name.\r\nName: ", d);
#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
            verify_every_pointer_we_can_think_of();
#endif
            return;
          }
        if (d->character == NULL) {
          d->character = Mem->GetCh();
          d->character->load_origin = PC_LOAD_REASON_CHARACTER_CREATION;

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
        SEND_TO_Q("Sorry, new characters are not allowed from your site. Please connect to an existing character, or reach out to " STAFF_CONTACT_EMAIL " for next steps.\r\n", d);
        STATE(d) = CON_CLOSE;
#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
        verify_every_pointer_we_can_think_of();
#endif
        return;
      }
      if (restrict_mud) {
        if (restrict_mud == LVL_MAX)
          SEND_TO_Q("The MUD is being reconfigured and is not accepting connections.  Try again a bit later.\r\n", d);
        else {
          SEND_TO_Q(WIZLOCK_MSG, d);
        }
        snprintf(buf, sizeof(buf), "Request for new char %s denied from %s (wizlock)", GET_CHAR_NAME(d->character), d->host);
        mudlog(buf, d->character, LOG_CONNLOG, TRUE);
        STATE(d) = CON_CLOSE;
#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
        verify_every_pointer_we_can_think_of();
#endif
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
    d->idle_ticks = 0;

    if (!*arg) {
      close_socket(d);
#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
      verify_every_pointer_we_can_think_of();
#endif
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
#ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
      verify_every_pointer_we_can_think_of();
#endif
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
          SEND_TO_Q("The MUD is about to reboot. Please try again in a few minutes.\r\n", d);
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
      log_vfprintf("[CONNLOG: %s connecting from %s with fingerprint %s and JSON '''%s''']", GET_CHAR_NAME(d->character), d->host, get_descriptor_fingerprint(d), d->pProtocol ? d->pProtocol->new_environ_info.dump().c_str() : "{}");
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
      PLR_FLAGS(d->character).SetBit(PLR_IN_CHARGEN);
      init_create_vars(d);
      ccr_pronoun_menu(d);
    } else {
      if (STATE(d) != CON_CHPWD_VRFY)
        d->character = playerDB.LoadChar(GET_CHAR_NAME(d->character), TRUE, PC_LOAD_REASON_CON_QVERIFYPW);
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
          load_room_vnum = archetypes[GET_ARCHETYPAL_TYPE(d->character)]->start_room;
          // Correct for invalid archetype start rooms.
          if (real_room(load_room_vnum) == NOWHERE) {
            snprintf(buf, sizeof(buf), "WARNING: Start room %ld for archetype %s does not exist!",
                     load_room_vnum,
                     archetypes[GET_ARCHETYPAL_TYPE(d->character)]->name);
            mudlog(buf, NULL, LOG_SYSLOG, TRUE);
            load_room_vnum = newbie_start_room;
          }
          do_start(d->character, FALSE);
        } else {
          load_room_vnum = newbie_start_room;
          do_start(d->character, TRUE);
        }

        playerDB.SaveChar(d->character, load_room_vnum);
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
        char char_name[MAX_INPUT_LENGTH + 1];
        strlcpy(char_name, GET_CHAR_NAME(d->character), sizeof(char_name));
        extract_char(d->character, FALSE);

        d->character = playerDB.LoadChar(char_name, false, PC_LOAD_REASON_MAIN_MENU_1);
        d->character->desc = d;
        PLR_FLAGS(d->character).RemoveBits(PLR_JUST_DIED, PLR_DOCWAGON_READY, ENDBIT);
        if (PLR_FLAGGED(d->character, PLR_NEWBIE)) {
          GET_PHYSICAL(d->character) = GET_MAX_PHYSICAL(d->character);
          GET_MENTAL(d->character) = GET_MAX_MENTAL(d->character);
        } else {
          GET_PHYSICAL(d->character) = (int)(GET_MAX_PHYSICAL(d->character) * .4);
          GET_MENTAL(d->character) = (int)(GET_MAX_MENTAL(d->character) * .4);
        }

        {
          // Check if they're naked.
          int eq_idx = 0;
          while (eq_idx < NUM_WEARS) {
            if (GET_EQ(d->character, eq_idx++))
              break;
          }
          // If they are, give them a hospital gown.
          if (eq_idx >= NUM_WEARS) {
            struct obj_data *paper_gown = read_object(OBJ_DOCWAGON_PAPER_GOWN, VIRTUAL, OBJ_LOAD_REASON_SHAMEFUL_NUDITY);
            if (paper_gown) {
              equip_char(d->character, paper_gown, WEAR_BODY);
            } else {
              mudlog_vfprintf(d->character, LOG_SYSLOG, "SYSERR: Unabled to load DocWagon paper gown (object %ld)! You probably need to create it.", OBJ_DOCWAGON_PAPER_GOWN);
            }
          }
        }

        // Turn on perception for ghouls.
        if (IS_GHOUL(d->character)) {
          PLR_FLAGS(d->character).SetBit(PLR_PERCEIVE);
        }

        playerDB.SaveChar(d->character, GET_LOADROOM(d->character));
      }
      // Wipe out various pointers related to game state and recalculate carry weight.
      reset_char(d->character);
      PLR_FLAGS(d->character).RemoveBit(PLR_CUSTOMIZE);
      add_ch_to_character_list(d->character, "entering game from option 1");
      d->character->player.time.logon = time(0);

      if (PRF_FLAGGED(d->character, PRF_DISABLE_XTERM)) {
        disable_xterm_256(d);
      }

      if (PRF_FLAGGED(d->character, PRF_COERCE_ANSI) && d->pProtocol) {
        d->pProtocol->do_coerce_ansi_capable_colors_to_ansi = TRUE;
      }

      // Load their vehicles.
      load_vehicles_for_idnum(GET_IDNUM(d->character));

      // Clear any mistaken / bugged bits.
      PLR_FLAGS(d->character).RemoveBit(PLR_IS_TEMPORARILY_LOADED);

      // Rewrote the entire janky-ass load room tree.
      // First: Frozen characters. They go to the frozen start room.
      if (PLR_FLAGGED(d->character, PLR_FROZEN)) {
        mudlog_vfprintf(d->character, LOG_MISCLOG, "Setting loadroom for %s (%ld) to the frozen room (%ld).", GET_CHAR_NAME(d->character), GET_IDNUM(d->character), frozen_start_room);
        load_room_vnum = frozen_start_room;
      }

      // Next: Unauthed (chargen) characters. They go to the start of their chargen areas.
      else if (PLR_FLAGGED(d->character, PLR_NOT_YET_AUTHED)) {
        if (GET_ARCHETYPAL_MODE(d->character)) {
          load_room_vnum = archetypes[GET_ARCHETYPAL_TYPE(d->character)]->start_room;

          if (real_room(load_room_vnum) == NOWHERE) {
            mudlog_vfprintf(d->character, LOG_SYSLOG, "SYSERR: Archetypal start room %ld for arch %d (%s) did not exist! Redirecting to standard CG.", 
                            load_room_vnum, 
                            GET_ARCHETYPAL_TYPE(d->character),
                            archetypes[GET_ARCHETYPAL_TYPE(d->character)]->name);

            load_room_vnum = RM_CHARGEN_START_ROOM;
          } else {
            mudlog_vfprintf(d->character, LOG_MISCLOG, "Loadroom for %s (%ld) is the %s archetypal start room (%ld).", 
                          GET_CHAR_NAME(d->character), 
                          GET_IDNUM(d->character), 
                          archetypes[GET_ARCHETYPAL_TYPE(d->character)]->name,
                          load_room_vnum);
          }
        } else {
          load_room_vnum = RM_CHARGEN_START_ROOM;
          mudlog_vfprintf(d->character, LOG_MISCLOG, "Loadroom for %s (%ld) is the newbie start room (%ld).", 
                          GET_CHAR_NAME(d->character), 
                          GET_IDNUM(d->character), 
                          load_room_vnum);
        }
      }

      // Next: Characters who have GET_LAST_IN rooms load in there.
      else if (GET_LAST_IN(d->character) != NOWHERE)
        load_room_vnum = GET_LAST_IN(d->character);

      // Next: Characters who have load rooms rooms load in there.
      else if (GET_LOADROOM(d->character) != NOWHERE) {
        load_room_vnum = GET_LOADROOM(d->character);
      }

      // Non-chargens don't get to start in chargen.
      if (!PLR_FLAGGED(d->character, PLR_NOT_YET_AUTHED)) {
        for (int arch_idx = 0; arch_idx < NUM_CCR_ARCHETYPES; arch_idx++) {
          if (load_room_vnum == archetypes[arch_idx]->start_room) {
            mudlog_vfprintf(d->character, LOG_SYSLOG, "SYSERR: Non-chargen character %s would have started in arch chargen! Sending to mortal start.", GET_CHAR_NAME(d->character));
            load_room_vnum = mortal_start_room;
            break;
          }
        }

        if (load_room_vnum == RM_CHARGEN_START_ROOM) {
          mudlog_vfprintf(d->character, LOG_SYSLOG, "SYSERR: Non-chargen character %s would have started in standard chargen! Sending to mortal start.", GET_CHAR_NAME(d->character));
          load_room_vnum = mortal_start_room;
        }
      }

      // Non-newbies don't get to start in the newbie loadroom-- rewrite their loadroom value.
      if (load_room_vnum == RM_NEWBIE_LOADROOM && !PLR_FLAGGED(d->character, PLR_NEWBIE)) {
        mudlog_vfprintf(d->character, LOG_SYSLOG, "Moving %s from newbie loadroom to mortal start (they're no longer a newbie)", GET_CHAR_NAME(d->character));
        load_room_vnum = mortal_start_room;
      }

      // Post-processing: Invalid load room characters go to the newbie or mortal start rooms.
      if (load_room_vnum == NOWHERE || (load_room_rnum = real_room(load_room_vnum)) < 0) {
        // Log in case of someone loading in a room that doesn't exist.
        if (load_room_vnum != NOWHERE) {
          mudlog_vfprintf(d->character, LOG_SYSLOG, "SYSERR: %s had an invalid loadroom! Redirecting to the expected start room.", GET_CHAR_NAME(d->character));
        }

        if (PLR_FLAGGED(d->character, PLR_NEWBIE)) {
          load_room_vnum = RM_NEWBIE_LOADROOM;
        } else {
          load_room_vnum = mortal_start_room;
        }
        load_room_rnum = real_room(load_room_vnum);
      }

      // Post-processing: Characters who are trying to load into a house get rejected if they're not allowed in there.
      if (world[load_room_rnum].apartment && !world[load_room_rnum].apartment->can_enter(d->character)) {
        mudlog_vfprintf(d->character, LOG_MISCLOG, "%s wanted to load into apartment %ld, but has no guest rights there. Redirecting to the expected start room.", 
                        GET_CHAR_NAME(d->character),
                        load_room_vnum);
        
        load_room_vnum = mortal_start_room;
        load_room_rnum = real_room(mortal_start_room);
      }

      // First-time login. This overrides the above, but it's for a good cause.
      if (!GET_LEVEL(d->character)) {
        if (GET_ARCHETYPAL_MODE(d->character)) {
          mudlog_vfprintf(d->character, LOG_SYSLOG, "Overriding new character %s'd loadroom to archetypal chargen.", GET_CHAR_NAME(d->character));
          load_room_vnum = archetypes[GET_ARCHETYPAL_TYPE(d->character)]->start_room;
          load_room_rnum = real_room(load_room_vnum);
          // Correct for invalid archetype start rooms.
          if (load_room_rnum < 0) {
            mudlog_vfprintf(NULL, LOG_SYSLOG, "WARNING: Start room %ld for archetype %s does not exist!",
                            archetypes[GET_ARCHETYPAL_TYPE(d->character)]->start_room,
                            archetypes[GET_ARCHETYPAL_TYPE(d->character)]->name);
            load_room_vnum = newbie_start_room;
            load_room_rnum = real_room(load_room_vnum);
          }
          do_start(d->character, FALSE);
        } else {
          mudlog_vfprintf(d->character, LOG_SYSLOG, "Overriding new character %s's loadroom to standard chargen.", GET_CHAR_NAME(d->character));
          load_room_vnum = newbie_start_room;
          load_room_rnum = real_room(load_room_vnum);
          do_start(d->character, TRUE);
        }

        playerDB.SaveChar(d->character, GET_ROOM_VNUM(&world[load_room_rnum]));
        send_to_char(START_MESSG, d->character);
      } else {
        // Save their updated load room.
        if (GET_LOADROOM(d->character) != load_room_vnum && load_room_vnum != GET_LAST_IN(d->character)) {
          playerDB.SaveChar(d->character, GET_ROOM_VNUM(&world[load_room_rnum]));
        }

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
        char_to_room(d->character, &world[load_room_rnum]);
      act("$n has entered the game.", TRUE, d->character, 0, 0, TO_ROOM);
      mudlog_vfprintf(d->character, LOG_CONNLOG, "%s has entered the game.", GET_CHAR_NAME(d->character));

      STATE(d) = CON_PLAYING;

      // KaVir's protocol snippet.
      MXPSendTag( d, "<VERSION>" );

      // GMCP Protocl injection
      SendGMCPCoreSupports ( d );
      SendGMCPCharInfo ( d->character );
      SendGMCPCharVitals ( d->character );
      SendGMCPCharPools ( d->character );

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
          add_veh_to_chs_subscriber_list(veh, d->character, "login sub regen", TRUE);
        }
      }
      regenerate_subscriber_list_rankings(d->character);

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

      // Refund syspoints for prestige purchases if they're in chargen.
      refund_chargen_prestige_syspoints_if_needed(d->character);

      DeleteChar(GET_IDNUM(d->character));

      snprintf(buf, sizeof(buf), "Character '%s' deleted!\r\nGoodbye.\r\n", GET_CHAR_NAME(d->character));
      SEND_TO_Q(buf, d);
      mudlog_vfprintf(d->character, LOG_MISCLOG, "%s (lev %d) has self-deleted.", GET_CHAR_NAME(d->character), GET_LEVEL(d->character));


      const char *char_name = str_dup(GET_CHAR_NAME(d->character));
      idnum_t idnum = GET_IDNUM(d->character);
      extract_char(d->character);

      // Validate that there are no objects left in the game that point to this character in any way.
      ObjList.CheckForDeletedCharacterFuckery(d->character, char_name, idnum);
      delete [] char_name;

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

  // Is a break here the right move? Need to test this.
  case CON_TEMPDESC_EDIT:
    break;

  default:
    log_vfprintf("SYSERR: Nanny: illegal state of con'ness %d; closing connection", STATE(d));
    close_socket(d);
    break;
  }
}

#ifdef LOG_COMMANDS
void log_command(struct char_data *ch, const char *argument, const char *tcname) {
  if (!ch)
    return;

  // Discard directional commands and other high-noise things that can't affect other players.
  const char *directional_and_purely_informational_commands[] = {
    "north", "south", "east", "west", "up", "down",
    "northeast", "ne",
    "southeast", "se",
    "southwest", "sw",
    "northwest", "nw",
    "score", "equipment", "inventory", "status", "affects",
    "look", "scan", "probe", "alias", "help",
    "progress", "time",
    "skills", "powers", "spells",
    "list", "info", "recap", "balance",
    "\n" // this MUST be last
  };
  const char *discard_commands[] = {
    "search",
    "hail", "push",
    "radio", "phone",
    "drive", "speed", "rig",
    "stand", "sit", "rest", "nod",
    "open", "close", "receive", "buy", "sell",
    "wear", "remove", "draw", "holster",
    "kill", "hit", "shoot", "kick",
    "\n" // this MUST be last
  };

  // Skip any commands that are of very low interest to us.
  for (int i = 0; *directional_and_purely_informational_commands[i] != '\n'; i++)
    if (str_str(directional_and_purely_informational_commands[i], argument))
      return;

  // If they haven't earned additional scrutiny, skip common spammy commands as well.
  if (GET_LEVEL(ch) <= LVL_MORTAL && !PLR_FLAGGED(ch, PLR_ADDITIONAL_SCRUTINY)) {
    for (int i = 0; *discard_commands[i] != '\n'; i++)
      if (str_str(discard_commands[i], argument))
        return;
  }

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
  } else if (PLR_FLAGGED(ch, PLR_REMOTE)) {
    struct veh_data *veh;
    RIG_VEH(ch, veh);
    snprintf(location_buf, sizeof(location_buf), "%ld (rig)", GET_ROOM_VNUM(get_veh_in_room(veh)));
  } else if (ch->in_room)
    snprintf(location_buf, sizeof(location_buf), "%ld", GET_ROOM_VNUM(ch->in_room));
  else if (ch->in_veh)
    snprintf(location_buf, sizeof(location_buf), "veh #%ld (@%ld)", ch->in_veh->idnum, GET_ROOM_VNUM(get_ch_in_room(ch)));

  // Compose name string.
  char name_buf[250];
  if (ch->desc && ch->desc->original)
    snprintf(name_buf, sizeof(name_buf), "%s (as %s)", GET_CHAR_NAME(ch->desc->original), GET_NAME(ch));
  else
    strlcpy(name_buf, GET_CHAR_NAME(ch), sizeof(name_buf) - 1);

  // If it's a REPLY command, add in last-told info.
  char tell_buf[250] = { '\0' };
  // todo: turn this into a regex comparison against '^\s?re?p?l?y?\s'
  if (GET_LAST_TELL(ch) > 0 && *argument == 'r'
      && (argument[1] == ' '
          || (argument[1] == 'e' && (argument[2] == ' '
                                     || (argument[2] == 'p' && (argument[3] == ' ' || argument[3] == 'l'))))))
  {
    for (struct descriptor_data *desc = descriptor_list; desc; desc = desc->next) {
      struct char_data *tch = desc->original ? desc->original : desc->character;
   
      if (tch && !IS_NPC(tch) && GET_IDNUM(tch) == GET_LAST_TELL(ch)) {
        snprintf(tell_buf, sizeof(tell_buf), "(to %s) ", GET_CHAR_NAME(tch));
        break;
      }
    }
    if (!*tell_buf)
      snprintf(tell_buf, sizeof(tell_buf), "(to %ld) ", GET_LAST_TELL(ch));
  }

  // Write the command to the buffer.
  char cmd_buf[MAX_INPUT_LENGTH * 3];
  snprintf(cmd_buf, sizeof(cmd_buf), "COMMANDLOG: %s @ %s: %s%s", name_buf, location_buf, tell_buf, argument);

  // TODO: Save to a file based on the PC's name.
  log(cmd_buf);
}
#endif

ACMD(do_unsupported_command) {
  switch (subcmd) {
    case SCMD_INTRODUCE:
      send_to_char("(Sorry, that command isn't supported here. Instead, you can introduce yourself with the ^WSAY^n or ^WEMOTE^n commands, and the listeners can ^WREMEMBER^n you.)\r\n", ch);
      return;
    default:
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Unsupported SCMD to do_unsupported_command(): %d", subcmd);
      send_to_char("Sorry, that command isn't supported here.\r\n", ch);
      return;
  }
}

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
  COMMAND_ALIAS("bw", "northwest");
  COMMAND_ALIAS("mw", "northwest");
  COMMAND_ALIAS("en", "northeast");
  COMMAND_ALIAS("nr", "northeast");
  COMMAND_ALIAS("es", "southeast");
  COMMAND_ALIAS("sse", "southeast");
  COMMAND_ALIAS("ssw", "southwest");
  COMMAND_ALIAS("norht", "north"); // this one happened 18 times
  COMMAND_ALIAS("esat", "east"); // this one only 8
  COMMAND_ALIAS("leve", "leave");
  COMMAND_ALIAS("laeve", "leave");
  COMMAND_ALIAS("elave", "leave");
  COMMAND_ALIAS("mw", "northwest");

  // Common typos and fuckups.
  COMMAND_ALIAS("receieve", "receive");
  COMMAND_ALIAS("recieve", "receive");
  COMMAND_ALIAS("dorp", "drop");
  COMMAND_ALIAS("weild", "wield");
  COMMAND_ALIAS("prove", "probe");
  COMMAND_ALIAS("prbe", "probe");
  COMMAND_ALIAS("chekc", "check");
  COMMAND_ALIAS("opend", "open");
  COMMAND_ALIAS("opene", "open");
  COMMAND_ALIAS("opwn", "open");
  COMMAND_ALIAS("leaev", "leave");
  COMMAND_ALIAS("leve", "leave");
  COMMAND_ALIAS("swith", "switch");
  COMMAND_ALIAS("swtich", "switch");
  COMMAND_ALIAS("drie", "drive");
  COMMAND_ALIAS("cyberwear", "cyberware");
  COMMAND_ALIAS("biowear", "bioware");
  COMMAND_ALIAS("lisy", "list");
  COMMAND_ALIAS("lst", "list");
  COMMAND_ALIAS("lsit", "list");
  COMMAND_ALIAS("ist", "list");
  COMMAND_ALIAS("lisr", "list");
  COMMAND_ALIAS("lost", "list");
  COMMAND_ALIAS("listr", "list");
  COMMAND_ALIAS("lit", "list");
  COMMAND_ALIAS("llist", "list");
  COMMAND_ALIAS("slel", "sell");
  COMMAND_ALIAS("ivn", "inventory");
  COMMAND_ALIAS("inc", "inventory");
  COMMAND_ALIAS("hoslter", "holster");
  COMMAND_ALIAS("stnad", "stand");
  COMMAND_ALIAS("saerch", "search");
  COMMAND_ALIAS("serach", "search");
  COMMAND_ALIAS("searcg", "search");
  COMMAND_ALIAS("searhc", "search");
  COMMAND_ALIAS("searcch", "search");
  COMMAND_ALIAS("sarch", "search");
  COMMAND_ALIAS("seach", "search");
  COMMAND_ALIAS("earch", "search");
  COMMAND_ALIAS("shot", "shoot");
  COMMAND_ALIAS("trian", "train");
  COMMAND_ALIAS("recpa", "recap");
  COMMAND_ALIAS("scoe", "score");
  COMMAND_ALIAS("core", "score");
  COMMAND_ALIAS("scire", "score");
  COMMAND_ALIAS("scoer", "score");
  COMMAND_ALIAS("sore", "score");
  COMMAND_ALIAS("hial", "hail");
  COMMAND_ALIAS("haul", "hail");
  COMMAND_ALIAS("clsoe", "close");
  COMMAND_ALIAS("swithc", "switch");
  COMMAND_ALIAS("wl", "quicklook");
  COMMAND_ALIAS("skils", "skills");
  COMMAND_ALIAS("bind", "bond");
  COMMAND_ALIAS("doante", "donate");
  COMMAND_ALIAS("gird", "gridguide");
  COMMAND_ALIAS("percieve", "perceive");
  COMMAND_ALIAS("sheaht", "sheathe");
  COMMAND_ALIAS("senak", "sneak");
  COMMAND_ALIAS("etner", "enter");
  COMMAND_ALIAS("wehre", "where");
  COMMAND_ALIAS("whre", "where");
  COMMAND_ALIAS("socre", "score");
  COMMAND_ALIAS("dirve", "drive");
  COMMAND_ALIAS("park", "drive");
  COMMAND_ALIAS("drove", "drive");
  COMMAND_ALIAS("ener", "enter");
  COMMAND_ALIAS("satnd", "stand");
  COMMAND_ALIAS("waer", "wear");
  COMMAND_ALIAS("porbe", "probe");
  COMMAND_ALIAS("levae", "leave");
  COMMAND_ALIAS("lave", "leave");
  COMMAND_ALIAS("eave", "leave");
  COMMAND_ALIAS("keave", "leave");
  COMMAND_ALIAS("leace", "leave");
  COMMAND_ALIAS("relaod", "reload");
  COMMAND_ALIAS("relod", "reload");
  COMMAND_ALIAS("scpre", "score");
  COMMAND_ALIAS("llook", "look");
  COMMAND_ALIAS("sneka", "sneak");
  COMMAND_ALIAS("ear", "eat");
  COMMAND_ALIAS("holser", "holster");
  COMMAND_ALIAS("pugh", "push");
  COMMAND_ALIAS("oush", "push");
  COMMAND_ALIAS("emtoe", "emote");
  COMMAND_ALIAS("kust", "customize");
  COMMAND_ALIAS("daig", "diagnose");
  COMMAND_ALIAS("oc", "ooc");
  COMMAND_ALIAS("lleave", "leave");
  COMMAND_ALIAS("leavee", "leave");
  COMMAND_ALIAS("jov", "job");
  COMMAND_ALIAS("eeq", "equipment");

  COMMAND_ALIAS("but", "put");
  COMMAND_ALIAS("out", "put");
  COMMAND_ALIAS("pot", "put");
  COMMAND_ALIAS("ptu", "put");

  // Combat stuff.
  COMMAND_ALIAS("attack", "kill");
  COMMAND_ALIAS("stab", "kill");
  COMMAND_ALIAS("unload", "eject");
  COMMAND_ALIAS("fire", "shoot");
  COMMAND_ALIAS("shoto", "shoot");

  // Misc aliases.
  COMMAND_ALIAS("taxi", "hail");
  COMMAND_ALIAS("cab", "hail");
  COMMAND_ALIAS("yes", "nod");
  COMMAND_ALIAS("setup", "unpack");
  COMMAND_ALIAS("ability", "abilities");
  COMMAND_ALIAS("guest", "house");
  COMMAND_ALIAS("unready", "ready");
  COMMAND_ALIAS("strap", "holster"); // Not sure about this one.
  COMMAND_ALIAS("unholster", "draw");
  COMMAND_ALIAS("deck", "software");
  COMMAND_ALIAS("email", "register");
  COMMAND_ALIAS("clothing", "equipment");
  COMMAND_ALIAS("armor", "equipment");
  COMMAND_ALIAS("programs", "software");
  COMMAND_ALIAS("bank", "balance");
  COMMAND_ALIAS("atm", "balance");
  COMMAND_ALIAS("recall", "recap");
  COMMAND_ALIAS("summon", "conjure");
  COMMAND_ALIAS("smash", "destroy");
  COMMAND_ALIAS("crypt", "radio");
  COMMAND_ALIAS("deploy", "unpack");
  COMMAND_ALIAS("undeploy", "pack");

  // Toggles.
  COMMAND_ALIAS("settings", "toggle");
  COMMAND_ALIAS("preferences", "toggle");
  COMMAND_ALIAS("options", "toggle");

  // Job interaction commands.
  COMMAND_ALIAS("endjob", "endrun");
  COMMAND_ALIAS("resign", "endrun");
  COMMAND_ALIAS("work",   "recap");
  COMMAND_ALIAS("recao",  "recap");
  COMMAND_ALIAS("reacp",  "recap");

  // one of the most common commands, although people eventually learn to just use 'l'
  COMMAND_ALIAS("olok", "look");
  COMMAND_ALIAS("lok", "look");
  COMMAND_ALIAS("loko", "look");
  COMMAND_ALIAS("loook", "look");
  COMMAND_ALIAS("ook", "look");
  COMMAND_ALIAS("ll", "look");
  COMMAND_ALIAS("glance", "quicklook");

  // equipment seems to give people a lot of trouble
  COMMAND_ALIAS("unwield", "remove");
  COMMAND_ALIAS("unwear", "remove");
  COMMAND_ALIAS("unequip", "remove");
  COMMAND_ALIAS("remvoe", "remove");
  COMMAND_ALIAS("unhold", "remove");

  // Door-unlocking and manipulation commands.
  COMMAND_ALIAS("pick", "bypass");
  COMMAND_ALIAS("hack", "bypass");
  COMMAND_ALIAS("poen", "open");
  COMMAND_ALIAS("oepn", "open");
  COMMAND_ALIAS("opne", "open");
  COMMAND_ALIAS("oen", "open");

  // Must be after 'pick'
  COMMAND_ALIAS("pickup", "get");

  // Commands from other games and misc.
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
  COMMAND_ALIAS("speak", "language");
  COMMAND_ALIAS("pose", "emote");
  COMMAND_ALIAS("craft", "create");
  COMMAND_ALIAS("description", "describe");
  COMMAND_ALIAS("ride", "enter"); // for motorcycles
  COMMAND_ALIAS("unsheathe", "draw");
  COMMAND_ALIAS("unfreeze", "thaw");
  COMMAND_ALIAS("disband", "ungroup");
  COMMAND_ALIAS("lose", "unfollow");
  COMMAND_ALIAS("hide", "sneak");
  COMMAND_ALIAS("privateroll", "privatedice");
  COMMAND_ALIAS("otell", "tell");
  COMMAND_ALIAS("all", "items");
  COMMAND_ALIAS("claim", "collect");
  COMMAND_ALIAS("chip", "jack");
  COMMAND_ALIAS("slot", "jack");
  COMMAND_ALIAS("unchip", "unjack");
  COMMAND_ALIAS("unslot", "unjack");
  COMMAND_ALIAS("lie", "lay");
  COMMAND_ALIAS("whereismycar", "wheresmycar");
  COMMAND_ALIAS("store", "put");
  COMMAND_ALIAS("swap", "switch");
  COMMAND_ALIAS("unconceal", "reveal");
  COMMAND_ALIAS("snipe", "shoot");
  COMMAND_ALIAS("penalty", "penalties");
  COMMAND_ALIAS("whois", "finger");

  // Some otaku conveniences because human language is weird
  COMMAND_ALIAS("form", "forms");
  COMMAND_ALIAS("submerge", "submerse");
  COMMAND_ALIAS("complex", "forms");

  // Alternate spellings.
  COMMAND_ALIAS("customise", "customize");
  COMMAND_ALIAS("effects", "affects");

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

  COMMAND_ALIAS("scna", "scan");
  COMMAND_ALIAS("scab", "scan");
  COMMAND_ALIAS("sacn", "scan");
  COMMAND_ALIAS("sscan", "scan");
  COMMAND_ALIAS("csan", "scan");
  COMMAND_ALIAS("scam", "scan");
  COMMAND_ALIAS("scasn", "scan");
  COMMAND_ALIAS("san", "scan");

  COMMAND_ALIAS("sya", "say");

  COMMAND_ALIAS("proeb", "probe");

  COMMAND_ALIAS("hep", "help");
  COMMAND_ALIAS("hlep", "help");
  COMMAND_ALIAS("hepl", "help");

  COMMAND_ALIAS("psuh", "push");
  COMMAND_ALIAS("whpo", "who");

  COMMAND_ALIAS("newbie", "question");
  COMMAND_ALIAS("newbies", "questions");
  COMMAND_ALIAS("mutenewbie", "mutequestions");

  // regional spellings
  COMMAND_ALIAS("practise", "practice");
  COMMAND_ALIAS("unpractise", "unpractice");

  // the weird shit
  COMMAND_ALIAS("whomst've", "who");


  // Found nothing, return the failure code.
  return -1;
}
#undef MAP_TYPO
