/* ************************************************************************
*   File: act.wizard.c                                  Part of CircleMUD *
*  Usage: Player-level god commands and other goodies                     *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <unordered_map>

#if defined(WIN32) && !defined(__CYGWIN__)
#else
#include <sys/time.h>
#endif

#include "awake.hpp"
#include "structs.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "interpreter.hpp"
#include "handler.hpp"
#include "db.hpp"
#include "newdb.hpp"
#include "dblist.hpp"
#include "house.hpp"
#include "memory.hpp"
#include "newmagic.hpp"
#include "newshop.hpp"
#include "quest.hpp"
#include "utils.hpp"
#include "constants.hpp"
#include "config.hpp"
#include "newmatrix.hpp"
#include "limits.hpp"
#include "security.hpp"
#include "perfmon.hpp"
#include "newmail.hpp"
#include "transport.hpp"
#include "vision_overhaul.hpp"
#include "newhouse.hpp"
#include "deck_build.hpp"
#include "redit.hpp"
#include "zoomies.hpp"
#include "bullet_pants.hpp"
#include "moderation.hpp"
#include "vehicles.hpp"
#include "olc.hpp"

#if defined(__CYGWIN__)
#include <crypt.h>
#endif
extern class memoryClass *Mem;
extern class objList ObjList;

/*   external vars  */
extern FILE *player_fl;
extern int restrict_mud;

/* for rooms */
extern void save_all_apartments_and_storage_rooms();

/* for chars */

extern struct time_info_data time_info;
extern int mortal_start_room;
extern int immort_start_room;
extern vnum_t newbie_start_room;
extern int frozen_start_room;
extern int last;
extern int max_ability(int i);
extern int count_objects(struct obj_data *obj);
extern void list_mob_precast_spells_to_ch(struct char_data *mob, struct char_data *ch);
extern const char *get_faction_name(idnum_t idnum, struct char_data *viewer);

extern const char *wound_arr[];
extern const char *material_names[];
extern int ViolencePulse;

extern void list_detailed_shop(struct char_data *ch, long shop_nr);
extern void list_detailed_quest(struct char_data *ch, long rnum);
extern void disp_init_menu(struct descriptor_data *d);
extern struct obj_data *shop_package_up_ware(struct obj_data *obj);

extern const char *pgroup_print_privileges(Bitfield privileges, bool full);
extern void display_pockets_to_char(struct char_data *ch, struct char_data *vict);

extern struct elevator_data *elevator;
extern int num_elevators;

extern int write_quests_to_disk(int zone);
extern void write_objs_to_disk(vnum_t zone);
extern void alarm_handler(int signal);
extern bool can_edit_zone(struct char_data *ch, rnum_t real_zone);
extern bool can_edit_zone(struct char_data *ch, struct zone_data *zone);
extern const char *render_door_type_string(struct room_direction_data *door);
extern void save_shop_orders();
extern void turn_hardcore_on_for_character(struct char_data *ch);
extern void turn_hardcore_off_for_character(struct char_data *ch);
extern void stop_rigging(struct char_data *ch);
extern void stop_driving(struct char_data *ch, bool is_involuntary);
extern void write_zone_to_disk(int vnum);

extern void DBFinalize();

extern void display_characters_ignore_entries(struct char_data *viewer, struct char_data *target);
extern int count_objects(struct obj_data *obj);

ACMD_DECLARE(do_goto);

SPECIAL(fixer);
SPECIAL(shop_keeper);
SPECIAL(johnson);

/* Copyover Code, ported to Awake by Harlequin *
 * (c) 1996-97 Erwin S. Andreasen <erwin@andreasen.org> */

extern int mother_desc, port;

/* Prototypes. */
void restore_character(struct char_data *vict, bool reset_staff_stats);
bool is_invalid_ending_punct(char candidate);
bool vnum_is_from_zone(vnum_t vnum, int zone_num);
int _error_on_invalid_real_mob(rnum_t rnum, int zone_num, char *buf, size_t buf_len);
int _error_on_invalid_real_room(rnum_t rnum, int zone_num, char *buf, size_t buf_len);
int _error_on_invalid_real_obj(rnum_t rnum, int zone_num, char *buf, size_t buf_len);
int _error_on_invalid_real_host(rnum_t rnum, int zone_num, char *buf, size_t buf_len);
int _error_on_invalid_real_veh(rnum_t rnum, int zone_num, char *buf, size_t buf_len);
bool vnum_is_from_canon_zone(vnum_t vnum);
void show_host_sheaf_to_ch(struct char_data *ch, struct host_data *host, bool only_print_on_error);

#define EXE_FILE "bin/awake" /* maybe use argv[0] but it's not reliable */

ACMD(do_crash_mud) {
#ifdef DEBUG
  if (subcmd != SCMD_BOOM) {
    send_to_char("You must type 'crashmud' in its entirety to cause a deliberate crash.\r\n", ch);
    return;
  }

  snprintf(buf, sizeof(buf), "%s has chosen to crash the MUD on purpose. Hope there's a good reason!", GET_CHAR_NAME(ch));
  mudlog(buf, ch, LOG_WIZLOG, TRUE);
  log(buf);

  assert(1 == 0);
#else
  send_to_char("Sorry, but you can only deliberately crash the MUD when it's running with the DEBUG flag enabled.\r\n", ch);
#endif
}

ACMD(do_copyover)
{
  FILE *fp;
  struct descriptor_data *d, *d_next;
  struct char_data *och;

  /* Old messages, preserved for posterity.
  // "I like copyovers, yes I do!  Eating player corpses in a copyover stew!\r\n",
  // "A Haiku while you wait: Copyover time.  Your quests and corpses are fucked.  Ha ha ha ha ha.\r\n",
  // "Yes. We did this copyover solely to fuck YOUR character over.\r\n",
  // "Ahh drek, Maestra's broke the mud again!  Go bug Che and he might fix it.\r\n",
  // "Deleting player corpses, please wait...\r\n",
  // "Please wait while your character is deleted.\r\n",
  // "You are mortally wounded and will die soon if not aided.\r\n",
  // "Connection closed by foreign host.\r\n",
  // "Jerry Garcia told me to type copyover.  He is wise, isn't he?\r\n",
  */
  const char *messages[] =
    {
      "This copyover has been brought to you by NERPS.  It's more than a lubricant, it's a lifestyle!\r\n", // 0
      "Yes, the MUD is lagging.  Deal with it.\r\n",
      "It's a copyover.  Now would be a good time to take out the trash.\r\n",
      "My dog told me to copyover. Goood dog, good dog.\r\n",
      "It's called a changeover, the movie goes on, and nobody in the audience has any idea.\r\n",
      "Oh shit, I forgot to compile.  I'm gonna have to do this again!\r\n", // 5
      "An invisible staff member says \x1B[0;35mOOCly\x1B[0m, \"I'm going to get fired for this.\"\r\n",
      "Yum Yum Copyover Stew, out with the old code, in with the new!\r\n",
      "\x1B[0;35m[\x1B[0mSerge\x1B[0;35m] \x1B[0;31m(\x1B[0mOOC\x1B[0;31m)\x1B[0m, \"This porn's taking too long to download, needs more bandwidth. So the Mud'll be back up in a bit.\"\r\n",
      "\x1B[0;35m[\x1B[0mLucien\x1B[0;35m] \x1B[0;31m(\x1B[0mOOC\x1B[0;31m)\x1B[0m, \"Honestly, I give this new code a 30% chance of crashing outright.\"\r\n",
      "There's a sound like a record scratching, and everything around you stutters to a standstill.", // 10
      "One moment while we drive up the server cost with heavy CPU usage...\r\n",
      "You wake up. You're still a lizard sunning on a red rock. It was all a dream. The concept of selling 'feet pics' to pay back 'ripperdocs' is already losing its meaning as you open and lick your own eyeballs to moisten them. Time to eat a bug.\r\n",
      "For the briefest of moments, you peer beyond the veil, catching a glimpse of the whirling, gleaming machinery that lies at the heart of the world. Your mind begins to break down at the sight...\r\n",
      "Your vision goes black, then starts to fade in again. You're sitting in the back of a cart, your hands bound before you. A disheveled blonde man sitting across from you meets your eyes. \"Hey, you. You're finally awake.\"\r\n",
      "A sharp tone sounds, and suddenly all you can see is a glitchy field of yellow-and-black. A red info box hovers in front of your paralyzed form, reading \"FIXING ANOMALY, PLEASE STAND BY\"...\r\n", // 15
      "An unexplainable mist pours into the area, shrouding your surroundings into undefinable half-shapes that twitch oddly in the dimness. As you look around in bewilderment, a civil defense siren begins to sound...\r\n",
      "Your pocket secretary beeps, and you glance at it distractedly. But what's this? A notification of winning the lottery? A wire transfer of a hundred million nuyen?! You're finally free! You can retire from the shadows and live a normal life! You can--\r\n",
      "A bone-shaking rumble startles you, and your gaze flicks towards its source-- the rising sun on the horizon. But wait, the sun rose hours ago, and it's not usually shaped like a mushroom, is it...?\r\n",
      "You're one of the lucky ones-- you were looking at the night sky when it happened. Some barely-discernable speck flickered by, crossing the sky in the span of a second, and shattered the Moon into seven pieces...\r\n",
      "Some indescribable force urges you to visit https://youtu.be/x31tDT-4fQw.\r\n", // 20
      "A fuzz of static washes over your vision, and for a brief moment you realize that you're nothing more than a naked brain suspended in a jar, with electrodes plunging into you to carry a simulation of reality. You would scream, but you don't even have a mouth...\r\n",
      "A vibrant cobalt flash cracks across the sky, and as one, all the electronics around you shut down.\r\n",
      "You suddenly realize that you can't remember where you left your towel...!\r\n",
      "The leathery flap of dragon wings fills the air, and as you spin to look in that direction, all you see is a ball of fire racing towards you...!\r\n",
      "You're thrown from your feet as the Cascadia Fault ruptures! The scream of tortured steel fills the air as skyscrapers begin to collapse.\r\n", // 25
      "Remember when these used to take upwards of five minutes? Pepperidge Farm remembers.\r\n",
      "\"Right, see, the genre's called 'cyberpunk'. Sometimes you get cybered, and sometimes you get punked.\"\r\n",
      "\x1B[0;35m[\x1B[0mVile\x1B[0;35m] \x1B[0;31m(\x1B[0mOOC\x1B[0;31m)\x1B[0m, \"This one's probably my fault, too.\"\r\n",
      "\x1B[0;35m[\x1B[0mJank\x1B[0;35m] \x1B[0;31m(\x1B[0mOOC\x1B[0;31m)\x1B[0m, \"This is the perfect time to buy more NERPS!\"\r\n",
      "This is the way the world ends: Not with a bang, but with a copyover.\r\n", // 30
      "Your vision is briefly encompassed by a ring of ten candles, which extinguish one by one. As the final one darkens, a voice intones, 'These things are true: The world is dark.'\r\n",
      "The throaty rumble of your Super Destroyer's engines is a comforting feel beneath your feet. You clench the grip of your Liberator in anticipation as the PA calls out, \"Helldivers to Hellpods. Repeat, Helldivers, to Hellpods.\"\r\n",
      "Off in the distance, you can faintly make out the unearthly form of Vile standing atop a cliff. He's got his arms towards the sky in some sort of anime-esque power pose, and as he contorts his face with effort, an ominous blue glow starts to shine through the clouds above...\r\n",
      "Suddenly, your arm flops bonelessly to your side-- rather literally, I'm afraid. Damn you, Lockhart!\r\n",
      "Without warning, a raging torrent of red light crashes down on the city! This is it! The end times! The--\r\n" // 35
    };
  int mesnum = number(0, 35);

  fp = fopen (COPYOVER_FILE, "w");

  if (!fp) {
    send_to_char ("Copyover file not writeable, aborted.\r\n",ch);
    return;
  }

  // Check for PCs on quests and people not in PLAYING state.
  strncpy(buf, "Characters currently on quests: ", sizeof(buf));
  int num_questors = 0;
  int fucky_states = 0;
  int cab_inhabitants = 0;
  for (d = descriptor_list; d; d = d->next) {
    // Count PCs in weird states.
    if (STATE(d) != CON_PLAYING) {
      fucky_states++;
      continue;
    }

    if (!(och = d->character))
      continue;

    if (och == ch)
      continue;

    if (GET_QUEST(och)) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^c%s%s^n (idle: %d)",
               num_questors > 0 ? ", " : "",
               GET_CHAR_NAME(och),
               och->char_specials.timer);
      num_questors += 1;
    }
    // Count PCs in cabs.
    if (och->in_room && room_is_a_taxicab(GET_ROOM_VNUM(och->in_room)))
      cab_inhabitants++;
  }

  if (!ch->desc) {
    mudlog("SYSERR: Somehow, we ended up in COPYOVER with no ch->desc!", ch, LOG_SYSLOG, TRUE);
    return;
  }

  skip_spaces(&argument);
  // COPYOVER FORCE.
  if (!str_cmp(argument, "force")){
    snprintf(buf, sizeof(buf), "Forcibly copying over. This will disconnect %d player%s, refund %d cab fare%s, drop %d quest%s, and lose any repairman items.\r\n",
             fucky_states,    fucky_states    != 1 ? "s" : "",
             cab_inhabitants, cab_inhabitants != 1 ? "s" : "",
             num_questors,    num_questors    != 1 ? "s" : "");
    if (write_to_descriptor(ch->desc->descriptor, buf) < 0) {
      // Rofl, the copyover initiatior disconnected? Um.
      close_socket(ch->desc);
    }
  } 
  // Non-forcible copyover command (CHECK or CONFIRM).
  else {
    bool will_not_copyover = FALSE;

    // Check for questors.
    if (num_questors > 0) {
      send_to_char(ch, "There %s %d character%s doing autoruns right now.\r\n%s^n.\r\n",
                   num_questors != 1 ? "are" : "is",
                   num_questors,
                   num_questors != 1 ? "s" : "",
                   buf);
      will_not_copyover = TRUE;
    }

    // Check for PCs in non-playing states.
    if (fucky_states > 0) {
      send_to_char(ch, "%d player%s not in the playing state. Check USERS for details.\r\n",
                   fucky_states, fucky_states != 1 ? "s are" : " is");
      will_not_copyover = TRUE;
    }

    // Check for cab-riders.
    if (cab_inhabitants) {
      send_to_char(ch, "There %s %d %s.\r\n",
                   cab_inhabitants != 1 ? "are" : "is",
                   cab_inhabitants,
                   cab_inhabitants != 1 ? "people taking taxis" : "person taking a cab");
      will_not_copyover = TRUE;
    }

    // Check for repairman items.
    /*
    for (struct char_data *i = character_list; i; i = i->next_in_character_list) {
      if (IS_NPC(i) && MOB_HAS_SPEC(i, fixer) && i->carrying) {
        send_to_char("The repairman has unclaimed items.\r\n", ch);
        will_not_copyover = TRUE;
        break;
      }
    }
    */

    // There is no command in this if-statement that allows us to bypass a failed check state.
    if (will_not_copyover) {
      send_to_char("Copyover aborted. Use 'copyover force' to override this.\r\n", ch);
      return;
    } 
    // If there were no errors, note this.
    else {
      send_to_char("Copyover is possible, no error conditions noted.\r\n", ch);
    }

    // COPYOVER CHECK bails out at this point.
    if (!str_cmp(argument, "check")) {
      return;
    }

#ifdef USE_PRIVATE_CE_WORLD
#ifndef IS_BUILDPORT
    // Guard against accidental copyovers on main port.
    if (str_cmp(argument, "confirm") != 0) {
      send_to_char("This is the PLAYER PORT, so you need to type ^WCOPYOVER CONFIRM^n to execute.\r\n", ch);
      return;
    }
#endif // IS_BUILDPORT
#endif // USE_PRIVATE_CE_WORLD
  }

  snprintf(buf, sizeof(buf), "Copyover initiated by %s", GET_CHAR_NAME(ch));
  mudlog(buf, ch, LOG_WIZLOG, TRUE);

  rnum_t airborne_rnum = real_room(RM_AIRBORNE);
  rnum_t boneyard_rnum = real_room(65505);

  if (boneyard_rnum < 0 || airborne_rnum < 0) {
    log("COPYOVERLOG: Skipping flying folks, you have no airborne room and/or boneyard.");
  } else {
    log("COPYOVERLOG: Shifting flying folks to a runway.");
    struct room_data *boneyard = &world[boneyard_rnum];
    struct room_data *airborne = &world[airborne_rnum];

    for (struct veh_data *aircraft = airborne->vehicles, *next_veh; aircraft; aircraft = next_veh) {
      next_veh = aircraft->next_veh;
      mudlog_vfprintf(aircraft->people, LOG_SYSLOG, "Transferring '%s' to Boneyard for copyover.", GET_VEH_NAME(aircraft));
      veh_from_room(aircraft);
      veh_to_room(aircraft, boneyard);
      send_to_veh("Your vehicle has been safely transferred to the Boneyard.\r\n", aircraft, NULL, TRUE);
    }
  }

  log("COPYOVERLOG: Cleaning up repairman.");
  struct room_data *staff_workroom = &world[real_room(10000)];
  for (struct char_data *i = character_list; i; i = i->next_in_character_list) {
    if (IS_NPC(i) && MOB_HAS_SPEC(i, fixer)) {
      while (i->carrying) {
        struct obj_data *obj = i->carrying;

        char *representation = generate_new_loggable_representation(obj);

        // No PC, no problem.
        if (!does_player_exist(GET_OBJ_TIMER(obj))) {
          mudlog_vfprintf(NULL, LOG_SYSLOG, "Discarding unowned repairman object: %s", representation);
          obj_from_char(obj);
          extract_obj(obj);
          continue;
        } else {
          mudlog_vfprintf(NULL, LOG_SYSLOG, "Preserving %ld's repairman object: %s", GET_OBJ_TIMER(obj), representation);
        }

        delete [] representation;

        // Fully repair item.
        GET_OBJ_CONDITION(obj) = GET_OBJ_BARRIER(obj);

        // Fully repair MPCP of storebought deck.
        rnum_t obj_rnum = real_object(GET_OBJ_VNUM(obj));
        if (obj_rnum >= 0 && GET_OBJ_TYPE(obj) == ITEM_CYBERDECK) {
          GET_CYBERDECK_MPCP(obj) = GET_CYBERDECK_MPCP(&obj_proto[obj_rnum]);
        }

        // Box it up for handing off.
        struct obj_data *container = read_object(OBJ_LARGE_PLASTIBOARD_BOX, VIRTUAL, OBJ_LOAD_REASON_SPECPROC);

        char *player_name = get_player_name(GET_OBJ_TIMER(obj));
        snprintf(buf, sizeof(buf), "%s's boxed-up repairman item: %s^n", player_name, get_string_after_color_code_removal(GET_OBJ_NAME(obj), NULL));
        delete [] player_name;

        container->restring = str_dup(buf);
        container->graffiti = str_dup(buf);

        obj_from_char(obj);
        obj_to_obj(obj, container);

        // Put it in the staff workroom.
        obj_to_room(container, staff_workroom);
        if (staff_workroom->people)
          act("The repairman's assistant drops off $p for post-copyover distribution.", FALSE, staff_workroom->people, obj, 0, TO_ROOM);
      }
    }
  }


  log("COPYOVERLOG: Disconnecting players.");
  /* For each playing descriptor, save its state */
  for (d = descriptor_list; d ; d = d_next) {
    och = d->character;
    d_next = d->next; // delete from list, save stuff

    // drops those logging on
    if (!och || d->connected > CON_PLAYING) {
      write_to_descriptor (d->descriptor, "\r\nSorry, we are rebooting. Come back in a few minutes.\r\n");
      close_socket (d); // yer outta here!
      continue;
    }

    // Refund people in cabs for the extra cash. Fixes edge case of 'I only had enough to cover my original cab fare'.
    if (!PLR_FLAGGED(och, PLR_NEWBIE) && och->in_room && room_is_a_taxicab(GET_ROOM_VNUM(och->in_room))) {
      snprintf(buf, sizeof(buf), "You have been refunded %d nuyen to compensate for the extra cab fare.\r\n", MAX_CAB_FARE);
      write_to_descriptor(d->descriptor, buf);
      GET_NUYEN_RAW(och) += MAX_CAB_FARE;
    }

    fprintf (fp, "%d %s %s %s\n", d->descriptor, GET_CHAR_NAME(och), d->host, CopyoverGet(d));
    GET_LAST_IN(och) = get_ch_in_room(och)->number;
    if (!GET_LAST_IN(och) || GET_LAST_IN(och) == NOWHERE) {
      // Fuck it, send them to Grog's.
      snprintf(buf, sizeof(buf), "%s's location could not be determined by the current copyover logic. %s will load at Grog's (35500).",
              GET_CHAR_NAME(och), HSSH(och));
      mudlog(buf, och, LOG_SYSLOG, TRUE);
      GET_LAST_IN(och) = RM_ENTRANCE_TO_DANTES;
    }
    playerDB.SaveChar(och, GET_LOADROOM(och), TRUE);
    write_to_descriptor(d->descriptor, messages[mesnum]);
  }

  fprintf (fp, "-1\n");
  fclose (fp);

  log("Saving houses.");
  save_all_apartments_and_storage_rooms();
  /* Close reserve and other always-open files and release other resources */

  // Save vehicles.
  log("COPYOVERLOG: Saving vehicles.");
  save_vehicles(TRUE);

  // Save shop orders.
  log("COPYOVERLOG: Saving shop orders.");
  save_shop_orders();

  log("COPYOVERLOG: Closing database connection.");
  DBFinalize();

  log("COPYOVERLOG: Clearing alarm handler.");
  signal(SIGALRM, SIG_IGN);

  snprintf(buf, sizeof(buf), "%d", port);
  snprintf(buf2, sizeof(buf2), "-o%d", mother_desc);
  /* Ugh, seems it is expected we are 1 step above lib - this may be dangerous! */

  chdir ("..");

  execl (EXE_FILE, "awake", buf2, buf, (char *) NULL); // Flawfinder: ignore

  /* Failed - sucessful exec will not return */

  perror ("do_copyover: execl");
  send_to_char ("Copyover FAILED!\r\n",ch);

  exit (1); /* too much trouble to try to recover! */
}

/* End This Part of the Copyover System */

ACMD(do_olcon)
{
  if (olc_state == 0)
    olc_state = 1;
  else
    olc_state = 0;

  snprintf(buf, sizeof(buf), "OLC turned %s by %s.", ONOFF(olc_state), GET_CHAR_NAME(ch));
  mudlog(buf, ch, LOG_WIZLOG, TRUE);

  send_to_char(ch, "You turn the global OLC system %s.\r\n", ONOFF(olc_state));
}


extern int _NO_OOC_;

ACMD(do_oocdisable)
{
  if ( _NO_OOC_ )
    _NO_OOC_ = 0;
  else
    _NO_OOC_ = 1;

  snprintf(buf, sizeof(buf), "OOC channel turned %s by %s.", (_NO_OOC_ ? "OFF" : "ON"),
          GET_CHAR_NAME(ch));
  mudlog(buf, ch, LOG_WIZLOG, TRUE);
}

extern bool _OVERRIDE_ALLOW_PLAYERS_TO_USE_ROLLS_;

ACMD(do_playerrolls)
{
  if (_OVERRIDE_ALLOW_PLAYERS_TO_USE_ROLLS_) {
    _OVERRIDE_ALLOW_PLAYERS_TO_USE_ROLLS_ = FALSE;
    send_gamewide_annoucement("We hope you enjoyed being able to see the debug information! This ability has now been disabled.", TRUE);
  } else {
    _OVERRIDE_ALLOW_PLAYERS_TO_USE_ROLLS_ = TRUE;
    send_gamewide_annoucement("Players can now see the details on rolls by using the ^WTOGGLE ROLLS^n command.", TRUE);
  }
}

// Code for zecho (Neo)
ACMD(do_zecho)
{
  struct descriptor_data *d;
  struct room_data *room;

  skip_spaces(&argument);

  if (!*argument)
    send_to_char(ch, "Yes, but WHAT do you like to zecho?\r\n");
  else {
    room = get_ch_in_room(ch);
    act(argument, FALSE, ch, 0, 0, TO_CHAR);
    for (d = descriptor_list; d; d = d->next)
      if (!d->connected && d->character && d->character != ch)
        if (zone_table[get_ch_in_room(d->character)->zone].number == zone_table[room->zone].number &&
            !(subcmd == SCMD_AECHO && !SEES_ASTRAL(d->character)))
          act(argument, FALSE, d->character, 0, 0, TO_CHAR);
    snprintf(buf, sizeof(buf), "%s zechoed %s in zone #%d",
            GET_CHAR_NAME(ch), argument,
            zone_table[room->zone].number);
    mudlog(buf, ch, LOG_WIZLOG, TRUE);
  }
}
// End zecho code

ACMD(do_echo)
{
  struct veh_data *veh;
  skip_spaces(&argument);

  // Reject anyone who can't use the thing they're trying to use.
  if ((subcmd == SCMD_ECHO || subcmd == SCMD_AECHO)
      && (!PRF_FLAGGED(ch, PRF_QUESTOR) && (ch->desc && ch->desc->original ? GET_LEVEL(ch->desc->original) : GET_LEVEL(ch)) < LVL_ARCHITECT)) {
    send_to_char("Sorry, only Questors and staff can do that.\r\n", ch);
    return;
  }

  // Reject hitchers.
  if (PLR_FLAGGED(ch, PLR_MATRIX) && !ch->persona) {
    send_to_char(ch, "You can't do that while hitching.\r\n");
    return;
  }

  // Require an argument.
  if (!*argument) {
    send_to_char("Yes.. but what?\r\n", ch);
    return;
  }

  memset(buf, '\0', sizeof(buf));

  switch (subcmd) {
    case SCMD_EMOTE:
      // Possessive? We omit the starting space.
      if (argument[0] == '\'' && argument[1] == 's' ) {
        if (PLR_FLAGGED(ch, PLR_MATRIX))
          snprintf(buf, sizeof(buf), "%s%s", ch->persona->name, argument);
        else
          snprintf(buf, sizeof(buf), "$n%s", argument);
      } else {
        if (PLR_FLAGGED(ch, PLR_MATRIX))
          snprintf(buf, sizeof(buf), "%s %s", ch->persona->name, argument);
        else
          snprintf(buf, sizeof(buf), "$n %s", argument);
      }
      break;
    case SCMD_ECHO:
    case SCMD_AECHO:
      // Just write the entire argument into the buf verbatim.
      strncpy(buf, argument, sizeof(buf) - 1);
      break;
  }

  // send_to_char(ch, "Buf is: '%s'\r\n", buf);

  if (subcmd == SCMD_AECHO) {
    if (ch->in_room) {
      for (struct char_data *vict = ch->in_room->people; vict; vict = vict->next_in_room)
        if (ch != vict && SEES_ASTRAL(vict))
          act(buf, FALSE, ch, 0, vict, TO_VICT);
    } else {
      for (struct char_data *vict = ch->in_veh->people; vict; vict = vict->next_in_veh)
        if (ch != vict && SEES_ASTRAL(vict))
          act(buf, FALSE, ch, 0, vict, TO_VICT);
    }
  } else {
    if (PLR_FLAGGED(ch, PLR_MATRIX))
      send_to_host(ch->persona->in_host, buf, ch->persona, TRUE);
    else {
      struct char_data *vict;
      for (struct char_data *targ = ch->in_veh ? ch->in_veh->people : ch->in_room->people; targ; targ = ch->in_veh ? targ->next_in_veh : targ->next_in_room) {
        if (targ != ch) {
          if (IS_ASTRAL(ch) && !SEES_ASTRAL(targ))
            continue;
          int i = 0, newn = 0;
          memset(buf2, '\0', sizeof(buf2));
          for (; buf[i]; i++) {
            if (buf[i] == '@') {
              int i_at_char = i;
              char check[1024];
              int checkn = 0;
              memset(check, '\0', sizeof(check));
              for (i++; buf[i]; i++, checkn++)
                if (buf[i] == ' ' || buf[i] == '\'' || buf[i] == '.' || buf[i] == ',') {
                  i--;
                  break;
                } else check[checkn] = buf[i];
              if (!str_cmp(check, "self"))
                vict = ch;
              else
                vict = get_char_room_vis(ch, check);
              if (vict == targ)
                if (ch == vict) {
                  // First-character occurrence is actor.
                  if (i_at_char == 0)
                    strncpy(check, GET_CHAR_NAME(ch), sizeof(check) - 1);
                  else
                    strncpy(check, "yourself", sizeof(check) - 1);
                }
                else
                  strncpy(check, "you", sizeof(check) - 1);
              else if (vict)
                snprintf(check, sizeof(check), "%s", CAN_SEE(targ, vict) ? (safe_found_mem(targ, vict) ? CAP(safe_found_mem(targ, vict)->mem)
                        : GET_NAME(vict)) : "someone");
              snprintf(buf2 + newn, sizeof(buf) - newn, "%s", check);
              newn += strlen(check);
            } else {
              buf2[newn] = buf[i];
              newn++;
            }
          }
          act(buf2, FALSE, ch, 0, targ, TO_VICT);
        }
      }
      if (!ch->in_veh)
        for (veh = ch->in_room->vehicles; veh; veh = veh->next_veh)
          if (veh->type == VEH_DRONE && veh->rigger)
            act(buf, FALSE, ch, 0, veh->rigger, TO_VICT);
    }
  }
  if (PRF_FLAGGED(ch, PRF_NOREPEAT))
    send_to_char(OK, ch);
  else if (subcmd == SCMD_EMOTE)
    send_to_char(ch, "%s %s\r\n", GET_CHAR_NAME(ch), argument);
  else
    act(buf, FALSE, ch, 0, 0, TO_CHAR);

  if ( subcmd == SCMD_ECHO || subcmd == SCMD_AECHO) {
    snprintf(buf2, sizeof(buf2), "%s echoed %s at #%ld",
            GET_CHAR_NAME(ch), buf, ch->in_room ? GET_ROOM_VNUM(ch->in_room) : -1);
    mudlog(buf2, ch, LOG_WIZLOG, TRUE);
  }
}

ACMD(do_send)
{
  struct char_data *vict;

  half_chop(argument, arg, buf, sizeof(buf));

  if (!*arg) {
    send_to_char("Send what to who?\r\n", ch);
    return;
  }
  if (!(vict = get_char_vis(ch, arg))) {
    send_to_char(ch, "You don't see anyone named '%s' here.\r\n", arg);
    return;
  }
  if (!IS_NPC(vict) &&
      PLR_FLAGS(vict).AreAnySet(PLR_WRITING, PLR_MAILING, ENDBIT) &&
      !access_level(ch, LVL_EXECUTIVE)) {
    send_to_char(ch, "You can't send to someone who's writing a message.\r\n");
    return;
  } else
    if (!IS_NPC(vict) &&
        PLR_FLAGS(vict).AreAnySet(PLR_EDITING, PLR_SPELL_CREATE,
                                  PLR_CUSTOMIZE, ENDBIT) &&
        !access_level(ch, LVL_EXECUTIVE)) {
      send_to_char(ch, "You can't send to someone who's editing.\r\n");
      return;
    }
  send_to_char(buf, vict);
  send_to_char("\r\n", vict);
  if (PRF_FLAGGED(ch, PRF_NOREPEAT))
    send_to_char("Sent.\r\n", ch);
  else {
    send_to_char(ch, "You send '%s' to %s.\r\n", buf, GET_CHAR_NAME(vict));
    snprintf(buf2, sizeof(buf2), "%s sent %s to %s", GET_CHAR_NAME(ch), buf, GET_CHAR_NAME(vict));
    mudlog(buf2, ch, LOG_WIZLOG, TRUE);
  }
}

ACMD(do_gecho)
{
  struct descriptor_data *d;

  skip_spaces(&argument);

  if (!*argument)
    send_to_char("That must be a mistake...\r\n", ch);
  else {
    for (d = descriptor_list; d; d = d->next)
      if (!d->connected && d->character && d->character != ch &&
          !(subcmd == SCMD_AECHO && !SEES_ASTRAL(d->character)))
        act(argument, FALSE, d->character, 0, 0, TO_CHAR);

    if (PRF_FLAGGED(ch, PRF_NOREPEAT))
      send_to_char(OK, ch);
    else
      act(argument, FALSE, ch, 0, 0, TO_CHAR);
    snprintf(buf, sizeof(buf), "%s gecho'd '%s'", GET_CHAR_NAME(ch), argument);
    mudlog(buf, ch, LOG_WIZLOG, TRUE);
  }
}

/* take a string, and return an rnum.. used for goto, at, etc.  -je 4/6/93 */
struct room_data *find_target_room(struct char_data * ch, char *roomstr)
{
  int tmp;
  struct room_data *location = NULL;
  struct char_data *target_mob = NULL;
  struct obj_data *target_obj = NULL;

  if (!*roomstr)
  {
    send_to_char("You must supply a room number or name.\r\n", ch);
    return NULL;
  }
  if (isdigit(*roomstr) && !strchr(roomstr, '.'))
  {
    tmp = real_room(atoi(roomstr));
    if (tmp == NOWHERE || !(location = &world[tmp])) {
      send_to_char("No room exists with that number.\r\n", ch);
      return NULL;
    }
  } else if ((target_mob = get_char_vis(ch, roomstr)))
    location = target_mob->in_room;
  else if ((target_obj = get_obj_vis(ch, roomstr)))
  {
    if (target_obj->in_room)
      location = target_obj->in_room;
    else {
      if ((location = get_obj_in_room(target_obj))) {
        send_to_char(ch, "Going to that object (%s)'s containing room. Veh: %s, In-Obj: %s, Carried-By: %s, Worn-By: %s.\r\n",
                     GET_OBJ_NAME(target_obj),
                     target_obj->in_veh ? GET_VEH_NAME(target_obj->in_veh) : "(null)",
                     target_obj->in_obj ? GET_OBJ_NAME(target_obj->in_obj) : "(null)",
                     target_obj->carried_by ? GET_CHAR_NAME(target_obj->carried_by) : "(null)",
                     target_obj->worn_by ? GET_CHAR_NAME(target_obj->worn_by) : "(null)");
      } else {
        send_to_char("That object is lost in time and space.\r\n", ch);
        return NULL;
      }
    }
  } else
  {
    send_to_char("No such creature or object around.\r\n", ch);
    return NULL;
  }
  return location;
}

ACMD(do_at)
{
  char command[MAX_INPUT_LENGTH];
  struct room_data *location = NULL, *original_loc = NULL;
  struct veh_data *veh = NULL, *oveh = NULL;
  struct char_data *vict = NULL;

  half_chop(argument, buf, command, sizeof(command));
  if (!*buf) {
    send_to_char("You must supply a room number or a name.\r\n", ch);
    return;
  }

  if (!*command) {
    send_to_char("What do you want to do there?\r\n", ch);
    return;
  }

  if (!(location = find_target_room(ch, buf))) {
    if (!(vict = get_char_vis(ch, buf)))
      return;
    veh = vict->in_veh;
  }
  if (vict && PLR_FLAGGED(vict, PLR_EDITING)) {
    send_to_char("They are editing a room at the moment.\r\n", ch);
    return;
  }
  if (ch->in_veh)
    oveh = ch->in_veh;
  else
    original_loc = ch->in_room;

  FAILURE_CASE(!ch_can_bypass_edit_lock(ch, veh ? get_veh_in_room(veh) : location), "Sorry, that zone is locked to non-editors.");

  if (veh)
    char_to_veh(veh, ch);
  else {
    char_from_room(ch);
    ch->in_room = location;
  }
  command_interpreter(ch, command, GET_CHAR_NAME(ch));
  char_from_room(ch);
  if (oveh)
    char_to_veh(oveh, ch);
  else
    char_to_room(ch, original_loc);
}

ACMD(do_goto)
{
  char command[MAX_INPUT_LENGTH];
  struct room_data *location = NULL;
  struct char_data *vict = NULL;
  rnum_t rnum;

  half_chop(argument, buf, command, sizeof(command));

  // Only look for taxi destinations if our goto does not start with a digit.
  if (*buf && !isdigit(*buf)) {
    // Look for taxi destinations - Seattle.
    struct dest_data *dest_data_list = seattle_taxi_destinations;

    // Seattle taxi destinations, including deactivated and invalid ones.
    for (int dest = 0; !location && *(dest_data_list[dest].keyword) != '\n'; dest++) {
      if (!str_cmp(buf, dest_data_list[dest].keyword) && (rnum = real_room(dest_data_list[dest].vnum)) >= 0) {
        send_to_char(ch, "OK, going to Seattle taxi destination %s.\r\n", dest_data_list[dest].str);
        location = &world[rnum];
        break;
      }
    }

    // Portland taxi destinations.
    if (!location) {
      dest_data_list = portland_taxi_destinations;
      for (int dest = 0; !location && *(dest_data_list[dest].keyword) != '\n'; dest++) {
        if (!str_cmp(buf, dest_data_list[dest].keyword) && (rnum = real_room(dest_data_list[dest].vnum)) >= 0) {
          send_to_char(ch, "OK, going to Portland taxi destination %s.\r\n", dest_data_list->str);
          location = &world[rnum];
          break;
        }
      }
    }

    // Caribbean taxi destinations.
    if (!location) {
      dest_data_list = caribbean_taxi_destinations;
      for (int dest = 0; !location && *(dest_data_list[dest].keyword) != '\n'; dest++) {
        if (!str_cmp(buf, dest_data_list[dest].keyword) && (rnum = real_room(dest_data_list[dest].vnum)) >= 0) {
          send_to_char(ch, "OK, going to Caribbean taxi destination %s.\r\n", dest_data_list->str);
          location = &world[rnum];
          break;
        }
      }
    }

    // Also look for exact-match flight codes if the buf is exactly three characters long.
    if (!location && strlen(buf) == 3) {
      for (rnum_t rnum = 0; rnum <= top_of_world; rnum++) {
        if (world[rnum].flight_code && !str_cmp(world[rnum].flight_code, buf)) {
          send_to_char(ch, "OK, going to airstrip %s.\r\n", world[rnum].flight_code);
          location = &world[rnum];
          break;
        }
      }
    }
  }

  // Look for either a target room or someone standing in a room.
  if (!location) {
    location = find_target_room(ch, buf);
  }

  // We found no target in a room-- look for one in a veh.
  if (!location) {
    vict = get_char_vis(ch, buf);

    if (!vict || !vict->in_veh) {
      // Error message happens in get_char_vis.
      return;
    }

    // Set location to be the room their vehicle is in.
    location = get_veh_in_room(vict->in_veh);
  }

  // Don't bother with the rest of this if you're not moving.
  FAILURE_CASE(location == ch->in_room, "You're already there.");

  // Perform location validity check for room number.
  FAILURE_CASE(location->number == 0 || location->number == 1, "You're not able to GOTO that room. If you need to do something there, use AT.");

  // Perform location validity check for level lock.
  FAILURE_CASE_PRINTF(location->staff_level_lock > GET_REAL_LEVEL(ch), "Sorry, you need to be a level-%d immortal to go there.", location->staff_level_lock);

  // Block level-2 goto for anything outside their edit zone.
  FAILURE_CASE(builder_cant_go_there(ch, location), "Sorry, as a first-level builder you're only able to move to rooms you have edit access for.");

  // Edit lock.
  FAILURE_CASE(!ch_can_bypass_edit_lock(ch, location), "Sorry, that zone is locked to non-editors.");

#ifndef IS_BUILDPORT
  if (!access_level(ch, LVL_ADMIN)) {
    struct room_data *ch_in_room = get_ch_in_room(ch);
    mudlog_vfprintf(ch, LOG_WIZLOG, "%s goto'd from '%s^g' (%ld) to '%s^g' (%ld)",
                    GET_CHAR_NAME(ch),
                    GET_ROOM_NAME(ch_in_room),
                    GET_ROOM_VNUM(ch_in_room),
                    GET_ROOM_NAME(location),
                    GET_ROOM_VNUM(location));
  }
#endif

  stop_driving(ch, FALSE);

  if (POOFOUT(ch))
    act(POOFOUT(ch), TRUE, ch, 0, 0, TO_ROOM);
  else
    act("$n disappears in a puff of smoke.", TRUE, ch, 0, 0, TO_ROOM);

  if (vict && vict->in_veh) {
    // We must set this in case the character is using GOTO SELF from a vehicle.
    struct veh_data *target_veh = vict->in_veh;

    char_from_room(ch);
    char_to_veh(target_veh, ch);

    // We always want to have the same vfront as the vict.
    ch->vfront = vict->vfront;

    // Swapping vfront messes with seating, so repair it.
    repair_vehicle_seating(target_veh);
  } else {
    char_from_room(ch);
    char_to_room(ch, location);
    GET_POS(ch) = POS_STANDING;
    update_pos(ch);
  }

  if (POOFIN(ch))
    act(POOFIN(ch), TRUE, ch, 0, 0, TO_ROOM);
  else
    act("$n appears with an ear-splitting bang.", TRUE, ch, 0, 0, TO_ROOM);

  look_at_room(ch, 0, 0);

  GET_POS(ch) = POS_STANDING;
}

void transfer_ch_to_ch(struct char_data *victim, struct char_data *ch) {
  if (!ch || !victim)
    return;

  stop_driving(victim, FALSE);

  act("$n is whisked away by the game's administration.", TRUE, victim, 0, 0, TO_ROOM);
  if (AFF_FLAGGED(victim, AFF_PILOT))
    AFF_FLAGS(victim).ToggleBit(AFF_PILOT);
  if (victim->char_specials.rigging) {
    victim->char_specials.rigging->cspeed = SPEED_OFF;
  }
  char_from_room(victim);

  if (ch->in_veh) {
    char_to_veh(ch->in_veh, victim);
    snprintf(buf2, sizeof(buf2), "%s transferred %s to %s in %s^g.", GET_CHAR_NAME(ch), IS_NPC(victim) ? GET_NAME(victim) : GET_CHAR_NAME(victim),
            GET_VEH_NAME(victim->in_veh), GET_ROOM_NAME(get_ch_in_room(victim)));
  } else {
    char_to_room(victim, ch->in_room);
    snprintf(buf2, sizeof(buf2), "%s transferred %s to %s^g.", GET_CHAR_NAME(ch), IS_NPC(victim) ? GET_NAME(victim) : GET_CHAR_NAME(victim), GET_ROOM_NAME(victim->in_room));
  }
  act("$n arrives from a puff of smoke.", TRUE, victim, 0, 0, TO_ROOM);
  act("$n has transferred you!", FALSE, ch, 0, victim, TO_VICT);
  mudlog(buf2, ch, LOG_WIZLOG, TRUE);
  if (GET_POS(victim) == POS_SITTING)
    GET_POS(victim) = POS_STANDING;
  look_at_room(victim, 0, 0);
}

#ifdef IS_BUILDPORT
// On the buildport, any staff member can transfer anyone below their level.
#define TRANSFER_LEVEL_REQ  LVL_BUILDER
#else
// On the mainport, you must be 5+.
#define TRANSFER_LEVEL_REQ  LVL_CONSPIRATOR
#endif

ACMD(do_trans)
{
  ACMD_DECLARE(do_transfer);

  if (!access_level(ch, TRANSFER_LEVEL_REQ)) {
    do_transfer(ch, argument, 0, 0);
    return;
  }
  struct descriptor_data *i;
  struct char_data *victim;

  any_one_arg(argument, buf);

  if (!*buf)
    send_to_char("Whom do you wish to transfer to your location?\r\n", ch);
  else if (str_cmp("*", buf)) {
    if (!(victim = get_char_vis(ch, buf)))
      send_to_char(ch, "You don't see anyone named '%s' here.\r\n", buf);
    else if (victim == ch)
      send_to_char("That doesn't make much sense, does it?\r\n", ch);
    else {
      if ((GET_LEVEL(ch) < GET_LEVEL(victim)) && !IS_NPC(victim)) {
        send_to_char("Go transfer someone your own size.\r\n", ch);
        return;
      }
      if (PLR_FLAGGED(victim, PLR_EDITING)) {
        act("Not while $E's editing!", FALSE, ch, 0, victim, TO_CHAR);
        return;
      } else if (PLR_FLAGGED(victim, PLR_SWITCHED)) {
        act("Not while $E's switched!", FALSE, ch, 0, victim, TO_CHAR);
        return;
      }
      transfer_ch_to_ch(victim, ch);
    }
  } else {                      /* Trans All */
    if (!access_level(ch, LVL_PRESIDENT)) {
      send_to_char("I think not.\r\n", ch);
      return;
    }

    for (i = descriptor_list; i; i = i->next)
      if (!i->connected && i->character && i->character != ch) {
        victim = i->character;
        if ((GET_LEVEL(ch) < GET_LEVEL(victim) && !IS_NPC(victim)) || PLR_FLAGGED(victim, PLR_EDITING) || PLR_FLAGGED(victim, PLR_SWITCHED))
          continue;
        transfer_ch_to_ch(victim, ch);
      }
    send_to_char(OK, ch);
  }
}
ACMD(do_vteleport)
{
  struct veh_data *veh = NULL;
  struct room_data *target = NULL;

  two_arguments(argument, buf, buf2);

  if (!*buf)
    send_to_char("What vehicle do you wish to teleport?\r\n", ch);
  else if (!(veh = get_veh_list(buf, ch->in_veh ? ch->in_veh->carriedvehs : ch->in_room->vehicles, ch)))
    send_to_char(ch, "You don't see any vehicles named '%s' here.\r\n", buf);
  else if (!*buf2)
    send_to_char("Where do you wish to send this vehicle?\r\n", ch);
  else if ((target = find_target_room(ch, buf2))) {
    snprintf(buf, sizeof(buf), "%s drives off into the sunset.\r\n", GET_VEH_NAME(veh));
    if (veh->in_room) {
      send_to_room(buf, veh->in_room);
    } else if (veh->in_veh) {
      send_to_veh(buf, veh->in_veh, ch, FALSE);
    }
    char was_in[1000];
    snprintf(was_in, sizeof(was_in), "%s '%s'", veh->in_veh ? "vehicle" : "room", veh->in_veh ? GET_VEH_NAME(veh->in_veh) : GET_ROOM_NAME(veh->in_room));

    veh_from_room(veh);
    veh_to_room(veh, target);
    snprintf(buf, sizeof(buf), "%s screeches suddenly into the area.\r\n", GET_VEH_NAME(veh));
    send_to_room(buf, veh->in_room);
    send_to_veh("You lose track of your surroundings.\r\n", veh, NULL, TRUE);
    mudlog_vfprintf(ch, LOG_WIZLOG, "%s teleported %s from %s to %s.", GET_CHAR_NAME(ch), GET_VEH_NAME(veh), was_in, target->name);
  }
}
ACMD(do_teleport)
{
  struct char_data *victim = NULL;
  struct room_data *target = NULL;

  two_arguments(argument, buf, buf2);

  if (!*buf)
    send_to_char("Whom do you wish to teleport?\r\n", ch);
  else if (!(victim = get_char_vis(ch, buf)))
    send_to_char(ch, "You don't see anyone named '%s' here.\r\n", buf);
  else if (victim == ch)
    send_to_char("Use 'goto' to teleport yourself.\r\n", ch);
  else if (GET_LEVEL(victim) > GET_LEVEL(ch) &&
           !access_level(ch, LVL_VICEPRES))
    send_to_char("Maybe you shouldn't do that.\r\n", ch);
  else if (PLR_FLAGGED(victim, PLR_EDITING))
    act("Not while $E's editing!", FALSE, ch, 0, victim, TO_CHAR);
  else if (PLR_FLAGGED(victim, PLR_SWITCHED))
    act("Not while $E's switched!", FALSE, ch, 0, victim, TO_CHAR);
  else if (!*buf2)
    send_to_char("Where do you wish to send this person?\r\n", ch);
  else if ((target = find_target_room(ch, buf2))) {
    char was_in[1000];
    snprintf(was_in, sizeof(was_in), "%s '%s'", victim->in_veh ? "vehicle" : "room", victim->in_veh ? GET_VEH_NAME(victim->in_veh) : GET_ROOM_NAME(victim->in_room));

    stop_driving(victim, FALSE);

    send_to_char(OK, ch);
    act("$n disappears in a puff of smoke.", TRUE, victim, 0, 0, TO_ROOM);
    char_from_room(victim);
    char_to_room(victim, target);
    act("$n arrives from a puff of smoke.", TRUE, victim, 0, 0, TO_ROOM);
    act("$n has teleported you!", FALSE, ch, 0, victim, TO_VICT);
    look_at_room(victim, 0, 0);
    mudlog_vfprintf(ch, LOG_WIZLOG, "%s teleported %s from %s to %s",
                    GET_CHAR_NAME(ch),
                    IS_NPC(victim) ? GET_NAME(victim) : GET_CHAR_NAME(victim),
                    was_in,
                    target->name);
  }
}

#define VNUM_LOOKUP(name) {                                            \
  if (is_abbrev(buf, #name)) {                                         \
    extern int vnum_ ## name(char *searchname, struct char_data * ch); \
    if (!vnum_ ## name(remainder, ch)) {                               \
      send_to_char("No " #name "s by that name.\r\n", ch);             \
    }                                                                  \
    return;                                                            \
  }                                                                    \
}

#define VNUM_USAGE_STRING "Usage: vnum { obj | mob | veh | room | host | ic | quest } <name>\r\n"
ACMD(do_vnum)
{
  char *remainder = one_argument(argument, buf);

  if (!*buf || !*buf2) {
    send_to_char(VNUM_USAGE_STRING, ch);
    return;
  }

  VNUM_LOOKUP(vehicle);
  VNUM_LOOKUP(mobile);
  VNUM_LOOKUP(object);
  VNUM_LOOKUP(room);
  VNUM_LOOKUP(host);
  VNUM_LOOKUP(ic);
  VNUM_LOOKUP(quest);
  VNUM_LOOKUP(text);

  send_to_char(VNUM_USAGE_STRING, ch);
}
#undef VNUM_USAGE_STRING

SPECIAL(traffic);
void do_stat_room(struct char_data * ch)
{
  struct extra_descr_data *desc;
  struct room_data *rm = get_ch_in_room(ch);
  int i, found = 0;
  struct obj_data *j = 0;
  struct char_data *k = 0;

  FAILURE_CASE(!ch_can_bypass_edit_lock(ch, rm), "Sorry, you can't stat that due to a zone edit lock.");

  send_to_char(ch, "Room name: ^c%s\r\n", rm->name);

  sprinttype(rm->sector_type, spirit_name, buf2, sizeof(buf2));
  send_to_char(ch, "Zone: [%d (%d)], VNum: [^g%8ld^n], RNum: [%5ld], Rating: [%2d], Type: %s\r\n",
               zone_table[rm->zone].number,
               rm->zone,
               rm->number,
               real_room(rm->number),
               rm->rating,
               buf2);

  rm->room_flags.PrintBits(buf2, MAX_STRING_LENGTH, room_bits, ROOM_MAX);
  send_to_char(ch, "Extra: [%4d], SpecProc: %s, Flags: %s Light: %s Smoke: %s, jurisdiction %s\r\n",
               rm->spec,
               (rm->func == NULL) ? "None" : (rm->func == traffic ? "^CTraffic^n" : "^CExists^n"),
               buf2,
               light_levels[light_level(rm)],
               light_levels[rm->vision[1]],
               jurisdictions[GET_JURISDICTION(rm)]);

  send_to_char(ch, "Effects: light[^c%d^n][^c%d^n][^c%d^n], peaceful[^c%d^n], poltergeist[^c%d^n][^c%d^n], icesheet[^c%d^n][^c%d^n], shadow[^c%d^n][^c%d^n], silence[^c%d^n][^c%d^n]\r\n",
               rm->light[0],
               rm->light[1],
               rm->light[2],
               rm->peaceful,
               rm->poltergeist[0],
               rm->poltergeist[1],
               rm->icesheet[0],
               rm->icesheet[1],
               rm->shadow[0],
               rm->shadow[1],
               rm->silence[0],
               rm->silence[1]
              );

  send_to_char("Description:\r\n", ch);
  if (rm->description)
    send_to_char(rm->description, ch);
  else
    send_to_char("  None.\r\n", ch);

  if (GET_APARTMENT(rm)) {
    send_to_char(ch, "Complex: %s, Apartment: %s, Subroom: %s\r\n",
                 GET_APARTMENT(rm)->get_complex() ? GET_APARTMENT(rm)->get_complex()->get_name() : "^RN^n",
                 GET_APARTMENT(rm)->get_name(),
                 GET_APARTMENT_SUBROOM(rm) ? "Y" : "^RN^n");
  }
  if (GET_APARTMENT_DECORATION(rm)) {
    send_to_char(ch, "Decoration:\r\n%s", GET_APARTMENT_DECORATION(rm));
  }

  if (rm->ex_description)
  {
    strlcpy(buf, "Extra descs:^c", sizeof(buf));
    for (desc = rm->ex_description; desc; desc = desc->next) {
      strlcat(buf, " ", sizeof(buf));
      strlcat(buf, desc->keyword, sizeof(buf));
    }
    send_to_char(ch, "%s^n\r\n", buf);
  }
  strlcpy(buf, "Chars present:^y", sizeof(buf));
  for (found = 0, k = rm->people; k; k = k->next_in_room)
  {
    if (!CAN_SEE(ch, k))
      continue;
    snprintf(buf2, sizeof(buf2), "%s %s(%s)", found++ ? "," : "", IS_NPC(k) ? GET_NAME(k) : GET_CHAR_NAME(k),
            (!IS_NPC(k) ? "PC" : (!IS_MOB(k) ? "NPC" : "MOB")));
    strlcat(buf, buf2, sizeof(buf));
    if (strlen(buf) >= 62) {
      if (k->next_in_room)
        strlcat(buf, ",\r\n", sizeof(buf));
      else
        strlcat(buf, "\r\n", sizeof(buf));
      send_to_char(buf, ch);
      *buf = found = 0;
    }
  }

  if (*buf) {
    strlcat(buf, "\r\n", sizeof(buf));
    send_to_char(buf, ch);
  }

  if (rm->contents)
  {
    strlcpy(buf, "Contents:^g", sizeof(buf));
    for (found = 0, j = rm->contents; j; j = j->next_content) {
      if (!CAN_SEE_OBJ(ch, j))
        continue;
      snprintf(buf2, sizeof(buf2), "%s %s", found++ ? "," : "", GET_OBJ_NAME(j));
      strlcat(buf, buf2, sizeof(buf));
      if (strlen(buf) >= 62) {
        if (j->next_content)
          strlcat(buf, ",\r\n", sizeof(buf));
        else
          strlcat(buf, "\r\n", sizeof(buf));
        send_to_char(buf, ch);
        *buf = found = 0;
      }
    }

    if (*buf) {
      strlcat(buf, "\r\n", sizeof(buf));
      send_to_char(buf, ch);
    }
  }
  if (rm->dirty_bit)
    send_to_char("Dirty bit is set.\r\n", ch);

  {
    bool printed_workshop_yet = FALSE;
    send_to_char("Workshops: ", ch);
    for (int i = 0; i < NUM_WORKSHOP_TYPES; i++) {
      if (rm->best_workshop[i]) {
        send_to_char(ch, "%s^c%s (%s)^n", printed_workshop_yet ? ", " : "", GET_OBJ_NAME(rm->best_workshop[i]), workshops[i]);
        printed_workshop_yet = TRUE;
      }
    }
    if (!printed_workshop_yet)
      send_to_char("^cNone.^n", ch);
    send_to_char("\r\n", ch);
  }

  for (i = 0; i < NUM_OF_DIRS; i++)
  {
    if (rm->dir_option[i]) {
      if (!rm->dir_option[i]->to_room)
        strlcpy(buf1, " ^cNONE^n", sizeof(buf1));
      else
        snprintf(buf1, sizeof(buf1), "^c%8ld^n", rm->dir_option[i]->to_room->number);
      sprintbit(rm->dir_option[i]->exit_info, exit_bits, buf2, sizeof(buf2));
      snprintf(buf, sizeof(buf), "Exit ^c%-5s^n:  To: [^c%s^n], Key: [^c%8ld^n], Keyword: "
              "^c%s^n, Type: ^c%s^n\r\n ", dirs[i], buf1, rm->dir_option[i]->key,
              rm->dir_option[i]->keyword ? rm->dir_option[i]->keyword : "None", buf2);
      send_to_char(buf, ch);
      if (rm->dir_option[i]->general_description)
        strlcpy(buf, rm->dir_option[i]->general_description, sizeof(buf));
      else
        strlcpy(buf, "  No exit description.\r\n", sizeof(buf));
      send_to_char(buf, ch);
    }
  }
}

void do_stat_host(struct char_data *ch, struct host_data *host)
{
  FAILURE_CASE(!ch_can_bypass_edit_lock(ch, host), "Sorry, you can't stat that due to a zone edit lock.");

  snprintf(buf, sizeof(buf), "Name: '^y%s^n', Keywords: %s\r\n", host->name, host->keywords);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Vnum: [^g%8ld^n] Rnum: [%5ld] Parent: [%8ld]\r\n",
               host->vnum, real_host(host->vnum), host->parent);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Type: [%10s] Security: [%s-%d^n] Alert: [%6s]\r\n", host_type[host->type],
               host_color[host->color], host->security, alerts[host->alert]);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Shutdown: [%2d] Successes: [%2d] MPCP: [%2d]\r\n", host->shutdown,
               host->shutdown_success, host->shutdown_mpcp);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Access: [^B%2ld^n] Control: [^B%2ld^n] Index: [^B%2ld^n] Files: [^B%2ld^n] Slave: [^B%2ld^n]\r\n",
               host->stats[ACIFS_ACCESS][0], host->stats[ACIFS_CONTROL][0], host->stats[ACIFS_INDEX][0], host->stats[ACIFS_FILES][0], host->stats[ACIFS_SLAVE][0]);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Deckers Present: ^y");
  bool prev = FALSE;
  for (struct matrix_icon *icon = host->icons; icon; icon = icon->next_in_host)
    if (icon->decker && icon->decker->ch)
    {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s%s (%d)", prev ? ", " : "", GET_CHAR_NAME(icon->decker->ch), icon->decker->tally);
      prev = TRUE;
    }
  strlcat(buf, "^n\r\n", sizeof(buf));
  send_to_char(buf, ch);

  show_host_sheaf_to_ch(ch, host, FALSE);
}

void do_stat_veh(struct char_data *ch, struct veh_data * k)
{
  FAILURE_CASE(!ch_can_bypass_edit_lock(ch, k), "Sorry, you can't stat that due to a zone edit lock.");

  long virt;
  virt = veh_index[k->veh_number].vnum;
  snprintf(buf, sizeof(buf), "Name: '^y%s^n', Aliases: %s\r\n",
          k->short_description, k->name);

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Vnum: [^g%8ld^n] Rnum: [%5ld] Type: [%10s] Idnum: [%8ld] Owner: [%8ld]\r\n",
          virt, k->veh_number, veh_types[k->type], k->idnum, k->owner);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Han: [^B%d^n]  Spe: [^B%d^n]  Acc: [^B%d^n]  Bod: [^B%d^n]  Arm: [^B%d^n]\r\n",
          k->handling, k->speed, k->accel, k->body, k->armor);

  char flag_buf[1000];
  k->flags.PrintBits(flag_buf, sizeof(flag_buf), veh_flag, NUM_VFLAGS);

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Sig: [^B%d^n]  Aut: [^B%d^n]  Pil: [^B%d^n]  Sea: [^B%d/%d^n]  Loa: [^B%d/%d^n]  Cos: [^B%d^n]\r\nFlags: [^B%s^n]\r\n",
          k->sig, k->autonav, k->pilot, k->seating[SEATING_FRONT], k->seating[SEATING_REAR], (int)k->usedload, (int)k->load, k->cost, *flag_buf ? flag_buf : "none");
  send_to_char(buf, ch);

  struct char_data *driver = NULL, *rigger = NULL;
  for (struct char_data *tmp = k->people; tmp; tmp = tmp->next_in_veh) {
    if (AFF_FLAGGED(tmp, AFF_PILOT))
      driver = tmp;
    if (AFF_FLAGGED(tmp, AFF_RIG))
      rigger = tmp;
  }
  send_to_char(ch, "Driven by: ^c%s^n. Rigged by: ^c%s^n. Controlled by: ^c%s^n\r\n",
               driver ? GET_CHAR_NAME(driver) : "nobody",
               rigger ? GET_CHAR_NAME(rigger) : "nobody",
               k->rigger ? GET_CHAR_NAME(k->rigger) : "nobody");
  
  if (GET_LEVEL(ch) == LVL_PRESIDENT && k->sub) {
    send_to_char(ch, "Sub rank: %d, next %s, prev %s\r\n", k->sub_rank, GET_VEH_NAME(k->next_sub), GET_VEH_NAME(k->prev_sub));
  }
}

void do_stat_object(struct char_data * ch, struct obj_data * j)
{
  FAILURE_CASE(!ch_can_bypass_edit_lock(ch, j), "Sorry, you can't stat that due to a zone edit lock.");

  long virt;
  int i, found;
  struct obj_data *j2;
  struct extra_descr_data *desc;

  virt = GET_OBJ_VNUM(j);
  snprintf(buf, sizeof(buf), "Name: '^y%s^n', Aliases: %s\r\n",
          ((j->text.name) ? j->text.name : "<None>"),
          j->text.keywords);

  if (j->restring) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Restring: '%s'\r\n", j->restring);
  }

  if (j->source_info) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Source Book: '%s'\r\n", j->source_info);
  }

  if (GET_LEVEL(ch) == LVL_PRESIDENT) {
    if (j->pc_load_origin) {
      const char *pc_name = get_player_name(j->pc_load_idnum);
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Loaded with '^c%s^n' during PC load '^c%s^n' for ^c%s^n (^c%ld)^n @ ^c%ld^n\r\n",
             obj_load_reasons[(int) j->load_origin],
             pc_load_reasons[(int) j->pc_load_origin],
             pc_name,
             j->pc_load_idnum,
             j->load_time);
      delete [] pc_name;
    } else {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Loaded with '^c%s^n' @ epoch ^c%ld^n\r\n",
             obj_load_reasons[(int) j->load_origin],
             j->load_time);
    }
  }

  if (j->dropped_by_char || j->dropped_by_host || GET_OBJ_EXPIRATION_TIMESTAMP(j) > 0) {
    if (j->dropped_by_char || j->dropped_by_host) {
      const char *name = get_player_name(j->dropped_by_char);
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Dropped by '%s'(%ld)@'%s', expires in %ld seconds\r\n",
               access_level(ch, LVL_VICEPRES) ? name : "<redacted>",
               access_level(ch, LVL_VICEPRES) ? j->dropped_by_char : -1,
               access_level(ch, LVL_VICEPRES) ? j->dropped_by_host : "<redacted>",
               GET_OBJ_EXPIRATION_TIMESTAMP(j) - time(0));
      delete [] name;
    } else {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Dropped by an unknown force (NPC?), expires in %ld seconds\r\n",
               GET_OBJ_EXPIRATION_TIMESTAMP(j) - time(0));
    }
  }

  sprinttype(GET_OBJ_TYPE(j), item_types, buf1, sizeof(buf1));

  if (GET_OBJ_RNUM(j) >= 0)
  {
    if (GET_OBJ_TYPE(j) == ITEM_WEAPON)
      strlcpy(buf2, (obj_index[GET_OBJ_RNUM(j)].wfunc ? "^cExists^n" : "None"), sizeof(buf2));
    else
      strlcpy(buf2, (obj_index[GET_OBJ_RNUM(j)].func ? "^cExists^n" : "None"), sizeof(buf2));
  } else
    strlcpy(buf2, "None", sizeof(buf2));
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "VNum: [^g%8ld^n], RNum: [%5ld], IDnum: [^c%lu^n], Type: %s, SpecProc: %s\r\n",
           virt, GET_OBJ_RNUM(j), j->idnum, buf1, buf2);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "L-Des: %s\r\n",
          ((j->text.look_desc) ? j->text.look_desc : "None"));

  if (j->ex_description)
  {
    strlcat(buf, "Extra descs:^c", sizeof(buf));
    for (desc = j->ex_description; desc; desc = desc->next) {
      strlcat(buf, " ", sizeof(buf));
      strlcat(buf, desc->keyword, sizeof(buf));
    }
    strlcat(buf, "^n\r\n", sizeof(buf));
  }

  j->obj_flags.wear_flags.PrintBits(buf2, MAX_STRING_LENGTH,
                                    wear_bits, NUM_WEARS);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Can be worn on: %s\r\n", buf2);

  j->obj_flags.bitvector.PrintBits(buf2, MAX_STRING_LENGTH,
                                   affected_bits, AFF_MAX);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Set char bits : %s\r\n", buf2);

  GET_OBJ_EXTRA(j).PrintBits(buf2, MAX_STRING_LENGTH,
                             extra_bits, MAX_ITEM_EXTRA);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Extra flags   : %s\r\n", buf2);

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Weight: %.2f, Value: %d, Timer: %d, Availability: %d/%.2f Days, Attempt %d\r\n",
          GET_OBJ_WEIGHT(j), GET_OBJ_COST(j), GET_OBJ_TIMER(j), GET_OBJ_AVAILTN(j), GET_OBJ_AVAILDAY(j), GET_OBJ_ATTEMPT(j));

  strlcat(buf, "In room: ", sizeof(buf));
  if (!j->in_room)
    strlcat(buf, "Nowhere", sizeof(buf));
  else
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%ld (IR %ld)", j->in_room->number, real_room(j->in_room->number));
  strlcat(buf, ", In object: ", sizeof(buf));
  strlcat(buf, j->in_obj ? j->in_obj->text.name : "None", sizeof(buf));
  strlcat(buf, ", Carried by: ", sizeof(buf));
  if (j->carried_by)
    strlcat(buf, GET_CHAR_NAME(j->carried_by) ? GET_CHAR_NAME(j->carried_by): "BROKEN", sizeof(buf));
  else
    strlcat(buf, "Nobody", sizeof(buf));
  strlcat(buf, ", Worn by: ", sizeof(buf));
  if (j->worn_by)
    strlcat(buf, GET_CHAR_NAME(j->worn_by) ? GET_CHAR_NAME(j->worn_by): "BROKEN", sizeof(buf));
  else
    strlcat(buf, "Nobody", sizeof(buf));
  strlcat(buf, ", In vehicle: ", sizeof(buf));
  if (j->in_veh)
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%ld", j->in_veh->veh_number);
  else
    strlcat(buf, "None", sizeof(buf));

  strlcat(buf, "\r\n", sizeof(buf));

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Material: %s, Rating: %d, Condition: %d, Legality: %d%s-%s\r\n",
               material_names[(int)GET_OBJ_MATERIAL(j)], GET_OBJ_BARRIER(j),
               GET_OBJ_CONDITION(j), GET_LEGAL_NUM(j), GET_LEGAL_PERMIT(j) ? "P" : "", legality_codes[GET_LEGAL_CODE(j)][0]);

  switch (GET_OBJ_TYPE(j))
  {
  case ITEM_LIGHT:
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Color: [%d], Type: [%d], Hours: [%d]",
            GET_OBJ_VAL(j, 0), GET_OBJ_VAL(j, 1), GET_OBJ_VAL(j, 2));
    break;
  case ITEM_FIREWEAPON:
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Str Min: %d, Str+: %d, Power: %d, Wound: %s, Skill: %s, Type: %s\r\n",
            GET_OBJ_VAL(j, 6), GET_OBJ_VAL(j, 2), GET_OBJ_VAL(j, 0), wound_arr[GET_OBJ_VAL(j, 1)],
            skills[GET_OBJ_VAL(j, 4)].name, (GET_OBJ_VAL(j, 5) == 0 ? "Bow" : "Crossbow"));
    break;
  case ITEM_WEAPON:
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Power: %d, Wound: %s, Str+: %d, WeapType: %s (%d), Skill: %s\r\nMax Ammo: %d, Range: %d",
            GET_OBJ_VAL(j, 0), wound_arr[GET_OBJ_VAL(j, 1)], GET_OBJ_VAL(j, 2),
            GET_WEAPON_TYPE_NAME(GET_WEAPON_ATTACK_TYPE(j)), GET_WEAPON_ATTACK_TYPE(j),
            skills[GET_OBJ_VAL(j, 4)].name, GET_OBJ_VAL(j, 5), GET_OBJ_VAL(j, 6));
    if (GET_OBJ_VAL(j, 3) >= WEAP_HOLDOUT)
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ", Top: %d, Barrel: %d, Under: %d", GET_OBJ_VAL(j, 7), GET_OBJ_VAL(j, 8), GET_OBJ_VAL(j, 9));
    break;
  case ITEM_MISSILE:
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Missile type: %s", (GET_OBJ_VAL(j, 0) == 0 ? "Arrow" : "Bolt"));
    break;
  case ITEM_WORN:
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Ballistic: %d, Impact: %d", GET_OBJ_VAL(j, 5), GET_OBJ_VAL(j, 6));
    break;
  case ITEM_DOCWAGON:
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Contract: %s, Owner: %d", (GET_OBJ_VAL(j, 0) == 1 ? "basic" :
            (GET_OBJ_VAL(j, 0) == 2 ? "gold" : "platinum")), GET_OBJ_VAL(j, 1));
    break;
  case ITEM_CONTAINER:
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Max-contains: %d, Locktype: %d, Key: %d, Lock level: %d, Corpse: %s",
            GET_OBJ_VAL(j, 0), GET_OBJ_VAL(j, 1), GET_OBJ_VAL(j, 2),
            GET_OBJ_VAL(j, 3), GET_OBJ_VAL(j, 4) ? "Yes" : "No");
    break;
  case ITEM_DRINKCON:
  case ITEM_FOUNTAIN:
    sprinttype(GET_OBJ_VAL(j, 2), drinks, buf2, sizeof(buf2));
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Max-contains: %d, Contains: %d, Poisoned: %s, Liquid: %s",
            GET_OBJ_VAL(j, 0), GET_OBJ_VAL(j, 1),
            GET_OBJ_VAL(j, 3) ? "Yes" : "No", buf2);
    break;
  case ITEM_MONEY:
    if (!GET_OBJ_VAL(j, 1))
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Value: %d, Type: nuyen", GET_OBJ_VAL(j, 0));
    else
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Value: %d, Type: credstick, Security: %s", GET_OBJ_VAL(j, 0),
              (GET_OBJ_VAL(j, 2) == 1 ? "6-digit" : (GET_OBJ_VAL(j, 2) == 2 ?
                                                     "thumbprint" : "retina")));
    break;
  case ITEM_KEY:
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Keytype: %d", GET_OBJ_VAL(j, 0));
    break;
  case ITEM_FOOD:
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Makes full: %d, Poisoned: %d", GET_OBJ_VAL(j, 0), GET_OBJ_VAL(j, 3));
    break;
  case ITEM_QUIVER:
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Max-contains: %d, Type contains: %s", GET_OBJ_VAL(j, 0), (GET_OBJ_VAL(j, 1) == 0 ? "Arrow" :
            (GET_OBJ_VAL(j, 1) == 1 ? "Bolt" : (GET_OBJ_VAL(j, 1) == 2 ? "Shuriken" :
                                                (GET_OBJ_VAL(j, 1) == 3 ? "Throwing knife" : "Undefined")))));
    break;
  case ITEM_PATCH:
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Type: %s, Rating: %d", patch_names[GET_PATCH_TYPE(j)], GET_PATCH_RATING(j));
    break;
  case ITEM_CYBERDECK:
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "MPCP: %d, Hardening: %d, Active: %d, Storage: %d, Load: %d",
            GET_OBJ_VAL(j, 0), GET_OBJ_VAL(j, 1), GET_OBJ_VAL(j, 2),
            GET_OBJ_VAL(j, 3), GET_OBJ_VAL(j, 4));
    break;
  case ITEM_PROGRAM:
    if (GET_OBJ_VAL(j, 0) == SOFT_ATTACK)
      snprintf(buf2, sizeof(buf2), ", DamType: %s", GET_WOUND_NAME(GET_OBJ_VAL(j, 3)));
    else
      snprintf(buf2, sizeof(buf2), " ");
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Type: %s, Rating: %d, Size: %d%s",
            programs[GET_OBJ_VAL(j, 0)].name, GET_OBJ_VAL(j, 1), GET_OBJ_VAL(j, 2), buf2);
    break;
  case ITEM_BIOWARE:
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Rating: %d, Index: %d, Type: %s, Cultured: %s", GET_OBJ_VAL(j, 1), GET_OBJ_VAL(j, 4),
            bio_types[GET_OBJ_VAL(j, 0)], GET_OBJ_VAL(j, 2) || GET_OBJ_VAL(j, 0) >= BIO_CEREBRALBOOSTER ? "Yes" : "No");
    break;
  case ITEM_CYBERWARE:
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Rating: %d, Essence: %d, Type: %s, Grade: %s", GET_OBJ_VAL(j, 1),
            GET_OBJ_VAL(j, 4), cyber_types[GET_OBJ_VAL(j, 0)], cyber_grades[GET_OBJ_VAL(j, 2)]);
    break;
  case ITEM_WORKSHOP:
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Type: %s, For: %s",
             GET_WORKSHOP_GRADE(j) ? (GET_WORKSHOP_GRADE(j) == TYPE_FACILITY ? "Facility": "Workshop") : "Kit",
             workshops[GET_WORKSHOP_TYPE(j)]);
    break;
  case ITEM_FOCUS:
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Type: %s Force: %d", foci_type[GET_OBJ_VAL(j, 0)], GET_OBJ_VAL(j, 1));
    break;
  case ITEM_SPELL_FORMULA:
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Type: %s Force: %d Tradition: %s", spells[GET_OBJ_VAL(j, 1)].name, GET_OBJ_VAL(j, 0), GET_OBJ_VAL(j, 2) ? "Shamanic" : "Hermetic");
  default:
    break;
  }

  rnum_t proto_rnum = real_object(GET_OBJ_VNUM(j));
  strlcat(buf, "\r\nValues: ", sizeof(buf));
  for (int x = 0; x < NUM_OBJ_VALUES; x++) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " ^L%d^n[%s%d^n]",
             x,
             proto_rnum >= 0 && GET_OBJ_VAL(j, x) != GET_OBJ_VAL(&obj_proto[proto_rnum], x) ? "^C" : "^c",
             GET_OBJ_VAL(j, x));
  }

  /*
   * I deleted the "equipment status" code from here because it seemed
   * more or less useless and just takes up valuable screen space.
   */
  if (j->contains)
  {
    strlcat(buf, "\r\nContents:^g", sizeof(buf));
    for (found = 0, j2 = j->contains; j2; j2 = j2->next_content) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s %s", found++ ? "," : "", GET_OBJ_NAME(j2));
      if (strlen(buf) >= 62)
        if (j2->next_content)
          strlcat(buf, ",\r\n", sizeof(buf));
    }
  }
  if (j->cyberdeck_part_pointer) {
    if (GET_OBJ_TYPE(j) == ITEM_PART) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^n\r\nCyberdeck Part Pointer: ^c%s^n", GET_OBJ_NAME(j->cyberdeck_part_pointer));
    } else {
      mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Non-part %s (%ld) has a cyberdeck part pointer set!! Clearing it.", GET_OBJ_NAME(j), GET_OBJ_VNUM(j));
      clear_cyberdeck_part_pointer(j);
    }
  }
  strlcat(buf, "^n\r\n", sizeof(buf));
  found = 0;
  strlcat(buf, "Affections:", sizeof(buf));
  for (i = 0; i < MAX_OBJ_AFFECT; i++)
    if (j->affected[i].modifier)
    {
      if (GET_OBJ_TYPE(j) == ITEM_MOD)
        sprinttype(j->affected[i].location, veh_aff, buf2, sizeof(buf2));
      else
        sprinttype(j->affected[i].location, apply_types, buf2, sizeof(buf2));
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s %+d to %s", found++ ? "," : "",
              j->affected[i].modifier, buf2);
    }
  if (!found)
    strlcat(buf, " None", sizeof(buf));
  strlcat(buf, "\r\n", sizeof(buf));
  send_to_char(buf, ch);
}

void do_stat_character(struct char_data * ch, struct char_data * k)
{
  int i, i2, found = 0;
  struct obj_data *j;
  struct follow_type *fol;

  if (!access_level(ch, LVL_FIXER))
  {
    send_to_char("You're not quite erudite enough to do that!\r\n", ch);
    return;
  } else if (PLR_FLAGGED(k, PLR_NOSTAT) && !access_level(ch, LVL_DEVELOPER) &&
             GET_LEVEL(ch) <= GET_LEVEL(k))
  {
    send_to_char("That character is protected by the NoStat flag. You need to be a higher level than them to stat them.\r\n", ch);
    return;
  }

#ifndef IS_BUILDPORT
  mudlog_vfprintf(ch, LOG_WIZLOG, "Statted character %s (%ld)", GET_CHAR_NAME(k), GET_IDNUM(k));
#endif

  switch (GET_PRONOUNS(k))
  {
  case PRONOUNS_NEUTRAL:
    strlcpy(buf, "NEUTRAL-PRONOUNS", sizeof(buf));
    break;
  case PRONOUNS_MASCULINE:
    strlcpy(buf, "MALE", sizeof(buf));
    break;
  case PRONOUNS_FEMININE:
    strlcpy(buf, "FEMALE", sizeof(buf));
    break;
  default:
    strlcpy(buf, "ILLEGAL-PRONOUNS!!", sizeof(buf));
    break;
  }

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " PC '%s'  IDNum: [^c%5ld^n], In room [^c%5ld^n], WasIn [^c%5ld^n]\r\n",
          GET_CHAR_NAME(k), GET_IDNUM(k), k->in_room ? k->in_room->number : 0, k->was_in_room ? k->was_in_room->number : 0);

  if (GET_PGROUP_MEMBER_DATA(k)) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Rank ^c%d/%d^n member of group '^c%s^n' (^c%s^n), with privileges:\r\n  ^c%s^n\r\n",
            GET_PGROUP_MEMBER_DATA(k)->rank, MAX_PGROUP_RANK,
            GET_PGROUP(k)->get_name(), GET_PGROUP(k)->get_alias(),
            pgroup_print_privileges(GET_PGROUP_MEMBER_DATA(k)->privileges, FALSE));
  }

  if (GET_LEVEL(ch) == LVL_PRESIDENT) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Loaded by '^c%s^n' @ ^c%ld^n\r\n", pc_load_reasons[k->load_origin], k->load_time);
  }

  if (!IS_NPC(k)) {
    if (access_level(ch, LVL_PRESIDENT) && GET_CHAR_MULTIPLIER(k) != 100) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Multiplier: ^c%.2f^n\r\n", (float) GET_CHAR_MULTIPLIER(k) / 100);
    }
    if (access_level(ch, LVL_VICEPRES)) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Email: ^y%s^n", GET_EMAIL(k));
      if (ch->desc && ch->desc->pProtocol && ch->desc->pProtocol->pVariables[eMSDP_CLIENT_ID] && ch->desc->pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "; Current Client: ^c%s^n\r\n", ch->desc->pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString);
      }
    }
    if (access_level(ch, LVL_FIXER)) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "ShotsFired: ^c%d^n, ShotsTriggered: ^c%d^n\r\n", SHOTS_FIRED(k), SHOTS_TRIGGERED(k));
    }
  }

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Title: %s\r\n", (k->player.title ? k->player.title : "<None>"));

  switch (GET_TRADITION(k))
  {
  case TRAD_ADEPT:
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Tradition: Adept, Grade: ^c%d^n, AddPoint Used: %d/%d, points available %0.2f", GET_GRADE(k), k->points.extrapp, (int)(GET_TKE(k) / 50) + 1, ((float) GET_PP(k) / 100));
    if (BOOST(ch)[BOD][0] || BOOST(ch)[STR][0] || BOOST(ch)[QUI][0])
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "[Boosts: BOD ^c%d^n STR ^c%d^c QUI ^c%d^n]", BOOST(ch)[BOD][1], BOOST(ch)[STR][1], BOOST(ch)[QUI][1]);
    break;
  case TRAD_HERMETIC:
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Tradition: Hermetic, Aspect: %s, Grade: %d", aspect_names[GET_ASPECT(k)], GET_GRADE(k));
    break;
  case TRAD_SHAMANIC:
    sprinttype(GET_TOTEM(k), totem_types, buf2, sizeof(buf2));
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Tradition: Shamanic, Aspect: %s, Totem: %s, ", aspect_names[GET_ASPECT(k)], buf2);
    if (GET_TOTEM(k) == TOTEM_GATOR || GET_TOTEM(k) == TOTEM_SNAKE || GET_TOTEM(k) == TOTEM_WOLF)
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Spirit Bonus: %s, ", spirits[GET_TOTEMSPIRIT(k)].name);
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Grade: %d", GET_GRADE(k));
    break;
  case TRAD_MUNDANE:
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Tradition: Mundane");
    break;
  }

  strlcat(buf, ", Race: ", sizeof(buf));
  sprinttype(k->player.race, pc_race_types, buf2, sizeof(buf2));
  strlcat(buf, buf2, sizeof(buf));

  if (IS_SENATOR(k))
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ", Status: %s\r\n^WSysP^n: [^y%3d^n]\r\n",
             status_ratings[(int)GET_LEVEL(k)],
             GET_SYSTEM_POINTS(k));
  else
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ", Status: Mortal \r\nRep: [^y%3d^n] Not: [^y%3d^n] TKE: [^y%3d^n] ^WSysP^n: [^y%3d^n]\r\n",
             GET_REP(k),
             GET_NOT(k),
             GET_TKE(k),
             GET_SYSTEM_POINTS(k));

  strlcpy(buf1, (const char *) asctime(localtime(&(k->player.time.birth))), sizeof(buf1));
  strlcpy(buf2, (const char *) asctime(localtime(&(k->player.time.lastdisc))), sizeof(buf2));
  buf1[10] = buf2[10] = '\0';

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Created: [%s], Last Online: [%s], Played [%dh %dm]\r\n",
          buf1, buf2, k->player.time.played / 3600,
          ((k->player.time.played / 3600) % 60));

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Last room: [^y%ld^n], Start room: [^y%ld^n], Speaking: [%s]\r\n",
          k->player.last_room, GET_LOADROOM(k), skills[GET_LANGUAGE(k)].name);

  if (GET_QUEST(k))
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Quest: [^G%ld^n]\r\n", quest_table[GET_QUEST(k)].vnum);

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Bod: [^c%d^n]  Qui: [^c%d^n]  Str: [^c%d^n]  Cha: [^c%d^n] Int: [^c%d^n]"
          "  Wil: [^c%d^n]  Mag: [^c%d^n]\r\nRea: [^c%d^n]  Ess: [^c%0.2f^n] Ast[^c%d^n]  Com[^c%d^n]  Mag[%d: ^c%d/%d/%d/%d^n]  Hak[^c%d^n] Dod/Bod/Off[^c%d/%d/%d^n]\r\n",
          GET_BOD(k), GET_QUI(k), GET_STR(k), GET_CHA(k), GET_INT(k),
          GET_WIL(k), ((int)GET_MAG(k) / 100), GET_REA(k), ((float)GET_ESS(k) / 100), GET_ASTRAL(k),
          GET_COMBAT_POOL(k),
          GET_MAGIC_POOL(k), GET_CASTING(k), GET_DRAIN(k), GET_SDEFENSE(k), GET_REFLECT(k),
          GET_HACKING(k), GET_DODGE(k), GET_BODY_POOL(k), GET_OFFENSE(k));

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Bod: [^B%d^n]  Qui: [^B%d^n]  Str: [^B%d^n]  Cha: [^B%d^n] Int: [^B%d^n]"
          "  Wil: [^B%d^n]  Mag: [^B%d^n]\r\nRea: [^B%d^n]  Ess: [^B%0.2f^n] (Unmodified attributes)\r\n",
          GET_REAL_BOD(k), GET_REAL_QUI(k), GET_REAL_STR(k), GET_REAL_CHA(k), GET_REAL_INT(k),
          GET_REAL_WIL(k), ((int)GET_REAL_MAG(k) / 100), GET_REAL_REA(k), ((float)GET_REAL_ESS(k) / 100));

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Physical p.:[^G%d/%d^n]  Mental p.:[^G%d/%d^n]  Bio Index:[^c%0.2f^n]\r\n"
          "Ess Index:[^c%0.2f^n]\r\n",
          (int)(GET_PHYSICAL(k) / 100), (int)(GET_MAX_PHYSICAL(k) / 100),
          (int)(GET_MENTAL(k) / 100), (int)(GET_MAX_MENTAL(k) / 100),
          ((float)GET_INDEX(k) / 100), (((float)GET_ESS(k) / 100) + 3));

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Nuyen: [^c%9ld^n], Bank: [^c%9ld^n] (Total: ^C%ld^n), Karma: [^c%0.2f^n]\r\n",
          GET_NUYEN(k), GET_BANK(k), GET_NUYEN(k) + GET_BANK(k),
          ((float)GET_KARMA(k) / 100));

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Armor: ^W%d (%d) / %d (%d)^n, I-Dice: ^W%d^n, I-Roll: ^W%d^n, Sus: ^W%d^n, Foci: ^W%d^n, TargMod: ^W%d^n, Reach: ^W%d^n\r\n",
          GET_BALLISTIC(k), GET_TOTALBAL(k), GET_IMPACT(k), GET_TOTALIMP(k), GET_INIT_DICE(k), GET_INIT_ROLL(k),
          GET_SUSTAINED_NUM(k), GET_FOCI(k), GET_TARGET_MOD(k), GET_REACH(k));
  strlcat(buf, write_vision_string_for_display(k, VISION_STRING_MODE_STAFF), sizeof(buf));
  sprinttype(GET_POS(k), position_types, buf2, sizeof(buf2));
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Current Zone: %d, Pos: %s, Fighting: %s",
          k->player_specials->saved.zonenum, buf2,
          (FIGHTING(k) ? GET_CHAR_NAME(FIGHTING(k)) : (FIGHTING_VEH(k) ? GET_VEH_NAME(FIGHTING_VEH(k)) : "Nobody")));

  if (k->desc)
  {
    sprinttype(k->desc->connected, connected_types, buf2, sizeof(buf2));
    strlcat(buf, ", Connected: ", sizeof(buf));
    strlcat(buf, buf2, sizeof(buf));
  }
  strlcat(buf, "\r\n", sizeof(buf));

  strlcat(buf, "Default position: ", sizeof(buf));
  sprinttype(GET_DEFAULT_POS(k), position_types, buf2, sizeof(buf2));
  strlcat(buf, buf2, sizeof(buf));

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ", Idle Timer: [%d], Emote Timer: [%d]\r\n", k->char_specials.timer, k->char_specials.last_social_action);

  if (PLR_FLAGGED(k, PLR_SITE_HIDDEN) && !access_level(ch, LVL_PRESIDENT)) {
    PLR_FLAGS(k).RemoveBit(PLR_SITE_HIDDEN);
    PLR_FLAGS(k).PrintBits(buf2, MAX_STRING_LENGTH, player_bits, PLR_MAX);
    PLR_FLAGS(k).SetBit(PLR_SITE_HIDDEN);
  } else {
    PLR_FLAGS(k).PrintBits(buf2, MAX_STRING_LENGTH, player_bits, PLR_MAX);
  }
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "PLR: ^c%s^n\r\n", buf2);

  PRF_FLAGS(k).PrintBits(buf2, MAX_STRING_LENGTH, preference_bits, PRF_MAX);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "PRF: ^g%s^n\r\n", buf2);

  AFF_FLAGS(k).PrintBits(buf2, MAX_STRING_LENGTH, affected_bits, AFF_MAX);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "AFF: ^y%s^n\r\n", buf2);

  if (GET_BUILDING(k)) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Currently working on: ^c%s^n (%ld)\r\n", GET_OBJ_NAME(GET_BUILDING(k)), GET_OBJ_VNUM(GET_BUILDING(k)));
  }

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Height: %d cm, Weight: %d kg\r\n", GET_HEIGHT(k), GET_WEIGHT(k));

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Carried: weight: %.2f (Max: %d), items: %d; ",IS_CARRYING_W(k), CAN_CARRY_W(k),IS_CARRYING_N(k));
  for (i = 0, j = k->carrying; j; j = j->next_content, i++)
    ;
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Items in: inventory: %d, ", i);

  for (i = 0, i2 = 0; i < NUM_WEARS; i++)
    if (k->equipment[i])
      i2++;
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "eq: %d\r\n", i2);

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Hunger: %d, Thirst: %d, Drunk: %d,  Socialization Bonus: ^c%d^n\r\n",
          GET_COND(k, COND_FULL), GET_COND(k, COND_THIRST), GET_COND(k, COND_DRUNK), GET_CONGREGATION_BONUS(k));

  if (k->char_specials.rigging) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Rigging: %s^n (%ld) @ %ld\r\n",
             CAP(GET_VEH_NAME(k->char_specials.rigging)),
             GET_VEH_VNUM(k->char_specials.rigging),
             (k->char_specials.rigging->in_veh || k->char_specials.rigging->in_room) ? GET_ROOM_VNUM(get_veh_in_room(k->char_specials.rigging)) : -1);
  }

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Master is: %s, Followers are:",
          ((k->master) ? GET_CHAR_NAME(k->master) : "<none>"));

  for (fol = k->followers; fol; fol = fol->next)
  {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s %s", found++ ? "," : "", GET_CHAR_NAME(fol->follower));
    if (strlen(buf) >= 62) {
      if (fol->next)
        strlcat(buf, ",\r\n", sizeof(buf));
    }
  }
  strlcat(buf, "\r\n", sizeof(buf));
  send_to_char(buf, ch);
}

void do_stat_mobile(struct char_data * ch, struct char_data * k)
{
  FAILURE_CASE(!ch_can_bypass_edit_lock(ch, k), "Sorry, you can't stat that due to a zone edit lock.");

  int i, i2, found = 0, base;
  struct obj_data *j;
  struct follow_type *fol;
  extern struct attack_hit_type attack_hit_text[];
  extern int calc_karma(struct char_data *ch, struct char_data *vict);

  switch (GET_PRONOUNS(k))
  {
  case PRONOUNS_NEUTRAL:
    strlcpy(buf, "NEUTRAL-PRONOUNS", sizeof(buf));
    break;
  case PRONOUNS_MASCULINE:
    strlcpy(buf, "MALE", sizeof(buf));
    break;
  case PRONOUNS_FEMININE:
    strlcpy(buf, "FEMALE", sizeof(buf));
    break;
  default:
    strlcpy(buf, "ILLEGAL-PRONOUNS!!", sizeof(buf));
    break;
  }
  // Initial info (race, mob type, name)
  send_to_char(ch, "^c%s^n %s '%s' (Lv %d)", 
               pc_race_types[(int)GET_RACE(k)], 
               !IS_MOB(k) ? "NPC" : "MOB", 
               GET_NAME(k),
               GET_LEVEL(k));

  // Location
  if (k->in_room)
    snprintf(buf2, sizeof(buf2), ", In room [%8ld]", k->in_room->number);
  else if (k->in_veh)
    snprintf(buf2, sizeof(buf2), ", In veh [%s]", GET_VEH_NAME(k->in_veh));

  if (GET_MOB_FACTION_IDNUM(k)) {
    snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), ". Faction: %s (%ld)", get_faction_name(GET_MOB_FACTION_IDNUM(k), ch), GET_MOB_FACTION_IDNUM(k));
  }

  if (GET_LEVEL(ch) == LVL_PRESIDENT) {
    snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), ".  Loaded by '^c%s^n' @ ^c%ld^n", pc_load_reasons[k->load_origin], k->load_time);
  }

  // Append to existing buf with newline.
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s\r\n", buf2);

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Alias: %s, VNum: [^c%8ld^n], RNum: [%5ld], Unique ID [%s]\r\n", GET_KEYWORDS(k),
          GET_MOB_VNUM(k), GET_MOB_RNUM(k), get_printable_mob_unique_id(k));

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "L-Des: %s",
          (k->player.physical_text.look_desc ?
           k->player.physical_text.look_desc : "<None>\r\n"));

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Bod: [^c%d^n]  Qui: [^c%d^n]  Str: [^c%d^n]  Cha: [^c%d^n] Int: [^c%d^n]"
          "  Wil: [^c%d^n]  Mag: [^c%d^n]\r\nRea: [^c%d^n]  Ess: [^c%0.2f^n] Ast[^c%d^n]  Com[^c%d^n]  Mag[^c%d: %d/%d/%d/%d^n]  Hak[^c%d^n] Dod/Bod/Off[^c%d/%d/%d^n]\r\n",
          GET_BOD(k), GET_QUI(k), GET_STR(k), GET_CHA(k), GET_INT(k),
          GET_WIL(k), ((int)GET_MAG(k) / 100), GET_REA(k), ((float)GET_ESS(k) / 100), GET_ASTRAL(k),
          GET_COMBAT_POOL(k), 
          GET_MAGIC_POOL(k), GET_CASTING(k), GET_DRAIN(k), GET_SDEFENSE(k), GET_REFLECT(k),
          GET_HACKING(k), GET_DODGE(k), GET_BODY_POOL(k), GET_OFFENSE(k));

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Bod: [^B%d^n]  Qui: [^B%d^n]  Str: [^B%d^n]  Cha: [^B%d^n] Int: [^B%d^n]"
          "  Wil: [^B%d^n]  Mag: [^B%d^n]\r\nRea: [^B%d^n]  Ess: [^B%0.2f^n] (Unmodified attributes)\r\n",
          GET_REAL_BOD(k), GET_REAL_QUI(k), GET_REAL_STR(k), GET_REAL_CHA(k), GET_REAL_INT(k),
          GET_REAL_WIL(k), ((int)GET_REAL_MAG(k) / 100), GET_REAL_REA(k), ((float)GET_REAL_ESS(k) / 100));

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Physical p.:[^G%d/%d^n]  Mental p.:[^G%d/%d^n]; Ballistic / Impact [^y%d/%d^n]\r\n",
          (int)(GET_PHYSICAL(k) / 100), (int)(GET_MAX_PHYSICAL(k) / 100),
          (int)(GET_MENTAL(k) / 100), (int)(GET_MAX_MENTAL(k) / 100),
          GET_BALLISTIC(k),
          GET_IMPACT(k)
        );

  base = calc_karma(NULL, k);

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Nuyen: [%ld], Credstick: [%ld], Bonus karma: [%4d], Total karma: [%4d]\r\n",
          GET_NUYEN(k), GET_BANK(k), GET_KARMA(k), base);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "B: %d, I: %d, I-Dice: %d, I-Roll: %d, Sus: %d, TargMod: %d, Reach: %d\r\n",
          GET_BALLISTIC(k), GET_IMPACT(k), GET_INIT_DICE(k), GET_INIT_ROLL(k),
          GET_SUSTAINED_NUM(k), GET_TARGET_MOD(k), GET_REACH(k));

  sprinttype(GET_POS(k), position_types, buf2, sizeof(buf2));
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Position: %s, Fighting: %s", buf2,
          (FIGHTING(k) ? GET_NAME(FIGHTING(k)) : (FIGHTING_VEH(k) ? GET_VEH_NAME(FIGHTING_VEH(k)) : "Nobody")));

  strlcat(buf, ", Attack type: ", sizeof(buf));
  // gotta subtract TYPE_HIT for the array to work properly
  strlcat(buf, attack_hit_text[k->mob_specials.attack_type - TYPE_HIT].singular, sizeof(buf));
  strlcat(buf, "\r\n", sizeof(buf));


  strlcat(buf, "Default position: ", sizeof(buf));
  sprinttype(GET_DEFAULT_POS(k), position_types, buf2, sizeof(buf2));
  strlcat(buf, buf2, sizeof(buf));
  strlcat(buf, "     Mob Spec-Proc: ", sizeof(buf));
  if (mob_index[GET_MOB_RNUM(k)].func || mob_index[GET_MOB_RNUM(k)].sfunc)
  {
    int index;
    for (index = 0; index <= top_of_shopt; index++)
      if (GET_MOB_VNUM(k) == shop_table[index].keeper)
        break;
    if (index <= top_of_shopt)
      snprintf(buf2, sizeof(buf2), "%s : Shop %ld\r\n", "^CExists^n", shop_table[index].vnum);
    else
      snprintf(buf2, sizeof(buf2), "%s\r\n", "^CExists^n");
  } else
    snprintf(buf2, sizeof(buf2), "None\r\n");
  strlcat(buf, buf2, sizeof(buf));

  MOB_FLAGS(k).PrintBits(buf2, MAX_STRING_LENGTH, action_bits, MOB_MAX);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "NPC flags: ^c%s^n\r\n", buf2);

  if (mob_is_alert(k)) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Alarmed with ^c%ld^n entries: ", GET_MOB_ALARM_MAP(k).size());
    for (auto itr : GET_MOB_ALARM_MAP(k)) {
      if (itr.first == IDNUM_FOR_MOB_ALERT_STATE) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "<^calert^n %lds>; ", itr.second - time(0));
      } else {
        const char *ch_name = get_player_name(itr.first);
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^c%s^n %lds; ", ch_name, itr.second - time(0));
        delete [] ch_name;
      }
    }
    strlcat(buf, "\r\n", sizeof(buf));
  }

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Height: %d cm, Weight: %d kg\r\n", GET_HEIGHT(k), GET_WEIGHT(k));

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Carried: weight: %.2f, items: %d; ",
          IS_CARRYING_W(k), IS_CARRYING_N(k));

  for (i = 0, j = k->carrying; j; j = j->next_content, i++)
    ;
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Items in: inventory: %d, ", i);

  for (i = 0, i2 = 0; i < NUM_WEARS; i++)
    if (k->equipment[i])
      i2++;
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "eq: %d\r\n", i2);

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Master is: %s, Followers are:",
          ((k->master) ? GET_NAME(k->master) : "<none>"));

  for (fol = k->followers; fol; fol = fol->next)
  {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s %s", found++ ? "," : "", PERS(fol->follower, ch));
    if (strlen(buf) >= 62) {
      if (fol->next)
        strlcat(buf, ",\r\n", sizeof(buf));
    }
  }
  strlcat(buf, "\r\n", sizeof(buf));

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Spare1: %ld, spare2: %ld\r\n", GET_SPARE1(k), GET_SPARE2(k));

  /* Showing the bitvector */
  AFF_FLAGS(k).PrintBits(buf2, MAX_STRING_LENGTH, affected_bits, AFF_MAX);
  send_to_char(ch, "%sAFF: ^y%s\r\n", buf, buf2);
  if (k->mob_loaded_in_room) {
    send_to_char(ch, "Loaded in room: ^c%ld^n\r\n", k->mob_loaded_in_room);
  }
  strlcpy(buf2, write_vision_string_for_display(k, VISION_STRING_MODE_STAFF), sizeof(buf2));
  send_to_char(ch, "%s\r\n", buf2);

  /* Equipment */
  if (k->cyberware) {
    send_to_char(ch, "Cyberware:\r\n");
    for (struct obj_data *ware = k->cyberware; ware; ware = ware->next_content)
      send_to_char(ch, " - %s (^c%ld^n)\r\n", GET_OBJ_NAME(ware), GET_OBJ_VNUM(ware));
  }

  if (k->bioware) {
    send_to_char(ch, "Bioware:\r\n");
    for (struct obj_data *ware = k->bioware; ware; ware = ware->next_content)
      send_to_char(ch, " - %s (^c%ld^n)\r\n", GET_OBJ_NAME(ware), GET_OBJ_VNUM(ware));
  }

  {
    struct obj_data *worn_item;
    bool sent_eq_string = FALSE;

    for (int wearloc = 0; wearloc < NUM_WEARS; wearloc++) {
      if ((worn_item = GET_EQ(k, wearloc))) {
        if (!sent_eq_string) {
          send_to_char(ch, "Equipment:\r\n");
          sent_eq_string = TRUE;
        }
        send_to_char(ch, " - %s %s (^c%ld^n) (%d nuyen)\r\n", where[wearloc], GET_OBJ_NAME(worn_item), GET_OBJ_VNUM(worn_item), GET_OBJ_COST(worn_item));
      }
    }
  }

  {
    char ammo_buf[10000] = { '\0' };
    for (int weapon_idx = START_OF_AMMO_USING_WEAPONS; weapon_idx <= END_OF_AMMO_USING_WEAPONS; weapon_idx++) {
      for (int ammo_idx = 0; ammo_idx < NUM_AMMOTYPES; ammo_idx++) {
        int ammo_qty = GET_BULLETPANTS_AMMO_AMOUNT(k, weapon_idx, ammo_idx);

        if (ammo_qty) {
          snprintf(ENDOF(ammo_buf), sizeof(ammo_buf) - strlen(ammo_buf), "  ^c%d^n %s\r\n", ammo_qty, get_ammo_representation(weapon_idx, ammo_idx, ammo_qty, k));
        }
      }
    }
    if (*ammo_buf) {
      send_to_char(ch, "Ammo:\r\n%s", ammo_buf);
    }
  }

  {
    char skill_buf[1000] = { '\0' };
    for (int skill_idx = 0; skill_idx <= 8; skill_idx += 2) {
      if (k->mob_specials.mob_skills[skill_idx + 1] && (k->mob_specials.mob_skills[skill_idx] > 0 && k->mob_specials.mob_skills[skill_idx] < MAX_SKILLS)) {
        snprintf(ENDOF(skill_buf), sizeof(skill_buf) - strlen(skill_buf), "  %s: %d\r\n",
                 skills[k->mob_specials.mob_skills[skill_idx]].name,
                 k->mob_specials.mob_skills[skill_idx + 1]);
      }
    }
    if (*skill_buf) {
      send_to_char(ch, "Skills:\r\n%s", skill_buf);
    }
  }

  if (k->precast_spells) {
    send_to_char(ch, "Precast Spells:\r\n");
    list_mob_precast_spells_to_ch(k, ch);
  }
}

ACMD(do_stat)
{
  struct char_data *victim = NULL;
  struct obj_data *object = NULL;
  struct veh_data *veh = NULL;
  int tmp;

  half_chop(argument, buf1, buf2, sizeof(buf2));

  if (!*buf1) {
    send_to_char("Stats on who or what?\r\n", ch);
    return;
  } else if (is_abbrev(buf1, "room")) {
    do_stat_room(ch);
  } else if (is_abbrev(buf1, "file")) {
    if (!*buf2)
      send_to_char("Stats on which player?\r\n", ch);
    else {
      idnum_t idnum;
      if ((idnum = atoi(buf2)) > 0) {
        const char *char_name = get_player_name(idnum);
        if (char_name) {
          strlcpy(buf2, char_name, sizeof(buf2));
          delete [] char_name;
        } else {
          send_to_char(ch, "Idnum %ld does not correspond to any existing PC.\r\n", idnum);
          return;
        }
      }
      if (!(does_player_exist(buf2)))
        send_to_char(ch, "Couldn't find any PC named '%s'.\r\n", buf2);
      else {
        victim = playerDB.LoadChar(buf2, TRUE, PC_LOAD_REASON_OFFLINE_WIZSTAT);
        do_stat_character(ch, victim);
        extract_char(victim);
      }
    }
  } else {
    if ((object = get_object_in_equip_vis(ch, buf1, ch->equipment, &tmp))) {
      do_stat_object(ch, object);
    } else if ((object = get_obj_in_list_vis(ch, buf1, ch->carrying))) {
      do_stat_object(ch, object);
    } else if ((veh = get_veh_list(buf1, ch->in_veh ? ch->in_veh->carriedvehs : ch->in_room->vehicles, ch))) {
      do_stat_veh(ch, veh);
    } else if ((victim = get_char_room_vis(ch, buf1))) {
      if (IS_NPC(victim))
        do_stat_mobile(ch, victim);
      else
        do_stat_character(ch, victim);
    } else if ((object = get_obj_in_list_vis(ch, buf1, ch->in_veh ? ch->in_veh->contents : ch->in_room->contents))) {
      do_stat_object(ch, object);
    } else if ((victim = get_char_vis(ch, buf1))) {
      if (IS_NPC(victim))
        do_stat_mobile(ch, victim);
      else
        do_stat_character(ch, victim);
    } else if ((object = get_obj_vis(ch, buf1))) {
      do_stat_object(ch, object);
    } else {
      send_to_char("Nothing around by that name.\r\n", ch);
    }
  }
}

ACMD(do_shutdown)
{
  ACMD_CONST(do_not_here);

  if (IS_NPC(ch) || !access_level(ch, LVL_PRESIDENT)) {
    do_not_here(ch, "", 0, 0);
    return;
  }

  if (subcmd != SCMD_SHUTDOWN) {
    send_to_char("If you want to shut something down, say so!\r\n", ch);
    return;
  }
  one_argument(argument, arg);

  if (!*arg) {
    log_vfprintf("Shutdown by %s.", GET_CHAR_NAME(ch));
    send_to_all("Shutting down.\r\n");
    shutdown(SUCCESS);
  } else if (!str_cmp(arg, "reboot")) {
    log_vfprintf("Reboot by %s.", GET_CHAR_NAME(ch));
    send_to_all("Rebooting.. come back in a minute or two.\r\n");
    shutdown(SUCCESS);
  } else if (!str_cmp(arg, "die")) {
    log_vfprintf("Shutdown by %s.", GET_CHAR_NAME(ch));
    send_to_all("Shutting down for maintenance.\r\n");
    shutdown(SUCCESS);
  } else if (!str_cmp(arg, "pause")) {
    log_vfprintf("Shutdown by %s.", GET_CHAR_NAME(ch));
    send_to_all("Shutting down for maintenance.\r\n");
    shutdown(SUCCESS);
  } else
    send_to_char("Unknown shutdown option.\r\n", ch);

  save_all_apartments_and_storage_rooms();
}

void stop_snooping(struct char_data * ch)
{
  if (!ch->desc->snooping)
    send_to_char("You aren't snooping anyone.\r\n", ch);
  else {
    send_to_char("You stop snooping.\r\n", ch);

    mudlog_vfprintf(ch, LOG_WIZLOG, "%s stops snooping %s.",
                    GET_CHAR_NAME(ch),
                    GET_CHAR_NAME(ch->desc->snooping->original ? ch->desc->snooping->original : ch->desc->snooping->character));

    if (GET_LEVEL(ch) < LVL_REQUIRED_FOR_SILENT_SNOOP) {
      send_to_char(ch->desc->snooping->character, "^WStaff member %s is no longer watching your connection for debugging information.^n Thank you for your help!\r\n", GET_CHAR_NAME(ch));
    }

    ch->desc->snooping->snoop_by = NULL;
    ch->desc->snooping = NULL;
  }
}

ACMD(do_snoop)
{
  struct char_data *victim;

  if (!ch->desc)
    return;

  one_argument(argument, arg);

  if (!*arg) {
    stop_snooping(ch);
    return;
  }

  if (GET_LEVEL(ch) < LVL_REQUIRED_FOR_SILENT_SNOOP) {
    FAILURE_CASE_PRINTF(!(victim = get_char_room_vis(ch, arg)), "You don't see anyone named '%s' here.", arg);
  } else {
    FAILURE_CASE_PRINTF(!(victim = get_char_vis(ch, arg)), "You don't see anyone named '%s' in the game.", arg);
  }

  FAILURE_CASE_PRINTF(!victim->desc, "%s has no link.. nothing to snoop.", GET_CHAR_NAME(victim));
  
  if (victim == ch) {
    stop_snooping(ch);
  }

  if (victim->desc->snoop_by) {
    FAILURE_CASE_PRINTF(victim->desc->snoop_by->character && GET_LEVEL(victim->desc->snoop_by->character) > GET_LEVEL(ch),
                        "You don't see anyone named '%s' %s.", arg, GET_LEVEL(ch) < LVL_REQUIRED_FOR_SILENT_SNOOP ? "here" : "in the game");
    FAILURE_CASE_PRINTF(victim->desc->snoop_by, "%s is already being snooped.", GET_CHAR_NAME(victim));
  }
  
  FAILURE_CASE(victim->desc->snooping == ch->desc, "That would cause a loop.");
  FAILURE_CASE(!IS_NPC(victim) && PLR_FLAGS(victim).IsSet(PLR_NOSNOOP), "You can't snoop an unsnoopable person.");
  FAILURE_CASE(GET_LEVEL(victim->desc->original ? victim->desc->original : victim) >= GET_LEVEL(ch), "You can't.");

  // Clear old snooping data.
  if (ch->desc->snooping)
    ch->desc->snooping->snoop_by = NULL;

  // Set new snooping data.
  ch->desc->snooping = victim->desc;
  victim->desc->snoop_by = ch->desc;

  send_to_char(OK, ch);

  if (GET_LEVEL(ch) < LVL_REQUIRED_FOR_SILENT_SNOOP) {
    send_to_char(victim, "^WStaff member %s is now watching your connection for debugging information.^n This is an audited event.\r\n", GET_CHAR_NAME(ch));
    mudlog_vfprintf(ch, LOG_WIZLOG, "%s now being snooped by %s. Character has been notified.", GET_CHAR_NAME(victim),GET_CHAR_NAME(ch));
  } else {
    mudlog_vfprintf(ch, LOG_WIZLOG, "%s now being snooped by %s.", GET_CHAR_NAME(victim),GET_CHAR_NAME(ch));
  }
}

ACMD(do_wizpossess)
{
  struct char_data *victim;

  one_argument(argument, arg);

  if (ch->desc->original)
    send_to_char("You're already switched.\r\n", ch);
  else if (!*arg)
    send_to_char("Switch with who?\r\n", ch);
  else if (!(victim = get_char_vis(ch, arg)))
    send_to_char("No such character.\r\n", ch);
  else if (ch == victim)
    send_to_char("Hee hee... we are jolly funny today, eh?\r\n", ch);
  else if (victim->desc)
    send_to_char("You can't do that, the body is already in use!\r\n", ch);
  else if ((!access_level(ch, LVL_DEVELOPER)) && !IS_NPC(victim))
    send_to_char("You aren't holy enough to use a mortal's body.\r\n", ch);
  else {
    send_to_char(OK, ch);

    PLR_FLAGS(ch).SetBit(PLR_SWITCHED);

    ch->desc->character = victim;
    ch->desc->original = ch;

    snprintf(buf, sizeof(buf),"%s assumes the role of %s.",
            GET_CHAR_NAME(ch),GET_NAME(victim));
    mudlog(buf, ch, LOG_WIZLOG, TRUE);

    victim->desc = ch->desc;
    ch->desc = NULL;
  }
}

ACMD_CONST(do_return) {
  ACMD_DECLARE(do_return);
  do_return(ch, NULL, cmd, subcmd);
}

ACMD(do_return)
{
  extern void stop_chase(struct veh_data *veh);
  struct char_data *vict;

  if (PLR_FLAGGED(ch, PLR_REMOTE)) {
    stop_rigging(ch);
    return;
  }

  if (ch->desc) {
    if (ch->desc->original) {
      send_to_char("You return to your original body.\r\n", ch);
      if (PLR_FLAGGED(ch->desc->original, PLR_PROJECT)) {
        GET_TEMP_ESSLOSS(ch->desc->original) = GET_ESS(ch->desc->original) - GET_ESS(ch);
        affect_total(ch->desc->original);
        PLR_FLAGS(ch->desc->original).RemoveBit(PLR_PROJECT);

        // Make the projection unkillable.
        if (IS_NPC(ch))
          MOB_FLAGS(ch).SetBit(MOB_NOKILL);
      }
      if (PLR_FLAGGED(ch->desc->original, PLR_SWITCHED))
        PLR_FLAGS(ch->desc->original).RemoveBit(PLR_SWITCHED);

      /* JE 2/22/95 */
      /* if someone switched into your original body, disconnect them */
      if (ch->desc->original->desc)
        close_socket(ch->desc->original->desc);

      vict = ch;
      ch->desc->character = ch->desc->original;
      ch->desc->original = NULL;

      /* Needs to be changed when the level needed for switch goes up or down */
      if ( GET_REAL_LEVEL(ch) >= LVL_BUILDER ) {
        snprintf(buf, sizeof(buf),"%s discontinues the role of %s.",
                GET_CHAR_NAME(ch->desc->character),GET_NAME(vict));
        mudlog(buf, ch, LOG_WIZLOG, TRUE);
      }

      ch->desc->character->desc = ch->desc;
      update_pos(ch->desc->character);
      ch->desc = NULL;

      if (IS_NPC(vict) && GET_MOB_VNUM(vict) >= 50 && GET_MOB_VNUM(vict) < 70 &&
          PLR_FLAGGED(ch, PLR_PROJECT)) {
        GET_PLAYER_MEMORY(vict) = NULL;
        extract_char(vict);
        char_from_room(ch);
        char_to_room(ch, GET_WAS_IN(ch));
        GET_WAS_IN(ch) = NULL;
      }
    } else if (AFF_FLAGGED(ch, AFF_RIG)) {
      ACMD_DECLARE(do_rig);
      do_rig(ch, NULL, 0, 0);
    } else {
      send_to_char("But there's nothing for you to return from...\r\n", ch);
    }
  }
}

void perform_wizload_object(struct char_data *ch, int vnum) {
  int real_num, counter, i;
  bool found = FALSE;
  struct obj_data *obj = NULL;

  assert(ch != NULL);

  // Precondition: Number cannot be negative.
  if (vnum < 0) {
    send_to_char("You must specify a positive number.\r\n", ch);
    return;
  }

  // Precondition: Number must be a vnum for a real object.
  if ((real_num = real_object(vnum)) < 0) {
    send_to_char("There is no object with that number.\r\n", ch);
    return;
  }

  /* If you want to disable the ability for your imms to load credsticks, uncomment this block.

  // Precondition: Object cannot be a credstick.
  if (obj_proto[real_num].obj_flags.type_flag == ITEM_MONEY) {
    send_to_char("You can't wizload credsticks.\r\n", ch);
    return;
  }

  */

  // Precondition: Object must belong to a zone.
  for (counter = 0; counter <= top_of_zone_table; counter++)
    if ((vnum >= (zone_table[counter].number * 100)) && (vnum <= (zone_table[counter].top))) {
      found = TRUE;
      break;
    }

  if (!found) {
    send_to_char ("Sorry, that number is not part of any zone!\r\n", ch);
    return;
  }

  // Precondition: Staff member must have access to the zone the item is in.
  if (!access_level(ch, LVL_DEVELOPER) || (zone_table[counter].locked_to_non_editors && !can_edit_zone(ch, &zone_table[counter]))) {
    for (i = 0; i < NUM_ZONE_EDITOR_IDS; i++) {
      if (zone_table[counter].editor_ids[i] == GET_IDNUM(ch))
        break;
    }

    if ((i >= 5)) {
      send_to_char("Sorry, you don't have access to edit this zone.\r\n", ch);
      return;
    }
  }

  obj = read_object(real_num, REAL, OBJ_LOAD_REASON_WIZLOAD);
  obj_to_char(obj, ch);
  GET_OBJ_TIMER(obj) = 2;
  obj->obj_flags.extra_flags.SetBit(ITEM_EXTRA_WIZLOAD);
  act("$n makes a strange magical gesture.", TRUE, ch, 0, 0, TO_ROOM);
  act("$n has created $p!", TRUE, ch, obj, 0, TO_ROOM);
  act("You create $p.", FALSE, ch, obj, 0, TO_CHAR);
  snprintf(buf, sizeof(buf), "%s wizloaded object #%d (%s).",
          GET_CHAR_NAME(ch), vnum, get_zone_from_vnum(GET_OBJ_VNUM(obj))->locked_to_non_editors ? "<redacted>" : GET_OBJ_NAME(obj));
  mudlog(buf, ch, LOG_CHEATLOG, TRUE);

  // Make staff-loaded hardened armor wearable.
  if (GET_OBJ_TYPE(obj) == ITEM_WORN && IS_OBJ_STAT(obj, ITEM_EXTRA_HARDENED_ARMOR)) {
    GET_WORN_HARDENED_ARMOR_CUSTOMIZED_FOR(obj) = -1;
  }

  // Flag staff-loaded tokens as issued by them.
  if (GET_OBJ_VNUM(obj) == OBJ_STAFF_REBATE_FOR_CRAFTING) {
    GET_CRAFTING_TOKEN_ISSUED_BY(obj) = GET_IDNUM_EVEN_IF_PROJECTING(ch);
  } else if (GET_OBJ_VNUM(obj) == OBJ_HOLIDAY_GIFT) {
    GET_HOLIDAY_GIFT_ISSUED_BY(obj) = GET_IDNUM_EVEN_IF_PROJECTING(ch);
  } else if (GET_OBJ_VNUM(obj) == OBJ_ONE_SHOT_HEALING_INJECTOR) {
    GET_HEALING_INJECTOR_ISSUED_BY(obj) = GET_IDNUM_EVEN_IF_PROJECTING(ch);
  }
}

ACMD(do_iload)
{
  int number;

  one_argument(argument, buf2);

  // Message sent in function.
  if (!is_olc_available(ch)) {
    return;
  }

  if (!*buf2 || !isdigit(*buf2)) {
    send_to_char("Usage: iload <number>\r\n", ch);
    return;
  }
  if ((number = atoi(buf2)) < 0) {
    send_to_char("A NEGATIVE number??\r\n", ch);
    return;
  }

  perform_wizload_object(ch, number);
}

ACMD(do_wizload)
{
  struct char_data *mob;
  struct veh_data *veh;

  int numb, r_num;

  two_arguments(argument, buf, buf2);

  if (!*buf || !*buf2 || !isdigit(*buf2)) {
    send_to_char("Usage: wizload { obj | mob | veh } <number>\r\n", ch);
    return;
  }
  if ((numb = atoi(buf2)) < 0) {
    send_to_char("A NEGATIVE number??\r\n", ch);
    return;
  }

  // Precondition: Staff member must have access to the zone the item is in.
  FAILURE_CASE(!ch_can_bypass_edit_lock(ch, get_zone_from_vnum(numb)), "Sorry, you don't have access to that zone.");

  if (is_abbrev(buf, "veh")) {
    if ((r_num = real_vehicle(numb)) < 0 ) {
      send_to_char("There is no vehicle with that number.\r\n", ch);
      return;
    }

    veh = read_vehicle(r_num, REAL);
    generate_veh_idnum(veh);
    veh_to_room(veh, get_ch_in_room(ch));
    act("$n makes a quaint, magical gesture with one hand.", TRUE, ch,
        0, 0, TO_ROOM);
    snprintf(buf, sizeof(buf), "%s wizloaded vehicle #%d (%s).",
            GET_CHAR_NAME(ch), numb, veh->short_description);
    mudlog(buf, ch, LOG_CHEATLOG, TRUE);

  } else if (is_abbrev(buf, "mob")) {
    if ((r_num = real_mobile(numb)) < 0) {
      send_to_char("There is no monster with that number.\r\n", ch);
      return;
    }

    mob = read_mobile(r_num, REAL);
    mob->mob_loaded_in_room = GET_ROOM_VNUM(get_ch_in_room(ch));
    char_to_room(mob, get_ch_in_room(ch));

    // Reset questgivers so they talk to you faster.
    if (CHECK_FUNC_AND_SFUNC_FOR(mob, johnson)) {
      GET_SPARE1(mob) = -1;
    }

    act("$n makes a quaint, magical gesture with one hand.", TRUE, ch,
        0, 0, TO_ROOM);
    act("$n has created $N!", TRUE, ch, 0, mob, TO_ROOM);
    act("You create $N.", FALSE, ch, 0, mob, TO_CHAR);
    snprintf(buf, sizeof(buf), "%s wizloaded mob #%d (%s).", GET_CHAR_NAME(ch), numb, get_zone_from_vnum(numb)->locked_to_non_editors ? "<redacted>" : GET_CHAR_NAME(mob));
    mudlog(buf, ch, LOG_CHEATLOG, TRUE);
  } else if (is_abbrev(buf, "obj")) {
    perform_wizload_object(ch, numb);
  } else
    send_to_char("That'll have to be either 'obj', 'mob', or 'veh'.\r\n", ch);
}

ACMD(do_vfind) {
  struct room_data *room;
  idnum_t idnum = 0;
  int idx = 1;

  skip_spaces(&argument);

  if (!str_cmp(argument, "all")) {
    send_to_char("OK, listing ALL player-owned vehicles.\r\n", ch);
    mudlog_vfprintf(ch, LOG_WIZLOG, "vfinding ALL player-owned vehicles");
  } else if ((idnum = get_player_id(argument)) <= 0) {
    send_to_char(ch, "Didn't find anyone named '%s'. Syntax: ^WVFIND <character name>^n, or ^WVFIND ALL^n\r\n", argument);
    return;
  } else {
    const char *plr_name = get_player_name(idnum);
    mudlog_vfprintf(ch, LOG_WIZLOG, "vfinding vehicles owned by %s (%ld)", plr_name, idnum);
    delete [] plr_name;
  }

  for (struct veh_data *veh = veh_list; veh; veh = veh->next) {
    room = get_veh_in_room(veh);

    if (veh->owner && (!idnum || veh->owner == idnum)) {
      int count = 0;
      char color_char = 'c';

      for (struct obj_data *obj = veh->contents; obj; obj = obj->next_content) {
        count += count_objects(obj);
      }

      if (count < 25)
        color_char = 'c';
      else if (count < 50)
        color_char = 'y';
      else if (count < 75)
        color_char = 'Y';
      else if (count < 100)
        color_char = 'o';
      else if (count < 150)
        color_char = 'O';
      else if (count < 200)
        color_char = 'r';
      else
        color_char = 'R';

      send_to_char(ch, "%2d) ^%c%3d^n items", idx++, color_char, count);

      if (!idnum) {
        const char *plr_name = get_player_name(veh->owner);
        send_to_char(ch, ", owned by ^c%s^n", plr_name);
        delete [] plr_name;
      }

      send_to_char(ch, ": ^W%s^n (%ld) ^cin^n %s^n (%ld)",
                   GET_VEH_NAME(veh),
                   GET_VEH_VNUM(veh),
                   GET_ROOM_NAME(room),
                   GET_ROOM_VNUM(room)
                  );

      if (veh->in_veh) {
        send_to_char(ch, " ^cinside^n %s^n (%ld)",
                     GET_VEH_NAME(veh->in_veh),
                     GET_VEH_VNUM(veh->in_veh)
                    );
      }

      send_to_char("\r\n", ch);
    }
  }

  if (idx == 1) {
    send_to_char(ch, "Found no vehicles belonging to idnum %ld.", idnum);
  }
}

ACMD(do_vstat)
{
  struct char_data *mob;
  struct obj_data *obj;
  struct veh_data *veh;
  int number, r_num;

  two_arguments(argument, buf, buf2);

  if (!*buf || !*buf2 || !isdigit(*buf2)) {
    send_to_char("Usage: vstat { obj | mob | qst | shp | veh | host } <number>\r\n", ch);
    return;
  }
  if ((number = atoi(buf2)) < 0) {
    send_to_char("A NEGATIVE number??\r\n", ch);
    return;
  }

  // Require that they have access to edit the zone they're statting.
  for (int counter = 0; counter <= top_of_zone_table; counter++) {
    if ((number >= (zone_table[counter].number * 100)) && (number <= (zone_table[counter].top))) {
      // Allow vstatting in template zones. Right now these are hardcoded, eventually you should make them zone flags.
      if (// Item template zones.
          ((zone_table[counter].number >= 800 && zone_table[counter].number <= 807)
            || (zone_table[counter].number >= 850 && zone_table[counter].number <= 860)
            || zone_table[counter].number == 65
            || zone_table[counter].number == 68
            || zone_table[counter].number == 70) ||
          // Mob template zones.
          (zone_table[counter].number == 128)
        )
      {
        // Break out without doing anything, otherwise check for edit capabilities.
        break;
      }

      if (!can_edit_zone(ch, counter)) {
        send_to_char("Sorry, you don't have access to edit that zone.\r\n", ch);
        return;
      }
      break;
    }
  }


  if (is_abbrev(buf, "host")) {
    if ((r_num = real_host(number)) < 0) {
      send_to_char("There is no host with that number.\r\n", ch);
      return;
    }
    do_stat_host(ch, &matrix[r_num]);
  } else if (is_abbrev(buf, "veh")) {
    if ((r_num = real_vehicle(number)) < 0) {
      send_to_char("There is no vehicle with that number.\r\n", ch);
      return;
    }
    veh = read_vehicle(r_num, REAL);
    do_stat_veh(ch, veh);
    extract_veh(veh);
  } else if (is_abbrev(buf, "mob")) {
    if ((r_num = real_mobile(number)) < 0) {
      send_to_char("There is no monster with that number.\r\n", ch);
      return;
    }
    mob = read_mobile(r_num, REAL);
    mob->mob_loaded_in_room = GET_ROOM_VNUM(&world[0]);
    char_to_room(mob, &world[0]);
    do_stat_mobile(ch, mob);
    extract_char(mob);
  } else if (is_abbrev(buf, "obj")) {
    if ((r_num = real_object(number)) < 0) {
      send_to_char("There is no object with that number.\r\n", ch);
      return;
    }
    obj = read_object(r_num, REAL, OBJ_LOAD_REASON_WIZLOAD);
    do_stat_object(ch, obj);
    extract_obj(obj);
  } else if (is_abbrev(buf, "qst")) {
    if ((r_num = real_quest(number)) < 0) {
      send_to_char("There is no quest with that number.\r\n", ch);
      return;
    }
    list_detailed_quest(ch, r_num);
  } else if (is_abbrev(buf, "shp")) {
    if ((r_num = real_shop(number)) < 0) {
      send_to_char("There is no shop with that number.\r\n", ch);
      return;
    }
    list_detailed_shop(ch, r_num);
  } else
    send_to_char("That'll have to be either 'mob', 'obj', 'qst', or 'shp'.\r\n", ch);
}

bool staff_purge_character(struct char_data *staff, struct char_data *vict, bool single_purge) {
  if (vict == staff) {
    if (single_purge)
      send_to_char(staff, "You can't purge yourself.\r\n");
    return FALSE;
  }

  // Prevent purging someone over your level.
  if (!IS_NPC(vict) && GET_LEVEL(staff) < GET_LEVEL(vict) && !access_level(staff, LVL_EXECUTIVE)) {
    if (single_purge)
      send_to_char(staff, "You can't purge %s: They're a higher-level staff than you.\r\n", GET_CHAR_NAME(staff));
    return FALSE;
  }

  if (single_purge)
    act("$n disintegrates $N.", FALSE, staff, 0, vict, TO_NOTVICT);

  // Disconnect purged player.
  if (!IS_NPC(vict)) {
    mudlog_vfprintf(staff, LOG_PURGELOG, "%s has purged %s.", GET_CHAR_NAME(staff), GET_NAME(vict));
    if (vict->desc) {
      close_socket(vict->desc);
      vict->desc = NULL;
    }
  }

  extract_char(vict);

  if (single_purge)
    send_to_char(OK, staff);
  
  return TRUE;
}

bool staff_purge_object(struct char_data *staff, struct obj_data *obj, bool single_purge) {
  if (single_purge)
    act("$n destroys $p.", FALSE, staff, obj, 0, TO_ROOM);

  const char *representation = generate_new_loggable_representation(obj);
  mudlog_vfprintf(staff, LOG_PURGELOG, "%s has purged %s.", GET_CHAR_NAME(staff), representation);
  delete [] representation;
  extract_obj(obj);

  if (single_purge)
    send_to_char(OK, staff);

  if (staff->in_room)
    staff->in_room->dirty_bit = TRUE;
  if (staff->in_veh && single_purge)
    save_single_vehicle(staff->in_veh);
  
  return TRUE;
}

bool staff_purge_vehicle(struct char_data *staff, struct veh_data *veh, bool single_purge) {
  // Notify the room.
  if (single_purge) {
    snprintf(buf1, sizeof(buf1), "$n purges %s.", GET_VEH_NAME(veh));
    act(buf1, FALSE, staff, NULL, 0, TO_ROOM);
  }

  const char *representation = generate_new_loggable_representation(veh);
  mudlog_vfprintf(staff, LOG_PURGELOG, "%s has purged %s.", GET_CHAR_NAME(staff), representation);
  delete [] representation;

  // Notify the owner.
  if (veh->owner > 0) {
    snprintf(buf2, sizeof(buf2), "^ROOC Alert: Your vehicle '%s' has been deleted by administrator '%s'.^n\r\n", GET_VEH_NAME(veh), GET_CHAR_NAME(staff));
    store_mail(veh->owner, staff, buf2);
  }

  // Log the purge and finalize extraction.
  purgelog(veh);
  delete_veh_file(veh, "purged");
  extract_veh(veh);

  if (single_purge)
    send_to_char(OK, staff);
  
  return TRUE;
}

/* clean a room of all mobiles and objects */
ACMD(do_purge)
{
  struct char_data *vict, *next_v;
  struct obj_data *obj, *next_o;
  struct veh_data *veh, *next_ve;
  extern void die_follower(struct char_data * ch);

  if (!access_level(ch, LVL_EXECUTIVE)) {
    bool is_editor = FALSE;
    int zone_idx = get_ch_in_room(ch)->zone;
    for (int editor_idx = 0; editor_idx < NUM_ZONE_EDITOR_IDS; editor_idx++) {
      is_editor |= (zone_table[zone_idx].editor_ids[editor_idx] == GET_IDNUM(ch));
    }
    FAILURE_CASE(!is_editor, "You can only purge in zones you're an editor of.");
  }

  one_argument(argument, buf);

  if (*buf) {                   /* argument supplied. destroy single object, character, or vehicle. */
    if ((vict = get_char_room_vis(ch, buf))) {
      staff_purge_character(ch, vict, TRUE);
      return;
    }

    if ((obj = get_obj_in_list_vis(ch, buf, ch->in_veh ? ch->in_veh->contents : ch->in_room->contents))) {
      staff_purge_object(ch, obj, TRUE);
      return;
    }

    if ((veh = get_veh_list(buf, ch->in_veh ? ch->in_veh->carriedvehs : ch->in_room->vehicles, ch))) {
      staff_purge_vehicle(ch, veh, TRUE);
    } else {
      send_to_char("Nothing here by that name.\r\n", ch);
      return;
    }

    send_to_char(OK, ch);
  } else {                      /* no argument. clean out the room or veh */
    if (ch->in_veh) {
      send_to_veh("You are surrounded by scorching flames!\r\n", ch->in_veh, NULL, FALSE);

      for (vict = ch->in_veh->people; vict; vict = next_v) {
        next_v = vict->next_in_veh;
        if (IS_NPC(vict) && vict != ch) {
          staff_purge_character(ch, vict, FALSE);
        }
      }

      for (obj = ch->in_veh->contents; obj; obj = next_o) {
        next_o = obj->next_content;
        staff_purge_object(ch, obj, FALSE);
      }

      for (veh = ch->in_veh->carriedvehs; veh; veh = next_ve) {
        next_ve = veh->next;
        if (veh == ch->in_veh) {
          mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Found recursively nested vehicle when purging carried vehicles!");
          continue;
        }

        staff_purge_vehicle(ch, veh, FALSE);
      }

      send_to_veh("The vehicle seems a little cleaner.\r\n", ch->in_veh, NULL, FALSE);
      save_single_vehicle(ch->in_veh);
    }
    else {
      act("$n gestures... You are surrounded by scorching flames!", FALSE, ch, 0, 0, TO_ROOM);

      for (vict = ch->in_room->people; vict; vict = next_v) {
        next_v = vict->next_in_room;
        if (IS_NPC(vict) && vict != ch) {
          staff_purge_character(ch, vict, FALSE);
        }
      }

      for (obj = ch->in_room->contents; obj; obj = next_o) {
        next_o = obj->next_content;
        staff_purge_object(ch, obj, FALSE);
      }

      for (veh = ch->in_room->vehicles; veh; veh = next_ve) {
        next_ve = veh->next_veh;
        staff_purge_vehicle(ch, veh, FALSE);
      }

      send_to_room("The world seems a little cleaner.\r\n", ch->in_room);
      ch->in_room->dirty_bit = TRUE;
    }
  }
}

void do_advance_with_mode(struct char_data *ch, char *argument, int cmd, int subcmd, bool can_self_advance) {
  struct char_data *victim;
  char *name = arg, *level = buf2;
  int newlevel, i;
  void do_start(struct char_data *ch, bool wipe_skills);
  extern void check_autowiz(struct char_data * ch);
  bool is_file = FALSE;

  char *remainder = two_arguments(argument, name, level);

  if (*name) {
    if (!str_cmp(name, "file")) {
      name = level;
      level = remainder;

      for (struct descriptor_data *td = descriptor_list; td; td = td->next) {
        if (td->character && !str_cmp(name, GET_CHAR_NAME(td->character))) {
          send_to_char(ch, "%s is already loaded.\r\n", name);
          return;
        }
        if (td->original && !str_cmp(name, GET_CHAR_NAME(td->original))) {
          send_to_char(ch, "%s is already loaded.\r\n", name);
          return;
        }
      }

      if (!does_player_exist(name)) {
        send_to_char("There is no such player.\r\n", ch);
        return;
      }

      is_file = TRUE;
      victim = playerDB.LoadChar(name, FALSE, PC_LOAD_REASON_OFFLINE_ADVANCE);
    } else if (!(victim = get_char_vis(ch, name))) {
      send_to_char("That player is not here.\r\n", ch);
      return;
    }
  } else {
    send_to_char("Advance who?\r\n", ch);
    return;
  }

  if (GET_LEVEL(ch) <= GET_LEVEL(victim) && ch != victim) {
    send_to_char("You need to be a higher level than your victim to do that.\r\n", ch);
    if (is_file) { extract_char(victim); }
    return;
  }
  if (IS_NPC(victim)) {
    send_to_char("NO!  Not on NPC's.\r\n", ch);
    if (is_file) { extract_char(victim); }
    return;
  }
  if (!*level || (newlevel = atoi(level)) <= 0) {
    send_to_char("That's not a level!\r\n", ch);
    if (is_file) { extract_char(victim); }
    return;
  }
  if (newlevel > LVL_MAX) {
    send_to_char(ch, "%d is the highest possible level.\r\n", LVL_MAX);
    if (is_file) { extract_char(victim); }
    return;
  }
  if (can_self_advance) {
    // You can only advance to level 9 unless you're the President.
    int max_ch_can_advance_to = GET_LEVEL(ch) < LVL_MAX ? LVL_MAX - 1 : LVL_MAX;
    if (newlevel > max_ch_can_advance_to) {
      send_to_char(ch, "%d is the highest possible level you can advance someone to.\r\n", max_ch_can_advance_to);
      if (is_file) { extract_char(victim); }
      return;
    }
  } else {
    if (!access_level(ch, newlevel) ) {
      send_to_char("Yeah, right.\r\n", ch);
      if (is_file) { extract_char(victim); }
      return;
    }

    if (ch == victim) {
      send_to_char("You can't advance yourself.\r\n", ch);
      return;
    }
  }

  send_to_char(OK, ch);

  if (newlevel < GET_LEVEL(victim)) {
    send_to_char(victim, "%s has demoted you from %s to %s. Note that this has reset your skills, stats, karma, etc.\r\n", GET_CHAR_NAME(ch), status_ratings[(int) GET_LEVEL(victim)], status_ratings[newlevel]);
    snprintf(buf3, sizeof(buf3), "%s has demoted %s from %s to %s.",
            GET_CHAR_NAME(ch), GET_CHAR_NAME(victim), status_ratings[(int)GET_LEVEL(victim)], status_ratings[newlevel]);
    mudlog(buf3, ch, LOG_WIZLOG, TRUE);
    do_start(victim, TRUE);
    GET_LEVEL(victim) = newlevel;
  } else {
    /* Preserved for history's sake. -LS
    act("$n makes some strange gestures.\r\n"
        "A strange feeling comes upon you,\r\n"
        "Like a giant hand, light comes down\r\n"
        "from above, grabbing your body, that\r\n"
        "begins to pulse with colored lights\r\n"
        "from inside.\r\n\r\n"
        "Your head seems to be filled with demons\r\n"
        "from another plane as your body dissolves\r\n"
        "to the elements of time and space itself.\r\n"
        "Suddenly a silent explosion of light\r\n"
        "snaps you back to reality.\r\n\r\n"
        "You feel slightly different.", FALSE, ch, 0, victim, TO_VICT);
     */
    send_to_char(victim, "%s has promoted you from %s to %s. Note that this has reset your skills, stats, chargen data, etc.\r\n", GET_CHAR_NAME(ch), status_ratings[(int) GET_LEVEL(victim)], status_ratings[newlevel]);
    snprintf(buf3, sizeof(buf3), "%s has advanced %s%s from %s to %s.",
            GET_CHAR_NAME(ch), GET_CHAR_NAME(victim), is_file ? " [file]" : "", status_ratings[(int)GET_LEVEL(victim)], status_ratings[newlevel]);
    mudlog(buf3, ch, LOG_WIZLOG, TRUE);
    GET_LEVEL(victim) = newlevel;

    // Auto-restore them to set their skills, stats, etc.
    restore_character(victim, TRUE);
  }

  if (IS_SENATOR(victim)) {
    for (i = COND_DRUNK; i <= COND_THIRST; i++)
      GET_COND(victim, i) = (char) -1;
    if (PLR_FLAGS(victim).IsSet(PLR_NEWBIE)) {
      PLR_FLAGS(victim).RemoveBit(PLR_NEWBIE);
      PLR_FLAGS(victim).RemoveBit(PLR_NOT_YET_AUTHED);
    }
    PRF_FLAGS(victim).SetBit(PRF_HOLYLIGHT);
    PRF_FLAGS(victim).SetBit(PRF_CONNLOG);
    PRF_FLAGS(victim).SetBit(PRF_ROOMFLAGS);
    PRF_FLAGS(victim).SetBit(PRF_NOHASSLE);
    PRF_FLAGS(victim).SetBit(PRF_AUTOINVIS);
  }

  // We use INSERT IGNORE to cause it to not error out when updating someone who already had immort data.
  snprintf(buf, sizeof(buf), "INSERT IGNORE INTO pfiles_immortdata (idnum) VALUES (%ld);", GET_IDNUM(victim));
  mysql_wrapper(mysql, buf);

  if (is_file) {
    extract_char(victim);
  } else {
    playerDB.SaveChar(victim);
  }
}

ACMD(do_self_advance) {
  do_advance_with_mode(ch, argument, cmd, subcmd, TRUE);
}

ACMD(do_advance) {
  // Wrapper for do_advance with no self-advancement enabled.
  do_advance_with_mode(ch, argument, cmd, subcmd, FALSE);
}

ACMD(do_payout) {
  MYSQL_RES *res;
  MYSQL_ROW row;

  struct char_data *vict;
  char amt[MAX_STRING_LENGTH];
  char reason[MAX_STRING_LENGTH];
  int k;

  half_chop(argument, arg, buf, sizeof(buf));
  half_chop(buf, amt, reason, sizeof(reason));

  k = atoi(amt);

  if (!*arg || !*amt || !*reason || k <= 0 ) {
    send_to_char("Syntax: payout <player> <nuyen> <Reason for the payout>\r\n", ch);
    return;
  }

  if (!(vict = get_player_vis(ch, arg, FALSE))) {
    snprintf(buf, sizeof(buf), "SELECT idnum, cash FROM pfiles WHERE name='%s';", prepare_quotes(buf2, arg, sizeof(buf2) / sizeof(buf2[0])));
    if (mysql_wrapper(mysql, buf)) {
      send_to_char("An unexpected error occurred (query failed).\r\n", ch);
      return;
    }
    if (!(res = mysql_use_result(mysql))) {
      send_to_char("An unexpected error occurred (use_result failed).\r\n", ch);
      return;
    }
    row = mysql_fetch_row(res);
    if (!row && mysql_field_count(mysql)) {
      mysql_free_result(res);
      send_to_char(ch, "Could not find a PC named %s.\r\n", arg);
      return;
    }
    long idnum = atol(row[0]);
    int old_nuyen = atoi(row[1]);
    mysql_free_result(res);

    // Now that we've confirmed the player exists, update them.
    snprintf(buf, sizeof(buf), "UPDATE pfiles SET cash = cash + %d WHERE idnum='%ld';", k, idnum);
    if (mysql_wrapper(mysql, buf)) {
      send_to_char("An unexpected error occurred on update (query failed).\r\n", ch);
      return;
    }

    // Mail the victim.
    snprintf(buf, sizeof(buf), "You have been paid %d nuyen for %s%s^n\r\n",
             k,
             reason,
             ispunct(get_final_character_from_string(reason)) ? "" : ".");
    store_mail(idnum, ch, buf);

    // Notify the actor.
    send_to_char(ch, "You pay %d nuyen to %s for %s%s^n\r\n",
                 k,
                 capitalize(arg),
                 reason,
                 ispunct(get_final_character_from_string(reason)) ? "" : ".");

    // Log it.
    snprintf(buf, sizeof(buf), "%s paid %d nuyen to %s for %s^g (%d to %d).",
             GET_CHAR_NAME(ch),
             k,
             arg,
             reason,
             old_nuyen,
             old_nuyen + k);
    mudlog(buf, ch, LOG_WIZLOG, TRUE);
    return;
  }

  if (IS_SENATOR(vict)) {
    send_to_char(ch, "Staff can't receive nuyen this way. Use the SET command.\r\n", ch);
    return;
  }

  if (GET_NUYEN(vict) + k > MYSQL_UNSIGNED_INT_MAX) {
    send_to_char(ch, "That would put %s over the absolute nuyen maximum. You may award up to %d nuyen. Otherwise, tell %s to spend what %s has, or compensate %s some other way.\r\n",
                 GET_CHAR_NAME(vict), MYSQL_UNSIGNED_INT_MAX - GET_NUYEN(vict), HMHR(vict), HSSH(vict), HMHR(vict));
    return;
  }

  if (vict->desc && vict->desc->original)
    gain_nuyen(vict->desc->original, k, NUYEN_INCOME_STAFF_PAYOUT);
  else
    gain_nuyen(vict, k, NUYEN_INCOME_STAFF_PAYOUT);

  send_to_char(vict, "You have been paid %d nuyen for %s%s^n\r\n", k, reason, ispunct(get_final_character_from_string(reason)) ? "" : ".");

  send_to_char(ch, "You paid %d nuyen to %s for %s%s^n\r\n", k, GET_CHAR_NAME(vict), reason, ispunct(get_final_character_from_string(reason)) ? "" : ".");

  snprintf(buf2, sizeof(buf2), "%s paid %d nuyen to %s for %s^g (%ld to %ld).",
          GET_CHAR_NAME(ch), k,
          GET_CHAR_NAME(vict), reason, GET_NUYEN(vict) - k, GET_NUYEN(vict));
  mudlog(buf2, ch, LOG_WIZLOG, TRUE);
}

ACMD(do_charge) {
  MYSQL_RES *res;
  MYSQL_ROW row;

  struct char_data *vict;
  char amt[MAX_STRING_LENGTH];
  char reason[MAX_STRING_LENGTH];
  int k;

  half_chop(argument, arg, buf, sizeof(buf));
  half_chop(buf, amt, reason, sizeof(reason));

  k = atoi(amt);

  if (!*arg || !*amt || !*reason || k <= 0 ) {
    send_to_char("Syntax: charge <player> <nuyen> <Reason for the nuyen deduction>\r\n", ch);
    return;
  }

  if (!(vict = get_char_vis(ch, arg))) {
    snprintf(buf, sizeof(buf), "SELECT idnum, cash FROM pfiles WHERE name='%s' AND cash >= %d;", prepare_quotes(buf2, arg, sizeof(buf2) / sizeof(buf2[0])), k);
    if (mysql_wrapper(mysql, buf)) {
      send_to_char("An unexpected error occurred (query failed).\r\n", ch);
      return;
    }
    if (!(res = mysql_use_result(mysql))) {
      send_to_char("An unexpected error occurred (use_result failed).\r\n", ch);
      return;
    }
    row = mysql_fetch_row(res);
    if (!row && mysql_field_count(mysql)) {
      mysql_free_result(res);
      send_to_char(ch, "Could not find a PC named '%s' that had %d nuyen on them.\r\n", arg, k);
      return;
    }
    long idnum = atol(row[0]);
    int old_nuyen = atoi(row[1]);
    mysql_free_result(res);

    // Now that we've confirmed the player exists, update them.
    snprintf(buf, sizeof(buf), "UPDATE pfiles SET cash = cash - %d WHERE idnum='%ld';", k, idnum);
    if (mysql_wrapper(mysql, buf)) {
      send_to_char("An unexpected error occurred on update (query failed).\r\n", ch);
      return;
    }

    // Mail the victim.
    snprintf(buf, sizeof(buf), "You have been charged %d nuyen for %s%s^n\r\n",
             k,
             reason,
             ispunct(get_final_character_from_string(reason)) ? "" : ".");
    store_mail(idnum, ch, buf);

    // Notify the actor.
    send_to_char(ch, "You charge %d nuyen from %s for %s%s^n\r\n",
                 k,
                 capitalize(arg),
                 reason,
                 ispunct(get_final_character_from_string(reason)) ? "" : ".");

    // Log it.
    snprintf(buf, sizeof(buf), "%s charged %d nuyen from %s for %s^g (%d to %d).",
             GET_CHAR_NAME(ch),
             k,
             arg,
             reason,
             old_nuyen,
             old_nuyen - k);
    mudlog(buf, ch, LOG_WIZLOG, TRUE);
    return;
  }

  if (IS_SENATOR(vict)) {
    send_to_char(ch, "Staff can't lose nuyen this way. Use the SET command.\r\n", ch);
    return;
  }

  if (GET_NUYEN(vict) - k < 0 || GET_NUYEN(vict) - k > GET_NUYEN(vict)) {
    send_to_char(ch, "That would put %s at negative nuyen. You may charge up to %d nuyen from %s.\r\n",
                 GET_CHAR_NAME(vict), GET_NUYEN(vict), HMHR(vict));
    return;
  }

  if (vict->desc && vict->desc->original)
    lose_nuyen(vict->desc->original, k, NUYEN_OUTFLOW_STAFF_CHARGE);
  else
    lose_nuyen(vict, k, NUYEN_OUTFLOW_STAFF_CHARGE);

  send_to_char(vict, "You have been charged %d nuyen for %s%s^n\r\n", k, reason, ispunct(get_final_character_from_string(reason)) ? "" : ".");

  send_to_char(ch, "You charged %d nuyen from %s for %s%s^n\r\n", k, GET_CHAR_NAME(vict), reason, ispunct(get_final_character_from_string(reason)) ? "" : ".");

  snprintf(buf2, sizeof(buf2), "%s charged %d nuyen from %s for %s^g (%ld to %ld).",
          GET_CHAR_NAME(ch), k,
          GET_CHAR_NAME(vict), reason, GET_NUYEN(vict) + k, GET_NUYEN(vict));
  mudlog(buf2, ch, LOG_WIZLOG, TRUE);
}

void staff_induced_karma_alteration_for_online_char(struct char_data *ch, struct char_data *vict, int karma_times_100, const char *reason, bool add_karma) {
  float karma = ((float) karma_times_100) / 100;
  int old_karma = GET_KARMA(vict);

  if (add_karma) {
    if (GET_KARMA(vict) + karma_times_100 > MYSQL_UNSIGNED_MEDIUMINT_MAX) {
      send_to_char(ch, "That would put %s over the absolute karma maximum. You may award up to %d points of karma. Otherwise, tell %s to spend what %s has, or compensate %s some other way.\r\n",
                   GET_CHAR_NAME(vict),
                   MYSQL_UNSIGNED_MEDIUMINT_MAX - GET_KARMA(vict),
                   HMHR(vict),
                   HSSH(vict),
                   HMHR(vict));
      return;
    }

    // Add to karma.
    GET_KARMA(vict) += karma_times_100;

    // Add to rep.
    GET_REP(vict) += karma;

    // Add to TKE.
    GET_TKE(vict) += karma;
  } else {
    if (GET_KARMA(vict) - karma_times_100 < 0) {
      send_to_char(ch, "That would put %s under zero karma. You may deduct up to %d points of karma.\r\n",
                   GET_CHAR_NAME(vict),
                   GET_KARMA(vict));
      return;
    }

    // Add to karma.
    GET_KARMA(vict) -= karma_times_100;

    // Make no changes to rep or TKE.
  }

  // Notify the victim.
  send_to_char(vict, "You have been %s %0.2f karma for %s%s^n\r\n",
               add_karma ? "awarded" : "deducted",
               karma,
               reason,
               ispunct(get_final_character_from_string(reason)) ? "" : ".");

  // Notify the ch.
  send_to_char(ch, "You %s %s %0.2f karma for %s%s^n\r\n",
               add_karma ? "awarded" : "deducted",
               GET_CHAR_NAME(vict),
               karma,
               reason,
               ispunct(get_final_character_from_string(reason)) ? "" : ".");

  // Log it.
  snprintf(buf2, sizeof(buf2), "%s %s %0.2f karma to %s for %s^g (%d to %d).",
           GET_CHAR_NAME(ch),
           add_karma ? "awarded" : "deducted",
           karma,
           GET_CHAR_NAME(vict),
           reason,
           old_karma,
           GET_KARMA(vict));
  mudlog(buf2, ch, LOG_WIZLOG, TRUE);

  // Persist results.
  playerDB.SaveChar(vict);
}

void staff_induced_karma_alteration_for_offline_char(struct char_data *ch, const char *name, int karma_times_100, const char *reason, bool add_karma) {
  float karma = ((float) karma_times_100) / 100;

  char query_buf[MAX_INPUT_LENGTH + 1000], tmp[MAX_INPUT_LENGTH];

  snprintf(query_buf, sizeof(query_buf), "SELECT idnum, karma FROM pfiles WHERE name='%s';",
           prepare_quotes(tmp, name, sizeof(buf2) / sizeof(tmp[0])));

  FAILURE_CASE(mysql_wrapper(mysql, query_buf), "An unexpected error occurred (query failed).");

  MYSQL_RES *res = mysql_use_result(mysql);

  FAILURE_CASE(!res, "An unexpected error occurred (use_result failed).");

  MYSQL_ROW row = mysql_fetch_row(res);

  if (!row && mysql_field_count(mysql)) {
    mysql_free_result(res);
    send_to_char(ch, "Could not find a PC named %s^n.\r\n", name);
    return;
  }

  long idnum = atol(row[0]);
  int old_karma = atoi(row[1]);
  mysql_free_result(res);

  if (add_karma) {
    if (old_karma + karma_times_100 > MYSQL_UNSIGNED_MEDIUMINT_MAX) {
      send_to_char(ch, "That would put them over the absolute karma maximum. You may award up to %d points of karma.\r\n",
                   MYSQL_UNSIGNED_MEDIUMINT_MAX - old_karma);
      return;
    }

    snprintf(query_buf, sizeof(query_buf), "UPDATE pfiles SET Karma = Karma + %d, Rep = Rep + %d, TKE = TKE + %d WHERE idnum='%ld';",
             karma_times_100,
             (int) karma,
             (int) karma,
             idnum);
  } else {
    if (old_karma - karma_times_100 < 0) {
      send_to_char(ch, "That would put them at negative karma. You may deduct up to %d points of karma.\r\n",
                   old_karma);
      return;
    }

    snprintf(query_buf, sizeof(query_buf), "UPDATE pfiles SET Karma = Karma - %d WHERE idnum='%ld';",
             karma_times_100,
             idnum);
  }

  FAILURE_CASE(mysql_wrapper(mysql, query_buf), "An unexpected error occurred on update (query failed).");

  // Mail the victim.
  snprintf(buf, sizeof(buf), "You have been %s %0.2f karma for %s%s^n\r\n",
           add_karma ? "awarded" : "deducted",
           karma,
           reason,
           ispunct(get_final_character_from_string(reason)) ? "" : ".");
  store_mail(idnum, ch, buf);

  // Notify the actor.
  send_to_char(ch, "You %s %s %0.2f karma for %s%s^n\r\n",
               add_karma ? "award" : "deduct",
               capitalize(arg),
               karma,
               reason,
               ispunct(get_final_character_from_string(reason)) ? "" : ".");

  // Log it.
  snprintf(buf, sizeof(buf), "%s %s %s %0.2f karma for %s^g (%d to %d).",
           GET_CHAR_NAME(ch),
           add_karma ? "awarded" : "deducted",
           capitalize(arg),
           karma,
           reason,
           old_karma,
           add_karma ? old_karma + karma_times_100 : old_karma - karma_times_100);
  mudlog(buf, ch, LOG_WIZLOG, TRUE);
}

ACMD(do_award)
{
  struct char_data *vict = NULL;
  char amt[MAX_STRING_LENGTH];
  char reason[MAX_STRING_LENGTH];

  half_chop(argument, arg, buf, sizeof(buf));
  half_chop(buf, amt, reason, sizeof(reason));

  int karma_times_100 = atoi(amt);

  if (!*arg || !*amt || !*reason || karma_times_100 <= 0 ) {
    send_to_char("Syntax: award <player> <karma x 100> <Reason for award>\r\n", ch);
    return;
  }

  // Scan online PCs only.
  for (struct descriptor_data *d = descriptor_list; d; d = d->next) {
    struct char_data *tmp = d->original ? d->original : d->character;

    if (!tmp)
      continue;

    if (!str_cmp(arg, GET_CHAR_NAME(tmp))) {
      vict = tmp;
      break;
    }
  }

  if (!vict) {
    staff_induced_karma_alteration_for_offline_char(ch, arg, karma_times_100, reason, TRUE);
    return;
  }

  if (vict->desc && vict->desc->original)
    vict = vict->desc->original;

  FAILURE_CASE(IS_SENATOR(vict), "Staff can't receive karma this way. Use the SET command");
  FAILURE_CASE(IS_NPC(vict), "Not on NPCs.");

  staff_induced_karma_alteration_for_online_char(ch, vict, karma_times_100, reason, TRUE);
}

ACMD(do_deduct)
{
  struct char_data *vict;
  char amt[MAX_STRING_LENGTH];
  char reason[MAX_STRING_LENGTH];
  int karma_times_100;

  half_chop(argument, arg, buf, sizeof(buf));
  half_chop(buf, amt, reason, sizeof(reason));

  karma_times_100 = atoi(amt);

  if (!*arg || !*amt || !*reason || karma_times_100 <= 0 ) {
    send_to_char("Syntax: deduct <player> <karma x 100> <Reason for penalty>\r\n", ch);
    return;
  }
  if (!(vict = get_char_vis(ch, arg))) {
    staff_induced_karma_alteration_for_offline_char(ch, arg, karma_times_100, reason, FALSE);
    return;
  }

  if (vict->desc && vict->desc->original)
    vict = vict->desc->original;

  FAILURE_CASE(IS_SENATOR(vict), "Staff can't lose karma this way. Use the SET command");
  FAILURE_CASE(IS_NPC(vict), "Not on NPCs.");

  staff_induced_karma_alteration_for_online_char(ch, vict, karma_times_100, reason, FALSE);
}

// Restores a character to peak physical condition.
void restore_character(struct char_data *vict, bool reset_staff_stats) {
  if (!vict) {
    mudlog("SYSERR: Received NULL victim to restore_character!", vict, LOG_SYSLOG, TRUE);
    return;
  }
  // Restore their physical and mental damage levels.
  GET_PHYSICAL(vict) = GET_MAX_PHYSICAL(vict);
  GET_MENTAL(vict) = GET_MAX_MENTAL(vict);

  // Clear their qui loss.
  GET_TEMP_QUI_LOSS(vict) = 0;

  // Non-NPCs get further consideration.
  if (!IS_NPC(vict)) {
    if (GET_LEVEL(vict) > LVL_MORTAL) {
      // Staff? Purge all drug-related data including addiction, tolerance etc.
      clear_all_drug_data_for_char(vict);
    } else {
      // Otherwise, take them off the drugs and remove withdrawal state.
      reset_all_drugs_for_char(vict);
    }

    // Touch up their hunger, thirst, etc.
    for (int i = COND_DRUNK; i <= COND_THIRST; i++){
      // Staff don't deal with hunger, etc-- disable it for them.
      if (IS_SENATOR(vict)) {
        GET_COND(vict, i) = COND_IS_DISABLED;
      }

      // Mortals get theirs set to full (not drunk, full food/water).
      else {
        switch (i) {
          case COND_DRUNK:
            GET_COND(vict, i) = MIN_DRUNK;
            break;
          case COND_FULL:
            GET_COND(vict, i) = MAX_FULLNESS;
            break;
          case COND_THIRST:
            GET_COND(vict, i) = MAX_QUENCHED;
            break;
          default:
            snprintf(buf, sizeof(buf), "ERROR: Unknown condition type %d in restore_character switch statement.", i);
            mudlog(buf, vict, LOG_SYSLOG, TRUE);
            return;
        }
      }
    }

    // Staff members get their skills set to max, and also their stats boosted.
    if (IS_SENATOR(vict) && reset_staff_stats) {
      // Give them 100 in every skill.
      for (int i = MIN_SKILLS; i < MAX_SKILLS; i++)
        set_character_skill(vict, i, 100, FALSE);

      // Restore their essence to 6.00.
      GET_REAL_ESS(vict) = GET_RACIAL_STARTING_ESSENCE_FOR_RACE(GET_RACE(vict));

      // Non-executive staff get 15's in stats. Executives get 50s.
      int stat_level = access_level(vict, LVL_EXECUTIVE) ? 50 : 15;

      // Set all their stats to the requested level.
      GET_REAL_INT(vict) = stat_level;
      GET_REAL_WIL(vict) = stat_level;
      GET_REAL_QUI(vict) = stat_level;
      GET_REAL_STR(vict) = stat_level;
      GET_REAL_BOD(vict) = stat_level;
      GET_REAL_CHA(vict) = stat_level;
      GET_SETTABLE_REAL_MAG(vict) = stat_level * 100;

      // Recalculate their affects.
      affect_total(vict);
    }
  }

  // Update their position (restores them from death, etc)
  update_pos(vict);
}

ACMD(do_restore)
{
  struct char_data *vict;

  // Shifted here from interpreter to allow morts to use the restore command for the testing obj.
  if (!access_level(ch, LVL_CONSPIRATOR)) {
#ifdef IS_BUILDPORT
    send_to_char("OK, restoring yourself. This will also remove any drug affects.\r\n", ch);
    restore_character(ch, TRUE);
    clear_all_drug_data_for_char(ch);
#else
    send_to_char(ch, "You'll need to obtain one of the extremely rare self-heal injectors to use this command.\r\n");
#endif
    return;
  }

  one_argument(argument, buf);

  // We require an argument.
  if (!*buf) {
    send_to_char("Whom do you wish to restore?\r\n", ch);
    return;
  }

  // Restore-all mode.
  if (*buf == '*') {
    for (struct descriptor_data *d = descriptor_list; d; d = d->next) {
      if (d->character) {
        restore_character(d->character, FALSE);
        act("A wave of healing ripples over all online characters.\r\nYou have been fully healed by $N!", FALSE, d->character, 0, ch, TO_CHAR);
      }
      if (d->original) {
        restore_character(d->original, FALSE);
      }
    }
    snprintf(buf2, sizeof(buf2), "%s restored all players.", GET_CHAR_NAME(ch));
    mudlog(buf2, ch, LOG_WIZLOG, TRUE);
    send_to_char(OK, ch);
    return;
  }

  // Not restore all mode-- find their target.
  if (!(vict = get_char_vis(ch, buf))) {
    send_to_char(ch, "You don't see anyone named '%s' here.\r\n", buf);
    return;
  }

  // Restore-single-target mode.
  restore_character(vict, TRUE);

  // Single target also strips all drug info (fully remove edge, addiction etc)
  clear_all_drug_data_for_char(vict);

  act("You have been fully healed by $N!", FALSE, vict, 0, ch, TO_CHAR);
  snprintf(buf, sizeof(buf), "%s fully restored %s.", GET_CHAR_NAME(ch), GET_CHAR_NAME(vict));
  mudlog(buf, ch, LOG_WIZLOG, TRUE);
  send_to_char(OK, ch);
  return;
}

void perform_immort_vis(struct char_data *ch)
{
  void appear(struct char_data *ch);

  if (GET_INVIS_LEV(ch) == 0 && !IS_AFFECTED(ch, AFF_RUTHENIUM)) {
    send_to_char("You are already fully visible.\r\n", ch);
    return;
  }

  GET_INVIS_LEV(ch) = 0;
  appear(ch);
  send_to_char("You are now fully visible.\r\n", ch);
}

void perform_immort_invis(struct char_data *ch, int level)
{
  struct char_data *tch;

  if (STATE(ch->desc) == CON_PLAYING)
  {
    if (ch->in_room) {
      for (tch = ch->in_room->people; tch; tch = tch->next_in_room) {
        if (tch == ch)
          continue;
        if (access_level(tch, GET_INVIS_LEV(ch)) && !access_level(tch, level))
          act("You blink and suddenly realize that $n is gone.",
              FALSE, ch, 0, tch, TO_VICT);
        if (!access_level(tch, GET_INVIS_LEV(ch)) && access_level(tch, level))
          act("You suddenly realize that $n is standing beside you.",
              FALSE, ch, 0, tch, TO_VICT);
      }
    } else {
      for (tch = ch->in_veh->people; tch; tch = tch->next_in_veh) {
        if (tch == ch)
          continue;
        if (access_level(tch, GET_INVIS_LEV(ch)) && !access_level(tch, level))
          act("You blink and suddenly realize that $n is gone.",
              FALSE, ch, 0, tch, TO_VICT);
        if (!access_level(tch, GET_INVIS_LEV(ch)) && access_level(tch, level))
          act("You suddenly realize that $n is standing beside you.",
              FALSE, ch, 0, tch, TO_VICT);
      }
    }

    send_to_char(ch, "Your invisibility level is %d.\r\n", level);
  }

  GET_INVIS_LEV(ch) = level;
}
/* Recoded by root */
ACMD(do_invis)
{
  int level;

  /* Checking if user IS_NPC */
  if (IS_NPC(ch)) {
    send_to_char("You can't do that!\r\n", ch);
    return;
  }

  one_argument(argument, arg);
  /* Checking if argument is given, if Not, set user viable */
  if (!*arg) {
    if (GET_INVIS_LEV(ch) > 0)
      perform_immort_vis(ch);
    /* Fixing from going max invis */
    else if (GET_LEVEL(ch) >= LVL_EXECUTIVE)
      perform_immort_invis(ch, 2);
    else
      send_to_char(ch, "Due to player concerns, it is not possible for immortals under level %d to go invisible.\r\n", LVL_EXECUTIVE);
  } else {
    if (GET_LEVEL(ch) < LVL_EXECUTIVE) {
      send_to_char(ch, "Due to player concerns, it is not possible for immortals under level %d to go invisible.\r\n", LVL_EXECUTIVE);
      perform_immort_vis(ch);
      return;
    }

    level = atoi(arg);
    if (!access_level(ch, level)) {
      send_to_char("You can't go invisible above your own level.\r\n", ch);
    } else if (!access_level(ch, LVL_VICEPRES)
               && level > LVL_BUILDER) {
      send_to_char("All senators are equal and there is no need for above level 2 invisibility.\r\n", ch);
    } else if (level < 1) {
      perform_immort_vis(ch);
    } else {
      perform_immort_invis(ch, level);
    }
  }
}


ACMD(do_poofset)
{
  char **msg;

  switch (subcmd) {
  case SCMD_POOFIN:
    msg = &(POOFIN(ch));
    break;
  case SCMD_POOFOUT:
    msg = &(POOFOUT(ch));
    break;
  default:
    return;
  }

  skip_spaces(&argument);

  if (!*argument) {
    if (!POOFIN(ch))
      POOFIN(ch) = str_dup(DEFAULT_POOFIN_STRING);
    if (!POOFOUT(ch))
      POOFOUT(ch) = str_dup(DEFAULT_POOFOUT_STRING);

    if (subcmd == SCMD_POOFIN)
      send_to_char(ch, "Your current poofin is: ^m%s^n\r\n", POOFIN(ch));
    else
      send_to_char(ch, "Your current poofout is: ^m%s^n\r\n", POOFOUT(ch));
    return;
  } else if (strlen(argument) >= LINE_LENGTH) {
    send_to_char(ch, "Line too long (max %d characters). Function aborted.\r\n",
                 LINE_LENGTH - 1);
    return;
  }

  DELETE_ARRAY_IF_EXTANT(*msg);
  *msg = str_dup(argument);

  snprintf(buf, sizeof(buf), "UPDATE pfiles_immortdata SET poofin='%s', poofout='%s' WHERE idnum=%ld;",
          prepare_quotes(buf2, POOFIN(ch), sizeof(buf2) / sizeof(buf2[0])),
          prepare_quotes(buf3, POOFOUT(ch), sizeof(buf3) / sizeof(buf3[0])),
          GET_IDNUM(ch));
  mysql_wrapper(mysql, buf);
  send_to_char(OK, ch);
}

ACMD(do_dc)
{
  struct descriptor_data *d;
  struct char_data *vict;
  one_argument(argument, arg);

  if (!*arg) {
    send_to_char("Usage: dc <name>\r\n"
                 "       dc <connection number>\r\n"
                 "       dc *\r\n", ch);
    return;
  }

  if (*arg == '*') {
    send_to_char("All non-playing connections closed.\r\n", ch);
    snprintf(buf, sizeof(buf), "Non-playing connections closed by %s.", GET_CHAR_NAME(ch));
    mudlog(buf, ch, LOG_WIZLOG, TRUE);
    for (d = descriptor_list; d; d = d->next)
      if ((d->connected > 0 && d->connected < CON_SPELL_CREATE && d->connected != CON_PART_CREATE) || d->connected == CON_ASKNAME)
        close_socket(d);
    return;
  }

  if (atoi(arg) > 0) {
    for (d = descriptor_list; d; d = d->next) {
      if (d->desc_num == atoi(arg)) {
        if (d->connected == CON_PLAYING) {
          send_to_char(ch, "You can't DC someone in the playing state by number. Use DC <name> instead.\r\n");
          return;
        }

        send_to_char(ch, "OK, disconnecting connection %d.\r\n", d->desc_num);
        mudlog_vfprintf(ch, LOG_WIZLOG, "%s disconnecting non-playing connection %d.", GET_CHAR_NAME(ch), d->desc_num);
        close_socket(d);
        return;
      }
    }
    send_to_char(ch, "There is no connected descriptor numbered %d.\r\n", atoi(arg));
    return;
  }

  if (!(vict = get_player_vis(ch, arg, 0))) {
    send_to_char(ch, "You can't see anyone named '%s'.\r\n", arg);
    return;
  }
  if (GET_LEVEL(vict) >= GET_LEVEL(ch)) {
    send_to_char("You need to be a higher level than your victim to do that.\r\n", ch);
    return;
  }
  if (!vict->desc) {
    act("It seems $E's already linkdead...", FALSE, ch, 0, vict, TO_CHAR);
    return;
  }

  send_to_char(ch, "%s's connection closed.\r\n", GET_CHAR_NAME(vict));
  snprintf(buf, sizeof(buf), "%s's connection closed by %s.", GET_CHAR_NAME(vict),
          GET_CHAR_NAME(ch));
  mudlog(buf, ch, LOG_WIZLOG, TRUE);
  /* Since we are DCing somone lets remove them from the game as well*/
  if (vict->desc
      && !IS_SENATOR(vict))
    STATE(vict->desc) = CON_CLOSE;
  extract_char(vict);
  close_socket(vict->desc);

}

ACMD(do_wizlock)
{
  int value;
  bool changed = FALSE;

  one_argument(argument, arg);
  if (*arg) {
    value = atoi(arg);
    if (value < 0 || !access_level(ch, value)) {
      send_to_char(ch, "Invalid wizlock value. Enter a value between 0 and %d.\r\n", GET_REAL_LEVEL(ch));
      return;
    }
    if (restrict_mud != value) {
      restrict_mud = value;
      changed = TRUE;
    }
  }

  switch (restrict_mud) {
  case 0:
    snprintf(buf, sizeof(buf), "The game is %s completely open.\r\n", changed ? "now" : "currently");
    break;
  case 1:
    snprintf(buf, sizeof(buf), "The game is %s closed to new players.\r\n", changed ? "now" : "currently");
    break;
  default:
    snprintf(buf, sizeof(buf), "Only level %d and above may enter the game %s.\r\n",
            restrict_mud, changed ? "now" : "currently");
    break;
  }

  send_to_char(buf, ch);

  if (changed) {
    snprintf(buf, sizeof(buf), "%s set wizlock to level %d.", GET_CHAR_NAME(ch), restrict_mud);
    mudlog(buf, ch, LOG_WIZLOG, TRUE);
  }
}

ACMD(do_date)
{
  extern int global_last_booted_from;

  char *tmstr;
  time_t mytime;
  int d, h, m;
  extern time_t boot_time;

  if (subcmd == SCMD_DATE)
    mytime = time(0);
  else
    mytime = boot_time;

  tmstr = (char *) asctime(localtime(&mytime)); // technically kosher; "The returned value points to an internal array whose validity or value may be altered by any subsequent call to asctime or ctime."
  *(tmstr + strlen(tmstr) - 1) = '\0';

  if (subcmd == SCMD_DATE)
    snprintf(buf, sizeof(buf), "Current machine time: %s\r\n", tmstr);
  else {
    mytime = time(0) - boot_time;
    d = mytime / 86400;
    h = (mytime / 3600) % 24;
    m = (mytime / 60) % 60;

    snprintf(buf, sizeof(buf), "Up since %s: %d day%s, %d:%02d\r\nWe booted due to %s.\r\n", 
             tmstr, 
             d,
             d == 1 ? "" : "s", 
             h, 
             m,
             booted_from_string[global_last_booted_from]);
  }

  send_to_char(buf, ch);
}

ACMD(do_last)
{
  struct char_data *vict = NULL;
  bool from_file = FALSE;
  int level = 0;
  long idnum = 0, lastdisc = 0;
  char *name = NULL, *host = NULL;
  MYSQL_RES *res;
  MYSQL_ROW row;

  one_argument(argument, arg);
  if (!*arg) {
    send_to_char("For whom do you wish to search?\r\n", ch);
    return;
  }

  if (!(vict = get_player_vis(ch, arg, FALSE))) {
    from_file = TRUE;
    snprintf(buf, sizeof(buf), "SELECT Idnum, `Rank`, Host, LastD, Name FROM pfiles WHERE name='%s';", prepare_quotes(buf2, arg, sizeof(buf2) / sizeof(buf2[0])));
    if (mysql_wrapper(mysql, buf))
      return;
    if (!(res = mysql_use_result(mysql)))
      return;
    row = mysql_fetch_row(res);
    if (!row && mysql_field_count(mysql)) {
      mysql_free_result(res);
      send_to_char("There is no such player.\r\n", ch);
      return;
    }
    idnum = atol(row[0]);
    level = atoi(row[1]);
    host = str_dup(row[2]);
    lastdisc = atol(row[3]);
    name = str_dup(arg);
    mysql_free_result(res);
  } else {
    level = GET_LEVEL(vict);
    name = str_dup(GET_CHAR_NAME(vict));
    idnum = GET_IDNUM(vict);
    host = str_dup(vict->player.host);
    lastdisc = vict->player.time.lastdisc;
  }

  if ((level > GET_LEVEL(ch)) && !access_level(ch, LVL_ADMIN)) {
    if (subcmd == SCMD_FINGER)
      send_to_char("You cannot finger someone whose level is higher than yours.\r\n", ch);
    else
      send_to_char("You are not sufficiently erudite for that!\r\n", ch);

    return;
  }

  if (subcmd == SCMD_FINGER) {
    snprintf(buf, sizeof(buf), "%-12s :: Last online:  %-20s\r\n",
            name, ctime(&lastdisc));

  } else
    snprintf(buf, sizeof(buf), "[%8ld] [%2d] %-12s : %-18s : %-20s\r\n",
            idnum, level, name, host, ctime(&lastdisc));

  if (!from_file && CAN_SEE(ch, vict))
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf),"%s is currently logged on.\r\n", name);

  send_to_char(buf, ch);
}

ACMD(do_force)
{
  struct descriptor_data *i, *next_desc;
  struct char_data *vict, *next_force;
  char to_force[MAX_INPUT_LENGTH + 2];

  half_chop(argument, arg, to_force, sizeof(to_force));

  snprintf(buf1, sizeof(buf1), "%s has forced you to '%s'.\r\n", GET_CHAR_NAME(ch), to_force);

  if (!*arg || !*to_force) {
    send_to_char("Whom do you wish to force do what?\r\n", ch);
    return;
  }

  // Single-person force.
  if (!access_level(ch, LVL_ADMIN) || (str_cmp("all", arg) && str_cmp("room", arg))) {
    if (!(vict = get_char_vis(ch, arg)))
      send_to_char(ch, "You don't see anyone named '%s' here.\r\n", arg);
    else if (PLR_FLAGGED(ch, PLR_WRITING) || PLR_FLAGGED(ch, PLR_EDITING) ||
             PLR_FLAGGED(ch, PLR_MAILING) || PLR_FLAGGED(ch, PLR_SPELL_CREATE) ||
             PLR_FLAGGED(ch, PLR_CUSTOMIZE))
      send_to_char("Not now!\r\n", ch);
    else if (GET_LEVEL(ch) <= GET_LEVEL(vict))
      send_to_char("No, no, no!\r\n", ch);
    else {
      send_to_char(OK, ch);
      send_to_char(buf1, vict);
      snprintf(buf, sizeof(buf), "%s forced %s to %s",
              GET_CHAR_NAME(ch), GET_CHAR_NAME(vict), to_force);
      mudlog(buf, ch, LOG_WIZLOG, TRUE);
      command_interpreter(vict, to_force, GET_CHAR_NAME(ch));
    }
    return;
  }

  if (!str_cmp("room", arg)) {
    send_to_char(OK, ch);
    snprintf(buf, sizeof(buf), "%s forced room %ld to %s",
            GET_CHAR_NAME(ch), ch->in_room->number, to_force);
    mudlog(buf, ch, LOG_WIZLOG, TRUE);

    for (vict = ch->in_room->people; vict; vict = next_force) {
      next_force = vict->next_in_room;
      if (GET_LEVEL(vict) >= GET_LEVEL(ch))
        continue;
      send_to_char(buf1, vict);
      command_interpreter(vict, to_force, GET_CHAR_NAME(ch));
    }
    return;
  }

  /* force all */
  send_to_char(OK, ch);
  snprintf(buf, sizeof(buf), "%s forced all to %s", GET_CHAR_NAME(ch), to_force);
  mudlog(buf, ch, LOG_WIZLOG, TRUE);

  for (i = descriptor_list; i; i = next_desc) {
    next_desc = i->next;

    if (i->connected || !(vict = i->character) || GET_LEVEL(vict) >= GET_LEVEL(ch))
      continue;
    send_to_char(buf1, vict);
    command_interpreter(vict, to_force, GET_CHAR_NAME(ch));
  }
}

ACMD(do_wiztell)
{
  struct descriptor_data *d;

  skip_spaces(&argument);
  delete_doubledollar(argument);

  if (!*argument) {
    send_to_char("Don't bother the gods like that!\r\n", ch);
    return;
  }

  snprintf(buf1, sizeof(buf1), "^c[%s] (wt), \"%s\"\r\n", GET_CHAR_NAME(ch), argument);
  snprintf(buf2, sizeof(buf2), "^c[Someone] (wt), \"%s\"\r\n", argument);

  for (d = descriptor_list; d; d = d->next) {
    if ((!d->connected)
        && d->character
        && access_level(d->character, LVL_BUILDER) && !PLR_FLAGGED(d->character, PLR_WRITING)
        && (d != ch->desc || !(PRF_FLAGGED(d->character, PRF_NOREPEAT)))) {
      if (CAN_SEE(d->character, ch)) {
        send_to_char(buf1, d->character);
        store_message_to_history(d, COMM_CHANNEL_WTELLS, buf1);
      } else {
        send_to_char(buf2, d->character);
        store_message_to_history(d, COMM_CHANNEL_WTELLS, buf2);
      }
    }
  }

  if (PRF_FLAGGED(ch, PRF_NOREPEAT))
    send_to_char(OK, ch);
}

ACMD(do_wizwho)
{
  struct descriptor_data *d;
  struct char_data *tch;
  char line[MAX_INPUT_LENGTH];
  int immos = 0;

  send_to_char("\r\n", ch);

  for (d = descriptor_list; d; d = d->next) {
    if (!d->connected || (d->connected >= CON_SPELL_CREATE && d->connected <= CON_DECK_CREATE)) {
      if (d->original)
        tch = d->original;
      else if (!(tch = d->character))
        continue;
      if (!IS_SENATOR(tch) || !CAN_SEE(ch, tch))
        continue;
      if (GET_INVIS_LEV(d->character) > GET_LEVEL(ch)) /* Added by Washu 2-4-02 fixes invis level bug*/
        continue;
      if (GET_INCOG_LEV(d->character) > GET_LEVEL(ch))
        continue;
      if (!PRF_FLAGGED(tch, PRF_AFK))
        snprintf(line, sizeof(line), " [^c%6.6s^n]  ^b%-30s^n  Room: [^c%8ld^n] Idle: [^c%4d^n]",
                GET_WHOTITLE(tch),
                GET_CHAR_NAME(tch),
                tch->in_room->number, tch->char_specials.timer);
      else
        snprintf(line, sizeof(line), " [^c%6.6s^n]  ^b%-30s^n  Room: [^c%8ld^n] Idle: [^c-AFK^n]",
                GET_WHOTITLE(tch),
                GET_CHAR_NAME(tch), tch->in_room->number);
      if (d->connected)
        snprintf(buf, sizeof(buf), " ^r(Editing)^n\r\n");
      else if (tch == d->original)
        snprintf(buf, sizeof(buf), " ^r(Switched)^n\r\n");
      else if (GET_INVIS_LEV(d->character))
        snprintf(buf, sizeof(buf), " ^m(i%d)^n\r\n", GET_INVIS_LEV(d->character));
      else
        snprintf(buf, sizeof(buf), "\r\n");
      strlcat(line, buf, sizeof(line));
      send_to_char(line, ch);
      immos++;
    }
  }
  send_to_char(ch, "\r\nTotal : %d\r\n", immos);
}

ACMD(do_zreset)
{
  void reset_zone(int zone, int reboot);

  int i;

  one_argument(argument, arg);
  if (!*arg) {
    send_to_char("You must specify a zone.\r\n", ch);
    return;
  }
  if (*arg == '*') {
    if (!access_level(ch, LVL_ADMIN)) {
      send_to_char("You can't reset the entire world.\r\n", ch);
      return;
    }
    for (i = 0; i <= top_of_zone_table; i++)
      reset_zone(i, 0);
    snprintf(buf, sizeof(buf), "%s reset world.", GET_CHAR_NAME(ch));
    mudlog(buf, ch, LOG_WIZLOG, TRUE);
    send_to_char("Reset world.\r\n", ch);
    return;
  } else if (*arg == '.')
    i = ch->in_room->zone;
  else
    i = real_zone(atoi(arg));
  if (i >= 0 && i <= top_of_zone_table) {
    if ((!access_level(ch, LVL_ADMIN)) && (zone_table[i].number != ch->player_specials->saved.zonenum)) {
      send_to_char("You can only zreset zones you've zswitched to first.\r\n", ch);
      return;
    }
    reset_zone(i, 0);
    send_to_char(ch, "Reset zone %d (#%d): %s.\r\n", i, zone_table[i].number, zone_table[i].name);
    mudlog_vfprintf(ch, LOG_WIZLOG, "%s reset zone %d (#%d): %s", GET_CHAR_NAME(ch), i, zone_table[i].number, 
                    zone_table[i].locked_to_non_editors ? "<redacted>" : zone_table[i].name);
  } else
    send_to_char("Invalid zone number.\r\n", ch);
}

ACMD(do_wiztitle)
{
  if (IS_NPC(ch))
    send_to_char("Your title is fine... go away.\r\n", ch);
  else if (PLR_FLAGGED(ch, PLR_NOTITLE))
    send_to_char("You can't title yourself.\r\n", ch);
  else if (subcmd == SCMD_WHOTITLE) {
    skip_spaces(&argument);
    if (strstr((const char *)argument, "^"))
      send_to_char("Whotitles can't contain the ^^ character.\r\n", ch);
    else if (strlen(argument) > MAX_WHOTITLE_LENGTH) {
      send_to_char(ch, "Sorry, whotitles can't be longer than %d characters.\r\n", MAX_WHOTITLE_LENGTH);
    } else {
      set_whotitle(ch, argument);
      send_to_char(ch, "Okay, your whotitle is now %s.\r\n", GET_WHOTITLE(ch));
      snprintf(buf, sizeof(buf), "UPDATE pfiles SET Whotitle='%s' WHERE idnum=%ld;", prepare_quotes(buf2, GET_WHOTITLE(ch), sizeof(buf2) / sizeof(buf2[0])), GET_IDNUM(ch));
      mysql_wrapper(mysql, buf);
    }
  } else if (subcmd == SCMD_PRETITLE) {
    if (GET_TKE(ch) < 100 && GET_LEVEL(ch) < LVL_BUILDER) {
      send_to_char(ch, "You aren't erudite enough to do that.\r\n");
      return;
    }
    skip_spaces(&argument);
    if (GET_LEVEL(ch) < LVL_BUILDER && *argument)
      strlcat(buf, "^n", sizeof(buf));
    if (strstr((const char *)argument, "^l")) {
      send_to_char("Whotitles can't contain pure black.\r\n", ch);
    } else if (strlen(argument) > (MAX_TITLE_LENGTH -2)) {
      send_to_char(ch, "Sorry, pretitles can't be longer than %d characters.\r\n", MAX_TITLE_LENGTH - 2);
    } else {
      set_pretitle(ch, argument);
      snprintf(buf, sizeof(buf), "Okay, you're now %s %s %s.\r\n", GET_PRETITLE(ch), GET_CHAR_NAME(ch), GET_TITLE(ch));
      send_to_char(buf, ch);
      snprintf(buf, sizeof(buf), "UPDATE pfiles SET Pretitle='%s' WHERE idnum=%ld;", prepare_quotes(buf2, GET_PRETITLE(ch), sizeof(buf2) / sizeof(buf2[0])), GET_IDNUM(ch));
      mysql_wrapper(mysql, buf);
    }
  }
}

/*
 *  General fn for wizcommands of the sort: cmd <player>
 */

ACMD(do_wizutil)
{
  struct char_data *vict;
  long result;

  one_argument(argument, arg);
  if (!*arg)
    send_to_char("Yes, but for whom?!?\r\n", ch);
  else if (!(vict = get_char_vis(ch, arg)))
    send_to_char("There is no such player.\r\n", ch);
  else if (IS_NPC(vict))
    send_to_char("You can't do that to a mob!\r\n", ch);
  else if ((GET_LEVEL(vict) >= GET_LEVEL(ch)) && (ch != vict))
    send_to_char("Hmmm...you'd better not.\r\n", ch);
  else {
    switch (subcmd) {
      case SCMD_PARDON:
        if (!PLR_FLAGS(vict).AreAnySet(PLR_KILLER, PLR_WANTED, ENDBIT)) {
          send_to_char("Your victim is not flagged.\r\n", ch);
          return;
        }
        PLR_FLAGS(vict).RemoveBits(PLR_KILLER, PLR_WANTED, ENDBIT);
        send_to_char("Pardoned.\r\n", ch);
        send_to_char("You have been pardoned by the Gods!\r\n", vict);
        snprintf(buf, sizeof(buf), "%s pardoned by %s", GET_CHAR_NAME(vict), GET_CHAR_NAME(ch));
        mudlog(buf, ch, LOG_WIZLOG, TRUE);
        break;
      case SCMD_AUTHORIZE:
        if (!PLR_FLAGS(vict).IsSet(PLR_NOT_YET_AUTHED)) {
          send_to_char("Your victim is already authorized.\r\n", ch);
          return;
        }
        // Check to see if they're in Chargen still.
        if (ch->in_room) {
          for (int counter = 0; counter <= top_of_zone_table; counter++) {
            if ((GET_ROOM_VNUM(vict->in_room) >= (zone_table[counter].number * 100)) &&
                (GET_ROOM_VNUM(vict->in_room) <= (zone_table[counter].top))) {
              if (zone_table[counter].number == 605) {
                send_to_char("They're still in character generation, that would break them!\r\n", ch);
                return;
              }
            }
          }
        }

        PLR_FLAGS(vict).RemoveBit(PLR_NOT_YET_AUTHED);
        send_to_char("Authorized.\r\n", ch);
        send_to_char("Your character has been authorized!\r\n", vict);
        snprintf(buf, sizeof(buf), "%s authorized by %s", GET_CHAR_NAME(vict), GET_CHAR_NAME(ch));
        mudlog(buf, ch, LOG_WIZLOG, TRUE);
        break;
      case SCMD_NOTITLE:
        result = PLR_TOG_CHK(vict, PLR_NOTITLE);
        snprintf(buf, sizeof(buf), "Notitle %s for %s by %s.", ONOFF(result),
                GET_CHAR_NAME(vict), GET_CHAR_NAME(ch));
        mudlog(buf, ch, LOG_WIZLOG, TRUE);
        strlcat(buf, "\r\n", sizeof(buf));
        send_to_char(buf, ch);
        break;
      case SCMD_SQUELCH:
        result = PLR_TOG_CHK(vict, PLR_NOSHOUT);
        snprintf(buf, sizeof(buf), "Squelch %s for %s by %s.", ONOFF(result),
                GET_CHAR_NAME(vict), GET_CHAR_NAME(ch));
        mudlog(buf, ch, LOG_WIZLOG, TRUE);
        strlcat(buf, "\r\n", sizeof(buf));
        send_to_char(buf, ch);
        break;
      case SCMD_SQUELCHOOC:
        result = PLR_TOG_CHK(vict, PLR_NOOOC);
        snprintf(buf, sizeof(buf), "Squelch(OOC) %s for %s by %s.", ONOFF(result),
                GET_CHAR_NAME(vict), GET_CHAR_NAME(ch));
        mudlog(buf, ch, LOG_WIZLOG, TRUE);
        strlcat(buf, "\r\n", sizeof(buf));
        send_to_char(buf, ch);
        break;
      case SCMD_SQUELCHTELLS:
        result = PLR_TOG_CHK(vict, PLR_TELLS_MUTED);
        snprintf(buf, sizeof(buf), "Squelch(tells) %s for %s by %s.", ONOFF(result),
                GET_CHAR_NAME(vict), GET_CHAR_NAME(ch));
        mudlog(buf, ch, LOG_WIZLOG, TRUE);
        strlcat(buf, "\r\n", sizeof(buf));
        send_to_char(buf, ch);
        break;
      case SCMD_RPE:
        result = PLR_TOG_CHK(vict, PLR_RPE);
        snprintf(buf, sizeof(buf), "RPE toggled %s for %s by %s.", ONOFF(result),
                GET_CHAR_NAME(vict), GET_CHAR_NAME(ch));
        if (PLR_FLAGGED(vict, PLR_RPE)) {
          send_to_char(vict, "Congratulations, you are now an RPE. You have access to the RPE channel.\r\n");
          send_to_char(ch, "Your target is an RPE.\r\n");
        } else {
          send_to_char(vict, "You are no longer considered an RPE.\r\n");
          send_to_char(ch, "Your target is no longer an RPE.\r\n");
        }
        mudlog(buf, ch, LOG_WIZLOG, TRUE);
        break;

      case SCMD_FREEZE:
        FAILURE_CASE(ch == vict, "Oh, yeah, THAT'S real smart...");
        FAILURE_CASE(PLR_FLAGGED(vict, PLR_FROZEN), "Your victim is already pretty cold. Did you mean to ##^WTHAW^n them?");

        PLR_FLAGS(vict).SetBit(PLR_FROZEN);
        GET_FREEZE_LEV(vict) = GET_LEVEL(ch);

        send_to_char("A bitter wind suddenly rises and drains every erg of heat from your body!\r\nYou feel frozen!\r\n", vict);
        act("A sudden cold wind conjured from nowhere freezes $n!", TRUE, vict, 0, 0, TO_ROOM);
        mudlog_vfprintf(ch, LOG_WIZLOG, "%s frozen by %s.", GET_CHAR_NAME(vict), GET_CHAR_NAME(ch));
        send_to_char("Done. Note that freezing means that the target cannot execute ANY commands, including osay/tell and even quit! ##^WTHAW^n them if you're still talking, otherwise disconnect them with ##^WDC^n when you're done.\r\n", ch);
        break;
      case SCMD_THAW:
        FAILURE_CASE(!PLR_FLAGGED(vict, PLR_FROZEN), "Sorry, your victim is not morbidly encased in ice at the moment.");
        FAILURE_CASE_PRINTF(!access_level(ch, GET_FREEZE_LEV(vict)), "Sorry, a level %d staffer froze %s... you can't unfreeze %s.", GET_FREEZE_LEV(vict), GET_CHAR_NAME(vict), HMHR(vict));

        PLR_FLAGS(vict).RemoveBit(PLR_FROZEN);
        GET_FREEZE_LEV(vict) = 0;

        send_to_char("A fireball suddenly explodes in front of you, melting the ice!\r\nYou feel thawed.\r\n", vict);
        act("A sudden fireball conjured from nowhere thaws $n!", TRUE, vict, 0, 0, TO_ROOM);
        mudlog_vfprintf(ch, LOG_WIZLOG, "%s un-frozen by %s.", GET_CHAR_NAME(vict), GET_CHAR_NAME(ch));
        send_to_char("Thawed.\r\n", ch);
        break;
      case SCMD_MUTE_NEWBIE:
        result = PLR_TOG_CHK(vict, PLR_QUESTIONS_MUTED);
        snprintf(buf, sizeof(buf), "Questions muted %s for %s by %s.", ONOFF(result),
                GET_CHAR_NAME(vict), GET_CHAR_NAME(ch));
        mudlog(buf, ch, LOG_WIZLOG, TRUE);
        strlcat(buf, "\r\n", sizeof(buf));
        send_to_char(buf, ch);
        break;
      default:
        log("SYSERR: Unknown subcmd passed to do_wizutil (act.wizard.c)");
        break;
    }
    playerDB.SaveChar(vict);
  }
}


/* single zone printing fn used by "show zone" so it's not repeated in the
   code 3 times ... -je, 4/6/93 */

extern const char *render_zone_editor_ids(struct zone_data *zon, char *string_to_write_to, size_t str_sz);
void print_zone_to_buf(char *bufptr, int buf_size, int zone, int detailed, struct char_data *ch)
{
  int i, color = 0;

  if (!ch_can_bypass_edit_lock(ch, &zone_table[zone])) {
    snprintf(bufptr, buf_size - strlen(bufptr), "%4d %-30.30s^n %s%s (<redacted>) Age: ???; Res: ??? (?); Top: %6d; Sec: ??\r\n",
             zone_table[zone].number,
             "<redacted>",
             zone_table[zone].editing_restricted_to_admin ? "L" : " ",
             zone_table[zone].approved ? "A" : " ",
             zone_table[zone].top);
    return;
  }

  for (i = 0; zone_table[zone].name[i]; i++)
    if (zone_table[zone].name[i] == '^' && (i < 1 || zone_table[zone].name[i-1] != '^') &&
        zone_table[zone].name[i+1]) {
      switch (LOWER(zone_table[zone].name[i+1])) {
      case 'b':
      case 'c':
      case 'g':
      case 'l':
      case 'm':
      case 'n':
      case 'r':
      case 'w':
      case 'y':
        color += 2;
        break;
      case '^':
        color++;
        break;
      }
    }

  if (detailed == 0 || detailed == 2) {
    snprintf(bufptr, buf_size - strlen(bufptr), "^c%5d^n %-40.40s^n ", zone_table[zone].number,
            zone_table[zone].name);
    for (i = 0; i < color; i++)
      strlcat(bufptr, " ", buf_size);

    if (detailed == 2) {
      // Completed zones. We want info on the editors and the name of the first room.
      char editor_writeout[1000];
      rnum_t first_room_rnum = real_room(zone_table[zone].number * 100);
      snprintf(ENDOF(bufptr), buf_size - strlen(bufptr), "(%s): %s^n\r\n",
               render_zone_editor_ids(&zone_table[zone], editor_writeout, sizeof(editor_writeout)),
               first_room_rnum >= 0 ? GET_ROOM_NAME(&world[first_room_rnum]) : "<no room at 00>");
    } else {
      // All zones.
      const char *sec_color = "^n";
      if (zone_table[zone].security >= 10)
        sec_color = "^r";
      else if (zone_table[zone].security >= 7)
        sec_color = "^y";

      snprintf(ENDOF(bufptr), buf_size - strlen(bufptr), "%s%s (%-3.3s) Age: %3d; Res: %3d (%1d); Top: ^c%6d^n; Sec: %s%2d^n\r\n",
            zone_table[zone].editing_restricted_to_admin ? "L" : " ",
            zone_table[zone].approved ? "A" : " ",
            jurisdictions[zone_table[zone].jurisdiction],
            zone_table[zone].age, zone_table[zone].lifespan,
            zone_table[zone].reset_mode, zone_table[zone].top,
            sec_color, zone_table[zone].security);
    }
  } else if (detailed == 1) {
    int rooms = 0, objs = 0, mobs = 0, shops = 0, vehs = 0;
    for (i = 0; i <= top_of_world && world[i].number <= zone_table[zone].top; i++)
      if (world[i].number >= (zone_table[zone].number * 100))
        rooms++;
    for (i = 0; i <= top_of_mobt && MOB_VNUM_RNUM(i) <= zone_table[zone].top; i++)
      if (MOB_VNUM_RNUM(i) >= (zone_table[zone].number * 100))
        mobs++;
    for (i = 0; i <= top_of_objt && OBJ_VNUM_RNUM(i) <= zone_table[zone].top; i++)
      if (OBJ_VNUM_RNUM(i) >= (zone_table[zone].number * 100))
        objs++;
    for (i = 0; i <= top_of_shopt && shop_table[i].vnum <= zone_table[zone].top; i++)
      if (shop_table[i].vnum >= (zone_table[zone].number * 100))
        shops++;
    for (i = 0; i <= top_of_veht && VEH_VNUM_RNUM(i) <= zone_table[zone].top; i++)
      if (VEH_VNUM_RNUM(i) >= (zone_table[zone].number * 100))
        vehs++;

    snprintf(bufptr, buf_size, "Zone %d (%d): %s^n\r\n"
            "Age: %d, Commands: %d, Reset: %d (%d), Top: %d\r\n"
            "Rooms: %d, Mobiles: %d, Objects: %d, Shops: %d, Vehicles: %d\r\n"
            "Security: %d, Status: %s%s^n\r\nJurisdiction: %s^n, Editors: <not implemented>",
            zone_table[zone].number, zone, zone_table[zone].name,
            zone_table[zone].age, zone_table[zone].num_cmds,
            zone_table[zone].lifespan, zone_table[zone].reset_mode,
            zone_table[zone].top, rooms, mobs, objs, shops, vehs,
            zone_table[zone].security,
            zone_table[zone].editing_restricted_to_admin ? "Editing Locked" : "",
            zone_table[zone].approved ? ", Approved and Connected" : " ",
            jurisdictions[zone_table[zone].jurisdiction]);
/* FIXCHE   for (i = 0; i < NUM_ZONE_EDITOR_IDS; i++) {
      const char *name = playerDB.GetNameV(zone_table[zone].editor_ids[i]);

      if (name) {
        if (first)
          first = 0;
        else
          strlcat(bufptr, ", ");
        strlcat(bufptr, CAP(name));
      }
    }
    if (first)
      strlcat(bufptr, "None.\r\n");
    else
      strlcat(bufptr, ".\r\n");
*/
  }
}

SPECIAL(call_elevator);
bool room_has_any_exits(struct room_data *room) {
  for (int dir = 0; dir <= DOWN; dir++) {
    if (room->dir_option[dir] && room->dir_option[dir]->to_room) {
      return TRUE;
    }
  }

  // Can you call an elevator from here?
  if (room->func == call_elevator)
    return TRUE;

  // Is this an elevator car?
  if (ROOM_FLAGGED(room, ROOM_INDOORS))
    for (int j = 0; j < num_elevators; j++)
      if (room->number == elevator[j].room)
        return TRUE;

  // Is this a taxicab?
  if (room_is_a_taxicab(GET_ROOM_VNUM(room)))
    return TRUE;

  return FALSE;
}

#define MAXBUF 128
ACMD(do_show)
{
  if (GET_LEVEL(ch) < LVL_BUILDER) {
    ACMD_DECLARE(do_mort_show);
    do_mort_show(ch, argument, 0, 0);
    return;
  }
  int i, j, k, l, v, con, last = -1;
  char self = 0;
  struct char_data *vict;
  struct spell_data *temp;
  char field[40], value[40], birth[80], buf[MAX_STRING_LENGTH*4];
  extern int buf_switches, buf_largecount, buf_overflows;
  void show_shops(struct char_data * ch, char *value);
  void hcontrol_list_houses(struct char_data *ch);
  SPECIAL(call_elevator);

  int total_stats;
  bool printed;

  struct show_struct {
    const char *cmd;
    char level;
  }
  fields[] = {
               { "nothing",         0  },/* 0 */
               { "zones",           LVL_BUILDER },
               { "player",          LVL_BUILDER },
               { "rent",            LVL_BUILDER },
               { "stats",           LVL_ADMIN },
               { "errors",          LVL_ADMIN },
               { "deathrooms",      LVL_ADMIN },
               { "godrooms",        LVL_ADMIN },
               { "skills",          LVL_BUILDER },
               { "spells",          LVL_BUILDER },
               { "prompt",          LVL_CONSPIRATOR },
               { "lodges",          LVL_ADMIN },
               { "library",         LVL_ADMIN },
               { "jackpoints",      LVL_BUILDER },
               { "abilities",       LVL_BUILDER },
               { "aliases",         LVL_CONSPIRATOR },
               { "metamagic",       LVL_BUILDER },
               { "noexits",         LVL_BUILDER },
               { "traps",           LVL_BUILDER },
               { "ammo",            LVL_CONSPIRATOR },
               { "storage",         LVL_BUILDER },
               { "anomalies",       LVL_BUILDER },
               { "roomflag",        LVL_BUILDER },
               { "markets",         LVL_VICEPRES },
               { "weight",          LVL_PRESIDENT },
               { "ignore",          LVL_EXECUTIVE },
               { "vehicles",        LVL_ADMIN },
               { "dirtyelves",      LVL_BUILDER },
               { "wareshops",       LVL_CONSPIRATOR },
               { "smartshops",      LVL_CONSPIRATOR },
               { "powersites",      LVL_VICEPRES },
               { "fuckykeys",       LVL_ADMIN },
               { "strongboylifts",  LVL_ADMIN },
               { "longasszones",    LVL_ADMIN },
               { "extramagical",    LVL_ADMIN },
               { "tempdescs",       LVL_BUILDER },
               { "sameroomandlook", LVL_BUILDER },
               { "extraflag",       LVL_BUILDER },
               { "affflag",         LVL_BUILDER },
               { "mobflag",         LVL_BUILDER },
               { "finishedzones",   LVL_BUILDER },
               { "\n", 0 }
             };

  skip_spaces(&argument);

  if (!*argument) {
    strlcpy(buf, "Show options:\r\n", sizeof(buf));
    for (j = 0, i = 1; fields[i].level; i++)
      if (access_level(ch, fields[i].level))
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%-15s%s", fields[i].cmd, (!(++j % 5) ? "\r\n" : ""));
    strlcat(buf, "\r\n", sizeof(buf));
    send_to_char(buf, ch);
    return;
  }

  strlcpy(arg, two_arguments(argument, field, value), sizeof(arg));

  for (l = 0; *(fields[l].cmd) != '\n'; l++)
    if (!strncmp(field, fields[l].cmd, strlen(field)))
      break;

  if (!access_level(ch, fields[l].level)) {
    send_to_char("You are not erudite enough for that!\r\n", ch);
    return;
  }
  if (!strcmp(value, "."))
    self = 1;
  buf[0] = '\0';
  switch (l) {
      // Did someone seriously write a list with magic-number indexes and use that as a key for which command we're using? WTF -LS
  case 1:                     /* zone */
    /* tightened up by JE 4/6/93 */
    if (self) {
      if (access_level(ch, LVL_ADMIN))
        print_zone_to_buf(buf, sizeof(buf), ch->in_room->zone, 1, ch);
      else
        print_zone_to_buf(buf, sizeof(buf), ch->in_room->zone, 0, ch);
    }

    else if (*value && is_number(value)) {
      for (j = atoi(value), i = 0; i <= top_of_zone_table && zone_table[i].number != j; i++)
        ;
      if (i <= top_of_zone_table) {
        if (access_level(ch, LVL_ADMIN))
          print_zone_to_buf(buf, sizeof(buf), i, 1, ch);
        else
          print_zone_to_buf(buf, sizeof(buf), i, 0, ch);
      } else {
        send_to_char("That is not a valid zone.\r\n", ch);
        return;
      }
    }

    else {
      int last_seen_top = 0;
      for (i = 0; i <= top_of_zone_table; i++) {
        int bottom = zone_table[i].number * 100;
        if (GET_LEVEL(ch) >= LVL_PRESIDENT && bottom > last_seen_top + 1) {
          int start_vnum = last_seen_top + 1;
          int end_vnum = bottom - 1;
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " ^y(gap: ^Y%4d^y rooms between %d - %d)^n\r\n", end_vnum - start_vnum, start_vnum, end_vnum);
        }
        print_zone_to_buf(ENDOF(buf), sizeof(buf) - strlen(buf), i, 0, ch);
        last_seen_top = zone_table[i].top;
      }
    }
    page_string(ch->desc, buf, 1);
    break;
  case 2:                     /* player */
    if (!*value) {
      send_to_char("A name would help.\r\n", ch);
      return;
    }

    {
      if (!(vict = get_char_vis(ch, value))) {
        send_to_char(ch, "You can't see anyone named '%s'.\r\n", value);
        return;
      }
      if (!access_level(ch, LVL_ADMIN) &&
          GET_LEVEL(ch) <= GET_LEVEL(vict)) {
        send_to_char("You're not erudite enough to do that!\r\n", ch);
        return;
      }
      mudlog_vfprintf(ch, LOG_WIZLOG, "Viewed %s (%ld)'s info.", GET_CHAR_NAME(vict), GET_IDNUM(vict));
      snprintf(buf, sizeof(buf), "Player: %-12s (%s) [%2d]\r\n", GET_NAME(vict),
              genders[(int) GET_PRONOUNS(vict)], GET_LEVEL(vict));
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Y: %-8ld  Bal: %-8ld  Karma: %-8d\r\n",
              GET_NUYEN(vict), GET_BANK(vict), GET_KARMA(vict));
      strlcpy(birth, ctime(&vict->player.time.birth), sizeof(birth));
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Started: %-20.16s  Last: %-20.16s  Played: %3dh %2dm\r\n",
              birth, ctime(&vict->player.time.lastdisc),
              (int) (vict->player.time.played / 3600),
              (int) (vict->player.time.played / 60 % 60));
      send_to_char(buf, ch);
    }

    break;
  case 3:
    send_to_char("Command not implemented.\r\n", ch);
    break;
  case 4:
    i = 0;
    j = 0;
    k = 0;
    v = 0;
    con = 0;
    for (vict = character_list; vict; vict = vict->next_in_character_list) {
      if (IS_NPC(vict))
        j++;
      else if (CAN_SEE(ch, vict)) {
        i++;
        if (vict->desc)
          con++;
      }
    }
    for (struct veh_data *veh = veh_list; veh; veh = veh->next)
      v++;

    snprintf(buf, sizeof(buf), "Current stats:\r\n");
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  Players in game: ^C%-5d^n  Connected: ^g%-5d^n\r\n", i, con);
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  Registered players: ^c%-5d^n\r\n", playerDB.NumPlayers());
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  Mobiles: ^c%-5d^n          Prototypes: ^y%-5ld^n Available: ^L%-5ld^n\r\n",
            j, top_of_mobt + 1, top_of_mob_array - top_of_mobt + 1);
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  Objects: ^c%-5d^n          Prototypes: ^y%-5ld^n Available: ^L%-5ld^n\r\n",
            ObjList.NumItems(), top_of_objt + 1, top_of_obj_array - top_of_objt + 1);
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  Vehicles: ^c%-5d^n         Prototypes: ^y%-5d^n\r\n", v, top_of_veht + 1);
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  Rooms:   ^c%-5ld^n          Available: ^L%-5ld^n\r\n  Zones: %-5ld\r\n",
            top_of_world + 1, top_of_world_array - top_of_world + 1,
            top_of_zone_table + 1);
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  World chunk: ^c%-5d^n      Mob chunk: ^c%-5d^n  Obj chunk: ^c%-5d^n\r\n",
            world_chunk_size, mob_chunk_size, obj_chunk_size);
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  Large bufs: ^c%-5d^n       Buf switches: ^C%-5d^n Overflows: ^r%-5d^n\r\n",
            buf_largecount, buf_switches, buf_overflows);
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  OLC is %s\r\n",
            (olc_state ? "^GAvailable.^n" : "^RUnavailable.^n"));
    send_to_char(buf, ch);
    break;
  case 5:
    strlcpy(buf, "Errant Rooms\r\n------------\r\n", sizeof(buf));
    for (i = 0, k = 0; i <= top_of_world; i++)
      for (j = 0; j < NUM_OF_DIRS; j++)
        if (world[i].dir_option[j] && !world[i].dir_option[j]->to_room && i != last) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%2d: [%8ld] %s\r\n", ++k, world[i].number, world[i].name);
          last = i;
        }
    send_to_char(buf, ch);
    break;
  case 6:
    strlcpy(buf, "Death Traps\r\n-----------\r\n", sizeof(buf));
    for (i = 0, j = 0; i <= top_of_world; i++)
      if (ROOM_FLAGGED(&world[i], ROOM_DEATH))
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%2d: [%8ld] %s %s\r\n", ++j,
                world[i].number,
                vnum_from_non_approved_zone(world[i].number) ? " " : (PRF_FLAGGED(ch, PRF_SCREENREADER) ? "(approved)" : "*"),
                world[i].name);
    send_to_char(buf, ch);
    break;
  case 7:
#define GOD_ROOMS_ZONE 0

    strlcpy(buf, "Godrooms\r\n--------------------------\r\n", sizeof(buf));
    for (i = 0, j = 0; i <= zone_table[real_zone(GOD_ROOMS_ZONE)].top; i++)
      if (world[i].zone == GOD_ROOMS_ZONE && i > 1 && !(i >= 8 && i <= 12))
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%2d: [%8ld] %s %s\r\n", j++, world[i].number,
                vnum_from_non_approved_zone(world[i].number) ? " " : (PRF_FLAGGED(ch, PRF_SCREENREADER) ? "(approved)" : "*"),
                world[i].name);
    send_to_char(buf, ch);
    break;
  case 8:
    if (!*value) {
      send_to_char("A name would help.\r\n", ch);
      return;
    }
    if (!(vict = get_char_vis(ch, value))) {
      send_to_char(ch, "You can't see anyone named '%s'.\r\n", value);
      return;
    }

    mudlog_vfprintf(ch, LOG_WIZLOG, "Viewed %s (%ld)'s skills.", GET_CHAR_NAME(vict), GET_IDNUM(vict));
    send_to_char(ch, "%s's skills:", GET_NAME(vict));
    j = 0;
    snprintf(buf, sizeof(buf), "\r\n");
    for (i = MIN_SKILLS; i < MAX_SKILLS; i++)
      if (GET_SKILL(vict, i) > 0) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "[%-20s%4d]", skills[i].name, GET_SKILL(vict, i));
        j++;
        if (!(j % 3))
          strlcat(buf, "\r\n", sizeof(buf));
      }
    send_to_char(buf, ch);
    send_to_char("\r\n", ch);
    break;
  case 9:
    if (!*value) {
      send_to_char("A name would help.\r\n", ch);
      return;
    }
    if (!(vict = get_char_vis(ch, value))) {
      send_to_char(ch, "You can't see anyone named '%s'.\r\n", value);
      return;
    }
    if (!GET_SPELLS(vict)) {
      send_to_char(ch, "%s does not know any spells.\r\n", GET_NAME(vict));
      return;
    }
    mudlog_vfprintf(ch, LOG_WIZLOG, "Viewed %s (%ld)'s spells.", GET_CHAR_NAME(vict), GET_IDNUM(vict));
    send_to_char(ch, "%s's spells:\r\n", GET_NAME(vict));
    i = 0;
    *buf = '\0';
    for (temp = GET_SPELLS(vict); temp; temp = temp->next) {
      i += strlen(temp->name) + 4 + (int)(temp->force / 10);
      if (i < 79) {
        if (temp->next)
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s (%d), ", temp->name, temp->force);
        else
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s (%d).\r\n", temp->name, temp->force);
      } else {
        send_to_char(ch, "%s\r\n", buf);
        if (temp->next)
          snprintf(buf, sizeof(buf), "%s (%d), ", temp->name, temp->force);
        else
          snprintf(buf, sizeof(buf), "%s (%d).\r\n", temp->name, temp->force);
        i = strlen(buf);
      }
    }
    send_to_char(buf, ch);
    break;
  case 10:
    if (!*value) {
      send_to_char("A name would help.\r\n", ch);
      return;
    }
    if (!(vict = get_char_vis(ch, value))) {
      send_to_char(ch, "You can't see anyone named '%s'.\r\n", value);
      return;
    }
    send_to_char(ch, "%s's prompt:\r\n%s\r\n", GET_NAME(vict), GET_PROMPT(vict));
    break;
  case 13:
    {
      strlcpy(buf, "Jackpoints\r\n---------\r\n", sizeof(buf));

      char io_color[3];
      for (i = 0, j = 0; i <= top_of_world; i++) {
        if (world[i].matrix > 0 && ch_can_bypass_edit_lock(ch, &world[i])) {
          if (world[i].io > 0) {
            if (world[i].io < 50)
              strlcpy(io_color, "^R", sizeof(io_color));
            else if (world[i].io < 150)
              strlcpy(io_color, "^Y", sizeof(io_color));
            else if (world[i].io < 300)
              strlcpy(io_color, "^y", sizeof(io_color));
            else
              strlcpy(io_color, "^c", sizeof(io_color));
          } else if (world[i].io == -1) {
            strlcpy(io_color, "^m", sizeof(io_color));
          } else {
            strlcpy(io_color, "", sizeof(io_color));
          }
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%2d: [%8ld] %s^n (host %ld, rtg %ld, %sI/O %d^n)\r\n", ++j,
                  world[i].number, world[i].name, world[i].matrix, world[i].rtg, io_color, world[i].io);
        }
      }

      send_to_char(buf, ch);
    }

    break;
  case 14:
    if (!*value) {
      send_to_char("A name would help.\r\n", ch);
      return;
    }
    if (!(vict = get_char_vis(ch, value))) {
      send_to_char(ch, "You can't see anyone named '%s'.\r\n", value);
      return;
    }

    mudlog_vfprintf(ch, LOG_WIZLOG, "Viewed %s (%ld)'s abilities.", GET_CHAR_NAME(vict), GET_IDNUM(vict));
    send_to_char(ch, "%s's abilities:\r\n", GET_CHAR_NAME(vict));
    render_targets_abilities_to_viewer(ch, vict);
    break;
  case 15:
    if (!*value) {
      send_to_char("A name would help.\r\n", ch);
      return;
    }
    if (!(vict = get_char_vis(ch, value))) {
      send_to_char(ch, "You can't see anyone named '%s'.\r\n", value);
      return;
    }
    struct alias *a;
    mudlog_vfprintf(ch, LOG_WIZLOG, "Viewed %s (%ld)'s aliases.", GET_CHAR_NAME(vict), GET_IDNUM(vict));
    send_to_char(ch, "%s's defined aliases:\r\n", GET_CHAR_NAME(vict));
    if ((a = GET_ALIASES(vict)) == NULL)
      send_to_char(" None.\r\n", ch);
    else
      while (a != NULL) {
        send_to_char(ch, "%-15s %s\r\n", a->command, a->replacement);
        a = a->next;
      }
    break;
  case 16:
    if (!*value) {
      send_to_char("A name would help.\r\n", ch);
      return;
    }
    if (!(vict = get_char_vis(ch, value))) {
      send_to_char(ch, "You can't see anyone named '%s'.\r\n", value);
      return;
    }
    mudlog_vfprintf(ch, LOG_WIZLOG, "Viewed %s (%ld)'s metamagics.", GET_CHAR_NAME(vict), GET_IDNUM(vict));
    send_to_char(ch, "%s's metamagic abilities:\r\n", GET_NAME(vict));
    j = 0;
    snprintf(buf, sizeof(buf), "\r\n");
    for (i = 0; i < META_MAX; i++)
      if (GET_METAMAGIC(vict, i)) {
        if (i == META_CENTERING) {
          send_to_char(ch, "  %s%s^n (learned %d/%d)^n\r\n",
                   GET_METAMAGIC(vict, i) == 2 ? "" : "^r",
                   metamagic[i],
                   GET_METAMAGIC(ch, i) / METAMAGIC_STAGE_LEARNED,
                   (GET_METAMAGIC(ch, i) / METAMAGIC_STAGE_LEARNED) + GET_METAMAGIC(ch, i) % METAMAGIC_STAGE_LEARNED);
        }
        send_to_char(ch, "  %s%s^n\r\n", GET_METAMAGIC(vict, i) == 2 ? "" : "^r", metamagic[i]);
      }
    send_to_char("\r\n", ch);
    break;
  case 17:
    strlcpy(buf, "Exitless Rooms\r\n-----------\r\n", sizeof(buf));
    for (i = 0, j = 0; i <= top_of_world; i++) {
      // Don't need to hear about the imm zones.
      if (!room_has_any_exits(&world[i])) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%4d: [%6ld] %s %s^n\r\n", ++j,
                world[i].number,
                vnum_from_non_approved_zone(world[i].number) ? " " : (PRF_FLAGGED(ch, PRF_SCREENREADER) ? "(approved)" : "*"),
                world[i].name);
      }
    }
    send_to_char(buf, ch);
    break;
  case 18:
    strlcpy(buf, "Trap Rooms\r\n-----------\r\n", sizeof(buf));
    int dir;
    for (i = 0, j = 0; i <= top_of_world; i++) {
      for (dir = 0; dir <= DOWN; dir++) {
        if (world[i].dir_option[dir] && world[i].dir_option[dir]->to_room) {
          if (!room_has_any_exits(world[i].dir_option[dir]->to_room)) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%4d: [%6ld] %s %s^n: %s^n\r\n", ++j,
                    world[i].number,
                    vnum_from_non_approved_zone(world[i].number) ? " " : (PRF_FLAGGED(ch, PRF_SCREENREADER) ? "(approved)" : "*"),
                    world[i].name,
                    dirs[dir]);
            break;
          }
        }
      }
    }
    send_to_char(buf, ch);
    break;
  case 19: /* show ammo */
    if (!*value) {
      send_to_char("A name would help.\r\n", ch);
      return;
    }
    if (!(vict = get_char_vis(ch, value))) {
      send_to_char(ch, "You can't see anyone named '%s'.\r\n", value);
      return;


    }
    mudlog_vfprintf(ch, LOG_WIZLOG, "Viewed %s (%ld)'s pockets.", GET_CHAR_NAME(vict), GET_IDNUM(vict));
    display_pockets_to_char(ch, vict);
    break;
  case 20:
    strlcpy(buf, "Storage Rooms\r\n-----------\r\n", sizeof(buf));
    for (i = 0, j = 0; i <= top_of_world; i++)
      if (ROOM_FLAGGED(&world[i], ROOM_STORAGE))
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%4d: [%8ld] %s %s^n\r\n", ++j,
                world[i].number,
                vnum_from_non_approved_zone(world[i].number) ? " " : (PRF_FLAGGED(ch, PRF_SCREENREADER) ? "(approved)" : "*"),
                world[i].name);
    send_to_char(buf, ch);
    break;
  case 21:
    send_to_char("Anomalous Rooms -----------\r\n", ch);
    for (i = 0, j = 0; i <= top_of_world; i++) {
      // Check for hidden rating higher than 10 (or whatever threshold is configured as).
      for (k = 0; k < NUM_OF_DIRS; k++) {
        if (world[i].dir_option[k] && world[i].dir_option[k]->hidden > ANOMALOUS_HIDDEN_RATING_THRESHOLD) {
          send_to_char(ch, "%4d: [%8ld] %s %s^n: %s exit has hidden rating %d > %d\r\n", ++j,
                  world[i].number,
                  vnum_from_non_approved_zone(world[i].number) ? " " : (PRF_FLAGGED(ch, PRF_SCREENREADER) ? "(approved)" : "*"),
                  world[i].name,
                  dirs[k],
                  world[i].dir_option[k]->hidden,
                  ANOMALOUS_HIDDEN_RATING_THRESHOLD);
        }
      }
    }
    if (j == 0)
      send_to_char("...None.\r\n", ch);

    send_to_char("\r\n\r\nAnomalous Mobs -----------\r\n", ch);
    for (i = 0, j = 0; i <= top_of_mobt; i++) {
      printed = FALSE;

      // Skip non-connected areas or known staff areas.
      if (vnum_from_non_approved_zone(GET_MOB_VNUM(&mob_proto[i]))
          || GET_MOB_VNUM(&mob_proto[i]) < 100
          || (GET_MOB_VNUM(&mob_proto[i]) >= 1000 && GET_MOB_VNUM(&mob_proto[i]) <= 1099)
          || (GET_MOB_VNUM(&mob_proto[i]) >= 10000 && GET_MOB_VNUM(&mob_proto[i]) <= 10099))
        continue;

      // Check stats, etc
      total_stats = 0;
      for (k = BOD; k <= REA; k++)
        total_stats += GET_REAL_ATT(&mob_proto[i], k);

      snprintf(buf, sizeof(buf), "%4d: [%8ld] %s %s^n", ++j,
               GET_MOB_VNUM(&mob_proto[i]),
               vnum_from_non_approved_zone(GET_MOB_VNUM(&mob_proto[i])) ? " " : (PRF_FLAGGED(ch, PRF_SCREENREADER) ? "(approved)" : "*"),
               GET_CHAR_NAME(&mob_proto[i]));

      // Flag mobs with crazy stats
      if (total_stats > ANOMALOUS_TOTAL_STATS_THRESHOLD) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " has ^ctotal attributes %d > %d^n",
                 total_stats,
                 ANOMALOUS_TOTAL_STATS_THRESHOLD);
        printed = TRUE;
      }

      // Flag mobs with no stats
      if (total_stats == 0) {
        strlcat(buf, " has not had its attributes set yet", sizeof(buf));
        printed = TRUE;
      }

      // Flag mobs with crazy skills
      for (k = MIN_SKILLS; k < MAX_SKILLS; k++)
        if (GET_SKILL(&mob_proto[i], k) > ANOMALOUS_SKILL_THRESHOLD) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s ^c%s %d > %d^n",
                   printed ? ";" : " has",
                   skills[k].name,
                   GET_SKILL(&mob_proto[i], k),
                   ANOMALOUS_SKILL_THRESHOLD);
          printed = TRUE;
        }

      if (GET_RACE(&mob_proto[i]) <= RACE_UNDEFINED || GET_RACE(&mob_proto[i]) >= NUM_RACES) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s ^cundefined or unknown race^n", printed ? ";" : " has");
        printed = TRUE;
      }

      if ((GET_RACE(&mob_proto[i]) != RACE_SPIRIT && keyword_appears_in_char("spirit", &mob_proto[i]))
          || (GET_RACE(&mob_proto[i]) != RACE_ELEMENTAL && keyword_appears_in_char("elemental", &mob_proto[i])))
      {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s spirit or elemental keyword with mismatched race", printed ? ";" : " has");
        printed = TRUE;
      }

      if (printed)
        send_to_char(ch, "%s\r\n", buf);
    }
    if (j == 0)
      send_to_char("...None.\r\n", ch);

    /*
    send_to_char("\r\n\r\nAnomalous Objects -----------\r\n", ch);
    for (i = 0, j = 0; i <= top_of_objt; i++) {
      // Check stats, etc
    }
    if (j == 0)
      send_to_char("...None.\r\n", ch);
    */
    break;
  case 22:
    if (*value) {
      for (i = 0; i < ROOM_MAX; i++) {
        if (str_str(room_bits[i], value)) {
          send_to_char(ch, "Rooms with flag %s set:\r\n", room_bits[i]);
          for (k = 0, j = 0; k <= top_of_world; k++)
            if (ROOM_FLAGGED(&world[k], i) && ch_can_bypass_edit_lock(ch, &world[k]))
              send_to_char(ch, "%4d: [%8ld] %s %s\r\n", ++j,
                           world[k].number,
                           vnum_from_non_approved_zone(world[k].number) ? " " : (PRF_FLAGGED(ch, PRF_SCREENREADER) ? "(approved)" : "*"),
                           world[k].name);
          return;
        }
      }
    }

    send_to_char("Please specify a single roomflag from the following list:\r\n", ch);
    for (i = 0; i < ROOM_MAX; i++)
      send_to_char(ch, "  %s\r\n", room_bits[i]);
    return;
  case 23:
    send_to_char(ch, "Current paydata market values: B-%d, G-%d, O-%d, R-%d, L-%d.\r\n", market[0], market[1], market[2], market[3], market[4]);
    return;
  case 24:
    send_to_char("\r\n\r\nAnomalous Objects (weight 5.00) -----------\r\n", ch);
    for (i = 0, j = 0; i <= top_of_objt; i++) {
      if (GET_OBJ_WEIGHT(&obj_proto[i]) == 5 || GET_OBJ_WEIGHT(&obj_proto[i]) == 5.0) {
        send_to_char(ch, "%4d) [%8ld] %s %s^n\r\n",
                     j++,
                     GET_OBJ_VNUM(&obj_proto[i]),
                     vnum_from_non_approved_zone(GET_OBJ_VNUM(&obj_proto[i])) ? " " : (PRF_FLAGGED(ch, PRF_SCREENREADER) ? "(approved)" : "*"),
                     GET_OBJ_NAME(&obj_proto[i]));
      }
    }
    if (j == 0)
      send_to_char("...None.\r\n", ch);
    return;
  case 25:
    if (!*value) {
      send_to_char("A name would help.\r\n", ch);
      return;
    }
    if (!(vict = get_char_vis(ch, value))) {
      send_to_char(ch, "You can't see anyone named '%s'.\r\n", value);
      return;
    }
    mudlog_vfprintf(ch, LOG_WIZLOG, "Viewed %s (%ld)'s ignore entries.", GET_CHAR_NAME(vict), GET_IDNUM(vict));
    display_characters_ignore_entries(ch, vict);
    return;
  case 26:
    {
      idnum_t idnum;
      if (!*value) {
        idnum = -1;
        send_to_char("All player-owned vehicles in game:\r\n", ch);
      } else {
        if ((idnum = get_player_id(value)) <= 0) {
          send_to_char(ch, "'%s' is not a character name.\r\n", value);
          return;
        } else {
          send_to_char(ch, "Vehicles owned by %s:\r\n", value);
        }
      }

      int total_crap = 0;
      for (struct veh_data *tmp = veh_list; tmp; tmp = tmp->next) {
        if (tmp->owner == idnum || (tmp->owner > 0 && idnum == -1)) {
          // Count crap.
          int crap_count = tmp->contents ? count_objects(tmp->contents) : 0;
          total_crap += crap_count;

          // Display.
          struct room_data *in_room = get_veh_in_room(tmp);
          const char *owner_name = get_player_name(tmp->owner);
          send_to_char(ch, "%-30.30s^n  ", get_string_after_color_code_removal(GET_VEH_NAME(tmp), NULL));
          send_to_char(ch, "%30.30s^n (%6ld)  %3d item%s, owned by %s^n\r\n",
                       in_room ? get_string_after_color_code_removal(GET_ROOM_NAME(in_room), NULL) : "towed?",
                       in_room ? GET_ROOM_VNUM(in_room) : -1,
                       crap_count,
                       crap_count == 1 ? "" : "s",
                       owner_name);
          delete [] owner_name;
        }
      }
      send_to_char(ch, "Total crap: %d\r\n", total_crap);
    }
    return;
  case 27:
    send_to_char("The following mobs have 'elven' in their name or keywords w/o 'elf' as well:\r\n", ch);
    for (int idx = 0; idx <= top_of_mobt; idx++) {
      if (str_str(GET_KEYWORDS(&mob_proto[idx]), "elven") || str_str(GET_NAME(&mob_proto[idx]), "elven")) {
        if (!str_str(GET_KEYWORDS(&mob_proto[idx]), " elf") && !is_abbrev("elf", GET_KEYWORDS(&mob_proto[idx]))) {
          send_to_char(ch, "  [%6ld] %s\r\n", GET_MOB_VNUM(&mob_proto[idx]), GET_NAME(&mob_proto[idx]));
        }
      }
    }
    break;
  case 28:
    send_to_char("The following cyberdoc shops have a player-selling-to-shop profit percentage != 30%:\r\n", ch);
    for (int idx = 0; idx <= top_of_shopt; idx++) {
      if (shop_table[idx].flags.IsSet(SHOP_DOCTOR) && shop_table[idx].profit_sell != 0.3f) {
        if (!ch_can_bypass_edit_lock(ch, get_zone_from_vnum(shop_table[idx].vnum)))
          continue;

        send_to_char(ch, "  [%6ld] %.2f\r\n", shop_table[idx].vnum, shop_table[idx].profit_sell);
      }
    }
    break;
  case 29:
    send_to_char("The following shopkeepers have intelligence above their racial maximums:\r\n", ch);
    for (struct char_data *mob = character_list; mob; mob = mob->next_in_character_list) {
      int max_allowed = (GET_RACE(mob) <= RACE_UNDEFINED ? 6 : get_attr_max(mob, INT));
      if (MOB_HAS_SPEC(mob, shop_keeper) && GET_REAL_INT(mob) > max_allowed) {
        // Only show it on shops that negotiate.
        bool shop_negotiates = FALSE;
        for (int idx = 0; idx <= top_of_shopt; idx++) {
          if (shop_table[idx].keeper == GET_MOB_VNUM(mob)) {
            if (!ch_can_bypass_edit_lock(ch, get_zone_from_vnum(shop_table[idx].vnum)))
              continue;
            shop_negotiates = !shop_table[idx].flags.IsSet(SHOP_WONT_NEGO);
            break;
          }
        }

        if (shop_negotiates) {
          send_to_char(ch, "  [^c%6ld^n] %s (%s%d^n > ^c%d^n)\r\n", 
                       GET_MOB_VNUM(mob),
                       GET_CHAR_NAME(mob),
                       GET_REAL_INT(mob) > max_allowed * 1.5 ? "^r" : "^y",
                       GET_REAL_INT(mob),
                       max_allowed);
        }
      }
    }
    break;
  case 30:
    send_to_char("The following power sites are present in the game:\r\n", ch);
    for (int idx = 0; idx < top_of_world; idx++) {
      struct room_data *room = &world[idx];
      
      if (GET_BACKGROUND_AURA(room) == AURA_POWERSITE && GET_BACKGROUND_COUNT(room) && ch_can_bypass_edit_lock(ch, room)) {
        send_to_char(ch, "[^c%6d^n]: Rating ^c%2d^n @ %s^n.\r\n", GET_ROOM_VNUM(room), GET_BACKGROUND_COUNT(room), GET_ROOM_NAME(room));
      }
    }
    break;
  case 31:
    send_to_char("The following non-key items are used as keys for at least one door in an approved zone:\r\n", ch);
    {
      std::unordered_map<vnum_t, bool> seen_items = {};

      for (int idx = 0; idx < top_of_world; idx++) {
        struct room_data *room = &world[idx];

        if (vnum_from_non_approved_zone(GET_ROOM_VNUM(room)))
          continue;
        
        for (int dir = 0; dir < NUM_OF_DIRS; dir++) {
          if (EXIT2(room, dir) && EXIT2(room, dir)->key && !seen_items[EXIT2(room, dir)->key]) {
            rnum_t key_rnum = real_object(EXIT2(room, dir)->key);
            struct obj_data *key_obj = &obj_proto[key_rnum];

            if (key_rnum >= 0 && GET_OBJ_TYPE(key_obj) != ITEM_KEY && GET_OBJ_TYPE(key_obj) != ITEM_DECK_ACCESSORY) {
              send_to_char(ch, " [%6d] %s^n (for %ld's %s exit)%s\r\n",
                          GET_OBJ_VNUM(key_obj),
                          GET_OBJ_NAME(key_obj),
                          GET_ROOM_VNUM(room),
                          dirs[dir],
                          IS_OBJ_STAT(key_obj, ITEM_EXTRA_NODROP) ? "" : " ^y(droppable)^n");
              
              seen_items[GET_OBJ_VNUM(key_obj)] = TRUE;
            }
          }
        }
      }
    }
    break;
  case 32:
    send_to_char("The following extra-heavy items have TAKE flags:\r\n", ch);
    for (int idx = 0; idx < top_of_objt; idx++) {
      struct obj_data *obj = &obj_proto[idx];
      if (GET_OBJ_WEIGHT(obj) >= 200 && CAN_WEAR(obj, ITEM_WEAR_TAKE)) {
        send_to_char(ch, " [%6d] %s^n\r\n", GET_OBJ_VNUM(obj), GET_OBJ_NAME(obj));
      }
    }
    break;
  case 33:
    send_to_char("The following connected zones with zone commands have long lifetimes or don't reset:\r\n", ch);
    for (int idx = 0; idx < top_of_zone_table; idx++) {
      if (!ch_can_bypass_edit_lock(ch, &zone_table[idx]))
        continue;

      if (zone_table[idx].editing_restricted_to_admin
          && zone_table[idx].num_cmds > 0
          && (zone_table[idx].lifespan > 20 || zone_table[idx].reset_mode == ZONE_RESET_NEVER))
      {
        send_to_char(ch, " [%6d] (%3d) %s^n\r\n", 
                  zone_table[idx].number,
                  zone_table[idx].reset_mode == ZONE_RESET_NEVER ? -1 : zone_table[idx].lifespan,
                  zone_table[idx].name);
      } 
    }
    break;
  case 34:
    send_to_char("The following killable, connected NPCs have magic 12 or higher:\r\n", ch);
    for (rnum_t mob_idx = 0; mob_idx < top_of_mobt; mob_idx++) {
      struct char_data *mob = &mob_proto[mob_idx];
      if (GET_MAG(mob) / 100 < 12 || vnum_from_editing_restricted_zone(GET_MOB_VNUM(mob)) || MOB_FLAGGED(mob, MOB_NOKILL))
        continue;
      if (!ch_can_bypass_edit_lock(ch, mob))
        continue;
      send_to_char(ch, " [^c%6d^n] %s (%2d magic)\r\n", GET_MOB_VNUM(mob), GET_CHAR_NAME(mob), GET_MAG(mob) / 100);
    }
    break;
  case 35:
    send_to_char("The following rooms have tempdescs set:\r\n", ch);
    for (rnum_t world_idx = 0; world_idx < top_of_world; world_idx++) {
      if (world[world_idx].temp_desc)
        send_to_char(ch, " [^c%6d^n] %s^n\r\n", GET_ROOM_VNUM(&world[world_idx]), GET_ROOM_NAME(&world[world_idx]));
    }
    break;
  case 36:
    send_to_char("The following items have the same roomdesc and lookdesc:\r\n", ch);
    for (int idx = 0; idx < top_of_objt; idx++) {
      struct obj_data *obj = &obj_proto[idx];
      if (!strcmp(obj->text.room_desc, obj->text.look_desc)) {
        send_to_char(ch, " [%6d] %s^n\r\n", GET_OBJ_VNUM(obj), GET_OBJ_NAME(obj));
      }
    }
    break;
  case 37:
    // extraflag
    if (*value) {
      for (i = 0; i < MAX_ITEM_EXTRA; i++) {
        if (str_str(extra_bits[i], value)) {
          snprintf(buf, sizeof(buf), "Objects with flag %s set:\r\n", extra_bits[i]);
          for (k = 0, j = 0; k <= top_of_objt; k++)
            if (IS_OBJ_STAT(&obj_proto[k], i) && ch_can_bypass_edit_lock(ch, &obj_proto[k]))
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%6d: [%8ld] %s %s\r\n", ++j,
                       GET_OBJ_VNUM(&obj_proto[k]),
                       !vnum_from_non_approved_zone(GET_OBJ_VNUM(&obj_proto[k])) ? " " : (PRF_FLAGGED(ch, PRF_SCREENREADER) ? "(approved)" : "*"),
                       GET_OBJ_NAME(&obj_proto[k]));
          page_string(ch->desc, buf, 1);
          return;
        }
      }
    }

    send_to_char("Please specify a single extra flag from the following list:\r\n", ch);
    for (i = 0; i < MAX_ITEM_EXTRA; i++)
      send_to_char(ch, "  %s\r\n", extra_bits[i]);
    return;
  case 38:
    // affflag
    if (*value) {
      for (i = 0; i < MAX_AFFECT; i++) {
        if (str_str(affected_bits[i], value)) {
          snprintf(buf, sizeof(buf), "Objects with affflag %s set:\r\n", affected_bits[i]);
          for (k = 0, j = 0; k <= top_of_objt; k++)
            if (GET_OBJ_AFFECT(&obj_proto[k]).IsSet(i) && ch_can_bypass_edit_lock(ch, &obj_proto[k]))
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%6d: [%8ld] %s %s\r\n", ++j,
                       GET_OBJ_VNUM(&obj_proto[k]),
                       vnum_from_non_approved_zone(GET_OBJ_VNUM(&obj_proto[k])) ? " " : (PRF_FLAGGED(ch, PRF_SCREENREADER) ? "(approved)" : "*"),
                       GET_OBJ_NAME(&obj_proto[k]));
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nMobs with affflag %s set:\r\n", affected_bits[i]);
          for (k = 0, j = 0; k <= top_of_mobt; k++)
            if (AFF_FLAGGED(&mob_proto[k], i) && ch_can_bypass_edit_lock(ch, &mob_proto[k]))
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%6d: [%8ld] %s %s\r\n", ++j,
                       GET_MOB_VNUM(&mob_proto[k]),
                       vnum_from_non_approved_zone(GET_MOB_VNUM(&mob_proto[k])) ? " " : (PRF_FLAGGED(ch, PRF_SCREENREADER) ? "(approved)" : "*"),
                       GET_CHAR_NAME(&mob_proto[k]));
          page_string(ch->desc, buf, 1);
          return;
        }
      }
    }

    send_to_char("Please specify a single affflag from the following list:\r\n", ch);
    for (i = 0; i < MAX_AFFECT; i++)
      send_to_char(ch, "  %s\r\n", affected_bits[i]);
    return;
  case 39:
    // mobflag
    if (*value) {
      for (i = 0; i < MOB_MAX; i++) {
        if (str_str(action_bits[i], value)) {
          snprintf(buf, sizeof(buf), "\r\nMobs with mobflag %s set:\r\n", action_bits[i]);
          for (k = 0, j = 0; k <= top_of_mobt; k++)
            if (MOB_FLAGGED(&mob_proto[k], i) && ch_can_bypass_edit_lock(ch, &mob_proto[k]))
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%6d: [%8ld] %s %s\r\n", ++j,
                       GET_MOB_VNUM(&mob_proto[k]),
                       vnum_from_non_approved_zone(GET_MOB_VNUM(&mob_proto[k])) ? " " : (PRF_FLAGGED(ch, PRF_SCREENREADER) ? "(approved)" : "*"),
                       GET_CHAR_NAME(&mob_proto[k]));
          page_string(ch->desc, buf, 1);
          return;
        }
      }
    }

    send_to_char("Please specify a single mobflag from the following list:\r\n", ch);
    for (i = 0; i < MOB_MAX; i++)
      send_to_char(ch, "  %s\r\n", action_bits[i]);
    return;
  case 40:
    // finishedzones
    {
      *buf = '\0';

      for (i = 0; i <= top_of_zone_table; i++) {
        // Skip anything that's not marked as completed.
        if (!zone_table[i].approved) {
          continue;
        }

        // Skip zones that were part of Che's release.
        switch (zone_table[i].number) {
          case 0:
          case 2:
          case 3:
          case 5:
          case 6:
          case 7:
          case 10:
          case 11:
          case 12:
          case 13:
          case 14:
          case 15:
          case 16:
          case 18:
          case 20:
          case 21:
          case 22:
          case 23:
          case 24:
          case 25:
          case 26:
          case 27:
          case 29:
          case 30:
          case 31:
          case 32:
          case 33:
          case 34:
          case 35:
          case 36:
          case 37:
          case 38:
          case 40:
          case 41:
          case 42:
          case 46:
          case 47:
          case 48:
          case 49:
          case 50:
          case 51:
          case 52:
          case 54:
          case 55:
          case 56:
          case 57:
          case 58:
          case 59:
          case 60:
          case 61:
          case 62:
          case 64:
          case 65:
          case 67:
          case 69:
          case 70:
          case 72:
          case 74:
          case 77:
          case 78:
          case 79:
          case 80:
          case 81:
          case 83:
          case 84:
          case 88:
          case 91:
          case 95:
          case 96:
          case 98:
          case 99:
          case 100:
          case 101:
          case 102:
          case 103:
          case 105:
          case 106:
          case 108:
          case 112:
          case 113:
          case 114:
          case 115:
          case 120:
          case 123:
          case 124:
          case 125:
          case 127:
          case 128:
          case 130:
          case 134:
          case 143:
          case 145:
          case 146:
          case 148:
          case 150:
          case 151:
          case 156:
          case 157:
          case 158:
          case 160:
          case 162:
          case 164:
          case 171:
          case 172:
          case 175:
          case 176:
          case 179:
          case 181:
          case 183:
          case 184:
          case 188:
          case 194:
          case 196:
          case 197:
          case 198:
          case 203:
          case 206:
          case 208:
          case 210:
          case 217:
          case 219:
          case 220:
          case 221:
          case 222:
          case 224:
          case 226:
          case 228:
          case 229:
          case 231:
          case 232:
          case 233:
          case 235:
          case 236:
          case 237:
          case 241:
          case 243:
          case 245:
          case 248:
          case 249:
          case 250:
          case 253:
          case 254:
          case 261:
          case 262:
          case 264:
          case 267:
          case 268:
          case 269:
          case 274:
          case 276:
          case 278:
          case 279:
          case 280:
          case 284:
          case 286:
          case 287:
          case 288:
          case 289:
          case 290:
          case 291:
          case 293:
          case 294:
          case 297:
          case 298:
          case 299:
          case 301:
          case 302:
          case 305:
          case 309:
          case 310:
          case 311:
          case 312:
          case 315:
          case 316:
          case 317:
          case 318:
          case 320:
          case 321:
          case 322:
          case 323:
          case 325:
          case 328:
          case 329:
          case 330:
          case 332:
          case 333:
          case 350:
          case 352:
          case 353:
          case 355:
          case 357:
          case 360:
          case 375:
          case 380:
          case 382:
          case 384:
          case 392:
          case 394:
          case 396:
          case 398:
          case 400:
          case 402:
          case 403:
          case 404:
          case 405:
          case 406:
          case 407:
          case 408:
          case 410:
          case 411:
          case 420:
          case 421:
          case 450:
          case 452:
          case 500:
          case 503:
          case 600:
          case 602:
          case 603:
          case 605:
          case 607:
          case 608:
          case 610:
          case 615:
          case 620:
          case 621:
          case 622:
          case 623:
          case 624:
          case 625:
          case 649:
          case 650:
          case 651:
          case 652:
          case 654:
          case 700:
          case 701:
          case 702:
          case 703:
          case 704:
          case 705:
          case 710:
          case 711:
          case 720:
          case 721:
          case 807:
          case 850:
          case 852:
          case 854:
          case 856:
          case 858:
          case 859:
          case 860:
          case 1000:
          case 7000:
          case 7050:
          case 7060:
            continue;
        }

        print_zone_to_buf(ENDOF(buf), sizeof(buf) - strlen(buf), i, 2, ch);
      }
    }
    page_string(ch->desc, buf, 1);
    return;
  default:
    send_to_char("Sorry, I don't understand that.\r\n", ch);
    break;
  }
}

#define PC   1
#define NPC  2
#define BOTH 3

#define MISC    0
#define BINARY  1
#define NUMBER  2

#define SET_OR_REMOVE(flagset, flag) { \
 if (on) flagset.SetBit(flag); \
 else if (off) flagset.RemoveBit(flag); \
}

#define RANGE(low, high) (value = MAX((low), MIN((high), (value))))

#define SET_CLEANUP(save)                        \
if ((save) && !IS_NPC(vict)) {                   \
  if (is_file) {                                 \
    extract_char(vict);                          \
  } else {                                       \
    playerDB.SaveChar(vict, GET_LOADROOM(vict)); \
  }                                              \
}

ACMD(do_vset)
{
  struct veh_data *veh = NULL;
  int value = 0;
  char name[MAX_INPUT_LENGTH], field[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH];
  half_chop(argument, name, buf, sizeof(buf));
  if (!*name || !*buf) {
    send_to_char("Usage: vset <vehicle> (owner | locked | subscribed) (idnum | on/off | on/off)\r\n", ch);
    return;
  }
  if (!(veh = get_veh_list(name, ch->in_veh ? ch->in_veh->carriedvehs : ch->in_room->vehicles, ch))) {
    send_to_char("There is no such vehicle.\r\n", ch);
    return;
  }

  half_chop(buf, field, val_arg, sizeof(val_arg));
  snprintf(buf, sizeof(buf), "Choose ^rowner^n, ^rlocked^n, or ^rsubscribed^n.\r\n");

  if (is_abbrev(field, "owner")) {
    // Clear out the old save data.
    value = atoi(val_arg);
    int old_owner = veh->owner;
    set_veh_owner(veh, value);
    snprintf(buf, sizeof(buf), "%s's owner field set to %d.\r\n", GET_VEH_NAME(veh), value);
    mudlog_vfprintf(ch, LOG_SYSLOG, "Set %s [%ld]'s owner field from %d to %d.", GET_VEH_NAME(veh), GET_VEH_VNUM(veh), old_owner, value);
  } else if (is_abbrev(field, "locked")) {
    if (!strcmp(val_arg, "on") || !strcmp(val_arg, "yes"))
      value = 2;
    else if (!strcmp(val_arg, "off") || !strcmp(val_arg, "no"))
      value = 1;
    if (!value) {
      send_to_char("Value must be on or off.\r\n", ch);
      return;
    }
    value--;
    veh->locked = value;
    snprintf(buf, sizeof(buf), "%s's lock was set to %s", GET_VEH_NAME(veh), ONOFF(value));
    mudlog_vfprintf(ch, LOG_SYSLOG, "Set %s [%ld]'s lock to %s.", GET_VEH_NAME(veh), GET_VEH_VNUM(veh), ONOFF(value));
  } else if (is_abbrev(field, "subscribed")) {
    if (!strcmp(val_arg, "on") || !strcmp(val_arg, "yes"))
      value = 1;
    else if (!strcmp(val_arg, "off") || !strcmp(val_arg, "no"))
      value = 0;
    else {
      send_to_char("Value must be on or off.\r\n", ch);
      return;
    }
    veh->sub = value;
    snprintf(buf, sizeof(buf), "%s's subscribed status was set to %s", GET_VEH_NAME(veh), ONOFF(value));
    mudlog_vfprintf(ch, LOG_SYSLOG, "Set %s [%ld]'s subscribed status to %s.", GET_VEH_NAME(veh), GET_VEH_VNUM(veh), ONOFF(value));
  }
  send_to_char(buf, ch);
  return;
}
ACMD(do_set)
{
  int i, l;
  struct char_data *vict = NULL;
  char field[MAX_INPUT_LENGTH], name[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH];
  int on = 0, off = 0, value = 0;
  char is_file = 0, is_player = 0;
  int parse_class(struct descriptor_data *d, char *arg);

  struct set_struct {
    const char *cmd;
    char level;
    char pcnpc;
    char type;
  }
  fields[] = {
               { "connlog",  LVL_ADMIN, PC, BINARY }, // 0
               { "invstart",        LVL_VICEPRES,      PC,     BINARY },
               { "title",           LVL_ADMIN, PC,     MISC },
               { "afk",          LVL_ADMIN, PC,     BINARY },
               { "maxphysical",     LVL_VICEPRES,      BOTH,   NUMBER },
               { "maxmental",       LVL_VICEPRES,      BOTH,   NUMBER },       // 5
               { "physical",        LVL_ADMIN, BOTH,   NUMBER },
               { "mental",   LVL_ADMIN, BOTH,   NUMBER },
               { "align",           LVL_VICEPRES,      BOTH,   NUMBER },
               { "str",             LVL_ADMIN, BOTH,   NUMBER },
               { "ess",             LVL_ADMIN, BOTH,   NUMBER },       // 10
               { "int",             LVL_ADMIN, BOTH,   NUMBER },
               { "wil",             LVL_ADMIN, BOTH,   NUMBER },
               { "qui",             LVL_ADMIN, BOTH,   NUMBER },
               { "bod",             LVL_ADMIN, BOTH,   NUMBER },
               { "sex",             LVL_ADMIN, BOTH,   MISC },  // 15
               { "ballistic",       LVL_VICEPRES, BOTH,   NUMBER },
               { "nuyen",    LVL_ADMIN, BOTH,   NUMBER },
               { "bank",            LVL_ADMIN, PC,     NUMBER },
               { "reputation",      LVL_VICEPRES,      BOTH,   NUMBER },
               { "initdice",        LVL_VICEPRES,      BOTH,   NUMBER },       // 20
               { "initroll",        LVL_VICEPRES,      BOTH,   NUMBER },
               { "invis",           LVL_VICEPRES,      PC,     NUMBER },
               { "nohassle",        LVL_ADMIN, PC,     BINARY },
               { "frozen",   LVL_FREEZE,     PC,     BINARY },
               { "karma",           LVL_ADMIN, PC,     NUMBER },       // 25
               { "drunk",           LVL_ADMIN, BOTH,   MISC },
               { "hunger",   LVL_ADMIN, BOTH,   MISC },
               { "thirst",  LVL_ADMIN, BOTH,   MISC },
               { "killer",  LVL_VICEPRES, PC,     BINARY },
               { "level",           LVL_VICEPRES,      BOTH,   NUMBER }, // 30
               { "room",            LVL_ADMIN, BOTH,   NUMBER },
               { "roomflag",        LVL_ADMIN, PC,     BINARY },
               { "siteok",  LVL_ADMIN, PC, BINARY },
               { "deleted",         LVL_VICEPRES,      PC,     BINARY },
               { "nowizlist",       LVL_VICEPRES,      PC,     BINARY },//35
               { "loadroom",        LVL_ADMIN, PC,     MISC },
               { "color",           LVL_ADMIN, PC,     BINARY },
               { "idnum",           LVL_VICEPRES,      PC,     NUMBER },
               { "password",  LVL_VICEPRES,      PC,     MISC },
               { "nodelete",        LVL_VICEPRES,      PC,     BINARY }, // 40
               { "cha",             LVL_ADMIN, BOTH,   NUMBER },
               { "mag",  LVL_ADMIN, BOTH, NUMBER },
               { "rea",  LVL_VICEPRES, BOTH, NUMBER },
               { "impact",  LVL_VICEPRES, BOTH, NUMBER },
               { "newbie",  LVL_ADMIN, PC, BINARY }, // 45
               { "skillpoints", LVL_ADMIN, PC, NUMBER },
               { "totem",  LVL_ADMIN, PC, NUMBER },
               { "zone",            LVL_ADMIN, PC, NUMBER },
               { "olc",             LVL_ADMIN, PC, BINARY },
               { "nostat",  LVL_VICEPRES, PC, BINARY }, // 50
               { "pretitle", LVL_ADMIN, PC, MISC },
               { "whotitle", LVL_ADMIN, PC, MISC },
               { "pker",  LVL_ADMIN, PC, BINARY },
               { "index",  LVL_ADMIN, BOTH, NUMBER },
               { "tradition",       LVL_ADMIN, PC,     NUMBER }, // 55
               { "nosnoop",  LVL_VICEPRES, PC, BINARY },
               { "holylight", LVL_ADMIN, PC, BINARY },
               { "wanted",       LVL_ADMIN, PC, BINARY },
               { "authorize", LVL_ADMIN, PC, BINARY },
               { "edcon",           LVL_ADMIN, PC, BINARY }, // 60; 'edit rooms in a connected zone' (for low-level builders)
               { "notoriety",       LVL_ADMIN, PC, NUMBER },
               { "pg",              LVL_ADMIN,      PC,     BINARY },
               { "rpe",      LVL_FIXER, PC, BINARY },
               { "quest",    LVL_VICEPRES, PC , BINARY },
               { "questor",      LVL_CONSPIRATOR, PC, BINARY }, // 65
               { "aspect",       LVL_ADMIN, PC, NUMBER},
               { "helper",       LVL_CONSPIRATOR, PC, BINARY },
               { "blacklist",       LVL_VICEPRES, PC, BINARY },
               { "noidle", LVL_VICEPRES, PC, BINARY },
               { "tke", LVL_VICEPRES, PC, NUMBER }, //70
               { "sysp", LVL_VICEPRES, PC, NUMBER },
               { "socializationbonus", LVL_ADMIN, PC,     NUMBER },
               { "race", LVL_ADMIN, PC, NUMBER },
               { "rolls", LVL_PRESIDENT, PC, BINARY },
               { "multiplier", LVL_PRESIDENT, PC, NUMBER }, //75
               { "shotsfired", LVL_PRESIDENT, PC, NUMBER },
               { "shotstriggered", LVL_PRESIDENT, PC, NUMBER },
               { "powerpoints", LVL_VICEPRES, PC, NUMBER },
               { "cyberdoc", LVL_CONSPIRATOR, PC, BINARY },
               { "hardcore", LVL_PRESIDENT, PC, BINARY }, //80
               { "esshole",  LVL_ADMIN, BOTH,   NUMBER },
               { "noautosyspoints", LVL_FIXER, PC, BINARY },
               { "notells", LVL_FIXER, PC, BINARY },
               { "noooc", LVL_FIXER, PC, BINARY },
               { "noradio", LVL_FIXER, PC, BINARY }, // 85
               { "sitehidden",  LVL_PRESIDENT, PC, BINARY },
               { "lifestyle",  LVL_PRESIDENT, PC, MISC },
               { "highestindex", LVL_ADMIN, BOTH, NUMBER },
               { "strikes", LVL_FIXER, PC, NUMBER }, // 89
               { "\n", 0, BOTH, MISC }
             };

  half_chop(argument, name, buf, sizeof(buf));
  if (!strcmp(name, "file")) {
    is_file = 1;
    char remainder[MAX_INPUT_LENGTH];
    strlcpy(remainder, one_argument(buf, name), sizeof(remainder));
    strlcpy(buf, remainder, sizeof(buf));
  } else if (!str_cmp(name, "player")) {
    is_player = 1;
    char remainder[MAX_INPUT_LENGTH];
    strlcpy(remainder, one_argument(buf, name), sizeof(remainder));
    strlcpy(buf, remainder, sizeof(buf));
  } else if (!str_cmp(name, "mob")) {
    char remainder[MAX_INPUT_LENGTH];
    strlcpy(remainder, one_argument(buf, name), sizeof(remainder));
    strlcpy(buf, remainder, sizeof(buf));
  }
  half_chop(buf, field, buf2, sizeof(buf2));
  strlcpy(val_arg, buf2, sizeof(val_arg));

  if (!*name || !*field) {
    send_to_char("Usage: set <victim> <field> <value>\r\n", ch);
    return;
  }
  if (!is_file) {
    if (is_player) {
      if (!(vict = get_player_vis(ch, name, 0))) {
        send_to_char("There is no such player.\r\n", ch);
        return;
      }
    } else {
      if (!(vict = get_char_vis(ch, name))) {
        send_to_char("There is no such creature.\r\n", ch);
        return;
      }
    }
  } else if (is_file) {
    for (struct descriptor_data *td = descriptor_list; td; td = td->next) {
      if (td->character && !str_cmp(name, GET_CHAR_NAME(td->character))) {
        send_to_char(ch, "%s is already loaded.\r\n", name);
        return;
      }
      if (td->original && !str_cmp(name, GET_CHAR_NAME(td->original))) {
        send_to_char(ch, "%s is already loaded.\r\n", name);
        return;
      }
    }

    if (!does_player_exist(name)) {
      send_to_char("There is no such player.\r\n", ch);
      return;
    }

    vict = playerDB.LoadChar(name, FALSE, PC_LOAD_REASON_OFFLINE_SET);

    if (vict) {
      if (!access_level(ch, GET_LEVEL(vict)+1)) {
        send_to_char("You must be a higher level than your victim in order to affect them in this manner.\r\n", ch);
        SET_CLEANUP(false);
        return;
      }
    } else {
      send_to_char("There is no such player.\r\n", ch);
      return;
    }
  }

  if (ch != vict && !access_level(ch, LVL_ADMIN)) {
    if (!IS_NPC(vict) && !access_level(ch, GET_LEVEL(vict)+1)) {
      send_to_char("You need to be a higher level than your victim to do that.\r\n", ch);

      SET_CLEANUP(false);

      return;
    }
  }

  for (l = 0; *(fields[l].cmd) != '\n'; l++)
    if (!strncmp(field, fields[l].cmd, strlen(field)))
      break;

  if (!access_level(ch, fields[l].level)) {
    send_to_char("You are not erudite enough for that!\r\n", ch);

    SET_CLEANUP(false);

    return;
  }

  if (IS_NPC(vict) && fields[l].pcnpc == PC) {
    send_to_char("You can't do that to a mob!\r\n", ch);
    SET_CLEANUP(false);
    return;
  } else if (!IS_NPC(vict) && fields[l].pcnpc == NPC) {
    send_to_char("That can only be done to mobs!\r\n", ch);
    SET_CLEANUP(false);
    return;
  }

  if (fields[l].type == BINARY) {
    if (!strcmp(val_arg, "on") || !strcmp(val_arg, "yes"))
      on = 1;
    else if (!strcmp(val_arg, "off") || !strcmp(val_arg, "no"))
      off = 1;
    if (!(on || off)) {
      send_to_char("Value must be on or off.\r\n", ch);

      SET_CLEANUP(false);

      return;
    }
  } else if (fields[l].type == NUMBER) {
    value = atoi(val_arg);
  }

  if (IS_NPC(vict))
    snprintf(buf, sizeof(buf),"%s set %s -> %s.",GET_CHAR_NAME(ch),vict?GET_NAME(vict):"", argument);
  else
    snprintf(buf, sizeof(buf),"%s set %s -> %s.",GET_CHAR_NAME(ch),vict?GET_CHAR_NAME(vict):"", argument);
  if (l != 86)
    mudlog(buf, ch, LOG_WIZLOG, TRUE );

  strlcpy(buf, "Okay.", sizeof(buf));  /* can't use OK macro here 'cause of \r\n */
  switch (l) {
  case 0:
    SET_OR_REMOVE(PRF_FLAGS(vict), PRF_CONNLOG);
    break;
  case 1:
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_INVSTART);
    break;
  case 2:
    set_title(vict, val_arg);
    snprintf(buf, sizeof(buf), "%s's title is now: %s", GET_NAME(vict), GET_TITLE(vict));
    break;
  case 3:
    SET_OR_REMOVE(PRF_FLAGS(vict), PRF_AFK);
    on = !on;                   /* so output will be correct */
    break;
  case 4:
    RANGE(0, 100);
    vict->points.max_physical = value * 100;
    affect_total(vict);
    break;
  case 5:
    RANGE(0, 100);
    vict->points.max_mental = value * 100;
    affect_total(vict);
    break;
  case 6:
    RANGE(-(GET_BOD(vict) - 1), (int)(vict->points.max_physical / 100));
    vict->points.physical = value * 100;
    affect_total(vict);
    update_pos(vict);
    break;
  case 7:
    RANGE(-9, (int)(vict->points.max_mental / 100));
    vict->points.mental = value * 100;
    affect_total(vict);
    update_pos(vict);
    break;
  case 9:
    if (IS_NPC(vict) || access_level(vict, LVL_ADMIN))
      RANGE(1, 50);
    else
      RANGE(1, 15);
    if (!IS_SENATOR(vict))
      GET_TKE(vict) = GET_TKE(vict) + 2*(value - GET_REAL_STR(vict));
    GET_REAL_STR(vict) = value;
    affect_total(vict);
    break;
  case 10:
    if (IS_NPC(vict) || (IS_SENATOR(vict) && access_level(vict, LVL_ADMIN)))
      RANGE(1, 1000);
    else
      RANGE(1, GET_RACIAL_STARTING_ESSENCE_FOR_RACE(GET_RACE(vict)));
    GET_REAL_ESS(vict) = value;
    affect_total(vict);
    break;
  case 11:
    if (IS_NPC(vict) || (IS_SENATOR(vict) && access_level(vict, LVL_ADMIN)))
      RANGE(1, 50);
    else
      RANGE(1, 15);
    if (!IS_SENATOR(vict))
      GET_TKE(vict) = GET_TKE(vict) + 2*(value - GET_REAL_INT(vict));
    GET_REAL_INT(vict) = value;
    affect_total(vict);
    break;
  case 12:
    if (IS_NPC(vict) || (IS_SENATOR(vict) && access_level(vict, LVL_ADMIN)))
      RANGE(1, 50);
    else
      RANGE(1, 15);
    if (!IS_SENATOR(vict))
      GET_TKE(vict) = GET_TKE(vict) + 2*(value - GET_REAL_WIL(vict));
    GET_REAL_WIL(vict) = value;
    affect_total(vict);
    break;
  case 13:
    if (IS_NPC(vict) || (IS_SENATOR(vict) && access_level(vict, LVL_ADMIN)))
      RANGE(1, 50);
    else
      RANGE(1, 15);
    if (!IS_SENATOR(vict))
      GET_TKE(vict) = GET_TKE(vict) + 2*(value - GET_REAL_QUI(vict));
    GET_REAL_QUI(vict) = value;
    affect_total(vict);
    break;
  case 14:
    if (IS_NPC(vict) || (IS_SENATOR(vict) && access_level(vict, LVL_ADMIN)))
      RANGE(1, 50);
    else
      RANGE(1, 15);
    if (!IS_SENATOR(vict))
      GET_TKE(vict) = GET_TKE(vict) + 2*(value - GET_REAL_BOD(vict));
    GET_REAL_BOD(vict) = value;
    affect_total(vict);
    break;
  case 15:
    if (!str_cmp(val_arg, "male"))
      vict->player.pronouns = PRONOUNS_MASCULINE;
    else if (!str_cmp(val_arg, "female"))
      vict->player.pronouns = PRONOUNS_FEMININE;
    else if (!str_cmp(val_arg, "neutral"))
      vict->player.pronouns = PRONOUNS_NEUTRAL;
    else {
      send_to_char("Must be 'male', 'female', or 'neutral'.\r\n", ch);

      SET_CLEANUP(false);

      return;
    }
    break;
  case 16:
    RANGE(0, 100);
    GET_INNATE_BALLISTIC(vict) = value;
    affect_total(vict);
    break;
  case 17:
    RANGE(0, 100000000);
    GET_NUYEN_RAW(vict) = value;
    break;
  case 18:
    RANGE(0, 100000000);
    GET_BANK_RAW(vict) = value;
    break;
  case 19:
    RANGE(0, 7500);
    GET_REP(vict) = value;
    break;
  case 20:
    RANGE(0, 10);
    vict->points.init_dice = value;
    affect_total(vict);
    break;
  case 21:
    RANGE(0, 50);
    vict->points.init_roll = value;
    affect_total(vict);
    break;
  case 22:
    if (!access_level(ch, LVL_ADMIN) && ch != vict) {
      send_to_char("You aren't erudite enough for that!\r\n", ch);

      SET_CLEANUP(false);

      return;
    }
    if(!access_level(vict, value))
      RANGE(0, GET_LEVEL(vict));
    GET_INVIS_LEV(vict) = value;
    break;
  case 23:
    if (!access_level(ch, LVL_ADMIN) && ch != vict) {
      send_to_char("You aren't erudite enough for that!\r\n", ch);

      SET_CLEANUP(false);

      return;
    }
    SET_OR_REMOVE(PRF_FLAGS(vict), PRF_NOHASSLE);
    break;
  case 24:
    if (ch == vict) {
      send_to_char("Better not -- could be a long winter!\r\n", ch);

      SET_CLEANUP(false);

      return;
    }
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_FROZEN);
    if(off)
      GET_FREEZE_LEV(vict) = 0;
    else
      GET_FREEZE_LEV(vict) = GET_LEVEL(ch);

    break;
  case 25:
    RANGE(0, MYSQL_UNSIGNED_MEDIUMINT_MAX);
    //GET_KARMA(vict) = value;
    vict->points.karma = value;
    break;
  case 26:
  case 27:
  case 28:
    if (!str_cmp(val_arg, "off")) {
      GET_COND(vict, (l - 26)) = (char) -1;
      snprintf(buf, sizeof(buf), "%s's %s now off.", GET_NAME(vict), fields[l].cmd);
    } else if (is_number(val_arg)) {
      value = atoi(val_arg);
      RANGE(0, 24);
      GET_COND(vict, (l - 26)) = (char) value;
      snprintf(buf, sizeof(buf), "%s's %s set to %d.", GET_NAME(vict), fields[l].cmd, value);
    } else {
      send_to_char("Must be 'off' or a value from 0 to 24.\r\n", ch);

      SET_CLEANUP(false);

      return;
    }
    break;
  case 29:
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_KILLER);
    break;
  case 30:
    if (!access_level(ch, value) || value > LVL_MAX) {
      send_to_char("You can't do that.\r\n", ch);

      SET_CLEANUP(false);

      return;
    }

    /* Can't demote other owners this way, unless it's yourself */
    if ( access_level(vict, LVL_PRESIDENT) && vict != ch ) {
      send_to_char("You can't demote other presidents.\r\n",ch);

      SET_CLEANUP(false);

      return;
    }

    RANGE(1, LVL_MAX);
    GET_LEVEL(vict) = (byte) value;
    break;
  case 31:
    if ((i = real_room(value)) < 0) {
      send_to_char("No room exists with that number.\r\n", ch);

      SET_CLEANUP(false);
      return;
    }
    char_from_room(vict);
    char_to_room(vict, &world[i]);
    break;
  case 32:
    SET_OR_REMOVE(PRF_FLAGS(vict), PRF_ROOMFLAGS);
    break;
  case 33:
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_SITEOK);
    break;
  case 34:
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_DELETED);
    break;
  case 35:
    //    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_NOWIZLIST);
    break;
  case 36:
    if (!str_cmp(val_arg, "off"))
      GET_LOADROOM(vict) = 0;
    else if (is_number(val_arg)) {
      value = atoi(val_arg);
      if (real_room(value) != NOWHERE) {
        GET_LOADROOM(vict) = value;
        snprintf(buf, sizeof(buf), "%s will enter at room #%ld.", GET_NAME(vict), GET_LOADROOM(vict));
      } else
        snprintf(buf, sizeof(buf), "That room does not exist!");
    } else
      strlcpy(buf, "Must be 'off' or a room's virtual number.\r\n", sizeof(buf));
    break;
  case 38:
    if (GET_IDNUM(ch) != 1 || !IS_NPC(vict)) {
      SET_CLEANUP(false);
      return;
    }
    GET_IDNUM(vict) = value;
    break;
  case 39:
    if (!is_file) {
      send_to_char("You can only do that to offline characters.\r\n", ch);
      SET_CLEANUP(false);
      return;
    }

    if ((IS_SENATOR(vict) && !access_level(ch, LVL_ADMIN))) {
      send_to_char("You cannot change that.\r\n", ch);

      SET_CLEANUP(false);
      return;
    }
    hash_and_store_password(val_arg, GET_PASSWD(vict));
    snprintf(buf, sizeof(buf), "Password changed to '%s'.", val_arg);

    // Do the actual updating.
    char query_buf[2048];
#ifdef NOCRYPT
    char prepare_quotes_buf[2048];
    snprintf(query_buf, sizeof(query_buf), "UPDATE pfiles SET password='%s' WHERE idnum=%ld;",
            prepare_quotes(prepare_quotes_buf, GET_PASSWD(vict), sizeof(prepare_quotes_buf) / sizeof(prepare_quotes_buf[0])), GET_IDNUM(vict));
#else
    snprintf(query_buf, sizeof(query_buf), "UPDATE pfiles SET password='%s' WHERE idnum=%ld;", GET_PASSWD(vict), GET_IDNUM(vict));
#endif
    mysql_wrapper(mysql, query_buf);

    break;
  case 40:
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_NODELETE);
    snprintf(buf, sizeof(buf), "UPDATE pfiles SET NoDelete=%d WHERE idnum=%ld;", PLR_FLAGGED(vict, PLR_NODELETE) ? TRUE : FALSE,
            GET_IDNUM(vict));
    mysql_wrapper(mysql, buf);
    break;
  case 41:
    if (IS_NPC(vict) || (IS_SENATOR(vict) && access_level(vict, LVL_ADMIN)))
      RANGE(1, 50);
    else
      RANGE(1, 15);
    if (!IS_SENATOR(vict))
      GET_TKE(vict) = GET_TKE(vict) + 2*(value - GET_REAL_CHA(vict));
    GET_REAL_CHA(vict) = value;
    affect_total(vict);
    break;
  case 42:
    if (IS_NPC(vict) || (IS_SENATOR(vict) && access_level(vict, LVL_ADMIN)))
      RANGE(0, 5000);
    else
      RANGE(0, 3000);
    GET_SETTABLE_REAL_MAG(vict) = value;
    affect_total(vict);
    break;
  case 43:
    if (IS_NPC(vict) || (IS_SENATOR(vict) && access_level(vict, LVL_ADMIN)))
      RANGE(1, 50);
    else
      RANGE(1, 15);
    GET_REAL_REA(vict) = value;
    affect_total(vict);
    break;
  case 44:
    RANGE(0, 100);
    GET_INNATE_IMPACT(vict) = value;
    affect_total(vict);
    break;
  case 45:
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_NEWBIE);
    break;
  case 46:
    RANGE(0, 50);
    GET_SKILL_POINTS(vict) = value;
    break;
  case 47:
    if (GET_TRADITION(vict) != TRAD_SHAMANIC) {
      send_to_char("They're not a Shaman.\r\n", ch);
      SET_CLEANUP(false);
      return;
    }
    RANGE(0, NUM_TOTEMS - 1);
    GET_TOTEM(vict) = value;
    break;
  case 48:
    RANGE(0, 500);
    vict->player_specials->saved.zonenum = value;
    break;
  case 49:
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_OLC);
    break;
  case 50:
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_NOSTAT);
    break;
  case 51:
    set_pretitle(vict, val_arg);
    break;
  case 52:
    if (strstr((const char *)val_arg, "^")) {
      send_to_char("Whotitles can't contain the ^^ character.\r\n", ch);
      SET_CLEANUP(false);
      return;
    } else
      set_whotitle(vict, val_arg);
    break;
  case 53:
    SET_OR_REMOVE(PRF_FLAGS(vict), PRF_PKER);
    break;
  case 54:
    if (IS_NPC(vict) || (IS_SENATOR(vict) && access_level(vict, LVL_ADMIN)))
      RANGE(0, 1000);
    else
      RANGE(0, 900);
    GET_INDEX(vict) = value;
    affect_total(vict);
    break;
  case 55:
    RANGE(0, TRAD_ADEPT);
    // They need a new magic table entry.
    if (GET_TRADITION(vict) == TRAD_MUNDANE && value != TRAD_MUNDANE) {
      snprintf(buf, sizeof(buf), "INSERT INTO pfiles_magic (idnum, Totem, TotemSpirit, Aspect) VALUES"\
                 "('%ld', '%d', '%d', '%d');", GET_IDNUM(vict), GET_TOTEM(vict), GET_TOTEMSPIRIT(vict), value);
      mysql_wrapper(mysql, buf);
    }
    GET_TRADITION(vict) = value;
    send_to_char(ch, "%s is now %s %s.\r\n", GET_CHAR_NAME(vict), AN(aspect_names[value]), tradition_names[value]);
    break;
  case 56: /* nosnoop flag */
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_NOSNOOP);
    snprintf(buf, sizeof(buf),"%s changed %s's !SNOOP flag setting.",GET_CHAR_NAME(ch),GET_NAME(vict));
    mudlog(buf, ch, LOG_WIZLOG, TRUE );
    break;
  case 57:
    SET_OR_REMOVE(PRF_FLAGS(vict), PRF_HOLYLIGHT);
    break;
  case 58:
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_WANTED);
    break;
  case 59:
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_NOT_YET_AUTHED);
    send_to_char(ch, "%s is now %sauthorized.\r\n", GET_CHAR_NAME(vict), PLR_FLAGS(vict).IsSet(PLR_NOT_YET_AUTHED) ? "un-" : "");
    break;
  case 60:
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_EDCON);
    break;
  case 61:
    RANGE(0, 7500);
    GET_NOT(vict) = value;
    break;
  case 63:
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_RPE);
    break;
  case 64:
    SET_OR_REMOVE(PRF_FLAGS(vict), PRF_HIRED);
    break;
  case 65:
    SET_OR_REMOVE(PRF_FLAGS(vict), PRF_QUESTOR);
    break;
  case 66:
    RANGE(0, ASPECT_WATERMAGE);
    GET_ASPECT(vict) = value;
    send_to_char(ch, "%s is now %s %s.\r\n", GET_CHAR_NAME(vict), AN(aspect_names[value]), aspect_names[value]);
    break;
  case 67:
    SET_OR_REMOVE(PRF_FLAGS(vict), PRF_NEWBIEHELPER);
    break;
  case 68:
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_BLACKLIST);
    break;
  case 69:
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_NO_IDLE_OUT);
    break;
  case 70:
    RANGE(0, 10000);
    GET_TKE(vict) = value;
    break;
  case 71:
    RANGE(-10000, 10000);
    GET_SYSTEM_POINTS(vict) = value;
    break;
  case 72:
    RANGE(0, MAX_CONGREGATION_BONUS);
    GET_CONGREGATION_BONUS(vict) = value;
    break;
  case 73: /* race */
    {
      bool successful = FALSE;
      for (int race_idx = RACE_HUMAN; race_idx < NUM_RACES; race_idx++) {
        if (!str_cmp(pc_race_types[race_idx], val_arg)) {
          // Check to make sure this won't break their essence scores.
          int existing_ess_loss = GET_RACIAL_STARTING_ESSENCE_FOR_RACE(GET_RACE(vict)) - GET_REAL_ESS(vict);
          int new_ess = GET_RACIAL_STARTING_ESSENCE_FOR_RACE(race_idx) - existing_ess_loss;
          if (new_ess <= 0) {
            send_to_char(ch, "Changing %s's race to %s would put them at %.2f essence!", GET_CHAR_NAME(vict), pc_race_types[race_idx], (float) new_ess / 100);
            SET_CLEANUP(false);
            return;
          }

          // Set their race and essence.
          GET_RACE(vict) = race_idx;
          GET_REAL_ESS(vict) = new_ess;

          successful = TRUE;
          snprintf(buf, sizeof(buf), "%s's race set to %s by %s.\r\n", GET_CHAR_NAME(vict), pc_race_types[race_idx], GET_CHAR_NAME(ch));
          mudlog(buf, ch, LOG_WIZLOG, TRUE);

          // Update whotitle unless staff.
          if (!IS_SENATOR(vict)) {
            set_whotitle(vict, pc_race_types[race_idx]);
          }

          // Save the new race to disk.
          snprintf(buf, sizeof(buf), "UPDATE pfiles SET race=%d, Whotitle='%s' WHERE idnum = %ld;", 
                   GET_RACE(vict),
                   GET_WHOTITLE(vict),
                   GET_IDNUM(vict));
          mysql_wrapper(mysql, buf);
          break;
        }
      }
      if (!successful) {
        send_to_char(ch, "%s is not a valid race.\r\n", capitalize(val_arg));
        SET_CLEANUP(false);
        return;
      } else {
        send_to_char("OK.\r\n", ch);
        value = GET_RACE(vict);
      }
    }
    break;
  case 74: /* rolls for morts */
    SET_OR_REMOVE(PRF_FLAGS(vict), PRF_ROLLS);
    snprintf(buf, sizeof(buf),"%s changed %s's rolls flag setting.", GET_CHAR_NAME(ch), GET_NAME(vict));
    mudlog(buf, ch, LOG_WIZLOG, TRUE );
    break;
  case 75: /* multiplier */
    RANGE(0, 10000);
    GET_CHAR_MULTIPLIER(vict) = value;
    snprintf(buf, sizeof(buf),"%s changed %s's multiplier to %.2f.", GET_CHAR_NAME(ch), GET_NAME(vict), (float) GET_CHAR_MULTIPLIER(vict) / 100);
    mudlog(buf, ch, LOG_WIZLOG, TRUE );
    break;
  case 76: /* shotsfired */
    RANGE(0, 50000);
    SHOTS_FIRED(vict) = value;
    snprintf(buf, sizeof(buf),"%s changed %s's shots_fired to %d.", GET_CHAR_NAME(ch), GET_NAME(vict), value);
    mudlog(buf, ch, LOG_WIZLOG, TRUE );
    break;
  case 77: /* shotstriggered */
    RANGE(-1, 100);
    SHOTS_TRIGGERED(vict) = value;
    snprintf(buf, sizeof(buf),"%s changed %s's shots_triggered to %d.", GET_CHAR_NAME(ch), GET_NAME(vict), value);
    mudlog(buf, ch, LOG_WIZLOG, TRUE );
    break;
  case 78: /* powerpoints */
    RANGE(-10000, 10000);
    GET_PP(vict) = value;
    snprintf(buf, sizeof(buf),"%s changed %s's powerpoints to %d.", GET_CHAR_NAME(ch), GET_NAME(vict), value);
    mudlog(buf, ch, LOG_WIZLOG, TRUE );
    break;
  case 79: /* cyberdoc permission */
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_CYBERDOC);
    snprintf(buf, sizeof(buf),"%s turned %s's cyberdoc flag %s.", GET_CHAR_NAME(ch), GET_NAME(vict), PLR_FLAGGED(vict, PLR_CYBERDOC) ? "ON" : "OFF");
    mudlog(buf, ch, LOG_WIZLOG, TRUE );
    break;
  case 80: /* hardcore */
    if (PRF_FLAGS(vict).IsSet(PRF_HARDCORE)) {
      turn_hardcore_off_for_character(vict);
    } else {
      turn_hardcore_on_for_character(vict);
    }

    snprintf(buf, sizeof(buf),"%s turned %s's hardcore and nodelete flags %s.", GET_CHAR_NAME(ch), GET_NAME(vict), PRF_FLAGGED(vict, PRF_HARDCORE) ? "ON" : "OFF");
    mudlog(buf, ch, LOG_WIZLOG, TRUE );
    break;
  case 81: /* esshole */
    RANGE(0, GET_RACIAL_STARTING_ESSENCE_FOR_RACE(GET_RACE(vict)));
    snprintf(buf, sizeof(buf),"%s changed %s's esshole from %d to %d.", GET_CHAR_NAME(ch), GET_NAME(vict), GET_ESSHOLE(vict), value);
    GET_ESSHOLE(vict) = value;
    mudlog(buf, ch, LOG_WIZLOG, TRUE );
    break;
  case 82: /* no syspoint auto awards */
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_NO_AUTO_SYSP_AWARDS);
    snprintf(buf, sizeof(buf),"%s turned %s's no-auto-sysp-awards flag %s.", GET_CHAR_NAME(ch), GET_NAME(vict), PLR_FLAGGED(vict, PLR_NO_AUTO_SYSP_AWARDS) ? "ON" : "OFF");
    mudlog(buf, ch, LOG_WIZLOG, TRUE);
    break;
  case 83: /* no tells */
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_TELLS_MUTED);
    snprintf(buf, sizeof(buf),"%s turned %s's no-tells flag %s.", GET_CHAR_NAME(ch), GET_NAME(vict), PLR_FLAGGED(vict, PLR_TELLS_MUTED) ? "ON" : "OFF");
    mudlog(buf, ch, LOG_WIZLOG, TRUE);
    break;
  case 84: /* no ooc */
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_NOOOC);
    snprintf(buf, sizeof(buf),"%s turned %s's no-ooc flag %s.", GET_CHAR_NAME(ch), GET_NAME(vict), PLR_FLAGGED(vict, PLR_NOOOC) ? "ON" : "OFF");
    mudlog(buf, ch, LOG_WIZLOG, TRUE);
    break;
  case 85: /* no radio */
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_RADIO_MUTED);
    snprintf(buf, sizeof(buf),"%s turned %s's no-radio flag %s.", GET_CHAR_NAME(ch), GET_NAME(vict), PLR_FLAGGED(vict, PLR_RADIO_MUTED) ? "ON" : "OFF");
    mudlog(buf, ch, LOG_WIZLOG, TRUE);
    break;
  case 86: /* site hidden */
    SET_OR_REMOVE(PLR_FLAGS(vict), PLR_SITE_HIDDEN);
    log_vfprintf("CHEATLOG: %s turned %s's site-hidden flag %s.", GET_CHAR_NAME(ch), GET_NAME(vict), PLR_FLAGGED(vict, PLR_SITE_HIDDEN) ? "ON" : "OFF");
    break;
  case 87: /* lifestyle string */
    if (is_file) {
      send_to_char("Sorry, you can only do that to online characters right now.\r\n", ch);
      SET_CLEANUP(true);
      return;
    }
    set_lifestyle_string(vict, val_arg);
    mudlog_vfprintf(ch, LOG_WIZLOG, "%s changed %s's lifestyle to '%s^n'.", GET_CHAR_NAME(ch), GET_CHAR_NAME(vict), val_arg);
    strlcpy(buf, "OK.", sizeof(buf));
    break;
  case 88: /* highestindex */
    RANGE(0, 900);
    snprintf(buf, sizeof(buf),"%s changed %s's highest bioware index from %d to %d.", GET_CHAR_NAME(ch), GET_NAME(vict), GET_HIGHEST_INDEX(vict), value);
    GET_HIGHEST_INDEX(vict) = value;
    mudlog(buf, ch, LOG_WIZLOG, TRUE );
  case 89: /* strikes */
    {
      RANGE(0, 3);
      int old_val = GET_AUTOMOD_COUNTER(vict);
      GET_SETTABLE_AUTOMOD_COUNTER(vict) = value;
      mudlog_vfprintf(ch, LOG_WIZLOG, "%s changed %s's auto-moderator strikes from %d to %d.", GET_CHAR_NAME(ch), GET_NAME(vict), old_val, value);
    }
    break;
  default:
    snprintf(buf, sizeof(buf), "Can't set that!");
    break;
  }

  if (fields[l].type == BINARY) {
    snprintf(buf, sizeof(buf), "%s %s for %s.\r\n", fields[l].cmd, ONOFF(on), GET_CHAR_NAME(vict));
    CAP(buf);
  } else if (fields[l].type == NUMBER) {
    snprintf(buf, sizeof(buf), "%s's %s set to %d.\r\n", GET_CHAR_NAME(vict), fields[l].cmd, value);
  } else
    strlcat(buf, "\r\n", sizeof(buf));
  send_to_char(CAP(buf), ch);

  if (is_file) {
    SET_CLEANUP(true);
    send_to_char("Saved in file.\r\n", ch);
  }
}

ACMD(do_logwatch)
{
  one_argument(argument, buf);

  /*    Code that needs to be finished in the future. Has mistaken assumptions in it. -LS
  if (!*buf) {
    strncpy(buf, "You are currently watching the following:\r\n", sizeof(buf));
    for (int i = 0; i < NUM_LOGS; i++)
      if (PRF_FLAGGED(ch, i))
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  %s\r\n", log_types[i]);

    send_to_char(buf, ch);
    return;
  }

  for (int i = 0; i < NUM_LOGS; i++) {
    if (is_abbrev(buf, log_types[i])) {
      if (PRF_FLAGGED(ch, ...)) // This doesn't actually work, because there's no mapping between PRF and LOG yet. TODO.
    }
  }
  */

  if (!*buf) {
    snprintf(buf, sizeof(buf), "You are currently watching the following:\r\n%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
            (PRF_FLAGGED(ch, PRF_CONNLOG) ? "  ConnLog\r\n" : ""),
            (PRF_FLAGGED(ch, PRF_DEATHLOG) ? "  DeathLog\r\n" : ""),
            (PRF_FLAGGED(ch, PRF_MISCLOG) ? "  MiscLog\r\n" : ""),
            (PRF_FLAGGED(ch, PRF_WIZLOG) ? "  WizLog\r\n" : ""),
            (PRF_FLAGGED(ch, PRF_SYSLOG) ? "  SysLog\r\n" : ""),
            (PRF_FLAGGED(ch, PRF_ZONELOG) ? "  ZoneLog\r\n" : ""),
            (PRF_FLAGGED(ch, PRF_CHEATLOG) ? "  CheatLog\r\n" : ""),
            (PRF_FLAGGED(ch, PRF_BANLOG) ? "  BanLog\r\n" : ""),
            (PRF_FLAGGED(ch, PRF_GRIDLOG) ? "  GridLog\r\n" : ""),
            (PRF_FLAGGED(ch, PRF_WRECKLOG) ? "  WreckLog\r\n" : ""),
            (PRF_FLAGGED(ch, PRF_PGROUPLOG) ? "  PGroupLog\r\n" : ""),
            (PRF_FLAGGED(ch, PRF_HELPLOG) ? "  HelpLog\r\n" : ""),
            (PRF_FLAGGED(ch, PRF_PURGELOG) ? "  PurgeLog\r\n" : ""),
            (PRF_FLAGGED(ch, PRF_FUCKUPLOG) ? "  FuckupLog\r\n" : ""),
            (PRF_FLAGGED(ch, PRF_ECONLOG) ? "  EconLog\r\n" : ""),
            (PRF_FLAGGED(ch, PRF_RADLOG) ? "  RadLog\r\n" : ""),
            (PRF_FLAGGED(ch, PRF_IGNORELOG) ? "  IgnoreLog\r\n" : ""),
            (PRF_FLAGGED(ch, PRF_MAILLOG) ? "  MailLog\r\n" : "")
          );

    send_to_char(buf, ch);
    return;
  }

  if (is_abbrev(buf, "connlog")) {
    if (PRF_FLAGGED(ch, PRF_CONNLOG)) {
      send_to_char("You no longer watch the ConnLog.\r\n", ch);
      PRF_FLAGS(ch).RemoveBit(PRF_CONNLOG);
    } else {
      send_to_char("You will now see the ConnLog.\r\n", ch);
      PRF_FLAGS(ch).SetBit(PRF_CONNLOG);
    }
  } else if (is_abbrev(buf, "deathlog")) {
    if (PRF_FLAGGED(ch, PRF_DEATHLOG)) {
      send_to_char("You no longer watch the DeathLog.\r\n", ch);
      PRF_FLAGS(ch).RemoveBit(PRF_DEATHLOG);
    } else {
      send_to_char("You will now see the DeathLog.\r\n", ch);
      PRF_FLAGS(ch).SetBit(PRF_DEATHLOG);
    }
  } else if (is_abbrev(buf, "misclog")) {
    if (PRF_FLAGGED(ch, PRF_MISCLOG)) {
      send_to_char("You no longer watch the MiscLog.\r\n", ch);
      PRF_FLAGS(ch).RemoveBit(PRF_MISCLOG);
    } else {
      send_to_char("You will now see the MiscLog.\r\n", ch);
      PRF_FLAGS(ch).SetBit(PRF_MISCLOG);
    }
  } else if (is_abbrev(buf, "wizlog")) {
    if (PRF_FLAGGED(ch, PRF_WIZLOG)) {
      send_to_char("You no longer watch the WizLog.\r\n", ch);
      PRF_FLAGS(ch).RemoveBit(PRF_WIZLOG);
    } else if (access_level(ch, LVL_VICEPRES)) {
      send_to_char("You will now see the WizLog.\r\n", ch);
      PRF_FLAGS(ch).SetBit(PRF_WIZLOG);
    } else {
      send_to_char("You aren't permitted to view that log at your level.\r\n", ch);
    }
  } else if (is_abbrev(buf, "syslog")) {
    if (PRF_FLAGGED(ch, PRF_SYSLOG)) {
      send_to_char("You no longer watch the SysLog.\r\n", ch);
      PRF_FLAGS(ch).RemoveBit(PRF_SYSLOG);
    } else if (access_level(ch, LVL_CONSPIRATOR)) {
      send_to_char("You will now see the SysLog.\r\n", ch);
      PRF_FLAGS(ch).SetBit(PRF_SYSLOG);
    } else {
      send_to_char("You aren't permitted to view that log at your level.\r\n", ch);
    }
  } else if (is_abbrev(buf, "zonelog")) {
    if (PRF_FLAGGED(ch, PRF_ZONELOG)) {
      send_to_char("You no longer watch the ZoneLog.\r\n", ch);
      PRF_FLAGS(ch).RemoveBit(PRF_ZONELOG);
    } else if (access_level(ch, LVL_BUILDER)) {
      send_to_char("You will now see the ZoneLog.\r\n", ch);
      PRF_FLAGS(ch).SetBit(PRF_ZONELOG);
    } else {
      send_to_char("You aren't permitted to view that log at your level.\r\n", ch);
    }
  } else if (is_abbrev(buf, "cheatlog")) {
    if (PRF_FLAGGED(ch, PRF_CHEATLOG)) {
      send_to_char("You no longer watch the CheatLog.\r\n", ch);
      PRF_FLAGS(ch).RemoveBit(PRF_CHEATLOG);
    } else if (access_level(ch, LVL_VICEPRES)) {
      send_to_char("You will now see the CheatLog.\r\n", ch);
      PRF_FLAGS(ch).SetBit(PRF_CHEATLOG);
    } else {
      send_to_char("You aren't permitted to view that log at your level.\r\n", ch);
    }
  } else if (is_abbrev(buf, "banlog")) {
    if (PRF_FLAGGED(ch, PRF_BANLOG)) {
      send_to_char("You no longer watch the BanLog.\r\n", ch);
      PRF_FLAGS(ch).RemoveBit(PRF_BANLOG);
    } else {
      send_to_char("You will now see the BanLog.\r\n", ch);
      PRF_FLAGS(ch).SetBit(PRF_BANLOG);
    }
  } else if (is_abbrev(buf, "gridlog")) {
    if (PRF_FLAGGED(ch, PRF_GRIDLOG)) {
      send_to_char("You no longer watch the GridLog.\r\n", ch);
      PRF_FLAGS(ch).RemoveBit(PRF_GRIDLOG);
    } else if (access_level(ch, LVL_FIXER)) {
      send_to_char("You will now see the GridLog.\r\n", ch);
      PRF_FLAGS(ch).SetBit(PRF_GRIDLOG);
    } else {
      send_to_char("You aren't permitted to view that log at your level.\r\n", ch);
    }
  } else if (is_abbrev(buf, "wrecklog")) {
    if (PRF_FLAGGED(ch, PRF_WRECKLOG)) {
      send_to_char("You no longer watch the WreckLog.\r\n", ch);
      PRF_FLAGS(ch).RemoveBit(PRF_WRECKLOG);
    } else if (access_level(ch, LVL_FIXER)) {
      send_to_char("You will now see the WreckLog.\r\n", ch);
      PRF_FLAGS(ch).SetBit(PRF_WRECKLOG);
    } else {
      send_to_char("You aren't permitted to view that log at your level.\r\n", ch);
    }
  } else if (is_abbrev(buf, "pgrouplog")) {
    if (PRF_FLAGGED(ch, PRF_PGROUPLOG)) {
      send_to_char("You no longer watch the PGroupLog.\r\n", ch);
      PRF_FLAGS(ch).RemoveBit(PRF_PGROUPLOG);
    } else if (access_level(ch, LVL_VICEPRES)) {
      send_to_char("You will now see the PGroupLog.\r\n", ch);
      PRF_FLAGS(ch).SetBit(PRF_PGROUPLOG);
    } else {
      send_to_char("You aren't permitted to view that log at your level.\r\n", ch);
    }
  } else if (is_abbrev(buf, "helplog")) {
    if (PRF_FLAGGED(ch, PRF_HELPLOG)) {
      send_to_char("You no longer watch the HelpLog.\r\n", ch);
      PRF_FLAGS(ch).RemoveBit(PRF_HELPLOG);
    } else if (access_level(ch, LVL_DEVELOPER)) {
      send_to_char("You will now see the HelpLog.\r\n", ch);
      PRF_FLAGS(ch).SetBit(PRF_HELPLOG);
    } else {
      send_to_char("You aren't permitted to view that log at your level.\r\n", ch);
    }
  } else if (is_abbrev(buf, "purgelog")) {
    if (PRF_FLAGGED(ch, PRF_PURGELOG)) {
      send_to_char("You no longer watch the PurgeLog.\r\n", ch);
      PRF_FLAGS(ch).RemoveBit(PRF_PURGELOG);
    } else if (access_level(ch, LVL_ARCHITECT)) {
      send_to_char("You will now see the PurgeLog.\r\n", ch);
      PRF_FLAGS(ch).SetBit(PRF_PURGELOG);
    } else {
      send_to_char("You aren't permitted to view that log at your level.\r\n", ch);
    }
  } else if (is_abbrev(buf, "fuckuplog")) {
    if (PRF_FLAGGED(ch, PRF_FUCKUPLOG)) {
      send_to_char("You no longer watch the FuckupLog.\r\n", ch);
      PRF_FLAGS(ch).RemoveBit(PRF_FUCKUPLOG);
    } else if (access_level(ch, LVL_ARCHITECT)) {
      send_to_char("You will now see the FuckupLog.\r\n", ch);
      PRF_FLAGS(ch).SetBit(PRF_FUCKUPLOG);
    } else {
      send_to_char("You aren't permitted to view that log at your level.\r\n", ch);
    }
  } else if (is_abbrev(buf, "econlog")) {
    if (PRF_FLAGGED(ch, PRF_ECONLOG)) {
      send_to_char("You no longer watch the EconLog.\r\n", ch);
      PRF_FLAGS(ch).RemoveBit(PRF_ECONLOG);
    } else if (access_level(ch, LVL_VICEPRES)) {
      send_to_char("You will now see the EconLog.\r\n", ch);
      PRF_FLAGS(ch).SetBit(PRF_ECONLOG);
    } else {
      send_to_char("You aren't permitted to view that log at your level.\r\n", ch);
    }
  } else if (is_abbrev(buf, "radlog")) {
    if (PRF_FLAGGED(ch, PRF_RADLOG)) {
      send_to_char("You no longer watch the RadLog.\r\n", ch);
      PRF_FLAGS(ch).RemoveBit(PRF_RADLOG);
    } else if (access_level(ch, LVL_FIXER)) {
      send_to_char("You will now see the RadLog.\r\n", ch);
      PRF_FLAGS(ch).SetBit(PRF_RADLOG);
    } else {
      send_to_char("You aren't permitted to view that log at your level.\r\n", ch);
    }
  } else if (is_abbrev(buf, "ignorelog")) {
    if (PRF_FLAGGED(ch, PRF_IGNORELOG)) {
      send_to_char("You no longer watch the IgnoreLog.\r\n", ch);
      PRF_FLAGS(ch).RemoveBit(PRF_IGNORELOG);
    } else if (access_level(ch, LVL_ADMIN)) {
      send_to_char("You will now see the IgnoreLog.\r\n", ch);
      PRF_FLAGS(ch).SetBit(PRF_IGNORELOG);
    } else {
      send_to_char("You aren't permitted to view that log at your level.\r\n", ch);
    }
  } else if (is_abbrev(buf, "maillog")) {
    if (PRF_FLAGGED(ch, PRF_MAILLOG)) {
      send_to_char("You no longer watch the IgnoreLog.\r\n", ch);
      PRF_FLAGS(ch).RemoveBit(PRF_MAILLOG);
    } else if (access_level(ch, LVL_ADMIN)) {
      send_to_char("You will now see the MailLog.\r\n", ch);
      PRF_FLAGS(ch).SetBit(PRF_MAILLOG);
    } else {
      send_to_char("You aren't permitted to view that log at your level.\r\n", ch);
    }
  } else if (is_abbrev(buf, "all")) {
    if (!PRF_FLAGGED(ch, PRF_CONNLOG))
      PRF_FLAGS(ch).SetBit(PRF_CONNLOG);
    if (!PRF_FLAGGED(ch, PRF_DEATHLOG))
      PRF_FLAGS(ch).SetBit(PRF_DEATHLOG);
    if (!PRF_FLAGGED(ch, PRF_MISCLOG))
      PRF_FLAGS(ch).SetBit(PRF_MISCLOG);
    if (!PRF_FLAGGED(ch, PRF_BANLOG))
      PRF_FLAGS(ch).SetBit(PRF_BANLOG);
    if (!PRF_FLAGGED(ch, PRF_GRIDLOG) && access_level(ch, LVL_FIXER))
      PRF_FLAGS(ch).SetBit(PRF_GRIDLOG);
    if (!PRF_FLAGGED(ch, PRF_WRECKLOG) && access_level(ch, LVL_FIXER))
      PRF_FLAGS(ch).SetBit(PRF_WRECKLOG);
    if (!PRF_FLAGGED(ch, PRF_WIZLOG) && access_level(ch, LVL_VICEPRES))
      PRF_FLAGS(ch).SetBit(PRF_WIZLOG);
    if (!PRF_FLAGGED(ch, PRF_SYSLOG) && access_level(ch, LVL_ADMIN))
      PRF_FLAGS(ch).SetBit(PRF_SYSLOG);
    if (!PRF_FLAGGED(ch, PRF_ZONELOG) && access_level(ch, LVL_ADMIN))
      PRF_FLAGS(ch).SetBit(PRF_ZONELOG);
    if (!PRF_FLAGGED(ch, PRF_CHEATLOG) && access_level(ch, LVL_VICEPRES))
      PRF_FLAGS(ch).SetBit(PRF_CHEATLOG);
    if (!PRF_FLAGGED(ch, PRF_PGROUPLOG) && access_level(ch, LVL_VICEPRES))
      PRF_FLAGS(ch).SetBit(PRF_PGROUPLOG);
    if (!PRF_FLAGGED(ch, PRF_HELPLOG) && access_level(ch, LVL_DEVELOPER))
      PRF_FLAGS(ch).SetBit(PRF_HELPLOG);
    if (!PRF_FLAGGED(ch, PRF_PURGELOG) && access_level(ch, LVL_ARCHITECT))
      PRF_FLAGS(ch).SetBit(PRF_PURGELOG);
    if (!PRF_FLAGGED(ch, PRF_FUCKUPLOG) && access_level(ch, LVL_ARCHITECT))
      PRF_FLAGS(ch).SetBit(PRF_FUCKUPLOG);
    if (!PRF_FLAGGED(ch, PRF_ECONLOG) && access_level(ch, LVL_VICEPRES))
      PRF_FLAGS(ch).SetBit(PRF_ECONLOG);
    if (!PRF_FLAGGED(ch, PRF_RADLOG) && access_level(ch, LVL_FIXER))
      PRF_FLAGS(ch).SetBit(PRF_RADLOG);
    if (!PRF_FLAGGED(ch, PRF_IGNORELOG) && access_level(ch, LVL_ADMIN))
      PRF_FLAGS(ch).SetBit(PRF_IGNORELOG);
    if (!PRF_FLAGGED(ch, PRF_MAILLOG) && access_level(ch, LVL_ADMIN))
      PRF_FLAGS(ch).SetBit(PRF_MAILLOG);
    send_to_char("All available logs have been activated.\r\n", ch);
  } else if (is_abbrev(buf, "none")) {
    PRF_FLAGS(ch).RemoveBits(PRF_CONNLOG, PRF_DEATHLOG, PRF_MISCLOG, PRF_WIZLOG,
                             PRF_SYSLOG, PRF_ZONELOG, PRF_CHEATLOG, PRF_BANLOG, PRF_GRIDLOG,
                             PRF_WRECKLOG, PRF_PGROUPLOG, PRF_HELPLOG, PRF_PURGELOG,
                             PRF_FUCKUPLOG, PRF_ECONLOG, PRF_RADLOG, PRF_IGNORELOG, PRF_MAILLOG, ENDBIT);
    send_to_char("All logs have been disabled.\r\n", ch);
  } else
    send_to_char("Watch what log?\r\n", ch);

  return;
}

#define MOB(rnum) MOB_VNUM_RNUM(rnum)
#define OBJ(rnum) OBJ_VNUM_RNUM(rnum)
#define ROOM(rnum) world[rnum].number
#define VEH(rnum) VEH_VNUM_RNUM(rnum)
#define HOST(rnum) matrix[rnum].vnum
ACMD(do_zlist)
{
  int first, last, cmd_no, zone = 0;
  char buf[MAX_STRING_LENGTH*10];
  two_arguments(argument, buf, buf1);

  // Message sent in function.
  if (!is_olc_available(ch)) {
    return;
  }

  if (!*buf && !*buf1) {
    zone = real_zone(ch->player_specials->saved.zonenum);
    FAILURE_CASE(zone < 0, "You need to ZSWITCH to a valid zone first.");
    first = 0;
    last = zone_table[zone].num_cmds;
  } else if (*buf && !*buf1) {     // if there is not a second argument, then the
    zone = real_zone(atoi(buf));  // is considered the zone number
    FAILURE_CASE(zone < 0, "You need to ZSWITCH to a valid zone first.");
    first = 0;
    last = zone_table[zone].num_cmds;
  } else {
    zone = real_zone(ch->player_specials->saved.zonenum);
    FAILURE_CASE(zone < 0, "You need to ZSWITCH to a valid zone first.");
    first = MAX(0, MIN(atoi(buf), zone_table[zone].num_cmds));
    last = MIN(atoi(buf1), zone_table[zone].num_cmds);
  }

  // return if it us a non-existent zone
  if (zone < 0) {
    send_to_char("Zone: Non existent.\r\n", ch);
    return;
  }

  FAILURE_CASE(zone_table[zone].locked_to_non_editors && !can_edit_zone(ch, zone), "Sorry, that zone is locked to non-editors.");

  snprintf(buf, sizeof(buf), "Zone: %d (%d); Cmds: %d\r\n",
          zone_table[zone].number, zone,
          zone_table[zone].num_cmds);

  int last_mob = 0, last_veh = 0;

  for (cmd_no = first; cmd_no < last; cmd_no++) {
    if (ZCMD.if_flag)
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%3d) (if last)", cmd_no);
    else
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%3d) (always) ", cmd_no);

    switch (ZCMD.command) {
    default:
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "[nothing]\r\n");
      break;
    case 'M':
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "load (%ld) %s (%ld) at (%ld)\r\n",
              ZCMD.arg2, mob_proto[ZCMD.arg1].player.physical_text.name,
              MOB(ZCMD.arg1), ROOM(ZCMD.arg3));
      last_mob = ZCMD.arg1;
      break;
    case 'O':
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "load (%ld) %s (%ld) at (%ld)\r\n",
              ZCMD.arg2, obj_proto[ZCMD.arg1].text.name,
              OBJ(ZCMD.arg1), ROOM(ZCMD.arg3));
      break;
    case 'H':
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "load (%ld) %s (%ld) at host (%ld)\r\n",
              ZCMD.arg2, obj_proto[ZCMD.arg1].text.name,
              OBJ(ZCMD.arg1), HOST(ZCMD.arg3));
      break;
    case 'V':
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "load (%ld) %s (%ld) at (%ld)\r\n",
              ZCMD.arg2, veh_proto[ZCMD.arg1].short_description,
              VEH(ZCMD.arg1), ROOM(ZCMD.arg3));
      last_veh = ZCMD.arg1;
      break;
    case 'S':
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "load (%ld) %s (%ld) into %s (%ld)\r\n",
              ZCMD.arg2, mob_proto[ZCMD.arg1].player.physical_text.name,
              MOB(ZCMD.arg1), veh_proto[last_veh].short_description, VEH(last_veh));
      last_mob = ZCMD.arg1;
      break;
    case 'U':
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "upgrade (%ld) %s (%ld) on %s (%ld)\r\n",
              ZCMD.arg2, obj_proto[ZCMD.arg1].text.name,
              OBJ(ZCMD.arg1), veh_proto[last_veh].short_description, VEH(last_veh));
      break;
    case 'I':
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "load (%ld) %s (%ld) in %s (%ld)\r\n",
              ZCMD.arg2, obj_proto[ZCMD.arg1].text.name,
              OBJ(ZCMD.arg1), veh_proto[last_veh].short_description, VEH(last_veh));
      break;
    case 'P':
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "put (%ld) %s (%ld) in %s (%ld)\r\n",
              ZCMD.arg2, obj_proto[ZCMD.arg1].text.name,
              OBJ(ZCMD.arg1), obj_proto[ZCMD.arg3].text.name,
              OBJ(ZCMD.arg3));
      break;
    case 'G':
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "give (%ld) %s (%ld) to %s (%ld)\r\n",
              ZCMD.arg2, obj_proto[ZCMD.arg1].text.name,
              OBJ(ZCMD.arg1), mob_proto[last_mob].player.physical_text.name,
              MOB(last_mob));
      break;
    case 'E':
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "equip (%ld) %s (%ld) to %s (%ld) at %s\r\n",
              ZCMD.arg2, obj_proto[ZCMD.arg1].text.name,
              OBJ(ZCMD.arg1), mob_proto[last_mob].player.physical_text.name,
              MOB(last_mob), short_where[ZCMD.arg3]);
      break;
    case 'C':
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "install %s (%ld) to %s (%ld) at %s\r\n",
              obj_proto[ZCMD.arg1].text.name, OBJ(ZCMD.arg1),
              mob_proto[last_mob].player.physical_text.name, MOB(last_mob),
              (ZCMD.arg2 == 1 ? "bioware" : "cyberware"));
      break;
    case 'N':
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "give a total of %ld (%ld) %s (%ld) to %s (%ld)\r\n",
              ZCMD.arg3, ZCMD.arg2, obj_proto[ZCMD.arg1].text.name,
              OBJ(ZCMD.arg1), mob_proto[last_mob].player.physical_text.name,
              MOB(last_mob));
      break;
    case 'R':
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "remove %s (%ld) from (%ld)\r\n",
              obj_proto[ZCMD.arg2].text.name, OBJ(ZCMD.arg2),
              ROOM(ZCMD.arg1));
      break;
    case 'D':
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "set (%s) door at (%ld) to (%s)\r\n",
              dirs[ZCMD.arg2], ROOM(ZCMD.arg1),
              (ZCMD.arg3 == 0 ? "open" : (ZCMD.arg3 == 1 ? "closed"
                                          : "closed and locked")));
      break;
    }
  } // end for
  page_string(ch->desc, buf, 1);

}
#undef ZCMD
#undef MOB
#undef OBJ
#undef ROOM
#undef VEH
#undef HOST

ACMD(do_mlist)
{
  // Message sent in function.
  if (!is_olc_available(ch)) {
    return;
  }

  int first, last, nr, found = 0;
  two_arguments(argument, buf, buf2);

  if (!*buf || !*buf2) {
    send_to_char("Usage: mlist <beginning number> <ending number>\r\n", ch);
    return;
  }

  first = atoi(buf);
  last = atoi(buf2);

  if ((first < 0) || (last < 0)) {
    send_to_char("Values must be over 0.\r\n", ch);
    return;
  }

  if (first >= last) {
    send_to_char("Second value must be greater than first.\r\n", ch);
    return;
  }
#ifdef LIMIT_LIST_COMMANDS
  else if (last - first > LIST_COMMAND_LIMIT) {
    send_to_char(ch, "The range cannot exceed %d mobiles at a time.\r\n", LIST_COMMAND_LIMIT);
    return;
  }
#endif

  snprintf(buf, sizeof(buf), "Mobiles, %d to %d:\r\n", first, last);

  for (nr = MAX(0, real_mobile(first)); nr <= top_of_mobt &&
       (MOB_VNUM_RNUM(nr) <= last); nr++) {
    if (MOB_VNUM_RNUM(nr) < first)
      continue;

    if (!ch_can_bypass_edit_lock(ch, get_zone_from_vnum(MOB_VNUM_RNUM(nr))))
      continue;
    
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%5d. [%8ld] %s\r\n", ++found,
            MOB_VNUM_RNUM(nr), mob_proto[nr].player.physical_text.name);
  }

  if (!found)
    send_to_char("No mobiles were found in those parameters.\r\n", ch);
  else
    page_string(ch->desc, buf, 1);
}

ACMD(do_ilist)
{
  // Message sent in function.
  if (!is_olc_available(ch)) {
    return;
  }

  int first, last, nr, found = 0;

  two_arguments(argument, buf, buf2);

  if (!*buf || !*buf2) {
    send_to_char("Usage: ilist <beginning number> <ending number>\r\n", ch);
    return;
  }
  first = atoi(buf);
  last = atoi(buf2);


  if ((first < 0) || (last < 0)) {
    send_to_char("Values must be over 0.\r\n", ch);
    return;
  }
#ifdef LIMIT_LIST_COMMANDS
  else if (last - first > LIST_COMMAND_LIMIT) {
    send_to_char(ch, "The range cannot exceed %d objects at a time.\r\n", LIST_COMMAND_LIMIT);
    return;
  }
#endif

  if (first >= last) {
    send_to_char("Second value must be greater than first.\r\n", ch);
    return;
  }

  snprintf(buf, sizeof(buf), "Objects, %d to %d:\r\n", first, last);

  for (nr = MAX(0, real_object(first)); nr <= top_of_objt && (OBJ_VNUM_RNUM(nr) <= last); nr++) {
    if (OBJ_VNUM_RNUM(nr) < first)
      continue;
    
    if (!ch_can_bypass_edit_lock(ch, get_zone_from_vnum(OBJ_VNUM_RNUM(nr))))
      continue;
    
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%5d. [%8ld x%4d] %s%s\r\n", ++found,
    OBJ_VNUM_RNUM(nr),
    ObjList.CountObj(nr),
    obj_proto[nr].text.name,
    obj_proto[nr].source_info ? "  ^g(canon)^n" : "");
  }

  if (!found)
    send_to_char("No objects were found in those parameters.\r\n", ch);
  else
    page_string(ch->desc, buf, 1);
}

ACMD(do_vlist)
{
  // Message sent in function.
  if (!is_olc_available(ch)) {
    return;
  }

  int first, last, nr, found = 0;

  two_arguments(argument, buf, buf2);

  if (!*buf || !*buf2) {
    send_to_char("Usage: vlist <beginning number> <ending number>\r\n", ch);
    return;
  }
  first = atoi(buf);
  last = atoi(buf2);


  if ((first < 0) || (last < 0)) {
    send_to_char("Values must be over 0.\r\n", ch);
    return;
  }

  if (first >= last) {
    send_to_char("Second value must be greater than first.\r\n", ch);
    return;
  }

  snprintf(buf, sizeof(buf), "Vehicles, %d to %d:\r\n", first, last);

  for (nr = MAX(0, real_vehicle(first)); nr <= top_of_veht &&
       (VEH_VNUM_RNUM(nr) <= last); nr++) {
    if (VEH_VNUM_RNUM(nr) < first)
      continue;
    
    if (!ch_can_bypass_edit_lock(ch, get_zone_from_vnum(VEH_VNUM_RNUM(nr))))
      continue;

    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%5d. [%8ld] %s\r\n", ++found,
            VEH_VNUM_RNUM(nr), veh_proto[nr].short_description);
  }

  if (!found)
    send_to_char("No vehicles were found in those parameters.\r\n", ch);
  else
    page_string(ch->desc, buf, 1);
}

ACMD(do_qlist)
{
  // Message sent in function.
  if (!is_olc_available(ch)) {
    return;
  }

  long first, last, nr, found = 0;

  two_arguments(argument, buf, buf2);

  if (!*buf || !*buf2) {
    send_to_char("Usage: qlist <beginning number> <ending number>\r\n", ch);
    return;
  }

  first = atoi(buf);
  last = atoi(buf2);

  if ((first < 0) || (last < 0)) {
    send_to_char("Values must be over 0.\r\n", ch);
    return;
  }

  if (first >= last) {
    send_to_char("Second value must be greater than first.\r\n", ch);
    return;
  }

  snprintf(buf, sizeof(buf), "Quests, %ld to %ld:\r\n", first, last);

  int real_mob;
  for (nr = MAX(0, real_quest(first)); nr <= top_of_questt &&
       (quest_table[nr].vnum <= last); nr++) {
    if (quest_table[nr].vnum < first)
      continue;

    if (!ch_can_bypass_edit_lock(ch, get_zone_from_vnum(quest_table[nr].vnum)))
      continue;
    
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%5ld. [%8ld] %s (%ld)\r\n",
            ++found, quest_table[nr].vnum,
            (real_mob = real_mobile(quest_table[nr].johnson)) < 0 ? "None" : GET_NAME(&mob_proto[real_mob]),
            quest_table[nr].johnson);
  }

  if (!found)
    send_to_char("No quests were found in those parameters.\r\n", ch);
  else
    page_string(ch->desc, buf, 1);
}

// Shitty little function that will show up in the stacktrace to help diagnose what's crashing.
bool debug_bounds_check_rlist(int nr, int last) {
  if (nr > top_of_world)
    return FALSE;
  if (world[nr].number > last)
    return FALSE;
  return TRUE;
}

ACMD(do_rlist)
{
  // Message sent in function.
  if (!is_olc_available(ch)) {
    return;
  }

  int first, last, nr, found = 0;

  two_arguments(argument, buf, buf2);

  if (!*buf || !*buf2) {
    send_to_char("Usage: rlist <beginning number> <ending number>\r\n", ch);
    return;
  }

  first = atoi(buf);
  last = atoi(buf2);

  if ((first < 0) || (last < 0)) {
    send_to_char("Values must be over 0.\r\n", ch);
    return;
  }

  if (first >= last) {
    send_to_char("Second value must be greater than first.\r\n", ch);
    return;
  }
#ifdef LIMIT_LIST_COMMANDS
  else if (last - first > LIST_COMMAND_LIMIT) {
    send_to_char(ch, "The range cannot exceed %d rooms at a time.\r\n", LIST_COMMAND_LIMIT);
    return;
  }
#endif

  snprintf(buf, sizeof(buf), "Rooms, %d to %d:\r\n", first, last);

  for (nr = MAX(0, real_room(first)); debug_bounds_check_rlist(nr, last); nr++) {
    if (world[nr].number < first)
      continue;
    
    if (!ch_can_bypass_edit_lock(ch, get_zone_from_vnum(world[nr].number)))
      continue;

    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%5d. [%8ld] (%3d) %s\r\n", ++found,
            world[nr].number, world[nr].zone, world[nr].name);
  }

  if (!found)
    send_to_char("No rooms were found in those parameters.\r\n", ch);
  else
    page_string(ch->desc, buf, 1);
}

ACMD(do_hlist)
{
  // Message sent in function.
  if (!is_olc_available(ch)) {
    return;
  }

  int first, last, nr, found = 0;

  two_arguments(argument, buf, buf2);

  if (!*buf || !*buf2) {
    send_to_char("Usage: hlist <beginning number> <ending number>\r\n", ch);
    return;
  }

  first = atoi(buf);
  last = atoi(buf2);

  if ((first < 0) || (last < 0)) {
    send_to_char("Values must be over 0.\r\n", ch);
    return;
  }

  if (first >= last) {
    send_to_char("Second value must be greater than first.\r\n", ch);
    return;
  }
#ifdef LIMIT_LIST_COMMANDS
  else if (last - first > LIST_COMMAND_LIMIT) {
    send_to_char(ch, "The range cannot exceed %d hosts at a time.\r\n", LIST_COMMAND_LIMIT);
    return;
  }
#endif

  snprintf(buf, sizeof(buf), "Hosts, %d to %d:\r\n", first, last);

  for (nr = MAX(0, real_host(first)); nr <= top_of_matrix &&
       (matrix[nr].vnum <= last); nr++) {
    if (matrix[nr].vnum < first)
      continue;
    
    if (!ch_can_bypass_edit_lock(ch, get_zone_from_vnum(matrix[nr].vnum)))
      continue;

    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%5d. [%8ld] %s\r\n", ++found,
            matrix[nr].vnum, matrix[nr].name);
  }

  if (!found)
    send_to_char("No hosts were found in those parameters.\r\n", ch);
  else
    page_string(ch->desc, buf, 1);
}

ACMD(do_iclist)
{
  // Message sent in function.
  if (!is_olc_available(ch)) {
    return;
  }

  int first, last, nr, found = 0;

  two_arguments(argument, buf, buf2);

  if (!*buf || !*buf2) {
    send_to_char("Usage: iclist <beginning number> <ending number>\r\n", ch);
    return;
  }

  first = atoi(buf);
  last = atoi(buf2);

  if ((first < 0) || (last < 0)) {
    send_to_char("Values must be over 0.\r\n", ch);
    return;
  }

  if (first >= last) {
    send_to_char("Second value must be greater than first.\r\n", ch);
    return;
  }
#ifdef LIMIT_LIST_COMMANDS
  else if (last - first > LIST_COMMAND_LIMIT) {
    send_to_char(ch, "The range cannot exceed %d ICs at a time.\r\n", LIST_COMMAND_LIMIT);
    return;
  }
#endif

  snprintf(buf, sizeof(buf), "IC, %d to %d:\r\n", first, last);

  for (nr = MAX(0, real_ic(first)); nr <= top_of_ic && (ic_index[nr].vnum <= last); nr++) {
    if (ic_index[nr].vnum < first)
      continue;

    if (!ch_can_bypass_edit_lock(ch, get_zone_from_vnum(ic_index[nr].vnum)))
      continue;

    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%5d. [%8ld] %-50s [%s-%d]\r\n", ++found,
            ic_index[nr].vnum, ic_proto[nr].name, ic_type[ic_proto[nr].ic.type], ic_proto[nr].ic.rating);
  }

  if (!found)
    send_to_char("No IC were found in those parameters.\r\n", ch);
  else
    page_string(ch->desc, buf, 1);
}

ACMD(do_slist)
{
  // Message sent in function.
  if (!is_olc_available(ch)) {
    return;
  }

  int first, last, nr, found = 0;

  two_arguments(argument, buf, buf2);

  if (!*buf || !*buf2) {
    send_to_char("Usage: slist <beginning number> <ending number>\r\n", ch);
    return;
  }

  first = atoi(buf);
  last = atoi(buf2);

  if ((first < 0) || (last < 0)) {
    send_to_char("Values must be over 0.\r\n", ch);
    return;
  }

  if (first >= last) {
    send_to_char("Second value must be greater than first.\r\n", ch);
    return;
  }

  snprintf(buf, sizeof(buf), "Shops, %d to %d:\r\n", first, last);

  int real_mob;
  for (nr = MAX(0, real_shop(first)); nr <= top_of_shopt && (shop_table[nr].vnum <= last); nr++) {
    if (shop_table[nr].vnum < first || shop_table[nr].vnum > last) {
      // send_to_char(ch, "Skipping shop %ld: Not in range %d ≤ X ≤ %d.\r\n", shop_table[nr].vnum, first, last);
      continue;
    }
    
    if (!ch_can_bypass_edit_lock(ch, get_zone_from_vnum(shop_table[nr].vnum))) {
      if (GET_LEVEL(ch) == LVL_PRESIDENT)
        send_to_char(ch, "------ [%8ld] %s <hidden>\r\n", shop_table[nr].vnum, (real_mob = real_mobile(shop_table[nr].keeper)) < 0 ? "None" : GET_NAME(&mob_proto[real_mob]));
      continue;
    }

    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%5d. [%8ld] %s %s (%ld)\r\n", ++found,
            shop_table[nr].vnum,
            vnum_from_non_approved_zone(shop_table[nr].keeper) ? " " : (PRF_FLAGGED(ch, PRF_SCREENREADER) ? "(approved)" : "*"),
            (real_mob = real_mobile(shop_table[nr].keeper)) < 0 ? "None" : GET_NAME(&mob_proto[real_mob]),
            shop_table[nr].keeper);
  }

  if (!found)
    send_to_char(ch, "No shops were found between %d and %d.\r\n", first, last);
  else
    page_string(ch->desc, buf, 1);
}

ACMD(do_settime)
{

  int value;
  two_arguments(argument, buf, buf1);

  if (!*buf || !*buf1) {
    send_to_char("Usage: settime <h|d|m|y> <value>.\r\n", ch);
    return;
  }

  value = atoi(buf1);

  switch(*buf) {
  case 'h' :
    RANGE(0, 23);
    time_info.hours = value;
    if ((time_info.hours < 7) || (time_info.hours > 20))
      weather_info.sunlight = SUN_DARK;
    else
      if (time_info.hours < 9)
        weather_info.sunlight = SUN_RISE;
      else if (time_info.hours < 18)
        weather_info.sunlight = SUN_LIGHT;
      else
        weather_info.sunlight = SUN_SET;
    break;
  case 'd' :
    switch(time_info.month) {
    case 2:
      RANGE(0, 28);
      time_info.day = value;
      break;
    case 3:
    case 5:
    case 8:
    case 10:
      RANGE(0, 30);
      time_info.day = value;
      break;
    default:
      RANGE(0, 31);
      time_info.day = value;
      break;
    }
    if(time_info.day < 7)
      weather_info.moonphase = MOON_NEW;
    else if(time_info.day > 7 && time_info.day < 14)
      weather_info.moonphase = MOON_WAX;
    else if(time_info.day > 14 && time_info.day < 21)
      weather_info.moonphase = MOON_FULL;
    else
      weather_info.moonphase = MOON_WANE;
    break;
  case 'm':
    RANGE(0, 11);
    time_info.month = value;
    break;
  case 'y':
    RANGE(0, 3000);
    time_info.year = value;
    break;
  }

  send_to_char("Ok.\r\n", ch);
  return;
}

// Disabled due to potential for abuse: photos etc have system-generated restrings.
// This can be re-enabled when we have a tracker for who wrote the restring.
ACMD(do_destring)
{
  return;
  /*
  struct obj_data *obj;

  one_argument(argument, arg);

  if (!*arg) {
    send_to_char("You have to destring something!\r\n", ch);
    return;
  }
  if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
    send_to_char("You don't have that item.\r\n", ch);
    return;
  }
  if (!obj->restring) {
    send_to_char("That item hasn't been restrung.\r\n", ch);
    return;
  }
  GET_KARMA(ch) += 125;
  DELETE_AND_NULL_ARRAY(obj->restring);
  send_to_char(ch, "%s successfully destrung.\r\n", obj->text.name);
  */
}

bool restring_with_args(struct char_data *ch, char *argument, bool using_sysp) {
  struct obj_data *obj;
  half_chop(argument, arg, buf, sizeof(buf));

  if (!*arg) {
    send_to_char("Syntax: RESTRING <item> <new short description>\r\n", ch);
    return FALSE;
  }

  if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
    send_to_char("You're not carrying that item.\r\n", ch);
    return FALSE;
  }

  if (!*buf) {
    if (obj->restring) {
      send_to_char(ch, "%s's current restring is: %s\r\n", GET_OBJ_RAW_NAME(obj), double_up_color_codes(obj->restring));
    } else {
      send_to_char(ch, "%s isn't currently restrung. Syntax: RESTRING %s <string to change it to>\r\n", GET_OBJ_NAME(obj), arg);
    }
    return FALSE;
  }

  FALSE_CASE(GET_OBJ_TYPE(obj) == ITEM_GUN_ACCESSORY && GET_ACCESSORY_TYPE(obj) != ACCESS_SMARTGOGGLE, "Sorry, gun attachments can't be restrung.");
  FALSE_CASE(GET_OBJ_TYPE(obj) == ITEM_MOD || GET_OBJ_TYPE(obj) == ITEM_GUN_AMMO, "Sorry, vehicle mods and ammo containers can't be restrung.");
  FALSE_CASE(GET_OBJ_TYPE(obj) == ITEM_CLIMBING && GET_OBJ_VAL(obj, 1) == CLIMBING_TYPE_WATER_WINGS, "No amount of cosmetic changes could hide the garishness of water wings.");
  FALSE_CASE_PRINTF(GET_OBJ_TYPE(obj) == ITEM_PET, "%s gives you an offended look and refuses to cooperate.", CAP(GET_OBJ_NAME(obj)));
  FALSE_CASE(GET_OBJ_VNUM(obj) == OBJ_EYEBALL_KEY, "You're pretty sure that trying to alter the eyeball would ruin it.");
  FALSE_CASE(GET_OBJ_TYPE(obj) == ITEM_VEHCONTAINER, "Sorry, vehicle containers can't be restrung.");
  FALSE_CASE(obj_is_a_vehicle_title(obj), "Vehicle titles are consumable, so they can't be restrung.");

  // Ensure we don't contain any forbidden phrases. Error messages are shown in-function.
  if (check_for_banned_content(buf, ch)) {
    return FALSE;
  }

  // If it's too short to contain a terminal color code,
  if (strlen(buf) < 2 || 
      // Or the second to last character is not ^
      (buf[strlen(buf) - 2] != '^' 
       // Or the last character is not n
       || (buf[strlen(buf) - 1] != 'n' && buf[strlen(buf) - 1] != 'N')))
  {
    // Add a terminal color code.
    strlcat(buf, "^n", sizeof(buf));
  }

  int length_with_no_color = get_string_length_after_color_code_removal(buf, ch);

  // Silent failure: We already sent the error message in get_string_length_after_color_code_removal().
  if (length_with_no_color == -1)
    return FALSE;

  if (length_with_no_color >= LINE_LENGTH) {
    send_to_char(ch, "That restring is too long, please shorten it. The maximum length after color code removal is %d characters.\r\n", LINE_LENGTH - 1);
    return FALSE;
  }

  if (strlen(buf) >= MAX_RESTRING_LENGTH) {
    send_to_char(ch, "That restring is too long, please shorten it. The maximum length with color codes included is %d characters.\r\n", MAX_RESTRING_LENGTH - 1);
    return FALSE;
  }

  struct obj_data *shopcontainer = NULL;
  if (GET_OBJ_TYPE(obj) == ITEM_SHOPCONTAINER) {
    shopcontainer = obj;
    obj = shopcontainer->contains;

    if (GET_OBJ_VNUM(obj) == OBJ_CUSTOM_NERPS_BIOWARE || GET_OBJ_VNUM(obj) == OBJ_CUSTOM_NERPS_CYBERWARE) {
      if (GET_LEVEL(ch) < LVL_FIXER) {
        send_to_char(ch, "Sorry, only a Fixer-grade staff member can restring %s.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));
        return FALSE;
      }
    }
  }

  if (using_sysp) {
    if (GET_SYSTEM_POINTS(ch) < SYSP_RESTRING_COST) {
      send_to_char(ch, "It costs %d system point%s to restring something, and you only have %d.\r\n",
                   SYSP_RESTRING_COST,
                   SYSP_RESTRING_COST == 1 ? "" : "s",
                   GET_SYSTEM_POINTS(ch));
      return FALSE;
    }
    GET_SYSTEM_POINTS(ch) -= SYSP_RESTRING_COST;
  } else if (PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED)) {
    // In chargen, you can restring already-restrung items for free.
    if (!obj->restring) {
      if (!GET_RESTRING_POINTS(ch)) {
        send_to_char("You don't have enough restring points left to restring that.\r\n", ch);
        return FALSE;
      }
      GET_RESTRING_POINTS(ch)--;
      send_to_char(ch, "You spend one free restring point. (^c%d^n left)\r\n", GET_RESTRING_POINTS(ch));
    } else {
      send_to_char("You rewrite the restring, which is free during character generation.\r\n", ch);
    }
  } else {
    if (GET_KARMA(ch) < 250) {
      send_to_char("You don't have enough karma to restring that. It costs 2.5 karma.\r\n", ch);
      return FALSE;
    }
    GET_KARMA(ch) -= 250;
  }

  snprintf(buf2, sizeof(buf2), "%s restrung '%s' (%ld) to '%s'", GET_CHAR_NAME(ch), obj->text.name, GET_OBJ_VNUM(obj), buf);
  mudlog(buf2, ch, LOG_WIZLOG, TRUE);

  DELETE_ARRAY_IF_EXTANT(obj->restring);
  obj->restring = str_dup(buf);
  send_to_char(ch, "%s successfully restrung.\r\n", obj->text.name);

  // Repackage it to reflect its restrung status.
  if (shopcontainer) {
    extern struct obj_data *shop_package_up_ware(struct obj_data *obj);

    obj_from_obj(obj);
    obj_to_char(shop_package_up_ware(obj), ch);

    GET_OBJ_EXTRA(shopcontainer).RemoveBit(ITEM_EXTRA_KEPT);
    extract_obj(shopcontainer);
  }

  return TRUE;
}

ACMD(do_restring) {
  restring_with_args(ch, argument, FALSE);
}

ACMD(do_redesc) {
  struct obj_data *obj;
  half_chop(argument, arg, buf, sizeof(buf));

  FAILURE_CASE(!*arg, "Syntax: REDESC <item> <new full description>");
  FAILURE_CASE(!*buf, "You need to provide a short desc.");
  FAILURE_CASE(!(obj = get_obj_in_list_vis(ch, arg, ch->carrying)), "You're not carrying that item.");
  FAILURE_CASE(GET_OBJ_TYPE(obj) != ITEM_CUSTOM_DECK, "You can only redesc custom decks.");

  // TODO: Wrap this in an ifcheck so we don't double up on neutrals.
  strlcat(buf, "^n", sizeof(buf));

  // Silent failure: We already sent the error message in get_string_length_after_color_code_removal().
  if (get_string_length_after_color_code_removal(buf, ch) == -1)
    return;

  snprintf(buf2, sizeof(buf2), "%s redesced '%s' (%ld) (see command invocation for details)", GET_CHAR_NAME(ch), GET_OBJ_NAME(obj), GET_OBJ_VNUM(obj));
  mudlog(buf2, ch, LOG_WIZLOG, TRUE);

  DELETE_ARRAY_IF_EXTANT(obj->photo);
  obj->photo = str_dup(buf);
  send_to_char(ch, "%s successfully redesced.\r\n", GET_OBJ_NAME(obj));
}

ACMD(do_incognito)
{
  sh_int i;
  one_argument(argument, arg);
  if (!*arg)
    if (GET_INCOG_LEV(ch)) {
      GET_INCOG_LEV(ch) = 0;
    } else {
      GET_INCOG_LEV(ch) = 2;
    }
  else {
    i = atoi(arg);
    if (i > GET_LEVEL(ch)) {
      send_to_char("You cannot go Incognito above your own level.\r\n", ch);
      return;
    }
    if (i < 1) {
      GET_INCOG_LEV(ch) = 0;
    } else {
      GET_INCOG_LEV(ch) = i;
    }
  }

  if (GET_INCOG_LEV(ch) == 0)
    send_to_char("You are no longer incognito on the wholist.\r\n", ch);
  else
    send_to_char(ch, "Your wholist incognito is level %d.\r\n", i);
}

ACMD(do_zone) {
  send_to_char(ch, "Current zone: %d\r\n", ch->player_specials->saved.zonenum);
}

ACMD(do_room) {
  send_to_char(ch, "Current room num: %ld\r\n", ch->in_room->number);
}

ACMD(do_perfmon) {
    char arg1[MAX_INPUT_LENGTH];

    if (!ch->desc) {
      log("No descriptor in do_perfmon");
      return;
    }

    argument = one_argument(argument, arg1);

    if (arg1[0] == '\0')
    {
        send_to_char(
            "perfmon all             - Print all perfmon info.\r\n"
            "perfmon summ            - Print summary,\r\n"
            "perfmon prof            - Print profiling info.\r\n"
            "perfmon sect <section>  - Print profiling info for section.\r\n",
            ch );
        return;
    }

    if (!str_cmp( arg1, "all"))
    {
        char buf[MAX_STRING_LENGTH];

        size_t written = PERF_repr( buf, sizeof(buf) );
        written = PERF_prof_repr_total( buf + written, sizeof(buf) - written);
        page_string(ch->desc, buf, 1);

        return;
    }
    else if (!str_cmp( arg1, "summ"))
    {
        char buf[MAX_STRING_LENGTH];

        PERF_repr( buf, sizeof(buf) );
        page_string(ch->desc, buf, 1);
        return;
    }
    else if (!str_cmp( arg1, "prof"))
    {
        char buf[MAX_STRING_LENGTH];

        PERF_prof_repr_total( buf, sizeof(buf) );
        page_string(ch->desc, buf, 1);

        return;
    }
    else if (!str_cmp( arg1, "sect"))
    {
        char buf[MAX_STRING_LENGTH];

        PERF_prof_repr_sect( buf, sizeof(buf), argument );
        page_string(ch->desc, buf, 1);

        return;
    }
    else
    {
        char empty_arg[] = {0};
        do_perfmon( ch, empty_arg, cmd, subcmd );
        return;
    }
}

ACMD(do_setfind)
{
  int number;

  one_argument(argument, buf2);

  // Message sent in function.
  if (!is_olc_available(ch)) {
    return;
  }

  if (!*buf2) {
    send_to_char(ch, "Sets 1 to 999:\r\n");
    for (int i = 1; i < 1000; i++) {
      for (int obj_idx = 0; obj_idx <= top_of_objt; obj_idx++) {
        if (GET_OBJ_TYPE(&obj_proto[obj_idx]) == ITEM_WORN && GET_WORN_MATCHED_SET(&obj_proto[obj_idx]) == i) {
          send_to_char(ch, "[%5ld] Set ^y%3d^n: ^c%3.2fb %3.2fi^n %s^n%s\r\n",
                  OBJ_VNUM_RNUM(obj_idx),
                  i,
                  (float) GET_WORN_BALLISTIC(&obj_proto[obj_idx]) / 100,
                  (float) GET_WORN_IMPACT(&obj_proto[obj_idx]) / 100,
                  obj_proto[obj_idx].text.name,
                  obj_proto[obj_idx].source_info ? "  ^g(canon)^n" : "");
        }
      }
    }
    return;
  }

  if ((number = atoi(buf2)) < 0) {
    send_to_char("A NEGATIVE number??\r\n", ch);
    return;
  }

  for (int obj_idx = 0; obj_idx <= top_of_objt; obj_idx++) {
    if (GET_OBJ_TYPE(&obj_proto[obj_idx]) == ITEM_WORN && GET_WORN_MATCHED_SET(&obj_proto[obj_idx]) == number) {
      send_to_char(ch, "[%5ld] Set ^y%3d^n: ^c%3.2fb %3.2fi^n %s^n%s\r\n",
              OBJ_VNUM_RNUM(obj_idx),
              number,
              (float) GET_WORN_BALLISTIC(&obj_proto[obj_idx]) / 100,
              (float) GET_WORN_IMPACT(&obj_proto[obj_idx]) / 100,
              obj_proto[obj_idx].text.name,
              obj_proto[obj_idx].source_info ? "  ^g(canon)^n" : "");
    }
  }
}

ACMD(do_shopfind)
{
  vnum_t number;
  vnum_t location;

  one_argument(argument, buf2);

  // Message sent in function.
  if (!is_olc_available(ch)) {
    return;
  }

  if (!*buf2) {
    send_to_char("Usage: shopfind <number or keyword>\r\n", ch);
    return;
  }

  if ((number = atol(buf2)) < 0) {
    send_to_char("A NEGATIVE number??\r\n", ch);
    return;
  }

  if (number) {
    rnum_t obj_rnum = real_object(number);
    FAILURE_CASE_PRINTF(obj_rnum < 0, "There is no item with vnum %ld.", number);
    struct obj_data *obj = &obj_proto[obj_rnum];

#ifndef IS_BUILDPORT
    mudlog_vfprintf(ch, LOG_WIZLOG, "Ran shopfind for %s (%ld)", GET_OBJ_NAME(obj), number);
#endif
    send_to_char(ch, "Shops selling %s (%ld):\r\n", GET_OBJ_NAME(obj), number);
  } else {
#ifndef IS_BUILDPORT
    mudlog_vfprintf(ch, LOG_WIZLOG, "Ran shopfind for keyword '%s'", buf2);
#endif
    send_to_char(ch, "Shops selling items with keyword %s:\r\n", buf2);
  }

  int index = 0;
  for (int shop_nr = 0; shop_nr <= top_of_shopt; shop_nr++) {
    int real_mob = real_mobile(shop_table[shop_nr].keeper);
    if (real_mob < 0) {
      snprintf(buf, sizeof(buf), "Warning: Shop %ld does not have a valid keeper!", shop_table[shop_nr].vnum);
      mudlog(buf, NULL, LOG_SYSLOG, TRUE);
      continue;
    }

    // Get their location.
    location = -1;
    for (struct char_data *i = character_list; i; i = i->next_in_character_list)
      if (GET_MOB_VNUM(i) == shop_table[shop_nr].keeper && i->in_room) {
        location = GET_ROOM_VNUM(i->in_room);
        break;
      }

    for (struct shop_sell_data *sell = shop_table[shop_nr].selling; sell; sell = sell->next) {
      rnum_t real_obj = real_object(sell->vnum);
      if (real_obj < 0) {
        mudlog_vfprintf(NULL, LOG_SYSLOG, "Warning: Shop %ld has invalid item %ld for sale!", shop_table[shop_nr].vnum, sell->vnum);
        continue;
      }

      if (number) {
        if (sell->vnum == number) {
          send_to_char(ch, "%3d)  Shop %8ld (%s @ %ld) for %.0f nuyen\r\n",
                       ++index,
                       shop_table[shop_nr].vnum,
                       mob_proto[real_mob].player.physical_text.name,
                       location,
                       GET_OBJ_COST(&obj_proto[real_obj]) * shop_table[shop_nr].profit_buy);
        }
      } else if (keyword_appears_in_obj(buf2, &obj_proto[real_obj])) {
        send_to_char(ch, "%3d)  Shop %8ld (%s @ %ld) sells %s (%ld) for %.0f nuyen\r\n",
                     ++index,
                     shop_table[shop_nr].vnum,
                     mob_proto[real_mob].player.physical_text.name,
                     location,
                     GET_OBJ_NAME(&obj_proto[real_obj]),
                     GET_OBJ_VNUM(&obj_proto[real_obj]),
                     GET_OBJ_COST(&obj_proto[real_obj]) * shop_table[shop_nr].profit_buy);
      }
    }
  }
  if (index == 0)
    send_to_char("  - None.\r\n", ch);
}

void print_x_fuckups(struct char_data *ch, int print_count) {
  MYSQL_RES *res;
  MYSQL_ROW row;
  char query_buf[1000];

  snprintf(query_buf, sizeof(query_buf), "SELECT Name, Count FROM command_fuckups ORDER BY COUNT DESC LIMIT %d;", print_count);

  mysql_wrapper(mysql, query_buf);
  if (!(res = mysql_use_result(mysql))) {
    mudlog("SYSERR: Failed to access command fuckups data!", ch, LOG_SYSLOG, TRUE);
    send_to_char(ch, "Sorry, the fuckups system is offline at the moment.\r\n");
    return;
  }

  send_to_char(ch, "^CTop %d fuckups:^n\r\n", print_count);
  int counter = 1;
  while ((row = mysql_fetch_row(res))) {
    send_to_char(ch, "%2d) %-20s: %-5s\r\n", counter++, row[0], row[1]);
  }
  if (counter == 1)
    send_to_char(ch, "...None! Lucky you, not a single command typo in the entire game.\r\n");
  mysql_free_result(res);
}

ACMD(do_fuckups) {
  // fuckups [x] -- list the top X fuckups, default 10
  // fuckups del <cmd> -- remove something from the fuckups list
  int fuckup_count = 10;

  skip_spaces(&argument);

  // Display 10.
  if (!*argument) {
    print_x_fuckups(ch, 10);
    return;
  }

  // Display X.
  if ((fuckup_count = atoi(argument))) {
    if (fuckup_count <= 0) {
      send_to_char("1 or more, please.\r\n", ch);
      return;
    }

    print_x_fuckups(ch, fuckup_count);
    return;
  }

  // Delete mode.
  char mode[100];
  const char *deletion_string = one_argument(argument, mode);
  if (!strncmp(mode, "delete", strlen(mode))) {
    if (!access_level(ch, LVL_PRESIDENT) || IS_NPC(ch)) {
      send_to_char("Sorry, only level-10 PC staff can delete fuckups.\r\n", ch);
      return;
    }

    char query_buf[1000];
    snprintf(query_buf, sizeof(query_buf), "DELETE FROM command_fuckups WHERE Name = '%s'",
             prepare_quotes(buf, deletion_string, sizeof(buf)));
    mysql_wrapper(mysql, query_buf);
    send_to_char("Done.\r\n", ch);

    snprintf(buf, sizeof(buf), "%s deleted command fuckup '%s' from the database.", GET_CHAR_NAME(ch), deletion_string);
    return;
  }

  send_to_char("Syntax: `fuckups [count]` to list, or `fuckups delete <command>` to remove.", ch);
}

ACMD(do_rewrite_world) {
  if (subcmd != 1) {
    send_to_char("You have to type out the full command to rewrite the world. Syntax: 'rewrite_world'\r\n", ch);
    return;
  }

  if (!ch->desc || !ch->desc->descriptor) {
    send_to_char("You can fuck right off with that.\r\n", ch);
    return;
  }

  write_to_descriptor(ch->desc->descriptor, "Clearing alarm handler and rewriting all world files. This will take some time.\r\n");

  for (struct descriptor_data *d = descriptor_list; d; d = d->next)
    write_to_descriptor(d->descriptor, "World rewriting initiated. Please hold...\r\n");

  // Clear our alarm handler.
  signal(SIGALRM, SIG_IGN);

  // Perform any rectification needed.
  if (FALSE) {
    bool all_flags_are_2_or_less = TRUE;

    for (int rnum = 0; rnum <= top_of_objt; rnum++) {
      struct obj_data *obj = &obj_proto[rnum];
      if (GET_OBJ_TYPE(obj) == ITEM_MOD && (GET_VEHICLE_MOD_DESIGNED_FOR_FLAGS(obj) > 2 || GET_VEHICLE_MOD_DESIGNED_FOR_FLAGS(obj) < 0)) {
        mudlog("SYSERR: Veh mod type swap code is enabled despite already having done that function!", ch, LOG_SYSLOG, TRUE);
        all_flags_are_2_or_less = FALSE;
        break;
      }
    }

    if (all_flags_are_2_or_less) {
      int flags_with_all_common_vehs = 0;
      SET_BIT(flags_with_all_common_vehs, 1 << VEH_BIKE);
      SET_BIT(flags_with_all_common_vehs, 1 << VEH_TRUCK);
      SET_BIT(flags_with_all_common_vehs, 1 << VEH_CAR);
      SET_BIT(flags_with_all_common_vehs, 1 << VEH_DRONE);

      int flags_for_non_drone = 0;
      SET_BIT(flags_for_non_drone, 1 << VEH_BIKE);
      SET_BIT(flags_for_non_drone, 1 << VEH_TRUCK);
      SET_BIT(flags_for_non_drone, 1 << VEH_CAR);

      int flags_for_drone = 0;
      SET_BIT(flags_for_drone, 1 << VEH_DRONE);

      for (int rnum = 0; rnum <= top_of_objt; rnum++) {
        struct obj_data *obj = &obj_proto[rnum];
        if (GET_OBJ_TYPE(obj) == ITEM_MOD) {
          switch (GET_VEHICLE_MOD_DESIGNED_FOR_FLAGS(obj)) {
            case 0:
              GET_VEHICLE_MOD_DESIGNED_FOR_FLAGS(obj) = flags_for_non_drone;
              break;
            case 1:
              GET_VEHICLE_MOD_DESIGNED_FOR_FLAGS(obj) = flags_for_drone;
              break;
            case 2:
              GET_VEHICLE_MOD_DESIGNED_FOR_FLAGS(obj) = flags_with_all_common_vehs;
              break;
          }
        }
      }
    }
  }

  // Perform writing for all zones that have rooms.
  for (int i = 0; i <= top_of_zone_table; i++) {
    snprintf(buf, sizeof(buf), "Writing zone %d...\r\n", zone_table[i].number);
    write_to_descriptor(ch->desc->descriptor, buf);

      // Purge all stale flags from all rooms.
    for (vnum_t vnum = zone_table[i].number * 100; vnum <= zone_table[i].top; vnum++) {
      rnum_t rnum = real_room(vnum);
      if (rnum >= 0)
        ROOM_FLAGS(&world[rnum]).RemoveBits(ROOM_DARK, 6, 11, 13, ROOM_LOW_LIGHT, 17, ENDBIT);
    }
    
    // Save it.
    write_world_to_disk(zone_table[i].number);
    write_quests_to_disk(zone_table[i].number);
    write_objs_to_disk(zone_table[i].number);
  }

  // Re-register our alarm handler.
  signal(SIGALRM, alarm_handler);

  mudlog("World rewriting complete.", ch, LOG_SYSLOG, TRUE);

  send_to_char("Done. Alarm handler has been restored.\r\n", ch);
}

int audit_zone_rooms_(struct char_data *ch, int zone_num, bool verbose) {
  int real_rm, issues = 0;
  struct room_data *room;
  char candidate;

  if (verbose)
    send_to_char(ch, "\r\n^WAuditing rooms for zone %d...^n\r\n", zone_table[zone_num].number);

  std::unordered_map<std::string, int> rooms = {};

  for (int i = zone_table[zone_num].number * 100; i <= zone_table[zone_num].top; i++) {
    int old_issues = issues;

    if ((real_rm = real_room(i)) < 0)
      continue;

    room = &world[real_rm];

    snprintf(buf, sizeof(buf), "^c[%8ld]^n %s %s^n:\r\n",
             room->number,
             vnum_from_non_approved_zone(room->number) ? " " : (PRF_FLAGGED(ch, PRF_SCREENREADER) ? "(approved)" : "*"),
             room->name);

    // Check its strings.
    if (!strcmp(GET_ROOM_NAME(room), STRING_ROOM_TITLE_UNFINISHED)) {
      strlcat(buf, "  - ^YDefault room title used.^n This room will NOT be saved in the world files.\r\n", sizeof(buf));
      issues++;
    } else if (is_invalid_ending_punct((candidate = get_final_character_from_string(GET_ROOM_NAME(room))))) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^yname ending in punctuation (%c)^n.\r\n", candidate);
      issues++;
    }

    if (!strcmp(room->description, STRING_ROOM_DESC_UNFINISHED)) {
      strlcat(buf, "  - ^yDefault room desc used.^n\r\n", sizeof(buf));
      issues++;
    }

    if (rooms.find(std::string(GET_ROOM_NAME(room))) != rooms.end()) {
      strlcat(buf, "  - ^yRoom title is not unique within this zone.^n This will cause problems for screenreader users.\r\n", sizeof(buf));
      issues++;
    }
    rooms.emplace(std::string(GET_ROOM_NAME(room)), 1);

    if (room->matrix > 0) {
      if (real_host(room->matrix) < 1) {
        strlcat(buf, "  - ^yInvalid Matrix host specified.^n\r\n", sizeof(buf));
        issues++;
      }

      if (!strcmp(room->address, STRING_ROOM_JACKPOINT_NO_ADDR)) {
        strlcat(buf, "  - ^yDefault jackpoint physical address used.^n\r\n", sizeof(buf));
        issues++;
      }
    }

    // Check its flags.
    if (ROOM_FLAGGED(room, ROOM_ENCOURAGE_CONGREGATION)) {
      strlcat(buf, "  - Socialization flag is set.\r\n", sizeof(buf));
      issues++;
      
    }

    if (ROOM_FLAGGED(room, ROOM_NOMAGIC)) {
      strlcat(buf, "  - ^y!magic flag is set.^n\r\n", sizeof(buf));
      issues++;
      
    }

    if (ROOM_FLAGGED(room, ROOM_ARENA)) {
      strlcat(buf, "  - Arena flag is set.\r\n", sizeof(buf));
      issues++;
      
    }

    // Staff-only is allowed in the staff HQ area. Otherwise, flag it.
    if ((GET_ROOM_VNUM(room) < 10000 || GET_ROOM_VNUM(room) > 10099) && ROOM_FLAGGED(room, ROOM_STAFF_ONLY)) {
      strlcat(buf, "  - ^yStaff-only flag is set.^n\r\n", sizeof(buf));
      issues++;
      
    }

    if (ROOM_FLAGS(room).IsSet(ROOM_NOQUIT)) {
      strlcat(buf, "  - ^yRoom is flagged !QUIT.^n\r\n", sizeof(buf));
      issues++;
      
    }

#ifdef USE_SLOUCH_RULES
    if (room->z < 2.0) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - Low ceilings - %0.2f meters.\r\n", room->z);
      issues++;
      
    }
#endif

    if (room->staff_level_lock > 0) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^yStaff-locked to level %d.^n\r\n", room->staff_level_lock);
      issues++;
      
    }

    if ((IS_WATER(room) || ROOM_FLAGGED(room, ROOM_FALL))) {
      if (room->rating == 0) {
        strlcat(buf, "  - ^ySwim / Fall rating not set.^n\r\n", sizeof(buf));
        issues++;
        
      } else if (room->rating > 6) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - Swim / Fall rating is high (%d > 6).\r\n", room->rating);
        issues++;
        
      }
    }

    if (GET_BACKGROUND_COUNT(room)) {
      if (GET_BACKGROUND_AURA(room) == AURA_POWERSITE) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - Background count: ^yPower site^n (%d).\r\n", GET_BACKGROUND_COUNT(room));
        issues++;
        
      } else if (GET_BACKGROUND_COUNT(room) > 5) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - Background count: Mana warp (%d > 5).\r\n", GET_BACKGROUND_COUNT(room));
        issues++;
        
      } else if (GET_BACKGROUND_COUNT(room) < 0) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - Background count: Negative (%d < 0).\r\n", GET_BACKGROUND_COUNT(room));
        issues++;
        
        mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: A NEGATIVE background count? (%ld)", GET_ROOM_VNUM(room));
      }
    }

    // Check for issues with flight location.
    {
      bool is_landable = ROOM_FLAGGED(room, ROOM_HELIPAD) || ROOM_FLAGGED(room, ROOM_RUNWAY);
      bool has_flight_code = GET_ROOM_FLIGHT_CODE(room) && strcmp(room->flight_code, INVALID_FLIGHT_CODE);

      if (is_landable || has_flight_code) {
        if (is_landable && has_flight_code) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - Has a ^c%s^n w/ flight code ^c%s^n.\r\n", 
                   ROOM_FLAGGED(room, ROOM_HELIPAD) ? "helipad" : "runway",
                   GET_ROOM_FLIGHT_CODE(room));
        } else if (is_landable) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - Is a ^c%s^n with ^yno^n flight code.\r\n", 
                   ROOM_FLAGGED(room, ROOM_HELIPAD) ? "helipad" : "runway");
        } else {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - Has a flight code (^c%s^n) but no helipad/runway flag.\r\n",
                   GET_ROOM_FLIGHT_CODE(room));
        }
        issues++;
        

        // Make sure it's not at origin.
        if (room->latitude == 0 || room->longitude == 0) {
          strlcat(buf, "  - Has flight-related info with at least one ^yzeroed lat/long field^n.\r\n", sizeof(buf));
          issues++;
          
        } 
        // Calculate distance to Seattle.
        else {
          struct room_data east_seattle;
          east_seattle.latitude = 47.598457;
          east_seattle.longitude = -122.338623;
          
          char tmp_name[] = { "(tmp audit room: east seattle)" };
          east_seattle.name = tmp_name;

          int distance = get_flight_distance_to_room(room, &east_seattle);

          if (distance > 200) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - Is far from Seattle (%d kms > 200 kms).\r\n",
                     distance);
            issues++;
            
          }
        }
      }
    }

    // Check its exits.
    for (int k = 0; k < NUM_OF_DIRS; k++) {
      if (!room->dir_option[k])
        continue;

      // Check for hidden rating higher than 10 (or whatever threshold is configured as).
      if (room->dir_option[k]->hidden > ANOMALOUS_HIDDEN_RATING_THRESHOLD) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - %s exit has hidden rating %d > %d.\r\n",
                dirs[k],
                room->dir_option[k]->hidden,
                ANOMALOUS_HIDDEN_RATING_THRESHOLD);
        issues++;
        
      }
      // Check for keywords and descs on doors.
      if (room->dir_option[k]->exit_info != 0
          && (!room->dir_option[k]->keyword || !room->dir_option[k]->general_description)) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - %s exit is ^ymissing keywords and/or description^n.\r\n",
                dirs[k]);
        issues++;
        
      }
      // Check for valid exits.
      if (!room->dir_option[k]->to_room) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - %s exit ^yleads to an invalid room.^n\r\n",
                dirs[k]);
        issues++;
        
      }
      // to_room guaranteed to exist. Check if the exit on the other side comes back in our direction.
      else {
        if (!room->dir_option[k]->to_room->dir_option[rev_dir[k]]) {
          if (!IS_SET(room->dir_option[k]->exit_info, EX_WINDOWED) && !IS_SET(room->dir_option[k]->exit_info, EX_BARRED_WINDOW)) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - %s exit is one-way: There is no return exit.\r\n", dirs[k]);
            issues++;
            
          }
        }
        // Exit coming back in our direction is guaranteed to exist. Check if it comes to us, and if so, if the flags are kosher.
        else {
          if (room->dir_option[k]->to_room->dir_option[rev_dir[k]]->to_room != room) {
            if (!IS_SET(room->dir_option[k]->exit_info, EX_WINDOWED) && !IS_SET(room->dir_option[k]->exit_info, EX_BARRED_WINDOW)) {
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - %s exit: Return exit %s from %ld ^ydoes not point here^n.\r\n",
                      dirs[k],
                      dirs[rev_dir[k]],
                      GET_ROOM_VNUM(room->dir_option[k]->to_room));
              issues++;
              
            }
          }
          else {
            int outbound = room->dir_option[k]->to_room->dir_option[rev_dir[k]]->exit_info;
            int inbound = room->dir_option[k]->exit_info;
            REMOVE_BIT(outbound, EX_HIDDEN);
            REMOVE_BIT(inbound, EX_HIDDEN);

            if (outbound != inbound) {
              char extant_type[500];
              strlcpy(extant_type, render_door_type_string(room->dir_option[k]), sizeof(extant_type));

              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - %s exit has flags that don't match the return direction's flags (%s from here: %s, %s from %ld: %s).\r\n",
                      dirs[k],
                      dirs[k],
                      extant_type,
                      dirs[rev_dir[k]],
                      GET_ROOM_VNUM(room->dir_option[k]->to_room),
                      render_door_type_string(room->dir_option[k]->to_room->dir_option[rev_dir[k]])
                    );
              issues++;
              
            }

            // Check for can't-shoot.
            if (IS_SET(inbound, EX_CANT_SHOOT_THROUGH)) {
              strlcat(buf, "  - Exit is NoShoot.\r\n", sizeof(buf));
              issues++;
              
            }
          }
        }
      }
    }
    
    if (old_issues != issues) {
      send_to_char(ch, "%s\r\n", buf);
    }
  }

  return issues;
}

int audit_zone_mobs_(struct char_data *ch, int zone_num, bool verbose) {
  int issues = 0, total_stats, real_mob, k;
  char candidate;

  if (verbose)
    send_to_char(ch, "\r\n^WAuditing mobs for zone %d...^n\r\n", zone_table[zone_num].number);

  int pronoun_split[NUM_PRONOUNS];
  memset(pronoun_split, 0, NUM_PRONOUNS * sizeof(*pronoun_split));

  // Get a count of number of mobs currently loaded by vnum.
  std::unordered_map<vnum_t, int> active_counts = {};
  for (struct char_data *mob = character_list; mob; mob = mob->next_in_character_list) {
    if (IS_NPC(mob)) {
      auto it = active_counts.find(GET_MOB_VNUM(mob));
      if( it != active_counts.end() ) {
        it->second++;
      } else {
        active_counts.insert(std::make_pair(GET_MOB_VNUM(mob), 1));
      }
    }
  }

  for (int i = zone_table[zone_num].number * 100; i <= zone_table[zone_num].top; i++) {
    int old_issues = issues;

    if ((real_mob = real_mobile(i)) < 0)
      continue;

    struct char_data *mob = &mob_proto[real_mob];
    bool is_shopkeeper = CHECK_FUNC_AND_SFUNC_FOR(mob, shop_keeper);
    bool is_johnson = CHECK_FUNC_AND_SFUNC_FOR(mob, johnson);


    // Track pronoun count that are currently loaded.
    auto it = active_counts.find(GET_MOB_VNUM(mob));
    if( it != active_counts.end() ) {
      pronoun_split[(int) GET_PRONOUNS(mob)] += it->second;
    }

    // Check stats, etc
    total_stats = 0;
    for (k = BOD; k <= REA; k++)
      total_stats += GET_REAL_ATT(mob, k);

    snprintf(buf, sizeof(buf), "^c[%8ld]^n %s %s^n:\r\n",
             GET_MOB_VNUM(mob),
             vnum_from_non_approved_zone(GET_MOB_VNUM(mob)) ? " " : (PRF_FLAGGED(ch, PRF_SCREENREADER) ? "(approved)" : "*"),
             GET_CHAR_NAME(mob));

    // Flag mobs with crazy stats
    if (!(is_johnson || is_shopkeeper) && !MOB_FLAGGED(mob, MOB_NOKILL) && total_stats > ANOMALOUS_TOTAL_STATS_THRESHOLD) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - has total attributes %d > %d^n.\r\n",
               total_stats,
               ANOMALOUS_TOTAL_STATS_THRESHOLD);
      issues++;
    }

    // Flag mobs with no stats
    if (!(is_johnson || is_shopkeeper) && total_stats == 0) {
      strlcat(buf, "  - ^yhas not had its attributes set yet.^n\r\n", sizeof(buf));
      issues++;
    }

    // Flag mobs with crazy skills
    for (k = MIN_SKILLS; k < MAX_SKILLS; k++)
      if (GET_SKILL(mob, k) > ANOMALOUS_SKILL_THRESHOLD) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - %s %d > %d^n.\r\n",
                 skills[k].name,
                 GET_SKILL(mob, k),
                 ANOMALOUS_SKILL_THRESHOLD);
        issues++;
      }

    // Flag shopkeepers with no negotiation or low int
    if (is_shopkeeper || is_johnson) {
      if (GET_SKILL(mob, SKILL_NEGOTIATION) == 0) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - is a ^y%s with no negotiation skill.^n\r\n", is_shopkeeper ? "shopkeeper" : "Johnson");
        issues++;
      }

      if (GET_REAL_INT(mob) <= 4) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - is a ^y%s with low int (%d<=4).^n\r\n", is_shopkeeper ? "shopkeeper" : "Johnson", GET_REAL_INT(mob));
        issues++;
      }
    }

    // Flag mobs with no weight or height.
    if (GET_HEIGHT(mob) == 0 || GET_WEIGHT(mob) == 0.0) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^ymissing vital statistics^n (weight %d, height %d)^n.\r\n", GET_HEIGHT(mob), GET_WEIGHT(mob));
      issues++;
    }

    if ((GET_RACE(mob) != RACE_SPIRIT && keyword_appears_in_char("spirit", mob))
        || (GET_RACE(mob) != RACE_ELEMENTAL && keyword_appears_in_char("elemental", mob)))
    {
      strlcat(buf, "  - spirit or elemental keyword with ^ymismatched race.^n\r\n", sizeof(buf));
      issues++;
    }

    // Flag mobs with inappropriate genders.
    if (GET_RACE(mob) == RACE_ELEMENTAL || GET_RACE(mob) == RACE_SPIRIT || MOB_FLAGGED(mob, MOB_INANIMATE)) {
      if (GET_PRONOUNS(mob) != PRONOUNS_NEUTRAL) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - is a spirit/elemental/machine with a gender (is this intentional?).\r\n");
        issues++;
      }
    } else {
      if (GET_PRONOUNS(mob) == PRONOUNS_NEUTRAL) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - neutral (default) gender (is this intentional?).\r\n");
        issues++;
      }
    }

    // Flag emplaced mobs that aren't inanimate.
    if (MOB_FLAGGED(mob, MOB_EMPLACED) && !MOB_FLAGGED(mob, MOB_INANIMATE)) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - is ^yemplaced, but not inanimate.^n\r\n");
      issues++;
    }

    // Flag mobs with weird races.
    if (GET_RACE(mob) <= RACE_UNDEFINED || GET_RACE(mob) >= NUM_RACES) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - undefined or unknown race^n.\r\n");
      issues++;
    }

    // Flag mobs with bad strings.
    if (!mob->player.physical_text.name || !*mob->player.physical_text.name || !strcmp(mob->player.physical_text.name, STRING_MOB_NAME_UNFINISHED)) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^ymissing name^n.\r\n");
      
      issues++;
    } else {
      if (is_invalid_ending_punct((candidate = get_final_character_from_string(mob->player.physical_text.name)))) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^yname ending in punctuation (%c)^n.\r\n", candidate);
        issues++;
      }
    }

    if (!mob->player.physical_text.keywords || !*mob->player.physical_text.keywords || !strcmp(mob->player.physical_text.keywords, STRING_MOB_KEYWORDS_UNFINISHED)) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^ymissing keywords^n.\r\n");
      issues++;
    }

    if (!mob->player.physical_text.look_desc || !*mob->player.physical_text.look_desc || !strcmp(mob->player.physical_text.look_desc, STRING_MOB_LDESC_UNFINISHED)) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^ymissing look desc^n.\r\n");
      issues++;
    }

    if (!mob->player.physical_text.room_desc || !*mob->player.physical_text.room_desc || !strcmp(mob->player.physical_text.room_desc, STRING_MOB_RDESC_UNFINISHED)) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^ymissing room desc^n.\r\n");
      issues++;
    } else {
      if (!ispunct((candidate = get_final_character_from_string(mob->player.physical_text.room_desc)))) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^yroom desc not ending in punctuation (%c)^n.\r\n", candidate);
        issues++;
      }
    }

    if (IS_PC_CONJURED_ELEMENTAL(mob)) {
      strlcat(buf, "  - ^yis a PC Conjured Elemental (change the race!)\r\n", sizeof(buf));
      issues++;
    }

    // Alert on combat/equipment/bio/cyber, but only if they're killable.
    if (!MOB_FLAGGED(mob, MOB_NOKILL) && !npc_is_protected_by_spec(mob)) {
      if (GET_BALLISTIC(mob) > 10 || GET_IMPACT(mob) > 10) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - has high armor ratings %db / %di^n.\r\n",
                 GET_BALLISTIC(mob),
                 GET_IMPACT(mob));
        issues++;
      }

      // Flag mobs with high nuyen.
      if (GET_NUYEN(mob) >= 200 || GET_BANK(mob) >= 200) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - high grinding rewards (%ld/%ld)^n.\r\n", GET_NUYEN(mob), GET_BANK(mob));
        issues++;
      }

      // Check for special ammo or high quantities of it.
      for (int weapon_idx = START_OF_AMMO_USING_WEAPONS; weapon_idx <= END_OF_AMMO_USING_WEAPONS; weapon_idx++) {
        for (int ammo_idx = 0; ammo_idx < NUM_AMMOTYPES; ammo_idx++) {
          int ammo_qty = GET_BULLETPANTS_AMMO_AMOUNT(mob, weapon_idx, ammo_idx);

          if (ammo_qty < 0) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - has ^RNEGATIVE^n %d %s^n.\r\n",
                     ammo_qty,
                     get_ammo_representation(weapon_idx, ammo_idx, ammo_qty, mob));
            issues++;
          }
          else if (ammo_qty > 200) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - has ^ylarge amount^n of %s (%d > 200)^n.\r\n",
                     get_ammo_representation(weapon_idx, ammo_idx, ammo_qty, mob),
                     ammo_qty);
            issues++;
          } 
          else if (ammo_qty > 0 && ammo_idx != AMMO_NORMAL) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - has %d %s^n.\r\n",
                     ammo_qty,
                     get_ammo_representation(weapon_idx, ammo_idx, ammo_qty, mob));
            issues++;
          }
        }
      }
      

      if (mob->cyberware || mob->bioware) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - has cyberware / bioware.\r\n");
        issues++;
      }

      {
        int total_value = 0;
        int total_items = 0;

        for (int wearloc = 0; wearloc < NUM_WEARS; wearloc++) {
          struct obj_data *worn = GET_EQ(mob, wearloc);
          if (worn) {
            if (!(IS_OBJ_STAT(worn, ITEM_EXTRA_NOSELL) || IS_OBJ_STAT(worn, ITEM_EXTRA_STAFF_ONLY))) {
              total_value += GET_OBJ_COST(worn);
            }

            total_items++;

            vnum_t vnum = GET_OBJ_VNUM(worn);

            if (!vnum_is_from_zone(vnum, zone_num) && !vnum_is_from_canon_zone(vnum)) {
              rnum_t obj_rnum = real_object(vnum);

              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - is equipped with %sexternal non-canon-zone item %ld (%s).\r\n", // *immature giggle*
                       vnum_from_non_approved_zone(vnum) ? "^ynon-approved^n " : "",
                       vnum,
                       obj_rnum >= 0 ? GET_OBJ_NAME(&obj_proto[obj_rnum]) : "(does not exist)");
            }

            if (wearloc == WEAR_WIELD) {
              if (GET_OBJ_TYPE(worn) == ITEM_WEAPON) {
                if (GET_SKILL(mob, GET_WEAPON_SKILL(worn)) <= 0 && GET_SKILL(mob, return_general(GET_WEAPON_SKILL(worn))) <= 0) {
                  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - has no ^c%s^n or ^c%s^n skill, which is needed for wielded weapon.\r\n",
                           skills[GET_WEAPON_SKILL(worn)].name, skills[return_general(GET_WEAPON_SKILL(worn))].name);
                }
              } else if (GET_OBJ_TYPE(worn) == ITEM_FIREWEAPON) {
                strlcat(buf, "  - is wielding a non-implemented fireweapon.\r\n", sizeof(buf));
              } else {
                strlcat(buf, "  - is wielding a non-weapon item.\r\n", sizeof(buf));
              }
            }
          }
        }

        if (total_items > 0) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - has %d piece%s of equipment (total sellable value: ^c%d^n).\r\n", total_items, total_items == 1 ? "" : "s", total_value);
          issues++;
        }
      }
    }

    if (old_issues != issues) {
      send_to_char(ch, "%s\r\n", buf);
    }
  }

  send_to_char("Informative: This zone has ", ch);
  for (int pron_idx = 0; pron_idx < NUM_PRONOUNS; pron_idx++)
    send_to_char(ch, "%s^c%d^n %s", pron_idx > 0 ? ", " : "", pronoun_split[pron_idx], genders_decap[pron_idx]);
  send_to_char(" mobs loaded.\r\n", ch);

  return issues;
}

int audit_zone_objects_(struct char_data *ch, int zone_num, bool verbose) {
  int issues = 0, real_obj;
  struct obj_data *obj;
  char candidate;

  if (verbose)
    send_to_char(ch, "\r\n^WAuditing objects for zone %d...^n\r\n", zone_table[zone_num].number);

  for (int i = zone_table[zone_num].number * 100; i <= zone_table[zone_num].top; i++) {
    int old_issues = issues;

    if ((real_obj = real_object(i)) < 0)
      continue;

    obj = &obj_proto[real_obj];

    snprintf(buf, sizeof(buf), "^c[%8ld]^n %s %s^n:\r\n",
             GET_OBJ_VNUM(obj),
             vnum_from_non_approved_zone(GET_OBJ_VNUM(obj)) ? " " : (PRF_FLAGGED(ch, PRF_SCREENREADER) ? "(approved)" : "*"),
             GET_OBJ_NAME(obj));

    // Flag objects with odd typing
    if (GET_OBJ_TYPE(obj) < MIN_ITEM || GET_OBJ_TYPE(obj) >= NUM_ITEMS) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^yinvalid type %d^n.\r\n", GET_OBJ_TYPE(obj));
      
      issues++;
    }

    // Call out all objects with affs
    if (GET_OBJ_TYPE(obj) != ITEM_MOD) {
      for (int aff_index = 0; aff_index < MAX_OBJ_AFFECT; aff_index++) {
        if (obj->affected[aff_index].modifier) {
          sprinttype(obj->affected[aff_index].location, apply_types, buf2, sizeof(buf2));
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - AFF %s %d^n.\r\n",
                   buf2,
                   obj->affected[aff_index].modifier);
          
          issues++;
        }
      }
    } else {
      for (int wearloc = 0; wearloc < NUM_WEARS; wearloc++) {
        if (wearloc != ITEM_WEAR_TAKE && CAN_WEAR(obj, wearloc)) {
          strlcat(buf, "  - ^yVehicle mod can be worn.^n\r\n", sizeof(buf));
          
          issues++;
        }
      }

      if (GET_VEHICLE_MOD_LOCATION(obj) == MOD_NOWHERE) {
        strlcat(buf, "  - ^yMounts to NOWHERE.\r\n", sizeof(buf));
        
        issues++;
      }
    }

    // Flag objects with zero weight or cost
    switch (GET_OBJ_TYPE(obj)) {
      case ITEM_DESTROYABLE:
      case ITEM_FOUNTAIN:
      case ITEM_GRAFFITI:
      case ITEM_LOADED_DECORATION:
        // Never complain about item types that usually just live in rooms.
        break;
      case ITEM_KEY:
        // Keys should be light.
        if (GET_OBJ_WEIGHT(obj) >= 0.4) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - high key weight ^c%0.2f kgs^n.\r\n", GET_OBJ_WEIGHT(obj));
          
          issues++;
        }
        // Fall through
      default:
        // Don't complain about weight for files.
        if (GET_OBJ_TYPE(obj) != ITEM_DECK_ACCESSORY || GET_DECK_ACCESSORY_TYPE(obj) != TYPE_FILE) {
          // Weightless?
          if (GET_OBJ_WEIGHT(obj) <= 0) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - no or negative weight^n.\r\n");
            issues++;
          }
          // Extremely heavy?
          if (GET_OBJ_WEIGHT(obj) >= 100.0) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - high weight %0.2f kgs^n%s\r\n", GET_OBJ_WEIGHT(obj), GET_OBJ_WEIGHT(obj) > 500 ? " (should this be Fountain / Loaded Decoration?)" : ".");
            issues++;
          }
        }

        // Flag objects with zero cost
        if (GET_OBJ_TYPE(obj) != ITEM_OTHER && GET_OBJ_COST(obj) <= 0 && !IS_OBJ_STAT(obj, ITEM_EXTRA_NOSELL)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - cost %d without !SELL set^n.\r\n", GET_OBJ_COST(obj));
          issues++;
        }
        break;
    }

    // Flag objects that can't be picked up, other than the ones that we generally expect to be non-gettable.
    switch (GET_OBJ_TYPE(obj)) {
      // Fountains, graffiti, and destroyables should not be gettable.
      case ITEM_FOUNTAIN:
      case ITEM_GRAFFITI:
      case ITEM_DESTROYABLE:
      case ITEM_LOADED_DECORATION:
        if (GET_OBJ_WEAR(obj).IsSet(ITEM_WEAR_TAKE)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^yCAN be picked up if dropped (should this be !TAKE?)^n.\r\n");
          
          issues++;
        }
        break;
      // Workshops can be either, we don't care.
      case ITEM_WORKSHOP:
        break;
      // Everything else: warn about !TAKE.
      default:
        if (!GET_OBJ_WEAR(obj).IsSet(ITEM_WEAR_TAKE)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^ycannot be picked up if dropped^n (should this be obj type Loaded Decoration?)^n.\r\n");
          
          issues++;
        }
        break;
    }

    // Flag non-wieldable weapon.
    if (GET_OBJ_TYPE(obj) == ITEM_WEAPON && !CAN_WEAR(obj, ITEM_WEAR_WIELD)) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - unwieldable weapon (mob only?)^n.\r\n");
      
      issues++;
    }

    // Flag non-wearable equipment.
    if (GET_OBJ_TYPE(obj) == ITEM_WORN) {
      bool are_any_set = FALSE;
      for (int wear_idx = 0; wear_idx < NUM_WEARS; wear_idx++) {
        if (wear_idx == ITEM_WEAR_TAKE)
          continue;
        if (CAN_WEAR(obj, wear_idx)) {
          are_any_set = TRUE;
          break;
        }
      }

      if (!are_any_set) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - unwearable equipment (mob only?)^n.\r\n");
        
        issues++;
      }
    }

    if (!obj->text.name || !*obj->text.name || !strcmp(obj->text.name, STRING_OBJ_SDESC_UNFINISHED)) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^ymissing name^n.\r\n");
      
      issues++;
    } else {
      if (is_invalid_ending_punct((candidate = get_final_character_from_string(obj->text.name)))) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^yname ending in punctuation (%c)^n.\r\n", candidate);
        
        issues++;
      }
    }

    if (!obj->text.keywords || !*obj->text.keywords || !strcmp(obj->text.room_desc, STRING_OBJ_NAME_UNFINISHED)) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^ymissing keywords^n.\r\n");
      
      issues++;
    }

    if (!obj->text.look_desc || !*obj->text.look_desc || !strcmp(obj->text.look_desc, STRING_OBJ_LDESC_UNFINISHED)) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^ymissing look desc^n.\r\n");
      
      issues++;
    }

    if (!obj->text.room_desc || !*obj->text.room_desc || !strcmp(obj->text.room_desc, STRING_OBJ_RDESC_UNFINISHED)) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^ymissing room desc^n.\r\n");
      
      issues++;
    }
    else {
      if (!ispunct((candidate = get_final_character_from_string(get_string_after_color_code_removal(obj->text.room_desc, ch))))) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^yroom desc not ending in punctuation (%c)^n.\r\n", candidate);
        
        issues++;
      }
    }

    if (GET_OBJ_TYPE(obj) == ITEM_DRINKCON && GET_DRINKCON_POISON_RATING(obj) > 0) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^Yis poisoned^n (^y%d^n)^n.\r\n", GET_DRINKCON_POISON_RATING(obj));
      
      issues++;
    }
    else if (GET_OBJ_TYPE(obj) == ITEM_FOUNTAIN && GET_FOUNTAIN_POISON_RATING(obj) > 0) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^Yis poisoned^n (^y%d^n)^n.\r\n", GET_FOUNTAIN_POISON_RATING(obj));
      
      issues++;
    }

    // Check for weapons with high stats etc.
    if (GET_OBJ_TYPE(obj) == ITEM_WEAPON) {
      #define WARN_ON_NON_KOSHER_VAL(val_macro, comparison, val_name)  if (val_macro(obj) comparison kosher_weapon_values[GET_WEAPON_ATTACK_TYPE(obj)].val_name) { \
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - weapon's %s value ^yis not in PGHQ spec for %s^n (%d %s %d).\r\n", \
                 #val_name, \
                 GET_WEAPON_TYPE_NAME(GET_WEAPON_ATTACK_TYPE(obj)), \
                 val_macro(obj), \
                 #comparison, \
                 kosher_weapon_values[GET_WEAPON_ATTACK_TYPE(obj)].val_name); \
         \
        issues++; \
      }

      // Check for shared value overruns.
      WARN_ON_NON_KOSHER_VAL(GET_WEAPON_POWER, >, power);

      if (GET_WEAPON_DAMAGE_CODE(obj) > kosher_weapon_values[GET_WEAPON_ATTACK_TYPE(obj)].damage_code) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - weapon's damage code ^yis not in PGHQ spec for %s^n (%s > %s).\r\n",
                 GET_WEAPON_TYPE_NAME(GET_WEAPON_ATTACK_TYPE(obj)),
                 GET_WOUND_NAME(GET_WEAPON_DAMAGE_CODE(obj)),
                 GET_WOUND_NAME(kosher_weapon_values[GET_WEAPON_ATTACK_TYPE(obj)].damage_code));
        
        issues++;
      }
      
      if (GET_WEAPON_SKILL(obj) != kosher_weapon_values[GET_WEAPON_ATTACK_TYPE(obj)].skill) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - weapon's skill (%s) may not match attack type %s's expected skill (%s)\r\n", 
                 skills[GET_WEAPON_SKILL(obj)].name,
                 GET_WEAPON_TYPE_NAME(GET_WEAPON_ATTACK_TYPE(obj)),
                 skills[kosher_weapon_values[GET_WEAPON_ATTACK_TYPE(obj)].skill].name);
        
        issues++;
      }

      if (WEAPON_IS_GUN(obj)) {
        // Ranged checks.
        WARN_ON_NON_KOSHER_VAL(GET_WEAPON_MAX_AMMO, >, max_ammo);
        WARN_ON_NON_KOSHER_VAL(GET_WEAPON_INTEGRAL_RECOIL_COMP, >, recoil_comp);

        // Firemode check.
        #define FIREMODE_CHECK(val_mode, val_name)  if (WEAPON_CAN_USE_FIREMODE(obj, val_mode) && !kosher_weapon_values[GET_WEAPON_ATTACK_TYPE(obj)].val_name) { \
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - weapon can use ^ynon-kosher^n %s firemode\r\n", #val_mode); \
           \
          issues++; \
        }

        FIREMODE_CHECK(MODE_SS, can_ss);
        FIREMODE_CHECK(MODE_SA, can_sa);
        FIREMODE_CHECK(MODE_BF, can_bf);
        FIREMODE_CHECK(MODE_FA, can_fa);

        // Notify on ANY recoil comp, erroneous or not
        if (GET_WEAPON_INTEGRAL_RECOIL_COMP(obj)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - has ^c%d^n integral recoil comp^n.\r\n", GET_WEAPON_INTEGRAL_RECOIL_COMP(obj));
          
          issues++;
        }

        // Attachments
        for (int idx = ACCESS_LOCATION_TOP; idx <= ACCESS_LOCATION_UNDER; idx++) {
          vnum_t attach_vnum = GET_WEAPON_ATTACH_LOC(obj, idx);
          const char *attach_loc = gun_accessory_locations[idx - ACCESS_LOCATION_TOP];
          bool should_be_able_to_take_attachment = FALSE;

          if (attach_vnum <= -1)
            continue;

          switch (idx) {
            case ACCESS_LOCATION_TOP:
              should_be_able_to_take_attachment = kosher_weapon_values[GET_WEAPON_ATTACK_TYPE(obj)].can_attach_top;
              break;
            case ACCESS_LOCATION_BARREL:
              should_be_able_to_take_attachment = kosher_weapon_values[GET_WEAPON_ATTACK_TYPE(obj)].can_attach_barrel;
              break;
            case ACCESS_LOCATION_UNDER:
              should_be_able_to_take_attachment = kosher_weapon_values[GET_WEAPON_ATTACK_TYPE(obj)].can_attach_bottom;
              break;
          }

          if (!should_be_able_to_take_attachment) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^yshouldn't^n be able to take %s attachments (based on weapon type), but can.\r\n", attach_loc);
            
            issues++;
          }

          if (attach_vnum > 0) {
            rnum_t attach_rnum = real_object(attach_vnum);

            if (attach_rnum < 0) {
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - has ^yinvalid^n %s-attached item (%ld).\r\n", attach_loc, attach_vnum);
              
              issues++;
            } else {
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - has %s-attached item '%s^n' (%ld).\r\n", 
                       attach_loc, 
                       GET_OBJ_NAME(&obj_proto[attach_rnum]),
                       attach_vnum);
              
              issues++;
            }
          }
        }
      } else {
        // Melee checks.
        WARN_ON_NON_KOSHER_VAL(GET_WEAPON_STR_BONUS, >, str_bonus);
        WARN_ON_NON_KOSHER_VAL(GET_WEAPON_REACH, >, reach);

        if (GET_WEAPON_FOCUS_RATING(obj)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - is a rating-^c%d^n ^Wweapon focus^n.\r\n", GET_WEAPON_FOCUS_RATING(obj));
          
          issues++;
        }
      }
    }

    // Check for cash items with high values.
    switch (GET_OBJ_TYPE(obj)) {
      case ITEM_MONEY:
        if (GET_ITEM_MONEY_VALUE(obj)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^Yis money^n with ^c%d^n value^n.\r\n", GET_ITEM_MONEY_VALUE(obj));
          
          issues++;
        }
        break;
      case ITEM_DECK_ACCESSORY:
        if (GET_DECK_ACCESSORY_TYPE(obj) == TYPE_PARTS && GET_OBJ_COST(obj)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^Wis chips/parts^n with ^c%d^n value^n.\r\n", GET_OBJ_COST(obj));
          
          issues++;
        }
        break;
      case ITEM_MAGIC_TOOL:
        if (GET_MAGIC_TOOL_TYPE(obj) == TYPE_SUMMONING && GET_OBJ_COST(obj)) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^Wis summoning mats^n with ^c%d^n value^n.\r\n", GET_OBJ_COST(obj));
          
          issues++;
        }
        break;
    }

    if (GET_OBJ_TYPE(obj) == ITEM_SPELL_FORMULA) {
      if (GET_SPELLFORMULA_FORCE(obj) <= 0) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^Yis an unrated spell formula^n (%d).\r\n", GET_SPELLFORMULA_FORCE(obj));
        
        issues++;
      }
    }

    // Check for foci with high ratings.
    if (GET_OBJ_TYPE(obj) == ITEM_FOCUS) {
      // Compose a list of the bits this focus can't have (default: all but take). Only checked for specific foci.
      Bitfield unacceptable_bits;
      for (int wear_idx = 0; wear_idx < ITEM_WEAR_MAX; wear_idx++) {
        if (wear_idx != ITEM_WEAR_TAKE)
          unacceptable_bits.SetBit(wear_idx);
      }

      if (GET_FOCUS_FORCE(obj) <= 0) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^Yis an unrated focus^n (%d).\r\n", GET_FOCUS_FORCE(obj));
        
        issues++;
      } else {
        switch (GET_FOCUS_TYPE(obj)) {
          case FOCI_EXPENDABLE:
            if (GET_FOCUS_FORCE(obj) > 8) {
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^Yis an over-strength expendable focus^n (%d).\r\n", GET_FOCUS_FORCE(obj));
              
              issues++;
            }
            break;
          case FOCI_SPEC_SPELL:
            if (GET_FOCUS_FORCE(obj) > 8) {
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^Yis an over-strength specific spell focus^n (%d).\r\n", GET_FOCUS_FORCE(obj));
              
              issues++;
            } else if (GET_FOCUS_FORCE(obj) > 4) {
              unacceptable_bits.RemoveBit(ITEM_WEAR_HOLD);
              if (GET_OBJ_WEAR(obj).AreAnyShared(unacceptable_bits)) {
                snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^Yis a high-strength specific spell focus^n (%d) ^youtside of hold only^n\r\n", GET_FOCUS_FORCE(obj));
                
                issues++;
              }
            }
            break;
          case FOCI_SPELL_CAT:
            if (GET_FOCUS_FORCE(obj) > 4) {
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^Yis an over-strength category focus^n (%d).\r\n", GET_FOCUS_FORCE(obj));
              
              issues++;
            }
            break;
          case FOCI_SPIRIT:
            if (GET_FOCUS_FORCE(obj) > 6) {
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^Yis an over-strength spirit focus^n (%d).\r\n", GET_FOCUS_FORCE(obj));
              
              issues++;
            }
            break;
          case FOCI_POWER:
            if (GET_FOCUS_FORCE(obj) > 4) {
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^Ris an over-strength power focus^n (%d).\r\n", GET_FOCUS_FORCE(obj));
              
              issues++;
            }
            break;
          case FOCI_SUSTAINED:
            if (GET_FOCUS_FORCE(obj) > 4) {
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^Ris an over-strength sustain focus^n (%d).\r\n", GET_FOCUS_FORCE(obj));
              
              issues++;
            } else if (GET_FOCUS_FORCE(obj) == 4) {
              unacceptable_bits.RemoveBits(ITEM_WEAR_HOLD, ITEM_WEAR_EYES, ENDBIT);
              if (GET_OBJ_WEAR(obj).AreAnyShared(unacceptable_bits)) {
                snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^Ris a max-strength sustain focus^n (%d) ^youtside of eye/hold slots^n\r\n", GET_FOCUS_FORCE(obj));
                
                issues++;
              }
            }
            break;
          case FOCI_SPELL_DEFENSE:
            if (GET_FOCUS_FORCE(obj) > 4) {
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^Yis an over-strength defense focus^n (%d).\r\n", GET_FOCUS_FORCE(obj));
              
              issues++;
            }
            break;
          default:
            mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Unknown focus type %d encountered in object audit-- add a case for it!", GET_FOCUS_TYPE(obj));
            break;
        }
      }
    }

    if (old_issues != issues) {
      send_to_char(ch, "%s\r\n", buf);
    }
  }
  // TODO: Make sure they've got all their strings set.

  return issues;
}

int audit_zone_quests_(struct char_data *ch, int zone_num, bool verbose) {
  int issues = 0, real_qst;
  struct quest_data *quest;  // qwest?  (^_^) 0={=====>
  int payout_karma = 0;
  int payout_nuyen = 0;

  if (verbose)
    send_to_char(ch, "\r\n^WAuditing quests for zone %d...^n\r\n", zone_table[zone_num].number);

  for (int zone_vnum = zone_table[zone_num].number * 100; zone_vnum <= zone_table[zone_num].top; zone_vnum++) {
    int old_issues = issues;

    if ((real_qst = real_quest(zone_vnum)) < 0)
      continue;

    quest = &quest_table[real_qst];

    rnum_t johnson_rnum = real_mobile(quest->johnson);
    snprintf(buf, sizeof(buf), "^c[%8ld]^n %s:\r\n", quest->vnum, johnson_rnum < 0 ? "" : GET_CHAR_NAME(&mob_proto[johnson_rnum]));

    payout_karma = quest_table[real_qst].karma;
    payout_nuyen = quest_table[real_qst].nuyen;

    // Flag invalid Johnsons
    if (quest->johnson <= 0 || johnson_rnum <= 0) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^yinvalid Johnson %ld^n.\r\n", quest->johnson);
      issues++;
    }

    // Flag invalid object objectives
    for (int obj_idx = 0; obj_idx < quest->num_objs; obj_idx++) {
      payout_nuyen += quest->obj[obj_idx].nuyen;
      payout_karma += quest->obj[obj_idx].karma;

      // invalid object
      if (real_object(quest->obj[obj_idx].vnum) <= -1) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - obj objective #%d: ^yinvalid object %ld^n.\r\n", obj_idx, quest->obj[obj_idx].vnum);
        issues++;
      }

      // Invalid mob targets for initialization
      switch (quest->obj[obj_idx].load) {
        case QOL_TARMOB_I:
        case QOL_TARMOB_E:
        case QOL_TARMOB_C:
          if (quest->obj[obj_idx].l_data < 0
              || quest->obj[obj_idx].l_data >= quest->num_mobs
              || real_mobile(quest->mob[quest->obj[obj_idx].l_data].vnum) <= -1) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - obj objective #%d: ^yinvalid load mobile M%ld^n.\r\n", obj_idx, quest->obj[obj_idx].l_data);
            issues++;
          }
          break;
      }

      // Invalid mob targets for delivery
      switch (quest->obj[obj_idx].objective) {
        case QOO_TAR_MOB:
          if (((quest->obj[obj_idx].o_data < 0 || quest->obj[obj_idx].o_data >= quest->num_mobs) || real_mobile(quest->mob[quest->obj[obj_idx].o_data].vnum) <= -1)
              && real_mobile(quest->obj[obj_idx].o_data) <= -1)
          {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - obj objective #%d: ^yinvalid dest mobile M%ld^n.\r\n", obj_idx, quest->obj[obj_idx].o_data);
            issues++;
          }
          break;
        case QOO_LOCATION:
          if (real_room(quest->obj[obj_idx].o_data) <= -1) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - obj objective #%d: ^yinvalid room %ld^n.\r\n", obj_idx, quest->obj[obj_idx].o_data);
            issues++;
          }
          break;
        case QOO_RETURN_PAY:
        case QOO_UPLOAD:
          if (real_host(quest->obj[obj_idx].o_data) <= -1) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - obj objective #%d: ^yinvalid host %ld^n.\r\n", obj_idx, quest->obj[obj_idx].o_data);
            issues++;
          }

          {
            struct obj_data *loaded = read_object(quest->obj[obj_idx].vnum, VIRTUAL, OBJ_LOAD_REASON_EDITING_EPHEMERAL_LOOKUP);
            if (GET_OBJ_TYPE(loaded) != ITEM_DECK_ACCESSORY) {
              snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - obj objective #%d: obj %ld is not a deck accessory ^y(it can't be uploaded)^n.\r\n", obj_idx, quest->obj[obj_idx].vnum);
              issues++;
            }
            extract_obj(loaded);
          }
          break;
      }
    }

    // Flag invalid mob objectives
    for (int mob_idx = 0; mob_idx < quest->num_mobs; mob_idx++) {
      payout_karma += quest->mob[mob_idx].karma;
      payout_nuyen += quest->mob[mob_idx].nuyen;

      // Check for invalid mob.
      if (real_mobile(quest->mob[mob_idx].vnum) <= -1) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - mob objective #%d: ^yinvalid mobile %ld^n.\r\n", mob_idx, quest->mob[mob_idx].vnum);
        issues++;
      }

      // Check for invalid load conditions.
      switch (quest->mob[mob_idx].load) {
        case QML_LOCATION:
          if (real_room(quest->mob[mob_idx].l_data) <= -1) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - mob objective #%d: ^yinvalid load room %ld^n.\r\n", mob_idx, quest->mob[mob_idx].l_data);
            issues++;
          }
          break;
      }

      switch (quest->mob[mob_idx].objective) {
        case QMO_LOCATION:
          if (real_room(quest->mob[mob_idx].o_data) <= -1) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - mob objective #%d: ^yinvalid destination room %ld^n.\r\n", mob_idx, quest->mob[mob_idx].o_data);
            issues++;
          }
          break;
        case QMO_KILL_ESCORTEE:
          if (real_room(quest->mob[mob_idx].o_data) <= -1) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - mob objective #%d: ^yinvalid escortee %ld^n.\r\n", mob_idx, quest->mob[mob_idx].o_data);
            issues++;
          }
          break;
      }
    }

    // Flag high payouts - karma.
    payout_karma *= KARMA_GAIN_MULTIPLIER;
    if (payout_karma > 600) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - karma payout after multiplier is at least ^c%.2f^n.\r\n", ((float) payout_karma) / 100);
      issues++;
    }

    // Flag high payouts - nuyen.
    payout_nuyen *= NUYEN_GAIN_MULTIPLIER;
    if (payout_nuyen > 10000) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - nuyen payout after multiplier is at least ^c%d^n.\r\n", payout_nuyen);
      issues++;
    }

    // Flag item rewards.
    if (quest->reward > 0){
      rnum_t quest_reward_rnum = real_object(quest->reward);
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^yitem reward^n ^c%ld^n (%s^n).\r\n", quest->reward, quest_reward_rnum >= 0 ? GET_OBJ_NAME(&obj_proto[quest_reward_rnum]) : "<invalid>");
      issues++;
    }

    // Flag invalid strings
    if (!quest->intro || !*quest->intro) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^yno intro string^n.\r\n");
      
      issues++;
    } else {
      if (!ispunct(get_final_character_from_string(quest->intro))) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^yintro string does not end in punctuation^n.\r\n");
        issues++;
      }
    }

    if (!quest->decline || !*quest->decline) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^yno decline string^n.\r\n");
      
      issues++;
    } else {
      if (!ispunct(get_final_character_from_string(quest->decline))) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^ydecline string does not end in punctuation^n.\r\n");
        issues++;
      }
    }

    if (!quest->finish || !*quest->finish) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^yno finish string^n.\r\n");
      
      issues++;
    } else {
      if (!ispunct(get_final_character_from_string(quest->finish))) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^yfinish string does not end in punctuation^n.\r\n");
        issues++;
      }
    }

    if (!quest->info || !*quest->info) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^yno info string^n.\r\n");
      
      issues++;
    } else {
      if (!ispunct(get_final_character_from_string(quest->info))) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^yinfo string does not end in punctuation^n.\r\n");
        
        issues++;
      }
    }

    if (!quest->quit || !*quest->quit) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^yno quit string^n.\r\n");
      
      issues++;
    } else {
      if (!ispunct(get_final_character_from_string(quest->quit))) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^yquit string does not end in punctuation^n.\r\n");
        
        issues++;
      }
    }

    if (!quest->done || !*quest->done) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^yno done string^n.\r\n");
      
      issues++;
    } else {
      if (!ispunct(get_final_character_from_string(quest->done))) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^ydone string does not end in punctuation^n.\r\n");
        
        issues++;
      }
    }

#ifdef USE_QUEST_LOCATION_CODE
    if (!quest->location || !*quest->location) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^yno location string^n.\r\n");
      
      issues++;
    } else {
      if (ispunct(get_final_character_from_string(quest->location))) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^ylocation string ends in punctuation^n.\r\n");
        
        issues++;
      }
    }
#endif

    if (old_issues != issues) {
      send_to_char(ch, "%s\r\n", buf);
    }
  }

  return issues;
}

int audit_zone_shops_(struct char_data *ch, int zone_num, bool verbose) {
  int issues = 0, real_shp;
  struct shop_data *shop;

  if (verbose)
    send_to_char(ch, "\r\n^WAuditing shops for zone %d...^n\r\n", zone_table[zone_num].number);

  for (int i = zone_table[zone_num].number * 100; i <= zone_table[zone_num].top; i++) {
    int old_issues = issues;

    if ((real_shp = real_shop(i)) < 0)
      continue;

    shop = &shop_table[real_shp];

    rnum_t shopkeeper_rnum = real_mobile(shop->keeper);

    if (shopkeeper_rnum <= -1) {
      snprintf(buf, sizeof(buf), "^c[%8ld]^n:\r\n  - ^yinvalid shopkeeper.\r\n", shop->vnum);
      
      issues++;
    } else {
      snprintf(buf, sizeof(buf), "^c[%8ld]^n: %s (%ld)\r\n", shop->vnum, GET_NAME(&mob_proto[shopkeeper_rnum]), shop->keeper);
      // Flag invalid sell multipliers
      if (shop->profit_buy < 1.0) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^Ytoo-low buy profit ^c%0.2f^n < 1.0^n.\r\n", shop->profit_buy);
        
        issues++;
      }

      if (shop->flags.IsSet(SHOP_CHARGEN) ? shop->profit_sell > (1.0000001) : (shop->flags.IsSet(SHOP_DOCTOR) ? shop->profit_sell > (0.3000001) : shop->profit_sell > 0.1000001))
      {
        bool buys_anything = FALSE;
        for (int type = 1; type < NUM_ITEMS && !buys_anything; type++)
          if (shop->buytypes.IsSet(type))
            buys_anything = TRUE;

        if (buys_anything) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^Yhigh sell profit ^c%0.2f^n > 0.1^n.\r\n", shop->profit_sell);
          
          issues++;
        }
      }

      // Flag the shopkeeper having wonky int values.
      int intelligence = GET_REAL_INT(&mob_proto[shopkeeper_rnum]);
#ifdef BE_STRICTER_ABOUT_SHOPKEEPER_INTELLIGENCE
      if (intelligence <= 4 || intelligence > 12) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - out-of-range shopkeeper intelligence ^c%d^n (expecting between 5 and 12)^n.\r\n", intelligence);
        
        issues++;
      }
#else
      if (intelligence <= 0) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - shopkeeper intelligence not set.\r\n");
        
        issues++;
      }
#endif
    }

    for (struct shop_sell_data *sell = shop_table[real_shp].selling; sell; sell = sell->next) {
      rnum_t obj_rnum = real_object(sell->vnum);
      if (obj_rnum < 0) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - item ^c%ld^n is listed as for sale, but ^ydoes not exist^n.\r\n", obj_rnum);
      }
      else if (sell->type == SELL_AVAIL && GET_OBJ_AVAILTN(&obj_proto[obj_rnum]) == 0) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - avail-listed item ^c%s ^n(^c%ld^n)^n ^yhas no TN^n.\r\n", GET_OBJ_NAME(&obj_proto[obj_rnum]), GET_OBJ_VNUM(&obj_proto[obj_rnum]));
      }
    }

    if (old_issues != issues) {
      send_to_char(ch, "%s\r\n", buf);
    }
  }

  // TODO: Make sure they've got all their strings set.

  return issues;
}

int audit_zone_vehicles_(struct char_data *ch, int zone_num, bool verbose) {
  int issues = 0, real_veh;
  struct veh_data *veh;

  if (verbose)
    send_to_char(ch, "\r\n^WAuditing vehicles for zone %d...^n\r\n", zone_table[zone_num].number);

  for (int i = zone_table[zone_num].number * 100; i <= zone_table[zone_num].top; i++) {
    int old_issues = issues;

    if ((real_veh = real_vehicle(i)) < 0)
      continue;

    veh = &veh_proto[real_veh];

    snprintf(buf, sizeof(buf), "^c[%8ld]^n:\r\n", veh->veh_number);

    /*
    // Flag invalid sell multipliers
    if (shop->profit_buy < 1.0) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " has too-low buy profit %0.2f < 1.0^n", shop->profit_buy);
      
      issues++;
    }

    // Flag invalid strings
    if (shop->profit_sell > 0.1) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s too-high sell profit %0.2f > 0.1^n", printed ? ";" : " has", shop->profit_sell);
      
      issues++;
    }
    */

    if (old_issues != issues) {
      send_to_char(ch, "%s\r\n", buf);
    }
  }

  // TODO: Make sure they've got all their strings set.

  return issues;
}

void show_host_sheaf_to_ch(struct char_data *ch, struct host_data *host, bool only_print_on_error) {
  if (!only_print_on_error)
    send_to_char("^gSheaf:^n\r\n", ch);

  bool printed_something = FALSE;

  for (struct trigger_step *trig = host->trigger; trig; trig = trig->next) {
    char sheafbuf[500];
    snprintf(sheafbuf, sizeof(sheafbuf), "   %3d) Alert: ^c%d^n", trig->step, trig->alert);
    if (trig->ic > 0) {
      rnum_t ic_rnum = real_ic(trig->ic);
      snprintf(ENDOF(sheafbuf), sizeof(sheafbuf) - strlen(sheafbuf), ", IC: ^y%ld^n (%s)",
                trig->ic,
                ic_rnum >= 0 ? ic_proto[ic_rnum].name : "^rinvalid^n");
    }
    if (!only_print_on_error)
      send_to_char(ch, "%s\r\n", sheafbuf);
    printed_something = TRUE;
  }

  if (!printed_something)
    send_to_char(ch, " ^Y<Sheaf missing - specify trigger steps for host %ld (%s^Y)>^n\r\n", host->vnum, host->name);
}

int audit_zone_hosts_(struct char_data *ch, int zone_num, bool verbose) {
  int issues = 0;

  if (verbose)
    send_to_char(ch, "\r\n^WAuditing hosts for zone %d... ^L(normal hosts no longer listed, use hlist)^n\r\n", zone_table[zone_num].number);

  for (int i = zone_table[zone_num].number * 100; i <= zone_table[zone_num].top; i++) {
    rnum_t real_hst;
    if ((real_hst = real_host(i)) < 0)
      continue;

    struct host_data *host = &matrix[real_hst];

    // send_to_char(ch, "^c[%8ld]^n %s^n\r\n", host->vnum, host->name);

    show_host_sheaf_to_ch(ch, host, TRUE);
  }

  // TODO: Make sure they've got all their strings set.

  return issues;
}

int audit_zone_ics_(struct char_data *ch, int zone_num, bool verbose) {
  int issues = 0, real_num;
  struct matrix_icon *ic;
  char candidate;

  if (verbose)
    send_to_char(ch, "\r\n^WAuditing ICs for zone %d...^n\r\n", zone_table[zone_num].number);

  for (int i = zone_table[zone_num].number * 100; i <= zone_table[zone_num].top; i++) {
    int old_issues = issues;
    
    if ((real_num = real_ic(i)) < 0)
      continue;

    ic = &ic_proto[real_num];

    snprintf(buf, sizeof(buf), "^c[%8d]^n (%s^n)\r\n", ic->idnum, ic->name);

    if (ic->name && *(ic->name) != toupper(*(ic->name))) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^yroom desc not capitalized (%c)^n.\r\n", *(ic->name));
      issues++;
    }

    if (!ispunct((candidate = get_final_character_from_string(ic->look_desc)))) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - ^yroom desc not ending in punctuation (%c)^n.\r\n", candidate);
      issues++;
    }

    /*
    // Flag invalid sell multipliers
    if (shop->profit_buy < 1.0) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " has too-low buy profit %0.2f < 1.0^n", shop->profit_buy);
      
      issues++;
    }

    // Flag invalid strings
    if (shop->profit_sell > 0.1) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s too-high sell profit %0.2f > 0.1^n", printed ? ";" : " has", shop->profit_sell);
      
      issues++;
    }
    */

    if (old_issues != issues) {
      send_to_char(ch, "%s\r\n", buf);
    }
  }

  // TODO: Make sure they've got all their strings set.

  return issues;
}

#define ZCMD zone_table[zone_num].cmd[cmd_no]
int audit_zone_commands_(struct char_data *ch, int zone_num, bool verbose) {
  int issues = 0;

  if (verbose)
    send_to_char(ch, "\r\n^WAuditing zone commands for zone %d...^n\r\n", zone_table[zone_num].number);

  bool zone_has_rooms = FALSE;
  for (int vnum = zone_table[zone_num].number * 100; !zone_has_rooms && vnum <= zone_table[zone_num].top; vnum++)
    zone_has_rooms = real_room(vnum) >= 0;
  // No rooms? No worries.
  if (!zone_has_rooms)
    return 0;

  if (zone_table[zone_num].lifespan > 10) {
    send_to_char(ch, "\r\nZone ^c%d^n (%s^n): Lifespan is high (^c%d^n > 10)\r\n",
                 zone_table[zone_num].number,
                 zone_table[zone_num].name,
                 zone_table[zone_num].lifespan);
    issues += 1;
  }

  if (zone_table[zone_num].reset_mode == 0) {
    send_to_char(ch, "\r\nZone ^c%d^n (%s^n): Zone ^ydoes not reset^n\r\n",
                 zone_table[zone_num].number,
                 zone_table[zone_num].name);
    issues += 1;
  }

  // Note the zone security rating.
  if (zone_table[zone_num].security <= 0) {
    send_to_char(ch, "\r\nZone ^c%d^n (%s^n): Zone security level is ^ynot set^n.\r\n",
                 zone_table[zone_num].number,
                 zone_table[zone_num].name);
    issues += 1;
  } else if (zone_table[zone_num].security <= 4) {
    send_to_char(ch, "\r\nZone ^c%d^n (%s^n): Zone is ^clow security^n (not an issue).\r\n",
                 zone_table[zone_num].number,
                 zone_table[zone_num].name);
  } else if (zone_table[zone_num].security <= 8) {
    send_to_char(ch, "\r\nZone ^c%d^n (%s^n): Zone is ^gmedium security^n (not an issue).\r\n",
                 zone_table[zone_num].number,
                 zone_table[zone_num].name);
  } else if (zone_table[zone_num].security <= 12) {
    send_to_char(ch, "\r\nZone ^c%d^n (%s^n): Zone is ^yhigh security^n (potential issue).\r\n",
                 zone_table[zone_num].number,
                 zone_table[zone_num].name);
    issues += 1;
  } else {
    send_to_char(ch, "\r\nZone ^c%d^n (%s^n): Zone is ^rmax security^n (potential issue).\r\n",
                 zone_table[zone_num].number,
                 zone_table[zone_num].name);
    issues += 1;
  }

  for (int cmd_no = 0; cmd_no < zone_table[zone_num].num_cmds; cmd_no++) {
    int num_found = 0;

    snprintf(buf, sizeof(buf), "^c[z%d cmd #%3d]^n:\r\n", zone_table[zone_num].number, cmd_no);

    // Parse the command and check for validity of rnums.
    switch (ZCMD.command) {
      case 'M':
        num_found += _error_on_invalid_real_mob(ZCMD.arg1, zone_num, buf, sizeof(buf));
        num_found += _error_on_invalid_real_room(ZCMD.arg3, zone_num, buf, sizeof(buf));
        break;
      case 'H':
        num_found += _error_on_invalid_real_obj(ZCMD.arg1, zone_num, buf, sizeof(buf));
        num_found += _error_on_invalid_real_host(ZCMD.arg3, zone_num, buf, sizeof(buf));
        break;
      case 'O':
        num_found += _error_on_invalid_real_obj(ZCMD.arg1, zone_num, buf, sizeof(buf));
        num_found += _error_on_invalid_real_room(ZCMD.arg3, zone_num, buf, sizeof(buf));
        break;
      case 'V': /* Vehicles */
        num_found += _error_on_invalid_real_veh(ZCMD.arg1, zone_num, buf, sizeof(buf));
        num_found += _error_on_invalid_real_room(ZCMD.arg3, zone_num, buf, sizeof(buf));
        break;
      case 'S':
        num_found += _error_on_invalid_real_mob(ZCMD.arg1, zone_num, buf, sizeof(buf));
        break;
      case 'G':
      case 'C':
      case 'U':
      case 'I':
      case 'E':
        num_found += _error_on_invalid_real_obj(ZCMD.arg1, zone_num, buf, sizeof(buf));
        break;
      case 'P':
        num_found += _error_on_invalid_real_obj(ZCMD.arg1, zone_num, buf, sizeof(buf));
        num_found += _error_on_invalid_real_obj(ZCMD.arg3, zone_num, buf, sizeof(buf));
        break;
      case 'D':
        num_found += _error_on_invalid_real_room(ZCMD.arg1, zone_num, buf, sizeof(buf));
        break;
      case 'R':
        num_found += _error_on_invalid_real_room(ZCMD.arg1, zone_num, buf, sizeof(buf));
        num_found += _error_on_invalid_real_obj(ZCMD.arg2, zone_num, buf, sizeof(buf));
        break;
      case 'N':
        num_found += _error_on_invalid_real_obj(ZCMD.arg1, zone_num, buf, sizeof(buf));
        break;
    }

    if (num_found > 0) {
      send_to_char(buf, ch);
      issues += num_found;
    }
  }

  return issues;
}
#undef ZCMD

#define AUDIT_ALL_ZONES(func_suffix)                           \
if (!str_cmp(arg1, #func_suffix)) {                            \
  for (zonenum = 0; zonenum <= top_of_zone_table; zonenum++) { \
    if (!zone_table[zonenum].approved                          \
        || zone_table[zonenum].number == 0                     \
        || zone_table[zonenum].number == 10                    \
        || zone_table[zonenum].number == 100)                  \
      continue;                                                \
    audit_zone_ ## func_suffix ## _(ch, zonenum, FALSE);       \
  }                                                            \
  return;                                                      \
}

#define AUDIT_CATEGORY(compare_string, unquoted_category)      \
if (!strcmp(arg1, compare_string)) {                           \
  if (number <= 0) {                                           \
    AUDIT_ALL_ZONES(unquoted_category);                        \
  } else {                                                     \
    audit_zone_ ## unquoted_category ## _(ch, zonenum, TRUE);  \
  }                                                            \
  return;                                                      \
}

float spider_connected_hosts_for_reward_func(struct host_data *host,
                                             std::unordered_map<vnum_t, struct host_data *> connected_hosts,
                                             std::unordered_map<vnum_t, struct matrix_icon *> connected_ics) {
  if (!host) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Null host to spider func");
    return 0;
  }
  
  try {
    // No-op: Already there.
    if (connected_hosts[host->vnum]) {
      return 0;
    }
  } catch (std::out_of_range) {
    // Add it to the map.
    connected_hosts[host->vnum] = host;
  }

  float return_value = 1;

  struct zone_data *host_zone = get_zone_from_vnum(host->vnum);

  // Spider all outgoing exits, unless in a different zone than this host.
  {
    if (host->parent) {
      rnum_t targ_host_rnum = real_host(host->parent);
      if (targ_host_rnum >= 0 && get_zone_from_vnum(host->parent) == host_zone)
        return_value += spider_connected_hosts_for_reward_func(&matrix[targ_host_rnum], connected_hosts, connected_ics);
    }

    for (int sub = ACIFS_ACCESS; sub < NUM_ACIFS; sub++) {
      if (host->stats[sub][MTX_STAT_TRAPDOOR]) {
        rnum_t targ_host_rnum = real_host(host->stats[sub][MTX_STAT_TRAPDOOR]);
        if (targ_host_rnum >= 0 && get_zone_from_vnum(host->stats[sub][MTX_STAT_TRAPDOOR]) == host_zone)
          return_value += spider_connected_hosts_for_reward_func(&matrix[targ_host_rnum], connected_hosts, connected_ics);
      }
    }

    for (struct exit_data *exit = host->exit; exit; exit = exit->next) {
      rnum_t targ_host_rnum = real_host(exit->host);
      if (targ_host_rnum >= 0 && get_zone_from_vnum(exit->host) == host_zone)
        return_value += spider_connected_hosts_for_reward_func(&matrix[targ_host_rnum], connected_hosts, connected_ics);
    }
  }

  // todo: spider all ICs in the trigger list and add those to the seen list

  return return_value;
}

float spider_connected_rooms_for_reward_func(struct room_data *room,
                                             std::unordered_map<vnum_t, struct room_data *> connected_rooms,
                                             std::unordered_map<vnum_t, struct host_data *> connected_hosts,
                                             std::unordered_map<vnum_t, struct matrix_icon *> connected_ics)
{
  if (!room) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Null room to spider func");
    return 0;
  }
  
  try {
    // No-op: Already there.
    if (connected_rooms[GET_ROOM_VNUM(room)]) {
      return 0;
    }
  } catch (std::out_of_range) {}

  float return_value = 1;
  
  // Add it to the map.
  connected_rooms[GET_ROOM_VNUM(room)] = room;

  // Reward room desc.
  if (room->description && *room->description) {
    return_value += MIN(0.5, (float) strlen(room->description) * 0.0025);
  }
  
  // Reward night desc.
  if (room->night_desc && *room->night_desc) {
    return_value += MIN(0.5, (float) strlen(room->night_desc) * 0.0025);
  }

  // Reward exdescs.
  int num_exdescs = 0;
  for (struct extra_descr_data *exdesc = room->ex_description; exdesc; exdesc = exdesc->next) {
    num_exdescs++;
  }
  return_value += MIN(0.5, (float) num_exdescs * 0.1);
  
  if (room->matrix > 0) {
    // It has a jacked host vnum listed. Make sure it's valid, then spider it and add all those hosts as connected to the world.
    rnum_t host_rnum = real_host(room->matrix);

    if (host_rnum >= 0) {
      return_value += spider_connected_hosts_for_reward_func(&matrix[host_rnum], connected_hosts, connected_ics);
    }
  }
  
  // Ensure all surrounding same-zone rooms are added to the list.
  for (int dir = 0; dir < NUM_OF_DIRS; dir++) {
    if (EXIT2(room, dir)
        && EXIT2(room, dir)->to_room
        && get_zone_from_vnum(GET_ROOM_VNUM(EXIT2(room, dir)->to_room)) == get_zone_from_vnum(GET_ROOM_VNUM(room)))
    {
      return_value += spider_connected_rooms_for_reward_func(EXIT2(room, dir)->to_room, connected_rooms, connected_hosts, connected_ics);
    }
  }
  
  return return_value;
}

void calculate_zone_payout(struct char_data *ch, rnum_t zone_rnum) {
  std::unordered_map<vnum_t, struct room_data *> connected_rooms = {};
  std::unordered_map<vnum_t, struct char_data *> connected_mobs = {};
  std::unordered_map<vnum_t, struct obj_data *> connected_objs = {};
  std::unordered_map<vnum_t, struct host_data *> connected_hosts = {};
  std::unordered_map<vnum_t, struct veh_data *> connected_vehs = {};
  std::unordered_map<vnum_t, struct matrix_icon *> connected_ics = {};

  float reward_value = 1;

  // Look for rooms that connect to a separate, connected zone.
  for (vnum_t vnum = zone_table[zone_rnum].number * 100; vnum <= zone_table[zone_rnum].top; vnum++) {
    rnum_t temp_rnum;

    if ((temp_rnum = real_room(vnum)) > 0) {
      struct room_data *room = &world[temp_rnum];

      // We want to see at least one link to another zone in here.
      for (int dir = 0; dir < NUM_OF_DIRS; dir++) {
        struct zone_data *seen_zone;
        if (EXIT2(room, dir)
            && EXIT2(room, dir)->to_room
            && (seen_zone = get_zone_from_vnum(GET_ROOM_VNUM(EXIT2(room, dir)->to_room)))
            && seen_zone != &zone_table[zone_rnum]
            && seen_zone->approved)
        {
          // We know this room is connected. Add all its exits etc to our map.
          reward_value += spider_connected_rooms_for_reward_func(room, connected_rooms, connected_hosts, connected_ics);
          break;
        }
      }     
    }
  }

  // TODO: Count quest-related things.
  for (vnum_t vnum = zone_table[zone_rnum].number * 100; vnum <= zone_table[zone_rnum].top; vnum++) {
    rnum_t temp_rnum;

    if ((temp_rnum = real_quest(vnum)) > 0) {
      if (quest_table[temp_rnum].johnson > 0 && real_mobile(quest_table[temp_rnum].johnson) > 0) {
        // todo: count quest objective mobs (only if spawning in connected room)
        // todo: count quest reward obj (only if spawning in connected room)
        // todo: count quest objective obj (only if spawning in connected room)
      }
    }
  }

  // Iterate zcmds using our connected_x maps from above.
  #define ZONECMD zone_table[zone_rnum].cmd[cmd_idx]
  for (int cmd_idx = 0; cmd_idx < zone_table[zone_rnum].num_cmds; cmd_idx++) {
    switch (ZONECMD.command) {
      case 'M':
        // If room (rnum arg3) is connected, reward
        try {
          if (connected_rooms[world[ZONECMD.arg3].number]) {
            connected_mobs[ZONECMD.arg2] = &mob_proto[ZONECMD.arg1];
          }
        } catch (std::out_of_range) {}
        break;
      case 'O':
        // If room (rnum arg3) is connected, reward
        try {
          if (connected_rooms[world[ZONECMD.arg3].number]) {
            connected_objs[ZONECMD.arg2] = &obj_proto[ZONECMD.arg1];
          }
        } catch (std::out_of_range) {}
        break;
      case 'H':
        // If host (rnum arg3) is connected, reward
        try {
          if (connected_hosts[matrix[ZONECMD.arg3].vnum]) {
            connected_objs[ZONECMD.arg2] = &obj_proto[ZONECMD.arg1];
          }
        } catch (std::out_of_range) {}
        break;
      case 'V':
        // If room (rnum arg3) is connected, reward
        try {
          if (connected_rooms[world[ZONECMD.arg3].number])
            connected_vehs[ZONECMD.arg2] = &veh_proto[ZONECMD.arg1];
        } catch (std::out_of_range) {}
        break;
      case 'S':
        // We assume the veh is connected. If someone games the system with these, they'll just get banned.
        connected_mobs[ZONECMD.arg2] = &mob_proto[ZONECMD.arg1];
        break;
      case 'U':
      case 'I':
      case 'P':
        // We assume the veh/obj is connected. If someone games the system with these, they'll just get banned.
        connected_objs[ZONECMD.arg2] = &obj_proto[ZONECMD.arg1];
        break;
      case 'R': // no good way to track (don't reward)
      case 'G': // item to inventory (obsolete, don't reward)
      case 'E': // item to equipment (obsolete, don't reward)
      case 'C': // item to cyberware (obsolete, don't reward)
      case 'N': // item with qty to mob (obsolete, don't reward)
      case 'D': // close door (don't reward)
        break;
    }
  }
  #undef ZONECMD

  // Post-condition checks, which require that associated vnums are already anchored to the grid somehow.
  for (vnum_t vnum = zone_table[zone_rnum].number * 100; vnum <= zone_table[zone_rnum].top; vnum++) {
    rnum_t temp_rnum;
    if ((temp_rnum = real_shop(vnum)) > 0) {
      if (shop_table[temp_rnum].keeper > 0 && real_mobile(shop_table[temp_rnum].keeper) > 0) {
        // require that the keeper is on grid
        try {
          if (connected_mobs[shop_table[temp_rnum].keeper]) { /* no-op, just looking for membership */ }
        } catch (std::out_of_range) {
          continue;
        }

        // todo: count for-sale items
      }
    }
  }

  // todo: for each connected mob, count 'ware/equipment
  // todo: for each connected mob, scale reward based on description length

  // todo: for each connected obj, count attachments etc

  // todo: iterate all the connected things and add them to the reward_value var

  send_to_char(ch, "Calculated payout with current formula: %.2f syspoints.\r\n", reward_value);
}

ACMD(do_audit) {
  char arg1[MAX_INPUT_LENGTH];
  rnum_t zonenum;
  vnum_t number;

  char *remainder = any_one_arg(argument, arg1);

  // First form: `audit <zonenum>`
  number = atoi(arg1);
  if (number > 0) {
    // Parse it into the real zone num.
    zonenum = real_zone(number);

    // Make sure it's valid.
    if (zonenum < 0 || zonenum > top_of_zone_table) {
      send_to_char(ch, "That's not a zone.\r\n");
      return;
    }

    // Audit all entries in the zone.
    int issues = 0;
    issues += audit_zone_rooms_(ch, zonenum, TRUE);
    issues += audit_zone_mobs_(ch, zonenum, TRUE);
    issues += audit_zone_objects_(ch, zonenum, TRUE);
    issues += audit_zone_quests_(ch, zonenum, TRUE);
    issues += audit_zone_shops_(ch, zonenum, TRUE);
    issues += audit_zone_vehicles_(ch, zonenum, TRUE);
    issues += audit_zone_hosts_(ch, zonenum, TRUE);
    issues += audit_zone_ics_(ch, zonenum, TRUE);
    issues += audit_zone_commands_(ch, zonenum, TRUE);

    send_to_char(ch, "\r\nDone. Found a total of %d potential issue%s. Note that something being in this list does not disqualify the zone from approval-- it just requires extra scrutiny. Conversely, something not being flagged here doesn't mean it's kosher, it just means we didn't write a coded check for it yet.\r\n",
                 issues, issues != 1 ? "s" : "");

    if (access_level(ch, LVL_PRESIDENT)) {
      // calculate_zone_payout(ch, zonenum);
    }
    return;
  }

  // Second form: `audit all`
  if (is_abbrev(arg1, "all")) {
    AUDIT_ALL_ZONES(rooms);
    AUDIT_ALL_ZONES(mobs);
    AUDIT_ALL_ZONES(objects);
    AUDIT_ALL_ZONES(quests);
    AUDIT_ALL_ZONES(shops);
    AUDIT_ALL_ZONES(vehicles);
    AUDIT_ALL_ZONES(hosts);
    AUDIT_ALL_ZONES(ics);
    AUDIT_ALL_ZONES(commands);
    return;
  }

  // Third form: `audit submit <confirm>`
  if (is_abbrev(arg1, "submit")) {
    FAILURE_CASE(!*remainder || !str_cmp(remainder, "confirm"), "To submit your zone for review, use AUDIT SUBMIT CONFIRM while standing"
                                                                " anywhere in the zone you want to submit. This will remove your ability"
                                                                " to edit the zone and notify staff that it is ready for review.");
    
    // Get the zone they're in.
    struct room_data *in_room = get_ch_in_room(ch);
    FAILURE_CASE(!in_room, "idk how you did it but you broke it, notify staff");
    rnum_t zone_rnum = get_zone_index_number_from_vnum(GET_ROOM_VNUM(in_room));

    // Check the zone for editors.
    for (int editor_idx = 0; editor_idx < NUM_ZONE_EDITOR_IDS; editor_idx++) {
      // If this editor ID is theirs, remove it and post a wizlog about it.
      if (zone_table[zone_rnum].editor_ids[editor_idx] == GET_IDNUM(ch)) {
        zone_table[zone_rnum].editor_ids[editor_idx] = 0;
        write_zone_to_disk(zone_table[zone_rnum].number);

        mudlog_vfprintf(ch, LOG_WIZLOG, "^W%s^W has flagged zone %d (%s^W) as ready for auditing.^n",
                        GET_CHAR_NAME(ch),
                        zone_table[zone_rnum].number,
                        zone_table[zone_rnum].name);
        send_to_char(ch, "OK. Your ability to edit %s^n has been removed, and staff have been notified it's ready for review.\r\n", zone_table[zone_rnum].name);
        return;
      }
    }

    // If we got here, they don't have edit privs on this zone.
    send_to_char("You don't seem to be an editor for the zone you're standing in.\r\n", ch);
    return;
  }

  // Third form: `audit (rooms|mobs|objects|quests|shops...) [zonenum]`
  if (*remainder) {
    number = atoi(remainder);
    if (number <= 0 || ((zonenum = real_zone(number)) < 0 || zonenum > top_of_zone_table)) {
      send_to_char(ch, "'%s' is not a zone number. Syntax: AUDIT (rooms|mobs|objects|...) [zonenum]\r\n", remainder);
      return;
    }
  }

  AUDIT_CATEGORY("rooms", rooms);
  AUDIT_CATEGORY("mobs", mobs);
  AUDIT_CATEGORY("quests", quests);
  AUDIT_CATEGORY("shops", shops);
  AUDIT_CATEGORY("vehicles", vehicles);
  AUDIT_CATEGORY("hosts", hosts);
  AUDIT_CATEGORY("ics", ics);
  AUDIT_CATEGORY("commands", commands);

  // This one can have multiple valid forms, so it's separate.
  if (is_abbrev(arg1, "objects") || is_abbrev(arg1, "items")) {
    if (number <= 0) {
      AUDIT_ALL_ZONES(objects);
    } else {
      audit_zone_objects_(ch, zonenum, TRUE);
    }
    return;
  }

  send_to_char(ch, "Invalid command syntax. Expected one of the following:\r\n"
                   " AUDIT <zonenum>\r\n"
                   " AUDIT (rooms|mobs|objects|...) [zonenum]\r\n"
                   " AUDIT SUBMIT\r\n");
  return;
}

#undef AUDIT_ALL_ZONES
#undef AUDIT_CATEGORY

int create_dump(void)
{
  int procnum = fork();
  if(!procnum) {
    // We are the child, so crash.
    raise(SIGSEGV);
  }

  return procnum;
}

ACMD(do_coredump) {
  send_to_char("Creating core dump...\r\n", ch);
  int procnum = create_dump();
  send_to_char(ch, "Done. Child process %d has been created and crashed.\r\n", procnum);
}

ACMD(do_forceput) {
  char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
  struct obj_data *obj = NULL, *cont = NULL;
  int obj_dotmode, cont_dotmode;

  two_arguments(argument, arg1, arg2);
  obj_dotmode = find_all_dots(arg1, sizeof(arg1));
  cont_dotmode = find_all_dots(arg2, sizeof(arg2));

  if (!*arg1) {
    send_to_char("Force-put what in what?\r\n", ch);
    return;
  }

  if (obj_dotmode != FIND_INDIV) {
    send_to_char("You can only force-put one thing at a time.\r\n", ch);
    return;
  }

  if (!(obj = get_obj_in_list_vis(ch, arg1, ch->carrying))) {
    send_to_char(ch, "You aren't carrying %s %s.\r\n", AN(arg1), arg1);
    return;
  }

  if (!*arg2) {
    send_to_char("You must specify a container to put it into.", ch);
    return;
  }

  if (cont_dotmode != FIND_INDIV) {
    send_to_char("You can only force-put into one thing at a time.\r\n", ch);
    return;
  }

  if (!(cont = get_obj_in_list_vis(ch, arg2, ch->carrying))) {
    send_to_char(ch, "You aren't carrying %s %s.\r\n", AN(arg2), arg2);
    return;
  }

  send_to_char(ch, "Bypassing all restrictions and calculations, you forcibly put %s in %s. Hope you know what you're doing!\r\n", GET_OBJ_NAME(obj), GET_OBJ_NAME(cont));
  obj_from_char(obj);
  obj_to_obj(obj, cont);
}

ACMD(do_forceget) {
  char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
  struct obj_data *obj = NULL, *cont = NULL;
  int obj_dotmode, cont_dotmode;

  two_arguments(argument, arg1, arg2);
  obj_dotmode = find_all_dots(arg1, sizeof(arg1));
  cont_dotmode = find_all_dots(arg2, sizeof(arg2));

  if (!*arg1 || !*arg2) {
    send_to_char("Force-get what from what?\r\n", ch);
    return;
  }

  if (cont_dotmode != FIND_INDIV) {
    send_to_char("You can only force-get from one thing at a time.\r\n", ch);
    return;
  }

  if (!(cont = get_obj_in_list_vis(ch, arg2, ch->carrying))) {
    send_to_char(ch, "You aren't carrying %s %s.\r\n", AN(arg2), arg2);
    return;
  }

  if (obj_dotmode != FIND_INDIV) {
    send_to_char("You can only force-get one thing at a time.\r\n", ch);
    return;
  }

  if (!(obj = get_obj_in_list_vis(ch, arg1, cont->contains))) {
    send_to_char(ch, "%s doesn't have %s %s in it.\r\n", capitalize(GET_OBJ_NAME(cont)), AN(arg1), arg1);
    return;
  }

  send_to_char(ch, "Bypassing all restrictions and calculations, you forcibly get %s from %s. Hope you know what you're doing!\r\n", GET_OBJ_NAME(obj), GET_OBJ_NAME(cont));
  obj_from_obj(obj);
  obj_to_char(obj, ch);
}

bool is_invalid_ending_punct(char candidate) {
  // Non-puncts are all valid.
  if (!ispunct(candidate))
    return FALSE;

  // All punctuation other than these are invalid.
  return candidate != '"' && candidate != '\'' && candidate != ')';
}

#define NERPS_WARE_USAGE_STRING "Syntax: ^WMAKENERPS <BIOWARE|CYBERWARE> <essence/index cost in decimal form like 1.3> <nuyen cost> <VISIBLE|INTERNAL> <name>"
ACMD(do_makenerps) {
  struct obj_data *ware;
  char vis_buf[MAX_INPUT_LENGTH];
  bool is_visible;

  // syntax: makenerps <bio|cyber> <essencecost|index> <name>
  skip_spaces(&argument);

  if (!*argument) {
    send_to_char(NERPS_WARE_USAGE_STRING, ch);
    return;
  }

  argument = one_argument(argument, buf);  // BIOWARE or CYBERWARE
  argument = one_argument(argument, buf2); // a float for the essence / index cost
  argument = one_argument(argument, buf3); // an integer of nuyen cost
  argument = one_argument(argument, vis_buf); // an integer of nuyen cost
  // argument contains the restring

  if (!*argument) {
    // Failed to provide a restring or any of the arguments leading up to it.
    send_to_char(NERPS_WARE_USAGE_STRING, ch);
    return;
  }

  // Parse out our float.
  float essence_cost = atof(buf2);
  if (essence_cost < 0) {
    send_to_char(ch, "%f is not a valid essence cost. Please reference the book value.\r\n%s", essence_cost, NERPS_WARE_USAGE_STRING);
    return;
  }
  if (((int) (essence_cost * 1000)) % 10 != 0) {
    send_to_char("The code does not support essence values with more than two digits of precision (ex: 0.05 OK, 0.045 not OK). Please round appropriately and try again.\r\n", ch);
    return;
  }

  int nuyen_cost = atoi(buf3);
  if (nuyen_cost < 0) {
    send_to_char(ch, "%d is not a valid nuyen cost. Please reference the book value.\r\n%s", nuyen_cost, NERPS_WARE_USAGE_STRING);
    return;
  }

  if (is_abbrev(vis_buf, "visible") || is_abbrev(vis_buf, "external")) {
    is_visible = TRUE;
  } else if (is_abbrev(vis_buf, "invisible") || is_abbrev(vis_buf, "internal")) {
    is_visible = FALSE;
  } else {
    send_to_char(ch, "You must choose either VISIBLE or INTERNAL, not '%s'.\r\n%s", buf, NERPS_WARE_USAGE_STRING);
    return;
  }

  if (is_abbrev(buf, "cyberware")) {
    ware = read_object(OBJ_CUSTOM_NERPS_CYBERWARE, VIRTUAL, OBJ_LOAD_REASON_WIZLOAD);
    GET_CYBERWARE_ESSENCE_COST(ware) = (int) (essence_cost * 100);

    if (is_visible)
      SET_BIT(GET_CYBERWARE_FLAGS(ware), NERPS_WARE_VISIBLE);
  }
  else if (is_abbrev(buf, "bioware")) {
    ware = read_object(OBJ_CUSTOM_NERPS_BIOWARE, VIRTUAL, OBJ_LOAD_REASON_WIZLOAD);
    GET_BIOWARE_ESSENCE_COST(ware) = (int) (essence_cost * 100);

    if (is_visible)
      SET_BIT(GET_BIOWARE_FLAGS(ware), NERPS_WARE_VISIBLE);
  }
  else {
    send_to_char(ch, "You must choose either CYBERWARE or BIOWARE, not '%s'.\r\n%s", buf, NERPS_WARE_USAGE_STRING);
    return;
  }

  // Set the restring and cost.
  ware->restring = str_dup(argument);
  GET_OBJ_COST(ware) = nuyen_cost;

  // Flag it for review.
  GET_OBJ_EXTRA(ware).SetBit(ITEM_EXTRA_WIZLOAD);
  GET_OBJ_EXTRA(ware).SetBit(ITEM_EXTRA_NERPS);
  snprintf(buf, sizeof(buf), "%s made new custom %s: '%s^g' (%0.2f%c, %d nuyen)",
           GET_CHAR_NAME(ch),
           GET_OBJ_TYPE(ware) == ITEM_CYBERWARE ? "cyberware" : "bioware",
           GET_OBJ_NAME(ware),
           essence_cost,
           GET_OBJ_TYPE(ware) == ITEM_CYBERWARE ? 'e' : 'i',
           nuyen_cost
          );
  mudlog(buf, ch, LOG_CHEATLOG, TRUE);

  // Package it and hand it over.
  struct obj_data *container = shop_package_up_ware(ware);
  GET_OBJ_EXTRA(container).RemoveBit(ITEM_EXTRA_KEPT);
  obj_to_char(container, ch);

  send_to_char(ch, "Done, you are now carrying %s^n (%0.2f %s, %d nuyen).",
               GET_OBJ_NAME(container),
               essence_cost,
               GET_OBJ_TYPE(ware) == ITEM_CYBERWARE ? "essence" : "index",
               nuyen_cost
              );
}

bool vnum_is_from_zone(vnum_t vnum, int zone_num) {
  return vnum >= zone_table[zone_num].number * 100 && vnum <= zone_table[zone_num].top;
}

bool vnum_is_from_canon_zone(vnum_t vnum) {
  bool vnum_is_from_800s = (vnum / 100) >= 800 && (vnum / 100) <= 899;
  bool vnum_is_from_classic_canon = vnum < 1000;
  return vnum_is_from_800s || vnum_is_from_classic_canon;
}

int _check_for_zone_error(vnum_t vnum, int zone_num, char *buf, size_t buf_len, const char *error_type) {
  // Require that the vnum is from this zone or a canon zone.
  if (!vnum_is_from_zone(vnum, zone_num) && !vnum_is_from_canon_zone(vnum)) {
    snprintf(ENDOF(buf), buf_len - strlen(buf), "  - %s %ld from %sexternal zone.\r\n",
             error_type,
             vnum,
             vnum_from_non_approved_zone(vnum) ? "^ynon-approved^n " : ""
            );
    return 1;
  }
  return 0;
}

int _error_on_invalid_real_mob(rnum_t rnum, int zone_num, char *buf, size_t buf_len) {
  // Parse the vnum from the rnum.
  if (rnum >= 0)
    return _check_for_zone_error(GET_MOB_VNUM(&mob_proto[rnum]), zone_num, buf, buf_len, "mob");
  return 0;
}

int _error_on_invalid_real_room(rnum_t rnum, int zone_num, char *buf, size_t buf_len) {
  // Parse the vnum from the rnum.
  if (rnum >= 0)
    return _check_for_zone_error(GET_ROOM_VNUM(&world[rnum]), zone_num, buf, buf_len, "room");
  return 0;
}

int _error_on_invalid_real_obj(rnum_t rnum, int zone_num, char *buf, size_t buf_len) {
  // Parse the vnum from the rnum.
  if (rnum >= 0)
    return _check_for_zone_error(GET_OBJ_VNUM(&obj_proto[rnum]), zone_num, buf, buf_len, "obj");
  return 0;
}

int _error_on_invalid_real_host(rnum_t rnum, int zone_num, char *buf, size_t buf_len) {
  // Parse the vnum from the rnum.
  if (rnum >= 0)
    return _check_for_zone_error(matrix[rnum].vnum, zone_num, buf, buf_len, "host");
  return 0;
}

int _error_on_invalid_real_veh(rnum_t rnum, int zone_num, char *buf, size_t buf_len) {
  // Parse the vnum from the rnum.
  if (rnum >= 0)
    return _check_for_zone_error(GET_VEH_VNUM(&veh_proto[rnum]), zone_num, buf, buf_len, "veh");
  return 0;
}

ACMD(do_cheatmark) {
  skip_spaces(&argument);

  struct obj_data *obj = get_obj_in_list_vis(ch, argument, ch->carrying);

  FAILURE_CASE_PRINTF(!obj, "You don't seem to have any %ss in your inventory.", argument);

  const char *repr = generate_new_loggable_representation(obj);

  if (IS_OBJ_STAT(obj, ITEM_EXTRA_CHEATLOG_MARK)) {
    send_to_char(ch, "OK, removing cheatlog mark from %s.\r\n", GET_OBJ_NAME(obj));
    GET_OBJ_EXTRA(obj).RemoveBit(ITEM_EXTRA_CHEATLOG_MARK);
    mudlog_vfprintf(ch, LOG_CHEATLOG, "%s removed cheatmark from %s.", GET_CHAR_NAME(ch), repr);
  } else {
    send_to_char(ch, "OK, adding cheatlog mark to %s.\r\n", GET_OBJ_NAME(obj));
    GET_OBJ_EXTRA(obj).SetBit(ITEM_EXTRA_CHEATLOG_MARK);
    mudlog_vfprintf(ch, LOG_CHEATLOG, "%s added cheatmark to %s.", GET_CHAR_NAME(ch), repr);
  }

  delete [] repr;
}

// valset <obj> <slot> <value>
ACMD(do_valset) {
  char slot_arg[MAX_INPUT_LENGTH], obj_name[MAX_INPUT_LENGTH];
  int slot, value;
  struct obj_data *obj;

  // Split input into arguments.
  char *value_arg = two_arguments(argument, obj_name, slot_arg);
  FAILURE_CASE(!value_arg || !*value_arg, "Syntax: VALSET <obj> <slot> <value>");

  // Set the object.
  FAILURE_CASE_PRINTF(!(obj = get_obj_in_list_vis(ch, obj_name, ch->carrying)),
                      "You don't seem to have any %ss in your inventory.", obj_name);
  // Parse the slot.
  FAILURE_CASE_PRINTF((slot = atoi(slot_arg)) < 0 || slot > NUM_OBJ_VALUES, 
                      "You must provide a valid slot between 0 and %d, not %s. (syntax: valset %s <slot> <value>)", NUM_OBJ_VALUES, slot_arg, obj_name);
  // Parse the value to apply.
  FAILURE_CASE_PRINTF((value = atoi(value_arg)) < 0, 
                      "You must provide a value that is 0 or greater, not %s. (syntax: valset %s %d <value>)", NUM_OBJ_VALUES, value_arg, obj_name, slot);

  // Log it and notify the character.
  mudlog_vfprintf(ch, LOG_WIZLOG, "Changed object value %d for %s (%ld-%lu) from %d to %d.", 
                  slot, GET_OBJ_NAME(obj), GET_OBJ_VNUM(obj), GET_OBJ_IDNUM(obj), GET_OBJ_VAL(obj, slot), value);
  send_to_char(ch, "OK, changed %s's value %d from %d to %d.\r\n", GET_OBJ_NAME(obj), slot, GET_OBJ_VAL(obj, slot), value);

  // Apply the change.
  GET_OBJ_VAL(obj, slot) = value;
}

ACMD(do_cheatlog) {
  skip_spaces(&argument);
  FAILURE_CASE(!*argument, "Syntax: CHEATLOG <note to write to cheatlog for posterity>\r\n");
  mudlog_vfprintf(ch, LOG_CHEATLOG, "Note from %s: %s", GET_CHAR_NAME(ch), argument);
}