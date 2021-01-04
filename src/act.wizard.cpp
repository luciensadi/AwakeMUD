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

#if defined(WIN32) && !defined(__CYGWIN__)
#define popen(x,y) _popen(x,y)
#else
#include <sys/time.h>
#endif

#include "awake.h"
#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "newdb.h"
#include "dblist.h"
#include "house.h"
#include "memory.h"
#include "newmagic.h"
#include "newshop.h"
#include "quest.h"
#include "utils.h"
#include "constants.h"
#include "config.h"
#include "newmatrix.h"
#include "limits.h"
#include "security.h"
#include "perfmon.h"
#include "newmail.h"
#include "transport.h"

#if defined(__CYGWIN__)
#include <crypt.h>
#endif
extern class memoryClass *Mem;
extern class objList ObjList;

/*   external vars  */
extern FILE *player_fl;
extern int restrict;

/* for rooms */
extern void House_save_all();
/* for chars */

extern struct time_info_data time_info;
extern int mortal_start_room;
extern int immort_start_room;
extern vnum_t newbie_start_room;
extern int frozen_start_room;
extern int last;
extern int max_ability(int i);

extern const char *wound_arr[];
extern const char *material_names[];
extern const char *wound_name[];
extern int ViolencePulse;

extern void list_detailed_shop(struct char_data *ch, long shop_nr);
extern void list_detailed_quest(struct char_data *ch, long rnum);
extern int vnum_vehicles(char *searchname, struct char_data * ch);
extern void disp_init_menu(struct descriptor_data *d);

extern const char *pgroup_print_privileges(Bitfield privileges);
extern void nonsensical_reply(struct char_data *ch, const char *arg);
extern void display_pockets_to_char(struct char_data *ch, struct char_data *vict);

extern struct elevator_data *elevator;
extern int num_elevators;

extern int write_quests_to_disk(int zone);
extern void write_world_to_disk(int vnum);
extern void alarm_handler(int signal);
extern bool can_edit_zone(struct char_data *ch, int zone);
extern const char *render_door_type_string(struct room_direction_data *door);

ACMD_DECLARE(do_goto);

/* Copyover Code, ported to Awake by Harlequin *
 * (c) 1996-97 Erwin S. Andreasen <erwin@andreasen.org> */

extern int mother_desc, port;

/* Prototypes. */
void restore_character(struct char_data *vict, bool reset_staff_stats);


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
  int mesnum = number(0, 18);
  
  /* Old messages, preserved for posterity.
  // "I like copyovers, yes I do!  Eating player corpses in a copyover stew!\r\n",
  // "A Haiku while you wait: Copyover time.  Your quests and corpses are fucked.  Ha ha ha ha ha.\r\n",
  // "Yes. We did this copyover solely to fuck YOUR character over.\r\n",
  // "Ahh drek, Maestra's broke the mud again!  Go bug Che and he might fix it.\r\n",
  // "Deleting player corpses, please wait...\r\n",
  */
  const char *messages[] =
    {
      "This copyover has been brought to you by NERPS.  It's more than a lubricant, it's a lifestyle!\r\n", // 0
      "Yes, the mud is lagging.  Deal with it.\r\n",
      "Its a copyover.  Now would be a good time to take out the trash.\r\n",
      "Jerry Garcia told me to type copyover.  He is wise, isn't he?\r\n",
      "My dog told me to copyover. Goood dog, good dog.\r\n",
      "It's called a changeover, the movie goes on, and nobody in the audience has any idea.\r\n", // 5
      "Oh shit, I forgot to compile.  I'm gonna have to do this again!\r\n",
      "Please wait while your character is deleted.\r\n",
      "You are mortally wounded and will die soon if not aided.\r\n",
      "Connection closed by foreign host.\r\n",
      "Someone says \x1B[0;35mOOCly\x1B[0m, \"I'm going to get fired for this.\"\r\n", // 10
      "Yum Yum Copyover Stew, out with the old code, in with the new!\r\n",
      "\x1B[0;35m[\x1B[0mSerge\x1B[0;35m] \x1B[0;31m(\x1B[0mOOC\x1B[0;31m)\x1B[0m, \"This porn's taking too long to download, needs more bandwidth. So the Mud'll be back up in a bit.\"\r\n",
      "\x1B[0;35m[\x1B[0mLucien\x1B[0;35m] \x1B[0;31m(\x1B[0mOOC\x1B[0;31m)\x1B[0m, \"Honestly, I give this new code a 30% chance of crashing outright.\"\r\n",
      "There's a sound like a record scratching, and everything around you stutters to a standstill.",
      "One moment while we drive up the server cost with heavy CPU usage...\r\n", //15
      "You wake up. You're still a lizard sunning on a red rock. It was all a dream. The concept of selling 'feet pics' to pay back 'ripperdocs' is already losing its meaning as you open and lick your own eyeballs to moisten them. Time to eat a bug.\r\n",
      "For the briefest of moments, you peer beyond the veil, catching a glimpse of the whirling, gleaming machinery that lies at the heart of the world. Your mind begins to break down at the sight...\r\n",
      "Your vision goes black, then starts to fade in again. You're sitting in the back of a cart, your hands bound before you. A disheveled blonde man sitting across from you meets your eyes. \"Hey, you. You're finally awake.\""
    };

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
      
    if (GET_QUEST(och)) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s%s",
               num_questors > 0 ? "^n, ^c" : "^c",
               GET_CHAR_NAME(och));
      num_questors += 1;
    }
    // Count PCs in cabs.
    if (och->in_room && room_is_a_taxicab(GET_ROOM_VNUM(och->in_room)))
      cab_inhabitants++;
  }
  
  // Check for PC corpses with things still in them.
  int num_corpses = ObjList.CountPlayerCorpses();
  
  skip_spaces(&argument);
  if (str_cmp(argument, "force") != 0) {
    if (num_questors > 0) {
      send_to_char(ch, "Copyover aborted, there %s %d character%s doing autoruns right now. Use 'copyover force' to override this.\r\n%s^n.\r\n",
                   num_questors != 1 ? "are" : "is",
                   num_questors, 
                   num_questors != 1 ? "s" : "",
                   buf);
      return;
    }
    
    // Check for PCs in non-playing states.
    if (fucky_states > 0) {
      send_to_char(ch, "Copyover aborted, %d player%s not in the playing state. Check USERS for details. Use 'copyover force' to override this.\r\n",
                   fucky_states, fucky_states != 1 ? "s are" : " is");
      return;
    }
    
    if (num_corpses) {
      send_to_char(ch, "Copyover aborted, there %s %d player corpse%s out there with things still in them.\r\n", 
                   num_corpses != 1 ? "are" : "is",
                   num_corpses,
                   num_corpses != 1 ? "s" : "");
      return;
    }
    
    if (cab_inhabitants) {
      send_to_char(ch, "Copyover aborted, there %s %d %s.\r\n", 
                   cab_inhabitants != 1 ? "are" : "is",
                   cab_inhabitants,
                   cab_inhabitants != 1 ? "people taking taxis" : "person taking a cab");
      return;
    }
  } else if (ch->desc){
    snprintf(buf, sizeof(buf), "Forcibly copying over. This will disconnect %d player%s, delete %d corpse%s, refund %d cab fare%s, and drop %d quest%s.\r\n",
             fucky_states,    fucky_states    != 1 ? "s" : "",
             num_corpses,     num_corpses     != 1 ? "s" : "",
             cab_inhabitants, cab_inhabitants != 1 ? "s" : "",
             num_questors,    num_questors    != 1 ? "s" : "");
    write_to_descriptor(ch->desc->descriptor, buf);    
  } else {
    log("WTF, ch who initiated copyover had no desc? ;-;");
  }

  snprintf(buf, sizeof(buf), "Copyover initiated by %s", GET_CHAR_NAME(ch));
  mudlog(buf, ch, LOG_WIZLOG, TRUE);


  log("Disconnecting players.");
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
      GET_NUYEN(och) += MAX_CAB_FARE;
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
    playerDB.SaveChar(och, GET_LOADROOM(och));
    write_to_descriptor(d->descriptor, messages[mesnum]);
  }

  fprintf (fp, "-1\n");
  fclose (fp);
  
  log("Saving houses.");
  House_save_all();
  /* Close reserve and other always-open files and release other resources */
  
  // Save vehicles.
  log("Saving vehicles.");
  save_vehicles();


  snprintf(buf, sizeof(buf), "%d", port);
  snprintf(buf2, sizeof(buf2), "-o%d", mother_desc);
  /* Ugh, seems it is expected we are 1 step above lib - this may be dangerous! */

  chdir ("..");

  execl (EXE_FILE, "awake", buf2, buf, (char *) NULL);

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
            !(subcmd == SCMD_AECHO && !(IS_ASTRAL(d->character) || IS_DUAL(d->character))))
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
  struct char_data *vict;
  struct veh_data *veh;
  skip_spaces(&argument);

  if (!*argument) {
    send_to_char("Yes.. but what?\r\n", ch);
    return;
  }
  else {
    if (PLR_FLAGGED(ch, PLR_MATRIX) && !ch->persona) {
      send_to_char(ch, "You can't do that while hitching.\r\n");
      return;
    }
    memset(buf, '\0', sizeof(buf));
    switch (subcmd) {
      case SCMD_EMOTE:
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
        if (!PRF_FLAGGED(ch, PRF_QUESTOR) && (ch->desc && ch->desc->original ? GET_LEVEL(ch->desc->original) : GET_LEVEL(ch)) < LVL_ARCHITECT) {
          nonsensical_reply(ch, NULL);
          return;
        }
        snprintf(buf, sizeof(buf), "%s", argument);
        break;
    }

    if (subcmd == SCMD_AECHO) {
      if (ch->in_room) {
        for (vict = ch->in_room->people; vict; vict = vict->next_in_room)
          if (ch != vict && (IS_ASTRAL(vict) || IS_DUAL(vict)))
            act(buf, FALSE, ch, 0, vict, TO_VICT);
      } else {
        for (vict = ch->in_veh->people; vict; vict = vict->next_in_veh)
          if (ch != vict && (IS_ASTRAL(vict) || IS_DUAL(vict)))
            act(buf, FALSE, ch, 0, vict, TO_VICT);
      }
    } else {
      if (PLR_FLAGGED(ch, PLR_MATRIX))
        send_to_host(ch->persona->in_host, buf, ch->persona, TRUE);
      else {
        /*
        const char *parse_emote_by_x_for_y(const char *emote, struct char_data *ch, struct char_data *targ) {
          char tmp_name[strlen(emote) + 1];
          int tmp_index, buf2_index;
          struct char_data *tmp_vict;
          
          // Actor is recipient? Skip (they're handled elsewhere in logic).
          if (targ == ch)
            return NULL;
          
          // Actor is intangible and invisible to recipient? Skip.
          if ((IS_ASTRAL(ch) || IS_PROJECT(ch)) && !(IS_ASTRAL(targ) || IS_PROJECT(targ) || IS_DUAL(targ)))
            return NULL;
          
          // Scan the emote for @ character, which indicates targeting.
          for (int i = 0; emote[i]; i++) {
            if (emote[i] == '@') {
              // Found an @. Set up for evaluation, and store current 'i' as marker.
              tmp_index = 0;
              marker = i;
              
              // Copy out the target's name, null-terminating it.
              while (isalpha(emote[++i]))
                tmp_name[tmp_index++] = i;
              tmp_name[tmp_index] = '\0';
              
              // Move 'i' back one character so it is pointing at the last character of the @-name.
              i--;
              
              // Search the room for the target.
              tmp_vict = get_char_room_vis(ch, tmp_name);
              if (tmp_vict == targ) {
                // Check for "@'s" -> "your"
                if (i <= strlen(emote) - 3 && emote[i+1] == '\'' && emote[i+2] == 's') {
                  i += 1; // Advance to the "s".
                  snprintf(tmp_name, sizeof(tmp_name), "your");
                } else {
                  snprintf(tmp_name, sizeof(tmp_name), "you");
                }
              } // End of targ == vict
              else if (tmp_vict) {
                // Check for visibility.
                if (CAN_SEE(targ, vict)) {
                  if (found_mem(GET_MEMORY(targ), vict)) {
                    strcpy(tmp_name, CAP(found_mem(GET_MEMORY(targ), vict)->mem));
                  } else {
                    strcpy(tmp_name, GET_NAME(vict));
                  }
                } else {
                  strcpy(tmp_name, "someone");
                }
              } // End of targ != vict
              else {
                // todo
                broken compile here
              }
            } // End of char == '@'
            else {
              // No special character, copy it 1:1.
              buf2[buf2_index++] = emote[i];
            }
          }
        }
         */
        for (struct char_data *targ = ch->in_veh ? ch->in_veh->people : ch->in_room->people; targ; targ = ch->in_veh ? targ->next_in_veh : targ->next_in_room)
          if (targ != ch) {
            if ((IS_ASTRAL(ch) || IS_PROJECT(ch)) && !(IS_ASTRAL(targ) || IS_PROJECT(targ) || IS_DUAL(targ)))
              continue;
            int i = 0, newn = 0;
            memset(buf2, '\0', sizeof(buf2));
            for (; buf[i]; i++)
              if (buf[i] == '@') {
                char check[1024];
                int checkn = 0;
                memset(check, '\0', sizeof(check));
                for (i++; buf[i]; i++, checkn++)
                  if (buf[i] == ' ' || buf[i] == '\'' || buf[i] == '.' || buf[i] == ',') {
                    i--;
                    break;
                  } else check[checkn] = buf[i];
                struct char_data *vict = get_char_room_vis(ch, check);
                if (vict == targ)
                  snprintf(check, sizeof(check), "you");
                else if (vict)
                  snprintf(check, sizeof(check), "%s", CAN_SEE(targ, vict) ? (found_mem(GET_MEMORY(targ), vict) ? CAP(found_mem(GET_MEMORY(targ), vict)->mem)
                          : GET_NAME(vict)) : "someone");
                snprintf(buf2 + newn, sizeof(buf) - newn, "%s", check);
                newn += strlen(check);
              } else {
                buf2[newn] = buf[i];
                newn++;
              }
            act(buf2, FALSE, ch, 0, targ, TO_VICT);
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
}

ACMD(do_send)
{
  struct char_data *vict;

  half_chop(argument, arg, buf);

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
          !(subcmd == SCMD_AECHO && !(IS_ASTRAL(d->character) || IS_DUAL(d->character))))
        act(argument, FALSE, d->character, 0, 0, TO_CHAR);

    if (PRF_FLAGGED(ch, PRF_NOREPEAT))
      send_to_char(OK, ch);
    else
      act(argument, FALSE, ch, 0, 0, TO_CHAR);
    argument[50] = '\0';
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
      send_to_char("That object is not available.\r\n", ch);
      return NULL;
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

  half_chop(argument, buf, command);
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
  
  half_chop(argument, buf, command);
  
  // Look for taxi destinations - Seattle.
  struct dest_data *dest_data_list = seattle_taxi_destinations;
  
  // Seattle taxi destinations, including deactivated and invalid ones.
  for (int dest = 0; !location && *(dest_data_list[dest].keyword) != '\n'; dest++) {
    if (str_str(buf, dest_data_list[dest].keyword) && (rnum = real_room(dest_data_list[dest].vnum)) >= 0) {
      location = &world[rnum];
      break;
    }
  }
  
  // Portland taxi destinations.
  if (!location) {
    dest_data_list = portland_taxi_destinations;
    for (int dest = 0; !location && *(dest_data_list[dest].keyword) != '\n'; dest++) {
      if (str_str(buf, dest_data_list[dest].keyword) && (rnum = real_room(dest_data_list[dest].vnum)) >= 0) {
        location = &world[rnum];
        break;
      }
    }
  }
  
  // Caribbean taxi destinations.
  if (!location) {
    dest_data_list = caribbean_taxi_destinations;
    for (int dest = 0; !location && *(dest_data_list[dest].keyword) != '\n'; dest++) {
      if (str_str(buf, dest_data_list[dest].keyword) && (rnum = real_room(dest_data_list[dest].vnum)) >= 0) {
        location = &world[rnum];
        break;
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
  
  // Perform location validity check for room number.
  if (location->number == 0 || location->number == 1) {
    send_to_char("You're not able to GOTO that room. If you need to do something there, use AT.\r\n", ch);
    return;
  }
  
  // Perform location validity check for level lock.
  if (location->staff_level_lock > GET_REAL_LEVEL(ch)) {
    send_to_char(ch, "Sorry, you need to be a level-%d immortal to go there.\r\n", location->staff_level_lock);
    return;
  }

  if (POOFOUT(ch))
    act(POOFOUT(ch), TRUE, ch, 0, 0, TO_ROOM);
  else
    act("$n disappears in a puff of smoke.", TRUE, ch, 0, 0, TO_ROOM);

  char_from_room(ch);
  
  if (vict) {
    char_to_veh(vict->in_veh, ch);
    vict->in_veh->seating[ch->vfront]++;
    ch->vfront = vict->vfront;
    vict->in_veh->seating[ch->vfront]--;
  } else {
    char_to_room(ch, location);
    GET_POS(ch) = POS_STANDING;
  }

  if (POOFIN(ch))
    act(POOFIN(ch), TRUE, ch, 0, 0, TO_ROOM);
  else
    act("$n appears with an ear-splitting bang.", TRUE, ch, 0, 0, TO_ROOM);

  look_at_room(ch, 0);
}

void transfer_ch_to_ch(struct char_data *victim, struct char_data *ch) {
  if (!ch || !victim)
    return;
  
  act("$n is whisked away by the game's administration.", TRUE, victim, 0, 0, TO_ROOM);
  if (AFF_FLAGGED(victim, AFF_PILOT))
    AFF_FLAGS(victim).ToggleBit(AFF_PILOT);
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
  look_at_room(victim, 0);
}

ACMD(do_trans)
{
  ACMD_DECLARE(do_transfer);

  if (!access_level(ch, LVL_CONSPIRATOR)) {
    do_transfer(ch, argument, 0, 0);
    return;
  }
  struct descriptor_data *i;
  struct char_data *victim;

  any_one_arg(argument, buf);

  if (!*buf)
    send_to_char("Whom do you wish to transfer?\r\n", ch);
  else if (str_cmp("all", buf)) {
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
    if (!access_level(ch, LVL_DEVELOPER)) {
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
    veh_from_room(veh);
    veh_to_room(veh, target);
    snprintf(buf, sizeof(buf), "%s screeches suddenly into the area.\r\n", GET_VEH_NAME(veh));
    send_to_room(buf, veh->in_room);
    send_to_veh("You lose track of your surroundings.\r\n", veh, NULL, TRUE);
    snprintf(buf2, sizeof(buf2), "%s teleported %s to %s.", GET_CHAR_NAME(ch), GET_VEH_NAME(veh), target->name);
    mudlog(buf2, ch, LOG_WIZLOG, TRUE);
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
    send_to_char(OK, ch);
    act("$n disappears in a puff of smoke.", TRUE, victim, 0, 0, TO_ROOM);
    if (AFF_FLAGGED(victim, AFF_PILOT))
      AFF_FLAGS(victim).ToggleBit(AFF_PILOT);
    char_from_room(victim);
    char_to_room(victim, target);
    act("$n arrives from a puff of smoke.", TRUE, victim, 0, 0, TO_ROOM);
    act("$n has teleported you!", FALSE, ch, 0, victim, TO_VICT);
    look_at_room(victim, 0);
    snprintf(buf2, sizeof(buf2), "%s teleported %s to %s",
            GET_CHAR_NAME(ch), IS_NPC(victim) ? GET_NAME(victim) : GET_CHAR_NAME(victim), target->name);
    mudlog(buf2, ch, LOG_WIZLOG, TRUE);
  }
}

ACMD(do_vnum)
{
  argument = one_argument(argument, buf);
  strcpy(buf2, argument);

  if (!*buf || !*buf2 || (!is_abbrev(buf, "mob") && !is_abbrev(buf, "obj") && !is_abbrev(buf, "veh"))) {
    send_to_char("Usage: vnum { obj | mob | veh } <name>\r\n", ch);
    return;
  }
  if (is_abbrev(buf, "veh"))
    if (!vnum_vehicles(buf2, ch))
      send_to_char("No vehicles by that name.\r\n", ch);

  if (is_abbrev(buf, "mob"))
    if (!vnum_mobile(buf2, ch))
      send_to_char("No mobiles by that name.\r\n", ch);

  if (is_abbrev(buf, "obj"))
    if (!vnum_object(buf2, ch))
      send_to_char("No objects by that name.\r\n", ch);
}

void do_stat_room(struct char_data * ch)
{
  struct extra_descr_data *desc;
  struct room_data *rm = ch->in_room;
  int i, found = 0;
  struct obj_data *j = 0;
  struct char_data *k = 0;

  send_to_char(ch, "Room name: ^c%s\r\n", rm->name);

  sprinttype(rm->sector_type, spirit_name, buf2, sizeof(buf2));
  snprintf(buf, sizeof(buf), "Zone: [%3d], VNum: [^g%8ld^n], RNum: [%5ld], Rating: [%2d], Type: %s\r\n",
          rm->zone, rm->number, real_room(rm->number), rm->rating, buf2);
  send_to_char(buf, ch);

  rm->room_flags.PrintBits(buf2, MAX_STRING_LENGTH, room_bits, ROOM_MAX);
  snprintf(buf, sizeof(buf), "Extra: [%4d], SpecProc: %s, Flags: %s Light: %s Smoke: %s\r\n", rm->spec,
          (rm->func == NULL) ? "None" : "^CExists^n", buf2, light_levels[light_level(rm)], light_levels[rm->vision[1]]);
  send_to_char(buf, ch);

  send_to_char("Description:\r\n", ch);
  if (rm->description)
    send_to_char(rm->description, ch);
  else
    send_to_char("  None.\r\n", ch);

  if (rm->ex_description)
  {
    strcpy(buf, "Extra descs:^c");
    for (desc = rm->ex_description; desc; desc = desc->next) {
      strcat(buf, " ");
      strcat(buf, desc->keyword);
    }
    send_to_char(strcat(buf, "^n\r\n"), ch);
  }
  strcpy(buf, "Chars present:^y");
  for (found = 0, k = rm->people; k; k = k->next_in_room)
  {
    if (!CAN_SEE(ch, k))
      continue;
    snprintf(buf2, sizeof(buf2), "%s %s(%s)", found++ ? "," : "", IS_NPC(k) ? GET_NAME(k) : GET_CHAR_NAME(k),
            (!IS_NPC(k) ? "PC" : (!IS_MOB(k) ? "NPC" : "MOB")));
    strcat(buf, buf2);
    if (strlen(buf) >= 62) {
      if (k->next_in_room)
        send_to_char(strcat(buf, ",\r\n"), ch);
      else
        send_to_char(strcat(buf, "\r\n"), ch);
      *buf = found = 0;
    }
  }

  if (*buf)
    send_to_char(strcat(buf, "\r\n"), ch);

  if (rm->contents)
  {
    strcpy(buf, "Contents:^g");
    for (found = 0, j = rm->contents; j; j = j->next_content) {
      if (!CAN_SEE_OBJ(ch, j))
        continue;
      snprintf(buf2, sizeof(buf2), "%s %s", found++ ? "," : "", GET_OBJ_NAME(j));
      strcat(buf, buf2);
      if (strlen(buf) >= 62) {
        if (j->next_content)
          send_to_char(strcat(buf, ",\r\n"), ch);
        else
          send_to_char(strcat(buf, "\r\n"), ch);
        *buf = found = 0;
      }
    }

    if (*buf)
      send_to_char(strcat(buf, "\r\n"), ch);
  }

  for (i = 0; i < NUM_OF_DIRS; i++)
  {
    if (rm->dir_option[i]) {
      if (!rm->dir_option[i]->to_room)
        strcpy(buf1, " ^cNONE^n");
      else
        snprintf(buf1, sizeof(buf1), "^c%8ld^n", rm->dir_option[i]->to_room->number);
      sprintbit(rm->dir_option[i]->exit_info, exit_bits, buf2);
      snprintf(buf, sizeof(buf), "Exit ^c%-5s^n:  To: [^c%s^n], Key: [^c%8ld^n], Keyword: "
              "^c%s^n, Type: ^c%s^n\r\n ", dirs[i], buf1, rm->dir_option[i]->key,
              rm->dir_option[i]->keyword ? rm->dir_option[i]->keyword : "None", buf2);
      send_to_char(buf, ch);
      if (rm->dir_option[i]->general_description)
        strcpy(buf, rm->dir_option[i]->general_description);
      else
        strcpy(buf, "  No exit description.\r\n");
      send_to_char(buf, ch);
    }
  }
}

void do_stat_host(struct char_data *ch, struct host_data *host)
{
  snprintf(buf, sizeof(buf), "Name: '^y%s^n', Keywords: %s\r\n", host->name, host->keywords);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Vnum: [^g%8ld^n] Rnum: [%5ld] Parent: [%8ld]\r\n",
               host->vnum, real_host(host->vnum), host->parent);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Type: [%10s] Security: [%s-%d^n] Alert: [%6s]\r\n", host_type[host->type],
               host_sec[host->colour], host->security, alerts[host->alert]);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Shutdown: [%2d] Successes: [%2d] MPCP: [%2d]\r\n", host->shutdown,
               host->shutdown_success, host->shutdown_mpcp);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Access: [^B%2ld^n] Control: [^B%2ld^n] Index: [^B%2ld^n] Files: [^B%2ld^n] Slave: [^B%2ld^n]\r\n",
               host->stats[ACCESS][0], host->stats[CONTROL][0], host->stats[INDEX][0], host->stats[FILES][0], host->stats[SLAVE][0]);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Deckers Present: ^y");
  bool prev = FALSE;
  for (struct matrix_icon *icon = host->icons; icon; icon = icon->next_in_host)
    if (icon->decker && icon->decker->ch)
    {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s%s (%d)", prev ? ", " : "", GET_CHAR_NAME(icon->decker->ch), icon->decker->tally);
      prev = TRUE;
    }
  strcat(buf, "^n\r\n");
  send_to_char(buf, ch);
}

void do_stat_veh(struct char_data *ch, struct veh_data * k)
{
  long virt;
  virt = veh_index[k->veh_number].vnum;
  snprintf(buf, sizeof(buf), "Name: '^y%s^n', Aliases: %s\r\n",
          k->short_description, k->name);

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Vnum: [^g%8ld^n] Rnum: [%5ld] Type: [%10s] Idnum: [%8ld] Owner: [%8ld]\r\n",
          virt, k->veh_number, veh_type[k->type], k->idnum, k->owner);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Han: [^B%d^n]  Spe: [^B%d^n]  Acc: [^B%d^n]  Bod: [^B%d^n]  Arm: [^B%d^n]\r\n",
          k->handling, k->speed, k->accel, k->body, k->armor);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Sig: [^B%d^n]  Aut: [^B%d^n]  Pil: [^B%d^n]  Sea: [^B%d/%d^n]  Loa: [^B%d/%d^n]  Cos: [^B%d^n]\r\n",
          k->sig, k->autonav, k->pilot, k->seating[1], k->seating[0], (int)k->usedload, (int)k->load, k->cost);
  send_to_char(buf, ch);
}

void do_stat_object(struct char_data * ch, struct obj_data * j)
{
  long virt;
  int i, found;
  struct obj_data *j2;
  struct extra_descr_data *desc;

  virt = GET_OBJ_VNUM(j);
  snprintf(buf, sizeof(buf), "Name: '^y%s^n', Aliases: %s\r\nSource Book: %s\r\n",
          ((j->text.name) ? j->text.name : "<None>"),
          j->text.keywords,
          j->source_info ? j->source_info : "<None>");

  sprinttype(GET_OBJ_TYPE(j), item_types, buf1, sizeof(buf1));

  if (GET_OBJ_RNUM(j) >= 0)
  {
    if (GET_OBJ_TYPE(j) == ITEM_WEAPON)
      strcpy(buf2, (obj_index[GET_OBJ_RNUM(j)].wfunc ? "^cExists^n" : "None"));
    else
      strcpy(buf2, (obj_index[GET_OBJ_RNUM(j)].func ? "^cExists^n" : "None"));
  } else
    strcpy(buf2, "None");
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "VNum: [^g%8ld^n], RNum: [%5ld], Type: %s, SpecProc: %s\r\n",
          virt, GET_OBJ_RNUM(j), buf1, buf2);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "L-Des: %s\r\n",
          ((j->text.look_desc) ? j->text.look_desc : "None"));

  if (j->ex_description)
  {
    strcat(buf, "Extra descs:^c");
    for (desc = j->ex_description; desc; desc = desc->next) {
      strcat(buf, " ");
      strcat(buf, desc->keyword);
    }
    strcat(buf, "\r\n");
  }

  j->obj_flags.wear_flags.PrintBits(buf2, MAX_STRING_LENGTH,
                                    wear_bits, NUM_WEARS);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Can be worn on: %s\r\n", buf2);

  j->obj_flags.bitvector.PrintBits(buf2, MAX_STRING_LENGTH,
                                   affected_bits, AFF_MAX);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Set char bits : %s\r\n", buf2);

  GET_OBJ_EXTRA(j).PrintBits(buf2, MAX_STRING_LENGTH,
                             extra_bits, ITEM_EXTRA_MAX);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Extra flags   : %s\r\n", buf2);

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Weight: %.2f, Value: %d, Timer: %d, Availability: %d/%.2f Days\r\n",
          GET_OBJ_WEIGHT(j), GET_OBJ_COST(j), GET_OBJ_TIMER(j), GET_OBJ_AVAILTN(j), GET_OBJ_AVAILDAY(j));

  strcat(buf, "In room: ");
  if (!j->in_room)
    strcat(buf, "Nowhere");
  else
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%ld (IR %ld)", j->in_room->number, real_room(j->in_room->number));
  strcat(buf, ", In object: ");
  strcat(buf, j->in_obj ? j->in_obj->text.name : "None");
  strcat(buf, ", Carried by: ");
  if (j->carried_by)
    strcat(buf, GET_CHAR_NAME(j->carried_by));
  else
    strcat(buf, "Nobody");
  strcat(buf, ", Worn by: ");
  if (j->worn_by)
    strcat(buf, GET_CHAR_NAME(j->worn_by) ? GET_CHAR_NAME(j->worn_by): "BROKEN");
  else
    strcat(buf, "Nobody");
  strcat(buf, ", In vehicle: ");
  if (j->in_veh)
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%ld", j->in_veh->veh_number);
  else
    strcat(buf, "None");

  strcat(buf, "\r\n");

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
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Power: %d, Wound: %s, Str+: %d, WeapType: %s, Skill: %s\r\nMax Ammo: %d, Range: %d",
            GET_OBJ_VAL(j, 0), wound_arr[GET_OBJ_VAL(j, 1)], GET_OBJ_VAL(j, 2),
            weapon_type[GET_OBJ_VAL(j, 3)],
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
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Type: %s, Rating: %d", patch_names[GET_OBJ_VAL(j, 0)],
            GET_OBJ_VAL(j, 1));
    break;
  case ITEM_CYBERDECK:
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "MPCP: %d, Hardening: %d, Active: %d, Storage: %d, Load: %d",
            GET_OBJ_VAL(j, 0), GET_OBJ_VAL(j, 1), GET_OBJ_VAL(j, 2),
            GET_OBJ_VAL(j, 3), GET_OBJ_VAL(j, 4));
    break;
  case ITEM_PROGRAM:
    if (GET_OBJ_VAL(j, 0) == SOFT_ATTACK)
      snprintf(buf2, sizeof(buf2), ", DamType: %s", wound_name[GET_OBJ_VAL(j, 3)]);
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
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Type: %s, For: %s", GET_OBJ_VAL(j, 1) ? GET_OBJ_VAL(j, 1) == 3 ? "Facility": "Workshop" : "Kit", 
            workshops[GET_OBJ_VAL(j, 0)]);
    break;
  case ITEM_FOCUS:
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Type: %s Force: %d", foci_type[GET_OBJ_VAL(j, 0)], GET_OBJ_VAL(j, 1));
    break;
  case ITEM_SPELL_FORMULA:
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Type: %s Force: %d Tradition: %s", spells[GET_OBJ_VAL(j, 1)].name, GET_OBJ_VAL(j, 0), GET_OBJ_VAL(j, 2) ? "Shamanic" : "Hermetic");
  default:
    break;
  }

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\nValues 0-9: [%d] [%d] [%d] [%d] [%d] [%d] [%d] [%d] [%d] [%d] [%d] [%d]",
          GET_OBJ_VAL(j, 0), GET_OBJ_VAL(j, 1),
          GET_OBJ_VAL(j, 2), GET_OBJ_VAL(j, 3), GET_OBJ_VAL(j, 4),
          GET_OBJ_VAL(j, 5), GET_OBJ_VAL(j, 6), GET_OBJ_VAL(j, 7),
          GET_OBJ_VAL(j, 8), GET_OBJ_VAL(j, 9), GET_OBJ_VAL(j, 10), GET_OBJ_VAL(j, 11));

  /*
   * I deleted the "equipment status" code from here because it seemed
   * more or less useless and just takes up valuable screen space.
   */
  if (j->contains)
  {
    strcat(buf, "\r\nContents:^g");
    for (found = 0, j2 = j->contains; j2; j2 = j2->next_content) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s %s", found++ ? "," : "", GET_OBJ_NAME(j2));
      if (strlen(buf) >= 62)
        if (j2->next_content)
          strcat(buf, ",\r\n");
    }
  }
  strcat(buf, "^n\r\n");
  found = 0;
  strcat(buf, "Affections:");
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
    strcat(buf, " None");
  strcat(buf, "\r\n");
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
    send_to_char("Sorry, you can't do that.\r\n", ch);
    return;
  }

  switch (GET_SEX(k))
  {
  case SEX_NEUTRAL:
    strcpy(buf, "NEUTRAL-SEX");
    break;
  case SEX_MALE:
    strcpy(buf, "MALE");
    break;
  case SEX_FEMALE:
    strcpy(buf, "FEMALE");
    break;
  default:
    strcpy(buf, "ILLEGAL-SEX!!");
    break;
  }

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " PC '%s'  IDNum: [%5ld], In room [%5ld]\r\n",
          GET_CHAR_NAME(k), GET_IDNUM(k), k->in_room ? k->in_room->number : 0);
  
  if (GET_PGROUP_MEMBER_DATA(k)) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Rank ^c%d/%d^n member of group '^c%s^n' (^c%s^n), with privileges:\r\n  ^c%s^n\r\n",
            GET_PGROUP_MEMBER_DATA(k)->rank, MAX_PGROUP_RANK,
            GET_PGROUP(k)->get_name(), GET_PGROUP(k)->get_alias(),
            pgroup_print_privileges(GET_PGROUP_MEMBER_DATA(k)->privileges));
  }

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Title: %s\r\n", (k->player.title ? k->player.title : "<None>"));

  switch (GET_TRADITION(k))
  {
  case TRAD_ADEPT:
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Tradition: Adept, Grade: ^c%d^n AddPoint Used: %d/%d", GET_GRADE(k), k->points.extrapp, (int)(GET_REP(k) / 50) + 1);
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

  strcat(buf, ", Race: ");
  sprinttype(k->player.race, pc_race_types, buf2, sizeof(buf2));
  strcat(buf, buf2);

  if (IS_SENATOR(k))
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ", Status: %s\r\n", status_ratings[(int)GET_LEVEL(k)]);
  else
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ", Status: Mortal \r\nRep: [^y%3d^n] Not: [^y%3d^n] TKE: [^y%3d^n]\r\n",
            GET_REP(k), GET_NOT(k), GET_TKE(k));

  strcpy(buf1, (const char *) asctime(localtime(&(k->player.time.birth))));
  strcpy(buf2, (const char *) asctime(localtime(&(k->player.time.lastdisc))));
  buf1[10] = buf2[10] = '\0';

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Created: [%s], Last Online: [%s], Played [%dh %dm]\r\n",
          buf1, buf2, k->player.time.played / 3600,
          ((k->player.time.played / 3600) % 60));

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Last room: [%ld], Start room: [%ld], Speaking: [%s]\r\n",
          k->player.last_room, GET_LOADROOM(k), skills[GET_LANGUAGE(k)].name);

  if (GET_QUEST(k))
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Quest: [%ld]\r\n", quest_table[GET_QUEST(k)].vnum);

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Bod: [^c%d^n]  Qui: [^c%d^n]  Str: [^c%d^n]  Cha: [^c%d^n] Int: [^c%d^n]"
          "  Wil: [^c%d^n]  Mag: [^c%d^n]\r\nRea: [^c%d^n]  Ess: [^c%0.2f^n] Ast[^c%d^n]  Com[^c%d^n]  Mag[^c%d/%d/%d^n]  Hak[^c%d^n] Dod/Bod/Off[^c%d/%d/%d^n]\r\n",
          GET_BOD(k), GET_QUI(k), GET_STR(k), GET_CHA(k), GET_INT(k),
          GET_WIL(k), ((int)GET_MAG(k) / 100), GET_REA(k), ((float)GET_ESS(k) / 100), GET_ASTRAL(k),
          GET_COMBAT(k), GET_CASTING(k), GET_DRAIN(k), GET_SDEFENSE(k), GET_HACKING(k), GET_DEFENSE(k), GET_BODY(k), GET_OFFENSE(k));

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Bod: [^B%d^n]  Qui: [^B%d^n]  Str: [^B%d^n]  Cha: [^B%d^n] Int: [^B%d^n]"
          "  Wil: [^B%d^n]  Mag: [^B%d^n]\r\nRea: [^B%d^n]  Ess: [^B%0.2f^n] (Unmodified attributes)\r\n",
          GET_REAL_BOD(k), GET_REAL_QUI(k), GET_REAL_STR(k), GET_REAL_CHA(k), GET_REAL_INT(k),
          GET_REAL_WIL(k), ((int)GET_REAL_MAG(k) / 100), GET_REAL_REA(k), ((float)GET_REAL_ESS(k) / 100));

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Physical p.:[^G%d/%d^n]  Mental p.:[^G%d/%d^n]  Bio Index:[^c%0.2f^n]\r\n"
          "Ess Index:[^c%0.2f^n]\r\n",
          (int)(GET_PHYSICAL(k) / 100), (int)(GET_MAX_PHYSICAL(k) / 100),
          (int)(GET_MENTAL(k) / 100), (int)(GET_MAX_MENTAL(k) / 100),
          ((float)GET_INDEX(k) / 100), (((float)GET_ESS(k) / 100) + 3));

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Nuyen: [%9ld], Bank: [%9ld] (Total: %ld), Karma: [%0.2f]\r\n",
          GET_NUYEN(k), GET_BANK(k), GET_NUYEN(k) + GET_BANK(k),
          ((float)GET_KARMA(k) / 100));

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Armor: %d (%d) / %d (%d), I-Dice: %d, I-Roll: %d, Sus: %d, Foci: %d, TargMod: %d, Reach: %d\r\n",
          GET_BALLISTIC(k), GET_TOTALBAL(k), GET_IMPACT(k), GET_TOTALIMP(k), GET_INIT_DICE(k), GET_INIT_ROLL(k),
          GET_SUSTAINED_NUM(k), GET_FOCI(k), GET_TARGET_MOD(k), GET_REACH(k));
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Current Vision: %s Natural Vision: %s\r\n",
          CURRENT_VISION(k) == NORMAL ? "Normal" : CURRENT_VISION(k) == THERMOGRAPHIC ? "Thermo" : "Low-Light",
          NATURAL_VISION(k) == NORMAL ? "Normal" : NATURAL_VISION(k) == THERMOGRAPHIC ? "Thermo" : "Low-Light");
  sprinttype(GET_POS(k), position_types, buf2, sizeof(buf2));
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Current Zone: %d, Pos: %s, Fighting: %s",
          k->player_specials->saved.zonenum, buf2,
          (FIGHTING(k) ? GET_CHAR_NAME(FIGHTING(k)) : (FIGHTING_VEH(k) ? GET_VEH_NAME(FIGHTING_VEH(k)) : "Nobody")));

  if (k->desc)
  {
    sprinttype(k->desc->connected, connected_types, buf2, sizeof(buf2));
    strcat(buf, ", Connected: ");
    strcat(buf, buf2);
  }
  strcat(buf, "\r\n");

  strcat(buf, "Default position: ");
  sprinttype((k->mob_specials.default_pos), position_types, buf2, sizeof(buf2));
  strcat(buf, buf2);

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ", Idle Timer: [%d]\r\n", k->char_specials.timer);

  PLR_FLAGS(k).PrintBits(buf2, MAX_STRING_LENGTH, player_bits, PLR_MAX);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "PLR: ^c%s^n\r\n", buf2);

  PRF_FLAGS(k).PrintBits(buf2, MAX_STRING_LENGTH, preference_bits, PRF_MAX);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "PRF: ^g%s^n\r\n", buf2);
  
  AFF_FLAGS(k).PrintBits(buf2, MAX_STRING_LENGTH, affected_bits, AFF_MAX);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "AFF: ^y%s^n\r\n", buf2);

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

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Master is: %s, Followers are:",
          ((k->master) ? GET_CHAR_NAME(k->master) : "<none>"));

  for (fol = k->followers; fol; fol = fol->next)
  {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s %s", found++ ? "," : "", GET_CHAR_NAME(fol->follower));
    if (strlen(buf) >= 62) {
      if (fol->next)
        strcat(buf, ",\r\n");
    }
  }
  strcat(buf, "\r\n");

  /* Showing the bitvector */
  AFF_FLAGS(k).PrintBits(buf2, MAX_STRING_LENGTH, affected_bits, AFF_MAX);
  printf(ENDOF(buf), "AFF: ^y%s\r\n", buf2);
  send_to_char(buf, ch);
}

void do_stat_mobile(struct char_data * ch, struct char_data * k)
{
  if (MOB_FLAGGED(k, MOB_PRIVATE) && !access_level(ch, LVL_FIXER))
  {
    send_to_char("You can't.\r\n", ch);
    return;
  }

  int i, i2, found = 0, base;
  struct obj_data *j;
  struct follow_type *fol;
  extern struct attack_hit_type attack_hit_text[];
  extern int calc_karma(struct char_data *ch, struct char_data *vict);

  switch (GET_SEX(k))
  {
  case SEX_NEUTRAL:
    strcpy(buf, "NEUTRAL-SEX");
    break;
  case SEX_MALE:
    strcpy(buf, "MALE");
    break;
  case SEX_FEMALE:
    strcpy(buf, "FEMALE");
    break;
  default:
    strcpy(buf, "ILLEGAL-SEX!!");
    break;
  }
  send_to_char(ch, "%s ", pc_race_types[(int)GET_RACE(k)]);
  if (k->in_room)
    snprintf(buf2, sizeof(buf2), " %s '%s', In room [%8ld]\r\n", (!IS_MOB(k) ? "NPC" : "MOB"), GET_NAME(k), k->in_room->number);
  else if (k->in_veh)
    snprintf(buf2, sizeof(buf2), " %s '%s', In veh [%s]\r\n", (!IS_MOB(k) ? "NPC" : "MOB"), GET_NAME(k), GET_VEH_NAME(k->in_veh));
  else
    snprintf(buf2, sizeof(buf2), " %s '%s'\r\n", (!IS_MOB(k) ? "NPC" : "MOB"), GET_NAME(k));
  strcat(buf, buf2);

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Alias: %s, VNum: [%8ld], RNum: [%5ld]\r\n", GET_KEYWORDS(k),
          GET_MOB_VNUM(k), GET_MOB_RNUM(k));

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "L-Des: %s",
          (k->player.physical_text.look_desc ?
           k->player.physical_text.look_desc : "<None>\r\n"));

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Bod: [^c%d^n]  Qui: [^c%d^n]  Str: [^c%d^n]  Cha: [^c%d^n] Int: [^c%d^n]"
          "  Wil: [^c%d^n]  Mag: [^c%d^n]\r\nRea: [^c%d^n]  Ess: [^c%0.2f^n] Ast[^c%d^n]  Com[^c%d^n]  Mag[^c%d/%d/%d^n]  Hak[^c%d^n] Dod/Bod/Off[^c%d/%d/%d^n]\r\n",
          GET_BOD(k), GET_QUI(k), GET_STR(k), GET_CHA(k), GET_INT(k),
          GET_WIL(k), ((int)GET_MAG(k) / 100), GET_REA(k), ((float)GET_ESS(k) / 100), GET_ASTRAL(k),
          GET_COMBAT(k), GET_CASTING(k), GET_DRAIN(k), GET_SDEFENSE(k), GET_HACKING(k), GET_DEFENSE(k), GET_BODY(k), GET_OFFENSE(k));

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Bod: [^B%d^n]  Qui: [^B%d^n]  Str: [^B%d^n]  Cha: [^B%d^n] Int: [^B%d^n]"
          "  Wil: [^B%d^n]  Mag: [^B%d^n]\r\nRea: [^B%d^n]  Ess: [^B%0.2f^n] (Unmodified attributes)\r\n",
          GET_REAL_BOD(k), GET_REAL_QUI(k), GET_REAL_STR(k), GET_REAL_CHA(k), GET_REAL_INT(k),
          GET_REAL_WIL(k), ((int)GET_REAL_MAG(k) / 100), GET_REAL_REA(k), ((float)GET_REAL_ESS(k) / 100));

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Physical p.:[^G%d/%d^n]  Mental p.:[^G%d/%d^n]\r\n",
          (int)(GET_PHYSICAL(k) / 100), (int)(GET_MAX_PHYSICAL(k) / 100),
          (int)(GET_MENTAL(k) / 100), (int)(GET_MAX_MENTAL(k) / 100));

  base = calc_karma(NULL, k);

  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Nuyen: [%ld], Credstick: [%ld], Bonus karma: [%4d], Total karma: [%4d]\r\n",
          GET_NUYEN(k), GET_BANK(k), GET_KARMA(k), base);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "B: %d, I: %d, I-Dice: %d, I-Roll: %d, Sus: %d, TargMod: %d, Reach: %d\r\n",
          GET_BALLISTIC(k), GET_IMPACT(k), GET_INIT_DICE(k), GET_INIT_ROLL(k),
          GET_SUSTAINED_NUM(k), GET_TARGET_MOD(k), GET_REACH(k));

  sprinttype(GET_POS(k), position_types, buf2, sizeof(buf2));
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Position: %s, Fighting: %s", buf2,
          (FIGHTING(k) ? GET_NAME(FIGHTING(k)) : (FIGHTING_VEH(k) ? GET_VEH_NAME(FIGHTING_VEH(k)) : "Nobody")));

  strcat(ENDOF(buf), ", Attack type: ");
  // gotta subtract TYPE_HIT for the array to work properly
  strcat(buf, attack_hit_text[k->mob_specials.attack_type - TYPE_HIT].singular);
  strcat(buf, "\r\n");


  strcat(buf, "Default position: ");
  sprinttype((k->mob_specials.default_pos), position_types, buf2, sizeof(buf2));
  strcat(buf, buf2);
  strcat(buf, "     Mob Spec-Proc: ");
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
  strcat(buf, buf2);

  MOB_FLAGS(k).PrintBits(buf2, MAX_STRING_LENGTH, action_bits, MOB_MAX);
  snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "NPC flags: ^c%s^n\r\n", buf2);

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
        strcat(buf, ",\r\n");
    }
  }
  strcat(buf, "\r\n");

  /* Showing the bitvector */
  AFF_FLAGS(k).PrintBits(buf2, MAX_STRING_LENGTH, affected_bits, AFF_MAX);
  send_to_char(ch, "%sAFF: ^y%s\r\n", buf, buf2);
}

ACMD(do_stat)
{
  struct char_data *victim = NULL;
  struct obj_data *object = NULL;
  struct veh_data *veh = NULL;
  int tmp;

  half_chop(argument, buf1, buf2);

  if (!*buf1) {
    send_to_char("Stats on who or what?\r\n", ch);
    return;
  } else if (is_abbrev(buf1, "room")) {
    do_stat_room(ch);
  } else if (is_abbrev(buf1, "file")) {
    if (!*buf2)
      send_to_char("Stats on which player?\r\n", ch);
    else {
      if (!(does_player_exist(buf2)))
        send_to_char("There is no such player.\r\n", ch);
      else {
        victim = playerDB.LoadChar(buf2, TRUE);
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

  if (IS_NPC(ch) || !access_level(ch, LVL_DEVELOPER)) {
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

  House_save_all();
}

void stop_snooping(struct char_data * ch)
{
  if (!ch->desc->snooping)
    send_to_char("You aren't snooping anyone.\r\n", ch);
  else
  {
    send_to_char("You stop snooping.\r\n", ch);

    if ( ch->desc->snooping->original )
      snprintf(buf, sizeof(buf),"%s stops snooping %s.",
              GET_CHAR_NAME(ch), GET_CHAR_NAME((ch->desc->snooping->original)));
    else
      snprintf(buf, sizeof(buf),"%s stops snooping %s.",
              GET_CHAR_NAME(ch),GET_CHAR_NAME((ch->desc->snooping->character)));

    ch->desc->snooping->snoop_by = NULL;
    ch->desc->snooping = NULL;

    mudlog(buf, ch, LOG_WIZLOG, FALSE);
  }
}

ACMD(do_snoop)
{
  struct char_data *victim, *tch;

  if (!ch->desc)
    return;

  one_argument(argument, arg);

  if (!*arg)
    stop_snooping(ch);
  else if (!(victim = get_char_vis(ch, arg)))
    send_to_char("No such person around.\r\n", ch);
  else if (!victim->desc)
    send_to_char("There's no link.. nothing to snoop.\r\n", ch);
  else if (victim == ch)
    stop_snooping(ch);
  else if (victim->desc->snoop_by)
    send_to_char("Busy already. \r\n", ch);
  else if (victim->desc->snooping == ch->desc)
    send_to_char("Don't be stupid.\r\n", ch);
  else if (!IS_NPC(victim) && PLR_FLAGS(victim).IsSet(PLR_NOSNOOP) )
    send_to_char("You can't snoop an unsnoopable person.\r\n",ch);
  else {
    if (victim->desc->original)
      tch = victim->desc->original;
    else
      tch = victim;

    if (GET_LEVEL(tch) >= GET_LEVEL(ch)) {
      send_to_char("You can't.\r\n", ch);
      return;
    }
    send_to_char(OK, ch);

    if (ch->desc->snooping)
      ch->desc->snooping->snoop_by = NULL;

    ch->desc->snooping = victim->desc;
    victim->desc->snoop_by = ch->desc;
    snprintf(buf, sizeof(buf),"%s now being snooped by %s.",
            GET_CHAR_NAME(victim),GET_CHAR_NAME(ch));
    mudlog(buf, ch, LOG_WIZLOG, FALSE);
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
    send_to_char("You return to your senses.\r\n", ch);
    for (struct obj_data *cyber = ch->cyberware; cyber; cyber = cyber->next_content)
      if (GET_OBJ_VAL(cyber, 0) == CYB_DATAJACK) {
        if (GET_OBJ_VAL(cyber, 3) == DATA_INDUCTION)
          snprintf(buf, sizeof(buf), "$n slowly removes $s hand from the induction pad.");
        else snprintf(buf, sizeof(buf), "$n carefully removes the jack from $s head.");
        break;
      } else if (GET_OBJ_VAL(cyber, 0) == CYB_EYES && IS_SET(GET_OBJ_VAL(cyber, 3), EYE_DATAJACK)) {
        snprintf(buf, sizeof(buf), "$n carefully removes the jack from $s eye.");
        break;
      }
    act(buf, TRUE, ch, 0, 0, TO_ROOM);
    PLR_FLAGS(ch).RemoveBit(PLR_REMOTE);
    ch->char_specials.rigging->rigger = NULL;
    ch->char_specials.rigging->cspeed = SPEED_OFF;
    stop_chase(ch->char_specials.rigging);
    send_to_veh("You slow to a halt.\r\n", ch->char_specials.rigging, NULL, 0);
    ch->char_specials.rigging = NULL;
    stop_fighting(ch);
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
        GET_MEMORY(vict) = NULL;
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
  if (!access_level(ch, LVL_DEVELOPER)) {
    for (i = 0; i < NUM_ZONE_EDITOR_IDS; i++) {
      if (zone_table[counter].editor_ids[i] == GET_IDNUM(ch))
        break;
    }
  
    if ((i >= 5)) {
      send_to_char("Sorry, you don't have access to edit this zone.\r\n", ch);
      return;
    }
  }
  
  obj = read_object(real_num, REAL);
  obj_to_char(obj, ch);
  GET_OBJ_TIMER(obj) = 2;
  obj->obj_flags.extra_flags.SetBit(ITEM_IMMLOAD); // Why the hell do we have immload AND wizload?
  obj->obj_flags.extra_flags.SetBit(ITEM_WIZLOAD);
  act("$n makes a strange magical gesture.", TRUE, ch, 0, 0, TO_ROOM);
  act("$n has created $p!", TRUE, ch, obj, 0, TO_ROOM);
  act("You create $p.", FALSE, ch, obj, 0, TO_CHAR);
  snprintf(buf, sizeof(buf), "%s wizloaded object #%d (%s).",
          GET_CHAR_NAME(ch), vnum, GET_OBJ_NAME(obj));
  mudlog(buf, ch, LOG_CHEATLOG, TRUE);
}

ACMD(do_iload)
{
  int number;

  one_argument(argument, buf2);

  if (!access_level(ch, LVL_PRESIDENT) && !PLR_FLAGGED(ch, PLR_OLC)) {
    send_to_char(YOU_NEED_OLC_FOR_THAT, ch);
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
  if (is_abbrev(buf, "veh")) {
    if ((r_num = real_vehicle(numb)) < 0 ) {
      send_to_char("There is no vehicle with that number.\r\n", ch);
      return;
    }
    veh = read_vehicle(r_num, REAL);
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
    char_to_room(mob, get_ch_in_room(ch));

    act("$n makes a quaint, magical gesture with one hand.", TRUE, ch,
        0, 0, TO_ROOM);
    act("$n has created $N!", TRUE, ch, 0, mob, TO_ROOM);
    act("You create $N.", FALSE, ch, 0, mob, TO_CHAR);
  } else if (is_abbrev(buf, "obj")) {
    perform_wizload_object(ch, numb);
  } else
    send_to_char("That'll have to be either 'obj', 'mob', or 'veh'.\r\n", ch);
}

ACMD(do_vstat)
{
  struct char_data *mob;
  struct obj_data *obj;
  struct veh_data *veh;
  int number, r_num;

  two_arguments(argument, buf, buf2);

  if (!*buf || !*buf2 || !isdigit(*buf2)) {
    send_to_char("Usage: vstat { obj | mob | qst | shp | veh} <number>\r\n", ch);
    return;
  }
  if ((number = atoi(buf2)) < 0) {
    send_to_char("A NEGATIVE number??\r\n", ch);
    return;
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
    char_to_room(mob, 0);
    do_stat_mobile(ch, mob);
    extract_char(mob);
  } else if (is_abbrev(buf, "obj")) {
    if ((r_num = real_object(number)) < 0) {
      send_to_char("There is no object with that number.\r\n", ch);
      return;
    }
    obj = read_object(r_num, REAL);
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

/* clean a room of all mobiles and objects */
ACMD(do_purge)
{
  struct char_data *vict, *next_v;
  struct obj_data *obj, *next_o;
  struct veh_data *veh, *next_ve;
  extern void die_follower(struct char_data * ch);

  if (!access_level(ch, LVL_EXECUTIVE) &&
      (ch->player_specials->saved.zonenum != zone_table[get_ch_in_room(ch)->zone].number)) {
    send_to_char("You can only purge in your zone.\r\n", ch);
    return;
  }

  one_argument(argument, buf);

  if (*buf) {                   /* argument supplied. destroy single object, character, or vehicle. */
    if ((vict = get_char_room_vis(ch, buf))) {
      if (!IS_NPC(vict) &&
          ((GET_LEVEL(ch) <= GET_LEVEL(vict)) ||
           (!access_level(ch, LVL_EXECUTIVE)))) {
        send_to_char("Fuuuuuuuuu!\r\n", ch);
        return;
      }
      act("$n disintegrates $N.", FALSE, ch, 0, vict, TO_NOTVICT);

      if (!IS_NPC(vict)) {
        snprintf(buf, sizeof(buf), "%s has purged %s.", GET_CHAR_NAME(ch), GET_NAME(vict));
        mudlog(buf, ch, LOG_PURGELOG, TRUE);
        if (vict->desc) {
          close_socket(vict->desc);
          vict->desc = NULL;
        }
      }
      extract_char(vict);
      send_to_char(OK, ch);
      return;
    }
    
    if ((obj = get_obj_in_list_vis(ch, buf, ch->in_veh ? ch->in_veh->contents : ch->in_room->contents))) {
      act("$n destroys $p.", FALSE, ch, obj, 0, TO_ROOM);
      const char *representation = generate_new_loggable_representation(obj);
      snprintf(buf, sizeof(buf), "%s has purged %s.", GET_CHAR_NAME(ch), representation);
      delete [] representation;
      mudlog(buf, ch, LOG_PURGELOG, TRUE);
      extract_obj(obj);
      send_to_char(OK, ch);
      return;
    }
    
    if ((veh = get_veh_list(buf, ch->in_veh ? ch->in_veh->carriedvehs : ch->in_room->vehicles, ch))) {
      // Notify the room.
      snprintf(buf1, sizeof(buf1), "$n purges %s.", GET_VEH_NAME(veh));
      act(buf1, FALSE, ch, NULL, 0, TO_ROOM);
      snprintf(buf1, sizeof(buf1), "%s purged %s.", GET_CHAR_NAME(ch), GET_VEH_NAME(veh));
      mudlog(buf1, ch, LOG_WIZLOG, TRUE);
      
      // Notify the owner.
      if (veh->owner > 0) {
        snprintf(buf2, sizeof(buf2), "^ROOC Alert: Your vehicle '%s' has been deleted by administrator '%s'.^n\r\n", GET_VEH_NAME(veh), GET_CHAR_NAME(ch));
        store_mail(veh->owner, ch, buf2);
      }
      
      // Log the purge and finalize extraction.
      purgelog(veh);
      extract_veh(veh);
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
          snprintf(buf, sizeof(buf), "%s has purged %s.", GET_CHAR_NAME(ch), GET_NAME(vict));
          mudlog(buf, ch, LOG_PURGELOG, TRUE);
          extract_char(vict);
        }
      }
      
      for (obj = ch->in_veh->contents; obj; obj = next_o) {
        next_o = obj->next_content;
        const char *representation = generate_new_loggable_representation(obj);
        snprintf(buf, sizeof(buf), "%s has purged %s.", GET_CHAR_NAME(ch), representation);
        mudlog(buf, ch, LOG_PURGELOG, TRUE);
        delete [] representation;
        extract_obj(obj);
      }
      
      for (veh = ch->in_veh->carriedvehs; veh; veh = next_ve) {
        next_ve = veh->next;
        if (veh == ch->in_veh)
          continue;
        snprintf(buf1, sizeof(buf1), "%s has purged %s.", GET_CHAR_NAME(ch), GET_VEH_NAME(veh));
        mudlog(buf1, ch, LOG_WIZLOG, TRUE);
        purgelog(veh);
        extract_veh(veh);
      }
      
      send_to_veh("The world seems a little cleaner.\r\n", ch->in_veh, NULL, FALSE);
    }
    else {
      act("$n gestures... You are surrounded by scorching flames!", FALSE, ch, 0, 0, TO_ROOM);

      for (vict = ch->in_room->people; vict; vict = next_v) {
        next_v = vict->next_in_room;
        if (IS_NPC(vict) && vict != ch) {
          snprintf(buf, sizeof(buf), "%s has purged %s.", GET_CHAR_NAME(ch), GET_NAME(vict));
          mudlog(buf, ch, LOG_PURGELOG, TRUE);
          extract_char(vict);
        }
      }

      for (obj = ch->in_room->contents; obj; obj = next_o) {
        next_o = obj->next_content;
        const char *representation = generate_new_loggable_representation(obj);
        snprintf(buf, sizeof(buf), "%s has purged %s.", GET_CHAR_NAME(ch), representation);
        mudlog(buf, ch, LOG_PURGELOG, TRUE);
        delete [] representation;
        extract_obj(obj);
      }
      
      for (veh = ch->in_room->vehicles; veh; veh = next_ve) {
        next_ve = veh->next_veh;
        snprintf(buf1, sizeof(buf1), "%s has purged %s.", GET_CHAR_NAME(ch), GET_VEH_NAME(veh));
        mudlog(buf1, ch, LOG_WIZLOG, TRUE);
        purgelog(veh);
        extract_veh(veh);
      }
      
      send_to_room("The world seems a little cleaner.\r\n", ch->in_room);
    }
  }
}

void do_advance_with_mode(struct char_data *ch, char *argument, int cmd, int subcmd, bool can_self_advance) {
  struct char_data *victim;
  char *name = arg, *level = buf2;
  int newlevel, i;
  void do_start(struct char_data *ch, bool wipe_skills);
  extern void check_autowiz(struct char_data * ch);

  two_arguments(argument, name, level);

  if (*name) {
    if (!(victim = get_char_vis(ch, name))) {
      send_to_char("That player is not here.\r\n", ch);
      return;
    }
  } else {
    send_to_char("Advance who?\r\n", ch);
    return;
  }

  if (GET_LEVEL(ch) <= GET_LEVEL(victim)
      && ch != victim) {
    send_to_char("Maybe that's not such a great idea.\r\n", ch);
    return;
  }
  if (IS_NPC(victim)) {
    send_to_char("NO!  Not on NPC's.\r\n", ch);
    return;
  }
  if (!*level || (newlevel = atoi(level)) <= 0) {
    send_to_char("That's not a level!\r\n", ch);
    return;
  }
  if (newlevel > LVL_MAX) {
    send_to_char(ch, "%d is the highest possible level.\r\n", LVL_MAX);
    return;
  }
  if (can_self_advance) {
    // You can only advance to level 9 unless you're the President.
    int max_ch_can_advance_to = GET_LEVEL(ch) < LVL_MAX ? LVL_MAX - 1 : LVL_MAX;
    if (newlevel > max_ch_can_advance_to) {
      send_to_char(ch, "%d is the highest possible level you can advance someone to.\r\n", max_ch_can_advance_to);
      return;
    }
  } else {
    if (!access_level(ch, newlevel) ) {
      send_to_char("Yeah, right.\r\n", ch);
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
    snprintf(buf3, sizeof(buf3), "%s has advanced %s from %s to %s.",
            GET_CHAR_NAME(ch), GET_CHAR_NAME(victim), status_ratings[(int)GET_LEVEL(victim)], status_ratings[newlevel]);
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

  mudlog(buf3, ch, LOG_WIZLOG, TRUE);
  // We use INSERT IGNORE to cause it to not error out when updating someone who already had immort data.
  snprintf(buf, sizeof(buf), "INSERT IGNORE INTO pfiles_immortdata (idnum) VALUES (%ld);", GET_IDNUM(victim));
  mysql_wrapper(mysql, buf);
}

ACMD(do_self_advance) {
  do_advance_with_mode(ch, argument, cmd, subcmd, TRUE);
}

ACMD(do_advance) {
  // Wrapper for do_advance with no self-advancement enabled.
  do_advance_with_mode(ch, argument, cmd, subcmd, FALSE);
}

ACMD(do_award)
{
  struct char_data *vict;
  char amt[MAX_STRING_LENGTH];
  char reason[MAX_STRING_LENGTH];
  int k;

  half_chop(argument, arg, buf);
  half_chop(buf, amt, reason);

  k = atoi(amt);

  if (!*arg || !*amt || !*reason || k <= 0 ) {
    send_to_char("Syntax: award <player> <karma x 100> <Reason for award>.\r\n", ch);
    return;
  }
  if (!(vict = get_char_vis(ch, arg))) {
    send_to_char(ch, "You don't see anyone named '%s' here.\r\n", arg);
    return;
  }
  
  if (IS_SENATOR(vict)) {
    send_to_char(ch, "Staff can't receive karma this way. Use the SET command.\r\n", ch);
    return;
  }
  
  if (GET_KARMA(vict) + k > MYSQL_UNSIGNED_MEDIUMINT_MAX) {
    send_to_char(ch, "That would put %s over the karma maximum. You may award up to %d points of karma. Otherwise, tell %s to spend what %s has, or compensate %s some other way.\r\n",
                 GET_CHAR_NAME(vict), MYSQL_UNSIGNED_MEDIUMINT_MAX - GET_KARMA(vict), HMHR(vict), HSSH(vict), HMHR(vict));
    return;
  }

  if (vict->desc && vict->desc->original)
    gain_karma(vict->desc->original, k, TRUE, FALSE, FALSE);
  else
    gain_karma(vict, k, TRUE, FALSE, FALSE);

  send_to_char(vict, "You have been awarded %0.2f karma for %s.\r\n", (float)k*0.01, reason);

  send_to_char(ch, "You awarded %0.2f karma to %s for %s.\r\n", (float)k*0.01, GET_CHAR_NAME(vict), reason);

  snprintf(buf2, sizeof(buf2), "%s awarded %0.2f karma to %s for %s (%d to %d).",
          GET_CHAR_NAME(ch), (float)k*0.01,
          GET_CHAR_NAME(vict), reason, GET_KARMA(vict) - k, GET_KARMA(vict));
  mudlog(buf2, ch, LOG_WIZLOG, TRUE);
}

ACMD(do_penalize)
{
  struct char_data *vict;
  char amt[MAX_STRING_LENGTH];
  char reason[MAX_STRING_LENGTH];
  int k;

  half_chop(argument, arg, buf);
  half_chop(buf, amt, reason);

  k = atoi(amt);

  if (!*arg || !*amt || !*reason || k <= 0 ) {
    send_to_char("Syntax: penalize <player> <karma x 100> <Reason for penalty>.\r\n", ch);
    return;
  }
  if (!(vict = get_char_vis(ch, arg))) {
    send_to_char(ch, "You don't see anyone named '%s' here.\r\n", arg);
    return;
  }

  if (vict->desc && vict->desc->original)
    gain_karma(vict->desc->original, k * -1, TRUE, FALSE, FALSE);
  else
    gain_karma(vict, k * -1, TRUE, FALSE, FALSE);

  send_to_char(vict, "You have been penalized %0.2f karma for %s.\r\n", (float)k*0.01, reason);

  send_to_char(ch, "You penalized %0.2f karma from %s for %s.\r\n", (float)k*0.01,
          GET_NAME(vict), reason);

  snprintf(buf2, sizeof(buf2), "%s penalized %0.2f karma from %s for %s (%d to %d).",
          GET_CHAR_NAME(ch), (float)k*0.01,
          GET_CHAR_NAME(vict), reason, GET_KARMA(vict) + k, GET_KARMA(vict));
  mudlog(buf2, ch, LOG_WIZLOG, TRUE);
}

// Restores a character to peak physical condition.
void restore_character(struct char_data *vict, bool reset_staff_stats) {  
  // Restore their physical and mental damage levels.
  GET_PHYSICAL(vict) = GET_MAX_PHYSICAL(vict);
  GET_MENTAL(vict) = GET_MAX_MENTAL(vict);
  
  // Non-NPCs get further consideration.
  if (!IS_NPC(vict)) {  
    // Touch up their hunger, thirst, etc. 
    for (int i = COND_DRUNK; i <= COND_THIRST; i++)
      // Staff don't deal with hunger, etc-- disable it for them.
      if IS_SENATOR(vict) {
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
      
      
    // Staff members get their skills set to max, and also their stats boosted.
    if (IS_SENATOR(vict) && reset_staff_stats) {
      // Give them 100 in every skill.
      for (int i = MIN_SKILLS; i < MAX_SKILLS; i++)
        set_character_skill(vict, i, 100, FALSE);
        
      // Restore their essence to 6.00.
      vict->real_abils.ess = 600;
      
      // Non-executive staff get 15's in stats. Executives get 50s.
      int stat_level = access_level(vict, LVL_EXECUTIVE) ? 50 : 15;
        
      // Set all their stats to the requested level.
      GET_REAL_INT(vict) = stat_level;
      GET_REAL_WIL(vict) = stat_level;
      GET_REAL_QUI(vict) = stat_level;
      GET_REAL_STR(vict) = stat_level;
      GET_REAL_BOD(vict) = stat_level;
      GET_REAL_CHA(vict) = stat_level;
      vict->real_abils.mag = stat_level * 100;

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
    nonsensical_reply(ch, NULL);
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
      restore_character(d->character, FALSE);
      act("You have been fully healed by $N!", FALSE, d->character, 0, ch, TO_CHAR);
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
  act("You have been fully healed by $N!", FALSE, vict, 0, ch, TO_CHAR);
  snprintf(buf, sizeof(buf), "%s restored %s.", GET_CHAR_NAME(ch), GET_CHAR_NAME(vict));
  mudlog(buf, ch, LOG_WIZLOG, TRUE);
  send_to_char(OK, ch);
  return;
}

void perform_immort_vis(struct char_data *ch)
{
  void appear(struct char_data *ch);

  if (GET_INVIS_LEV(ch) == 0 && !IS_AFFECTED(ch, AFF_HIDE) &&
      !IS_AFFECTED(ch, AFF_INVISIBLE))
  {
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
    else
      perform_immort_invis(ch, 2);
  } else {
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

  if (atoi(arg)) {
    send_to_char("Usage: dc <name>\r\n       dc *\r\n", ch);
    return;
  }

  if (*arg == '*') {
    send_to_char("All non-playing connections closed.\r\n", ch);
    snprintf(buf, sizeof(buf), "Non-playing connections closed by %s.", GET_CHAR_NAME(ch));
    mudlog(buf, ch, LOG_WIZLOG, TRUE);
    for (d = descriptor_list; d; d = d->next)
      if ((d->connected > 0 && d->connected < CON_SPELL_CREATE) || d->connected == CON_ASKNAME)
        close_socket(d);
    return;
  }

  if (!(vict = get_player_vis(ch, arg, 0))) {
    send_to_char(ch, "You can't see anyone named '%s'.\r\n", arg);
    return;
  }
  if (GET_LEVEL(vict) >= GET_LEVEL(ch)) {
    send_to_char("Maybe that's not such a good idea...\r\n", ch);
    return;
  }
  if (!vict->desc) {
    act("It seems $E's already linkdead...", FALSE, ch, 0, vict, TO_CHAR);
    return;
  }

  send_to_char(ch, "%s's connection closed.\r\n", GET_NAME(vict));
  snprintf(buf, sizeof(buf), "%s's connection closed by %s.", GET_NAME(vict),
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
  const char *when;

  one_argument(argument, arg);
  if (*arg) {
    value = atoi(arg);
    if (value < 0 || !access_level(ch, value)) {
      send_to_char("Invalid wizlock value.\r\n", ch);
      return;
    }
    restrict = value;
    when = "now";
  } else
    when = "currently";

  switch (restrict) {
  case 0:
    snprintf(buf, sizeof(buf), "The game is %s completely open.\r\n", when);
    break;
  case 1:
    snprintf(buf, sizeof(buf), "The game is %s closed to new players.\r\n", when);
    break;
  default:
    snprintf(buf, sizeof(buf), "Only level %d and above may enter the game %s.\r\n",
            restrict, when);
    break;
  }
  send_to_char(buf, ch);
  snprintf(buf, sizeof(buf), "%s set wizlock to level %d.", GET_CHAR_NAME(ch), restrict);
  mudlog(buf, ch, LOG_WIZLOG, TRUE);
}

ACMD(do_date)
{
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

    snprintf(buf, sizeof(buf), "Up since %s: %d day%s, %d:%02d\r\n", tmstr, d,
            ((d == 1) ? "" : "s"), h, m);
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
    snprintf(buf, sizeof(buf), "SELECT Idnum, Rank, Host, LastD, Name FROM pfiles WHERE name='%s';", prepare_quotes(buf2, arg, sizeof(buf2) / sizeof(buf2[0])));
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

  half_chop(argument, arg, to_force);

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

  snprintf(buf1, sizeof(buf1), "^c[%s]: %s\r\n", GET_CHAR_NAME(ch), argument);
  snprintf(buf2, sizeof(buf2), "^c[Someone]: %s\r\n", argument);

  for (d = descriptor_list; d; d = d->next) {
    if ((!d->connected)
        && d->character
        && access_level(d->character, LVL_BUILDER) && !PLR_FLAGGED(d->character, PLR_WRITING)
        && (d != ch->desc || !(PRF_FLAGGED(d->character, PRF_NOREPEAT)))) {
      if (CAN_SEE(d->character, ch)) {
        send_to_char(buf1, d->character);
        store_message_to_history(d, COMM_CHANNEL_WTELLS, str_dup(buf1));
      } else {
        send_to_char(buf2, d->character);
        store_message_to_history(d, COMM_CHANNEL_WTELLS, str_dup(buf2));
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
      strcat(line, buf);
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
      send_to_char("You can only zreset your zone.\r\n", ch);
      return;
    }
    reset_zone(i, 0);
    snprintf(buf, sizeof(buf), "Reset zone %d (#%d): %s.\r\n", i,
            zone_table[i].number,
            zone_table[i].name);
    send_to_char(buf, ch);
    snprintf(buf, sizeof(buf), "%s reset zone %d (#%d): %s",
            GET_CHAR_NAME(ch), i,
            zone_table[i].number,
            zone_table[i].name);
    mudlog(buf, ch, LOG_WIZLOG, TRUE);
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
      send_to_char("Whotitles can't contain the ^ character.\r\n", ch);
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
      strcat(buf, "^n");
    if (strstr((const char *)argument, "^l")) {
      send_to_char("Whotitles can't contain pure black.\r\n", ch);
    } else if (strlen(argument) > (MAX_TITLE_LENGTH -2)) {
      send_to_char(ch, "Sorry, pretitles can't be longer than %d characters.\r\n", MAX_TITLE_LENGTH - 2);
    } else {
      set_pretitle(ch, argument);
      snprintf(buf, sizeof(buf), "Okay, you're now %s %s %s.\r\n",
              GET_PRETITLE(ch), GET_CHAR_NAME(ch), GET_TITLE(ch));
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
        strcat(buf, "\r\n");
        send_to_char(buf, ch);
        break;
      case SCMD_SQUELCH:
        result = PLR_TOG_CHK(vict, PLR_NOSHOUT);
        snprintf(buf, sizeof(buf), "Squelch %s for %s by %s.", ONOFF(result),
                GET_CHAR_NAME(vict), GET_CHAR_NAME(ch));
        mudlog(buf, ch, LOG_WIZLOG, TRUE);
        strcat(buf, "\r\n");
        send_to_char(buf, ch);
        break;
      case SCMD_SQUELCHOOC:
        result = PLR_TOG_CHK(vict, PLR_NOOOC);
        snprintf(buf, sizeof(buf), "Squelch(OOC) %s for %s by %s.", ONOFF(result),
                GET_CHAR_NAME(vict), GET_CHAR_NAME(ch));
        mudlog(buf, ch, LOG_WIZLOG, TRUE);
        strcat(buf, "\r\n");
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
        if (ch == vict) {
          send_to_char("Oh, yeah, THAT'S real smart...\r\n", ch);
          return;
        }
        if (PLR_FLAGGED(vict, PLR_FROZEN)) {
          send_to_char("Your victim is already pretty cold. Did you mean to THAW them?\r\n", ch);
          return;
        }
        PLR_FLAGS(vict).SetBit(PLR_FROZEN);
        GET_FREEZE_LEV(vict) = GET_LEVEL(ch);
        send_to_char("A bitter wind suddenly rises and drains every erg of heat from your body!\r\nYou feel frozen!\r\n", vict);
        send_to_char("Frozen.\r\n", ch);
        act("A sudden cold wind conjured from nowhere freezes $n!", TRUE, vict, 0, 0, TO_ROOM);
        snprintf(buf, sizeof(buf), "%s frozen by %s.", GET_CHAR_NAME(vict), GET_CHAR_NAME(ch));
        mudlog(buf, ch, LOG_WIZLOG, TRUE);
        break;
      case SCMD_THAW:
        if (!PLR_FLAGGED(vict, PLR_FROZEN)) {
          send_to_char("Sorry, your victim is not morbidly encased in ice at the moment.\r\n", ch);
          return;
        }
        if (!access_level(ch, GET_FREEZE_LEV(vict))) {
          snprintf(buf, sizeof(buf), "Sorry, a level %d God froze %s... you can't unfreeze %s.\r\n",
                  GET_FREEZE_LEV(vict), GET_CHAR_NAME(vict), HMHR(vict));
          send_to_char(buf, ch);
          return;
        }
        snprintf(buf, sizeof(buf), "%s un-frozen by %s.", GET_CHAR_NAME(vict), GET_CHAR_NAME(ch));
        mudlog(buf, ch, LOG_WIZLOG, TRUE);
        PLR_FLAGS(vict).RemoveBit(PLR_FROZEN);
        send_to_char("A fireball suddenly explodes in front of you, melting the ice!\r\nYou feel thawed.\r\n", vict);
        send_to_char("Thawed.\r\n", ch);
        GET_FREEZE_LEV(vict) = 0;
        act("A sudden fireball conjured from nowhere thaws $n!", TRUE, vict, 0, 0, TO_ROOM);
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

void print_zone_to_buf(char *bufptr, int buf_size, int zone, int detailed)
{
  int i, color = 0;

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

  if (!detailed) {
    snprintf(bufptr, buf_size - strlen(bufptr), "%3d %-30.30s^n ", zone_table[zone].number,
            zone_table[zone].name);
    for (i = 0; i < color; i++)
      strcat(bufptr, " ");
    snprintf(ENDOF(bufptr), buf_size - strlen(bufptr), "%sAge: %3d; Res: %3d (%1d); Top: %5d; Sec: %2d\r\n",
            zone_table[zone].connected ? "* " : "  ",
            zone_table[zone].age, zone_table[zone].lifespan,
            zone_table[zone].reset_mode, zone_table[zone].top,
            zone_table[zone].security);
  } else {
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

    snprintf(bufptr, buf_size, "Zone %d (%d): %s\r\n"
            "Age: %d, Commands: %d, Reset: %d (%d), Top: %d\r\n"
            "Rooms: %d, Mobiles: %d, Objects: %d, Shops: %d, Vehicles: %d\r\n"
            "Security: %d, Status: %s\r\nJurisdiction: %s, Editors: ",
            zone_table[zone].number, zone, zone_table[zone].name,
            zone_table[zone].age, zone_table[zone].num_cmds,
            zone_table[zone].lifespan, zone_table[zone].reset_mode,
            zone_table[zone].top, rooms, mobs, objs, shops, vehs,
            zone_table[zone].security,
            zone_table[zone].connected ? "Connected" : "In Progress", jurid[zone_table[zone].jurisdiction]);
/* FIXCHE   for (i = 0; i < NUM_ZONE_EDITOR_IDS; i++) {
      const char *name = playerDB.GetNameV(zone_table[zone].editor_ids[i]);

      if (name) {
        if (first)
          first = 0;
        else
          strcat(bufptr, ", ");
        strcat(bufptr, CAP(name));
      }
    }
    if (first)
      strcat(bufptr, "None.\r\n");
    else
      strcat(bufptr, ".\r\n");
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
               { "nothing",        0  },/* 0 */
               { "zones",          LVL_BUILDER },
               { "player",         LVL_BUILDER },
               { "rent",           LVL_BUILDER },
               { "stats",          LVL_ADMIN },
               { "errors",         LVL_ADMIN },
               { "deathrooms",     LVL_ADMIN },
               { "godrooms",       LVL_BUILDER },
               { "skills",         LVL_BUILDER },
               { "spells",         LVL_BUILDER },
               { "prompt",         LVL_ADMIN },
               { "lodges",         LVL_ADMIN },
               { "library",        LVL_ADMIN },
               { "jackpoints",     LVL_BUILDER },
               { "abilities",      LVL_BUILDER },
               { "aliases",        LVL_ADMIN },
               { "metamagic",      LVL_BUILDER },
               { "noexits",        LVL_BUILDER },
               { "traps",          LVL_BUILDER },
               { "ammo",           LVL_ADMIN },
               { "storage",        LVL_BUILDER },
               { "anomalies",      LVL_BUILDER },
               { "\n", 0 }
             };

  skip_spaces(&argument);

  if (!*argument) {
    strcpy(buf, "Show options:\r\n");
    for (j = 0, i = 1; fields[i].level; i++)
      if (access_level(ch, fields[i].level))
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%-15s%s", fields[i].cmd, (!(++j % 5) ? "\r\n" : ""));
    strcat(buf, "\r\n");
    send_to_char(buf, ch);
    return;
  }

  strcpy(arg, two_arguments(argument, field, value));

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
        print_zone_to_buf(buf, sizeof(buf), ch->in_room->zone, 1);
      else
        print_zone_to_buf(buf, sizeof(buf), ch->in_room->zone, 0);
    } 
    
    else if (*value && is_number(value)) {
      for (j = atoi(value), i = 0; zone_table[i].number != j && i <= top_of_zone_table; i++)
        ;
      if (i <= top_of_zone_table) {
        if (access_level(ch, LVL_ADMIN))
          print_zone_to_buf(buf, sizeof(buf), i, 1);
        else
          print_zone_to_buf(buf, sizeof(buf), i, 0);
      } else {
        send_to_char("That is not a valid zone.\r\n", ch);
        return;
      }
    } 
    
    else
      for (i = 0; i <= top_of_zone_table; i++)
        print_zone_to_buf(ENDOF(buf), sizeof(buf) - strlen(buf), i, 0);
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
     
      snprintf(buf, sizeof(buf), "Player: %-12s (%s) [%2d]\r\n", GET_NAME(vict),
              genders[(int) GET_SEX(vict)], GET_LEVEL(vict));
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Y: %-8ld  Bal: %-8ld  Karma: %-8d\r\n",
              GET_NUYEN(vict), GET_BANK(vict), GET_KARMA(vict));
      strcpy(birth, ctime(&vict->player.time.birth));
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Started: %-20.16s  Last: %-20.16s  Played: %3dh %2dm\r\n",
              birth, ctime(&vict->player.time.lastdisc),
              (int) (vict->player.time.played / 3600),
              (int) (vict->player.time.played / 60 % 60));
      send_to_char(buf, ch);
    }

    break;
  case 3:
    break;
  case 4:
    i = 0;
    j = 0;
    k = 0;
    v = 0;
    con = 0;
    for (vict = character_list; vict; vict = vict->next) {
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
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  ObjStackSize: %d(%d), ChStackSize: %d(%d), RmStackSize: %d(%d)\r\n",
            Mem->ObjSize(), Mem->ObjMaxSize(), Mem->ChSize(), Mem->ChMaxSize(),
            Mem->RoomSize(), Mem->RoomMaxSize());
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  OLC is %s\r\n",
            (olc_state ? "^GAvailable.^n" : "^RUnavailable.^n"));
    send_to_char(buf, ch);
    break;
  case 5:
    strcpy(buf, "Errant Rooms\r\n------------\r\n");
    for (i = 0, k = 0; i <= top_of_world; i++)
      for (j = 0; j < NUM_OF_DIRS; j++)
        if (world[i].dir_option[j] && !world[i].dir_option[j]->to_room && i != last) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%2d: [%8ld] %s\r\n", ++k, world[i].number, world[i].name);
          last = i;
        }
    send_to_char(buf, ch);
    break;
  case 6:
    strcpy(buf, "Death Traps\r\n-----------\r\n");
    for (i = 0, j = 0; i <= top_of_world; i++)
      if (ROOM_FLAGGED(&world[i], ROOM_DEATH))
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%2d: [%8ld] %s %s\r\n", ++j,
                world[i].number,
                vnum_from_non_connected_zone(world[i].number) ? " " : "*",
                world[i].name);
    send_to_char(buf, ch);
    break;
  case 7:
#define GOD_ROOMS_ZONE 0

    strcpy(buf, "Godrooms\r\n--------------------------\r\n");
    for (i = 0, j = 0; i <= zone_table[real_zone(GOD_ROOMS_ZONE)].top; i++)
      if (world[i].zone == GOD_ROOMS_ZONE && i > 1 && !(i >= 8 && i <= 12))
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%2d: [%8ld] %s %s\r\n", j++, world[i].number,
                vnum_from_non_connected_zone(world[i].number) ? " " : "*",
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
    send_to_char(ch, "%s's skills:", GET_NAME(vict));
    j = 0;
    snprintf(buf, sizeof(buf), "\r\n");
    for (i = MIN_SKILLS; i < MAX_SKILLS; i++)
      if (GET_SKILL(vict, i) > 0) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "[%-20s%4d]", skills[i].name, GET_SKILL(vict, i));
        j++;
        if (!(j % 3))
          strcat(buf, "\r\n");
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
    strcpy(buf, "Jackpoints\r\n---------\r\n");
    for (i = 0, j = 0; i <= top_of_world; i++)
      if (world[i].matrix > 0)
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%2d: [%8ld] %s^n (%ld/%ld)\r\n", ++j,
                world[i].number, world[i].name, world[i].matrix, world[i].rtg);
    send_to_char(buf, ch);
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
    send_to_char(ch, "%s's abilities:", GET_NAME(vict));
    j = 0;
    snprintf(buf, sizeof(buf), "\r\n");
    for (i = 1; i < ADEPT_NUMPOWER; i++) {      
      if (GET_POWER_TOTAL(vict, i) > 0) {
        snprintf(buf2, sizeof(buf2), "%-20s", adept_powers[i]);
        if (max_ability(i) > 1)
          switch (i) {
          case ADEPT_KILLING_HANDS:
            snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), " %-8s", wound_name[MIN(4, GET_POWER_TOTAL(vict, i))]);
            if (GET_POWER_ACT(vict, i))
              snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), " ^Y(%-8s)^n", wound_name[MIN(4, GET_POWER_ACT(vict, i))]);
            strcat(buf2, "\r\n");
            break;
          default:
            snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), " +%d", GET_POWER_TOTAL(vict, i));
            if (GET_POWER_ACT(vict, i))
              snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), " ^Y(%d)^n", GET_POWER_ACT(vict, i)); 
            strcat(buf2, "\r\n");
            break;
          }
        else
          strcat(buf2, "\r\n");
        strcat(buf, buf2);
      }
    }
    send_to_char(buf, ch);
    send_to_char("\r\n", ch);
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
    send_to_char(ch, "%s's metamagic abilities:", GET_NAME(vict));
    j = 0;
    snprintf(buf, sizeof(buf), "\r\n");
    for (i = 0; i < META_MAX; i++)
      if (GET_METAMAGIC(vict, i))
        send_to_char(ch, "  %s%s^n\r\n", GET_METAMAGIC(vict, i) == 2 ? "" : "^r", metamagic[i]);
    send_to_char("\r\n", ch);
    break;
  case 17:
    strcpy(buf, "Exitless Rooms\r\n-----------\r\n");
    for (i = 0, j = 0; i <= top_of_world; i++) {
      // Don't need to hear about the imm zones.
      if (!room_has_any_exits(&world[i])) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%4d: [%6ld] %s %s\r\n", ++j,
                world[i].number,
                vnum_from_non_connected_zone(world[i].number) ? " " : "*",
                world[i].name);
      }
    }
    send_to_char(buf, ch);
    break;
  case 18:
    strcpy(buf, "Trap Rooms\r\n-----------\r\n");
    int dir;
    for (i = 0, j = 0; i <= top_of_world; i++) {      
      for (dir = 0; dir <= DOWN; dir++) {
        if (world[i].dir_option[dir] && world[i].dir_option[dir]->to_room) {
          if (!room_has_any_exits(world[i].dir_option[dir]->to_room)) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%4d: [%6ld] %s %s: %s\r\n", ++j,
                    world[i].number,
                    vnum_from_non_connected_zone(world[i].number) ? " " : "*",
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
    display_pockets_to_char(ch, vict);
    break;
  case 20:
    strcpy(buf, "Storage Rooms\r\n-----------\r\n");
    for (i = 0, j = 0; i <= top_of_world; i++)
      if (ROOM_FLAGGED(&world[i], ROOM_STORAGE))
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%4d: [%8ld] %s %s\r\n", ++j,
                world[i].number,
                vnum_from_non_connected_zone(world[i].number) ? " " : "*",
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
                  vnum_from_non_connected_zone(world[i].number) ? " " : "*",
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
      if (vnum_from_non_connected_zone(GET_MOB_VNUM(&mob_proto[i]))
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
               vnum_from_non_connected_zone(GET_MOB_VNUM(&mob_proto[i])) ? " " : "*",
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
        strncat(buf, " has not had its attributes set yet", sizeof(buf) - strlen(buf) - 1);
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
  half_chop(argument, name, buf);
  if (!*name || !*buf) {
    send_to_char("Usage: vset <victim> <field> <value>\r\n", ch);
    return;
  }
  if (!(veh = get_veh_list(name, ch->in_veh ? ch->in_veh->carriedvehs : ch->in_room->vehicles, ch))) {
    send_to_char("There is no such vehicle.\r\n", ch);
    return;
  }

  half_chop(buf, field, val_arg);
  snprintf(buf, sizeof(buf), "Chose either ^rowner^n or ^rlocked^n.\r\n");

  if (is_abbrev(field, "owner")) {
    value = atoi(val_arg);
    veh->owner = value;
    snprintf(buf, sizeof(buf), "%s's owner field set to %d.\r\n", GET_VEH_NAME(veh), value);
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
               { "rpe",      LVL_ADMIN, PC, BINARY },
               { "quest",    LVL_VICEPRES, PC , BINARY },
               { "questor",      LVL_PRESIDENT, PC, BINARY }, // 65
               { "aspect",       LVL_ADMIN, PC, NUMBER},
               { "helper",       LVL_ADMIN, PC, BINARY },
               { "blacklist",       LVL_VICEPRES, PC, BINARY },
               { "noidle", LVL_VICEPRES, PC, BINARY },
               { "tke", LVL_VICEPRES, PC, NUMBER }, //70
               { "sysp", LVL_VICEPRES, PC, NUMBER },
               { "\n", 0, BOTH, MISC }
             };

  half_chop(argument, name, buf);
  if (!strcmp(name, "file")) {
    is_file = 1;
    half_chop(buf, name, buf);
  } else if (!str_cmp(name, "player")) {
    is_player = 1;
    half_chop(buf, name, buf);
  } else if (!str_cmp(name, "mob")) {
    half_chop(buf, name, buf);
  }
  half_chop(buf, field, buf2);
  strcpy(val_arg, buf2);

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
    vict = playerDB.LoadChar(name, false);

    if (vict) {
      if (!access_level(ch, GET_LEVEL(vict)+1)) {
        send_to_char("Sorry, you can't do that.\r\n", ch);
        SET_CLEANUP(false);
        return;
      }
    } else {
      send_to_char("There is no such player.\r\n", ch);
      return;
    }
  }

  if (!access_level(ch, LVL_ADMIN)) {
    if (!IS_NPC(vict) && !access_level(ch, GET_LEVEL(vict)+1)) {
      send_to_char("Maybe that's not such a great idea...\r\n", ch);

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
    return;
  } else if (!IS_NPC(vict) && fields[l].pcnpc == NPC) {
    send_to_char("That can only be done to mobs!\r\n", ch);
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
  mudlog(buf, ch, LOG_WIZLOG, TRUE );

  strcpy(buf, "Okay.");  /* can't use OK macro here 'cause of \r\n */
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
      RANGE(1, 600);
    vict->real_abils.ess = value;
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
      vict->player.sex = SEX_MALE;
    else if (!str_cmp(val_arg, "female"))
      vict->player.sex = SEX_FEMALE;
    else if (!str_cmp(val_arg, "neutral"))
      vict->player.sex = SEX_NEUTRAL;
    else {
      send_to_char("Must be 'male', 'female', or 'neutral'.\r\n", ch);

      SET_CLEANUP(false);

      return;
    }
    break;
  case 16:
    RANGE(0, 100);
    GET_BALLISTIC(vict) = value;
    affect_total(vict);
    break;
  case 17:
    RANGE(0, 100000000);
    GET_NUYEN(vict) = value;
    break;
  case 18:
    RANGE(0, 100000000);
    GET_BANK(vict) = value;
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
      strcpy(buf, "Must be 'off' or a room's virtual number.\r\n");
    break;
  case 38:
    if (GET_IDNUM(ch) != 1 || !IS_NPC(vict))
      return;
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
      RANGE(0, 1500);
    vict->real_abils.mag = value;
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
    GET_IMPACT(vict) = value;
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
      return;
    }
    RANGE(0, 53);
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
    if (strstr((const char *)val_arg, "^"))
      send_to_char("Whotitles can't contain the ^ character.\r\n", ch);
    else
      set_whotitle(vict, val_arg);
    break;
  case 53:
    SET_OR_REMOVE(PRF_FLAGS(vict), PRF_PKER);
    break;
  case 54:
    if (IS_NPC(vict) || (IS_SENATOR(vict) && access_level(vict, LVL_ADMIN)))
      RANGE(1, 1000);
    else
      RANGE(1, 600);
    vict->real_abils.bod_index = value;
    affect_total(vict);
    break;
  case 55:
    RANGE(0, 7);
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
    SET_OR_REMOVE(PRF_FLAGS(vict), PRF_QUEST);
    break;
  case 65:
    SET_OR_REMOVE(PRF_FLAGS(vict), PRF_QUESTOR);
    break;
  case 66:
    RANGE(0, 7);
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
  default:
    snprintf(buf, sizeof(buf), "Can't set that!");
    break;
  }

  if (fields[l].type == BINARY) {
    snprintf(buf, sizeof(buf), "%s %s for %s.\r\n", fields[l].cmd, ONOFF(on), GET_NAME(vict));
    CAP(buf);
  } else if (fields[l].type == NUMBER) {
    snprintf(buf, sizeof(buf), "%s's %s set to %d.\r\n", GET_NAME(vict), fields[l].cmd, value);
  } else
    strcat(buf, "\r\n");
  send_to_char(CAP(buf), ch);

  if (is_file) {
    SET_CLEANUP(true);
    send_to_char("Saved in file.\r\n", ch);
  }
}

ACMD(do_logwatch)
{
  one_argument(argument, buf);

  if (!*buf) {
    snprintf(buf, sizeof(buf), "You are currently watching the following:\r\n%s%s%s%s%s%s%s%s%s%s%s%s%s",
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
            (PRF_FLAGGED(ch, PRF_PURGELOG) ? "  PurgeLog\r\n" : ""));

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
  } else if (is_abbrev(buf, "deathlog") && access_level(ch, LVL_FIXER) ) {
    if (PRF_FLAGGED(ch, PRF_DEATHLOG)) {
      send_to_char("You no longer watch the DeathLog.\r\n", ch);
      PRF_FLAGS(ch).RemoveBit(PRF_DEATHLOG);
    } else {
      send_to_char("You will now see the DeathLog.\r\n", ch);
      PRF_FLAGS(ch).SetBit(PRF_DEATHLOG);
    }
  } else if (is_abbrev(buf, "misclog") && access_level( ch, LVL_FIXER)) {
    if (PRF_FLAGGED(ch, PRF_MISCLOG)) {
      send_to_char("You no longer watch the MiscLog.\r\n", ch);
      PRF_FLAGS(ch).RemoveBit(PRF_MISCLOG);
    } else {
      send_to_char("You will now see the MiscLog.\r\n", ch);
      PRF_FLAGS(ch).SetBit(PRF_MISCLOG);
    }
  } else if (is_abbrev(buf, "wizlog") && access_level(ch, LVL_VICEPRES)) {
    if (PRF_FLAGGED(ch, PRF_WIZLOG)) {
      send_to_char("You no longer watch the WizLog.\r\n", ch);
      PRF_FLAGS(ch).RemoveBit(PRF_WIZLOG);
    } else if (access_level(ch, LVL_VICEPRES)) {
      send_to_char("You will now see the WizLog.\r\n", ch);
      PRF_FLAGS(ch).SetBit(PRF_WIZLOG);
    } else {
      send_to_char("You aren't permitted to view that log at your level.\r\n", ch);
    }
  } else if (is_abbrev(buf, "syslog") && access_level(ch, LVL_ADMIN)) {
    if (PRF_FLAGGED(ch, PRF_SYSLOG)) {
      send_to_char("You no longer watch the SysLog.\r\n", ch);
      PRF_FLAGS(ch).RemoveBit(PRF_SYSLOG);
    } else if (access_level(ch, LVL_ADMIN)) {
      send_to_char("You will now see the SysLog.\r\n", ch);
      PRF_FLAGS(ch).SetBit(PRF_SYSLOG);
    } else {
      send_to_char("You aren't permitted to view that log at your level.\r\n", ch);
    }
  } else if (is_abbrev(buf, "zonelog") && access_level(ch, LVL_ADMIN)) {
    if (PRF_FLAGGED(ch, PRF_ZONELOG)) {
      send_to_char("You no longer watch the ZoneLog.\r\n", ch);
      PRF_FLAGS(ch).RemoveBit(PRF_ZONELOG);
    } else if (access_level(ch, LVL_ADMIN)) {
      send_to_char("You will now see the ZoneLog.\r\n", ch);
      PRF_FLAGS(ch).SetBit(PRF_ZONELOG);
    } else {
      send_to_char("You aren't permitted to view that log at your level.\r\n", ch);
    }
  } else if (is_abbrev(buf, "cheatlog") && access_level(ch, LVL_VICEPRES)) {
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
    send_to_char("All available logs have been activated.\r\n", ch);
  } else if (is_abbrev(buf, "none")) {
    PRF_FLAGS(ch).RemoveBits(PRF_CONNLOG, PRF_DEATHLOG, PRF_MISCLOG, PRF_WIZLOG,
                             PRF_SYSLOG, PRF_ZONELOG, PRF_CHEATLOG, PRF_BANLOG, PRF_GRIDLOG,
                             PRF_WRECKLOG, PRF_PGROUPLOG, PRF_HELPLOG, PRF_PURGELOG, ENDBIT);
    send_to_char("All logs have been disabled.\r\n", ch);
  } else
    send_to_char("Watch what log?\r\n", ch);

  return;
}

ACMD(do_zlist)
{
  int first, last, nr, zonenum = 0;
  char buf[MAX_STRING_LENGTH*10];
  two_arguments(argument, buf, buf1);
  
  if (!access_level(ch, LVL_PRESIDENT) && !PLR_FLAGGED(ch, PLR_OLC)) {
    send_to_char(YOU_NEED_OLC_FOR_THAT, ch);
    return;
  }

  if (!*buf && !*buf1) {
    zonenum = real_zone(ch->player_specials->saved.zonenum);
    first = 0;
    last = zone_table[zonenum].num_cmds;
  } else if (*buf && !*buf1) {     // if there is not a second argument, then the
    zonenum = real_zone(atoi(buf));  // is considered the zone number
    first = 0;
    last = zone_table[zonenum].num_cmds;
  } else {
    zonenum = real_zone(ch->player_specials->saved.zonenum);
    first = MAX(0, MIN(atoi(buf), zone_table[zonenum].num_cmds));
    last = MIN(atoi(buf1), zone_table[zonenum].num_cmds);
  }

  // return if it us a non-existent zone
  if (zonenum < 0) {
    send_to_char("Zone: Non existent.\r\n", ch);
    return;
  }
  snprintf(buf, sizeof(buf), "Zone: %d (%d); Cmds: %d\r\n",
          zone_table[zonenum].number, zonenum,
          zone_table[zonenum].num_cmds);

  int last_mob = 0, last_veh = 0;
  #define ZCMD zone_table[zonenum].cmd[nr]
  #define MOB(rnum) MOB_VNUM_RNUM(rnum)
  #define OBJ(rnum) OBJ_VNUM_RNUM(rnum)
  #define ROOM(rnum) world[rnum].number
  #define VEH(rnum) VEH_VNUM_RNUM(rnum)
  #define HOST(rnum) matrix[rnum].vnum

  for (nr = first; nr < last; nr++) {
    if (ZCMD.if_flag)
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%3d) (if last)", nr);
    else
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%3d) (always) ", nr);

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

ACMD(do_mlist)
{
  if (!access_level(ch, LVL_PRESIDENT) && !PLR_FLAGGED(ch, PLR_OLC)) {
    send_to_char(YOU_NEED_OLC_FOR_THAT, ch);
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
       (MOB_VNUM_RNUM(nr) <= last); nr++)
    if (MOB_VNUM_RNUM(nr) >= first)
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%5d. [%8ld] %s\r\n", ++found,
              MOB_VNUM_RNUM(nr), mob_proto[nr].player.physical_text.name);

  if (!found)
    send_to_char("No mobiles were found in those parameters.\r\n", ch);
  else
    page_string(ch->desc, buf, 1);
}

ACMD(do_ilist)
{
  if (!access_level(ch, LVL_PRESIDENT) && !PLR_FLAGGED(ch, PLR_OLC)) {
    send_to_char(YOU_NEED_OLC_FOR_THAT, ch);
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
    if (OBJ_VNUM_RNUM(nr) >= first) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%5d. [%8ld x%4d] %s%s\r\n", ++found,
      OBJ_VNUM_RNUM(nr),
      ObjList.CountObj(nr),
      obj_proto[nr].text.name,
      obj_proto[nr].source_info ? "  ^g(canon)^n" : "");
    }
  }

  if (!found)
    send_to_char("No objects were found in those parameters.\r\n", ch);
  else
    page_string(ch->desc, buf, 1);
}

ACMD(do_vlist)
{
  if (!access_level(ch, LVL_PRESIDENT) && !PLR_FLAGGED(ch, PLR_OLC)) {
    send_to_char(YOU_NEED_OLC_FOR_THAT, ch);
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
       (VEH_VNUM_RNUM(nr) <= last); nr++)
    if (VEH_VNUM_RNUM(nr) >= first)
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%5d. [%8ld] %s\r\n", ++found,
              VEH_VNUM_RNUM(nr), veh_proto[nr].short_description);

  if (!found)
    send_to_char("No vehicles were found in those parameters.\r\n", ch);
  else
    page_string(ch->desc, buf, 1);
}

ACMD(do_qlist)
{
  if (!access_level(ch, LVL_PRESIDENT) && !PLR_FLAGGED(ch, PLR_OLC)) {
    send_to_char(YOU_NEED_OLC_FOR_THAT, ch);
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
       (quest_table[nr].vnum <= last); nr++)
    if (quest_table[nr].vnum >= first)
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%5ld. [%8ld] %s (%ld)\r\n",
              ++found, quest_table[nr].vnum,
              (real_mob = real_mobile(quest_table[nr].johnson)) < 0 ? "None" : GET_NAME(&mob_proto[real_mob]),
              quest_table[nr].johnson);

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
  if (!access_level(ch, LVL_PRESIDENT) && !PLR_FLAGGED(ch, PLR_OLC)) {
    send_to_char(YOU_NEED_OLC_FOR_THAT, ch);
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
    if (world[nr].number >= first)
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
  if (!access_level(ch, LVL_PRESIDENT) && !PLR_FLAGGED(ch, PLR_OLC)) {
    send_to_char(YOU_NEED_OLC_FOR_THAT, ch);
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
       (matrix[nr].vnum <= last); nr++)
    if (matrix[nr].vnum >= first)
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%5d. [%8ld] %s\r\n", ++found,
              matrix[nr].vnum, matrix[nr].name);

  if (!found)
    send_to_char("No hosts were found in those parameters.\r\n", ch);
  else
    page_string(ch->desc, buf, 1);
}

ACMD(do_iclist)
{
  if (!access_level(ch, LVL_PRESIDENT) && !PLR_FLAGGED(ch, PLR_OLC)) {
    send_to_char(YOU_NEED_OLC_FOR_THAT, ch);
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

  for (nr = MAX(0, real_ic(first)); nr <= top_of_ic &&
       (ic_index[nr].vnum <= last); nr++)
    if (ic_index[nr].vnum >= first)
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%5d. [%8ld] %-50s [%s-%d]\r\n", ++found,
              ic_index[nr].vnum, ic_proto[nr].name, ic_type[ic_proto[nr].ic.type], ic_proto[nr].ic.rating);

  if (!found)
    send_to_char("No IC were found in those parameters.\r\n", ch);
  else
    page_string(ch->desc, buf, 1);
}

ACMD(do_slist)
{
  if (!access_level(ch, LVL_PRESIDENT) && !PLR_FLAGGED(ch, PLR_OLC)) {
    send_to_char(YOU_NEED_OLC_FOR_THAT, ch);
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
  for (nr = MAX(0, real_shop(first)); nr <= top_of_shopt && (shop_table[nr].vnum <= last); nr++)
    if (shop_table[nr].vnum >= first)
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%5d. [%8ld] %s %s (%ld)\r\n", ++found,
              shop_table[nr].vnum,
              vnum_from_non_connected_zone(shop_table[nr].keeper) ? " " : "*",
              (real_mob = real_mobile(shop_table[nr].keeper)) < 0 ? "None" : GET_NAME(&mob_proto[real_mob]),
              shop_table[nr].keeper);

  if (!found)
    send_to_char("No shops were found in those parameters.\r\n", ch);
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

ACMD(do_tail)
{
  char arg[MAX_STRING_LENGTH];
  FILE *out = NULL;
  int lines = 20;

  //out = new FILE;
  char buf[MAX_STRING_LENGTH];

  two_arguments(argument, arg, buf);

  if ( !*arg ) {
    send_to_char( "Syntax note: tail <lines into history to read> <logfile>\r\n", ch );
    send_to_char( "The following logs are available:\r\n", ch );
    snprintf(buf, sizeof(buf), "ls -C ../log" );
  } else {
    if ( atoi( arg ) != 0 ) {
      lines = atoi( arg );
      if ( lines < 0 )
        lines = 0 - lines;
      // strcpy( arg, buf );
    }
    
    // Only allow letters, periods, numbers, and dashes.
    int index = 0;
    for (char *ptr = buf; *ptr && index < MAX_STRING_LENGTH; ptr++) {
      if (!isalnum(*ptr) && *ptr != '.' && *ptr != '-')
        continue;
      else
        arg[index++] = *ptr;
    }
    arg[index] = '\0';

    send_to_char(ch, "tail -%d ../log/%s\r\n", lines, arg );
  }
  snprintf(arg, sizeof(arg), "%s", buf );

  out=popen( arg, "r");

  while (fgets(buf, MAX_STRING_LENGTH-5, out) != NULL) {
    strcat(buf,"\r");
    send_to_char( buf, ch );
  }
  fclose( out );
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
  half_chop(argument, arg, buf);

  if (!*arg) {
    send_to_char("You have to restring something!\r\n", ch);
    return FALSE;
  }
  
  if (!*buf) {
    send_to_char("Restring to what?\r\n", ch);
    return FALSE;
  }
  
  if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
    send_to_char("You're not carrying that item.\r\n", ch);
    return FALSE;
  }
  if (GET_OBJ_TYPE(obj) == ITEM_GUN_ACCESSORY || GET_OBJ_TYPE(obj) == ITEM_MOD) {
    send_to_char("Sorry, gun attachments and vehicle mods can't be restrung.\r\n", ch);
    return FALSE;
  }
  if (strlen(buf) >= LINE_LENGTH) {
    send_to_char(ch, "That restring is too long, please shorten it. The maximum length is %d characters.\r\n", LINE_LENGTH - 1);
    return FALSE;
  }
  
  if (PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED)) {
    if (!GET_RESTRING_POINTS(ch)) {
      send_to_char("You don't have enough restring points left to restring that.\r\n", ch);
      return FALSE;
    }
    GET_RESTRING_POINTS(ch)--;
  } 
  
  else {
    if (using_sysp) {
      if (GET_SYSTEM_POINTS(ch) < SYSP_RESTRING_COST) {
        send_to_char(ch, "It costs %d system points to restring something, and you only have %d.\r\n",
                     SYSP_RESTRING_COST,
                     GET_SYSTEM_POINTS(ch));
        return FALSE;
      }
      GET_SYSTEM_POINTS(ch) -= SYSP_RESTRING_COST;
    } else {
      if (GET_KARMA(ch) < 250) {
        send_to_char("You don't have enough karma to restring that. It costs 2.5 karma.\r\n", ch);
        return FALSE;
      }
      GET_KARMA(ch) -= 250;
    }
  }
  
  snprintf(buf2, sizeof(buf2), "%s restrung '%s' to '%s'", GET_CHAR_NAME(ch), obj->text.name, buf);
  mudlog(buf2, ch, LOG_WIZLOG, TRUE);
  
  DELETE_ARRAY_IF_EXTANT(obj->restring);
  obj->restring = str_dup(buf);
  send_to_char(ch, "%s successfully restrung.\r\n", obj->text.name);
  
  return TRUE;
}

ACMD(do_restring) {
  restring_with_args(ch, argument, FALSE);
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

ACMD(do_shopfind)
{
  int number;

  one_argument(argument, buf2);

  if (!access_level(ch, LVL_PRESIDENT) && !PLR_FLAGGED(ch, PLR_OLC)) {
    send_to_char(YOU_NEED_OLC_FOR_THAT, ch);
    return;
  }
  
  if (!*buf2) {
    send_to_char("Usage: shopfind <number or keyword>\r\n", ch);
    return;
  }
  
  if ((number = atoi(buf2)) < 0) {
    send_to_char("A NEGATIVE number??\r\n", ch);
    return;
  }
  
  if (number)
    send_to_char(ch, "Shops selling the item with vnum %d:\r\n", number);
  else
    send_to_char(ch, "Shops selling items with keyword %s:\r\n", buf2);
  
  int index = 0;
  for (int shop_nr = 0; shop_nr <= top_of_shopt; shop_nr++) {
    int real_mob = real_mobile(shop_table[shop_nr].keeper);
    if (real_mob < 0) {
      snprintf(buf, sizeof(buf), "Warning: Shop %ld does not have a valid keeper!", shop_table[shop_nr].vnum);
      mudlog(buf, NULL, LOG_SYSLOG, TRUE);
      continue;
    }
    
    for (struct shop_sell_data *sell = shop_table[shop_nr].selling; sell; sell = sell->next) {
      int real_obj = real_object(sell->vnum);
      if (real_obj < 0)
        continue;
      
      if (number) {
        if (sell->vnum == number) {
          send_to_char(ch, "%3d)  Shop %8ld (%s)\r\n", 
                       ++index,
                       shop_table[shop_nr].vnum, 
                       mob_proto[real_mob].player.physical_text.name);
        }
      } else if (isname(buf2, obj_proto[real_obj].text.name) || isname(buf2, obj_proto[real_obj].text.keywords)) {
        send_to_char(ch, "%3d)  Shop %8ld (%s) sells %s (%ld)\r\n", 
                     ++index,
                     shop_table[shop_nr].vnum, 
                     mob_proto[real_mob].player.physical_text.name,
                     GET_OBJ_NAME(&obj_proto[real_obj]),
                     GET_OBJ_VNUM(&obj_proto[real_obj]));
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
  
  mudlog("World rewriting initiated. Please hold...", ch, LOG_SYSLOG, TRUE);
  
  // Clear our alarm handler.
  signal(SIGALRM, SIG_IGN);
  
  // Perform writing for all zones that have rooms.
  for (int i = 0; i <= top_of_zone_table; i++) {
    write_world_to_disk(zone_table[i].number);
    write_quests_to_disk(zone_table[i].number);
  }
  
  // Re-register our alarm handler.
  signal(SIGALRM, alarm_handler);
  
  mudlog("World rewriting complete.", ch, LOG_SYSLOG, TRUE);
    
  send_to_char("Done. Alarm handler has been restored.\r\n", ch);
}

int audit_zone_rooms(struct char_data *ch, int zone_num, bool verbose) {
  int real_rm, issues = 0;
  struct room_data *room;
  bool printed = FALSE;
  
  if (verbose)
    send_to_char(ch, "\r\n^WAuditing rooms for zone %d...^n\r\n", zone_table[zone_num].number);
    
  for (int i = zone_table[zone_num].number * 100; i <= zone_table[zone_num].top; i++) {
    if ((real_rm = real_room(i)) < 0)
      continue;
      
    room = &world[real_rm];
    
    printed = FALSE;
    
    snprintf(buf, sizeof(buf), "^c[%8ld]^n %s %s^n:\r\n",
             room->number,
             vnum_from_non_connected_zone(room->number) ? " " : "*",
             room->name);
    
    // Check its strings.
    if (!strcmp(GET_ROOM_NAME(room), STRING_ROOM_TITLE_UNFINISHED)) {
      strncat(buf, "  - Default room title used. This room will NOT be saved in the world files.\r\n", sizeof(buf) - strlen(buf) - 1);
      issues++;
      printed = TRUE;
    }
    
    if (!strcmp(GET_ROOM_DESC(room), STRING_ROOM_DESC_UNFINISHED)) {
      strncat(buf, "  - Default room desc used.\r\n", sizeof(buf) - strlen(buf) - 1);
      issues++;
      printed = TRUE;
    }
    
    if (room->matrix > 0) {
      if (real_host(room->matrix) < 1) {
        strncat(buf, "  - Invalid Matrix host specified.\r\n", sizeof(buf) - strlen(buf) - 1);
        issues++;
        printed = TRUE;
      }
      
      if (!strcmp(room->address, STRING_ROOM_JACKPOINT_NO_ADDR)) {
        strncat(buf, "  - Default jackpoint address used.\r\n", sizeof(buf) - strlen(buf) - 1);
        issues++;
        printed = TRUE;
      }
    }
    
    // Check its flags.
    if (ROOM_FLAGGED(room, ROOM_ENCOURAGE_CONGREGATION)) {
      strncat(buf, "  - Socialization flag is set.\r\n", sizeof(buf) - strlen(buf) - 1);
      issues++;
      printed = TRUE;
    }
    
    if (ROOM_FLAGGED(room, ROOM_NOMAGIC)) {
      strncat(buf, "  - !magic flag is set.\r\n", sizeof(buf) - strlen(buf) - 1);
      issues++;
      printed = TRUE;
    }
    
    if (ROOM_FLAGGED(room, ROOM_ARENA)) {
      strncat(buf, "  - Arena flag is set.\r\n", sizeof(buf) - strlen(buf) - 1);
      issues++;
      printed = TRUE;
    }
    
    // Staff-only is allowed in the staff HQ area. Otherwise, flag it.
    if ((GET_ROOM_VNUM(room) < 10000 || GET_ROOM_VNUM(room) > 10099) && ROOM_FLAGGED(room, ROOM_STAFF_ONLY)) {
      strncat(buf, "  - Staff-only flag is set.\r\n", sizeof(buf) - strlen(buf) - 1);
      issues++;
      printed = TRUE;
    }
    
    if (ROOM_FLAGS(room).AreAnySet(ROOM_HOUSE_CRASH, ROOM_BFS_MARK, ROOM_OLC, ROOM_NOQUIT, ROOM_ASTRAL, ENDBIT)) {
      strncat(buf, "  - System-controlled or unimplemented flags are set.\r\n", sizeof(buf) - strlen(buf) - 1);
      issues++;
      printed = TRUE;
    }
    
#ifdef USE_SLOUCH_RULES
    if (room->z < 2.0) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - Low ceilings - %0.2f meters.\r\n", room->z);
      issues++;
      printed = TRUE;
    }
#endif
    
    if (room->staff_level_lock > 0) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - Staff-locked to level %d.\r\n", room->staff_level_lock);
      issues++;
      printed = TRUE;
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
        printed = TRUE;
      }
      // Check for keywords and descs on doors.
      if (room->dir_option[k]->exit_info != 0 
          && (!room->dir_option[k]->keyword || !room->dir_option[k]->general_description)) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - %s exit is missing keywords and/or description.\r\n",
                dirs[k]);
        issues++;
        printed = TRUE;
      }
      // Check for valid exits.
      if (!room->dir_option[k]->to_room) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - %s exit leads to an invalid room.\r\n",
                dirs[k]);
        issues++;
        printed = TRUE;
      } 
      // to_room guaranteed to exist. Check if the exit on the other side comes back in our direction.
      else {
        if (!room->dir_option[k]->to_room->dir_option[rev_dir[k]]) {
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - %s exit is one-way: There is no return exit.\r\n", dirs[k]);
          issues++;
          printed = TRUE;
        } 
        // Exit coming back in our direction is guaranteed to exist. Check if it comes to us, and if so, if the flags are kosher.
        else {
          if (room->dir_option[k]->to_room->dir_option[rev_dir[k]]->to_room != room) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - %s exit: Return exit %s from %ld does not point here.\r\n",
                    dirs[k],
                    dirs[rev_dir[k]],
                    GET_ROOM_VNUM(room->dir_option[k]->to_room));
            issues++;
            printed = TRUE;
          }
          else if (room->dir_option[k]->to_room->dir_option[rev_dir[k]]->exit_info != room->dir_option[k]->exit_info) {
            snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - %s exit has flags that don't match the return direction's flags (%s from here: %s, %s from %ld: %s).\r\n",
                    dirs[k],
                    dirs[k],
                    render_door_type_string(room->dir_option[k]),
                    dirs[rev_dir[k]],
                    GET_ROOM_VNUM(room->dir_option[k]->to_room),
                    render_door_type_string(room->dir_option[k]->to_room->dir_option[rev_dir[k]])
                  );
            issues++;
            printed = TRUE;
          }
        }
      }
    }
    if (printed)
      send_to_char(ch, "%s\r\n", buf);
  }
  
  return issues;
}

int audit_zone_mobs(struct char_data *ch, int zone_num, bool verbose) {
  int issues = 0, total_stats, real_mob, k;
  bool printed = FALSE;
  char candidate;
  
  if (verbose)
    send_to_char(ch, "\r\n^WAuditing mobs for zone %d...^n\r\n", zone_table[zone_num].number);
    
  for (int i = zone_table[zone_num].number * 100; i <= zone_table[zone_num].top; i++) {
    if ((real_mob = real_mobile(i)) < 0)
      continue;
      
    struct char_data *mob = &mob_proto[real_mob];
    
    // Check stats, etc
    total_stats = 0;
    for (k = BOD; k <= REA; k++)
      total_stats += GET_REAL_ATT(mob, k);
    
    snprintf(buf, sizeof(buf), "^c[%8ld]^n %s %s^n:\r\n",
             GET_MOB_VNUM(mob),
             vnum_from_non_connected_zone(GET_MOB_VNUM(mob)) ? " " : "*",
             GET_CHAR_NAME(mob));
    
    printed = FALSE;
             
    // Flag mobs with crazy stats
    if (total_stats > ANOMALOUS_TOTAL_STATS_THRESHOLD) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - has total attributes %d > %d^n.\r\n",
               total_stats,
               ANOMALOUS_TOTAL_STATS_THRESHOLD);
      printed = TRUE;
      issues++;
    }
    
    // Flag mobs with no stats
    if (total_stats == 0) {
      strncat(buf, "  - has not had its attributes set yet.\r\n", sizeof(buf) - strlen(buf) - 1);
      printed = TRUE;
      issues++;
    }
    
    // Flag mobs with crazy skills
    for (k = MIN_SKILLS; k < MAX_SKILLS; k++)
      if (GET_SKILL(mob, k) > ANOMALOUS_SKILL_THRESHOLD) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - %s %d > %d^n.\r\n",
                 skills[k].name,
                 GET_SKILL(mob, k),
                 ANOMALOUS_SKILL_THRESHOLD);
        printed = TRUE;
        issues++;
      }
    
    // Flag mobs with high armor.
    if (GET_BALLISTIC(mob) > 10 || GET_IMPACT(mob) > 10) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - high armor (%d/%d)^n.\r\n", GET_BALLISTIC(mob), GET_IMPACT(mob));
      printed = TRUE;
      issues++;
    }
    
    // Flag mobs with no weight or height
    if (GET_HEIGHT(mob) == 0 || GET_WEIGHT(mob) == 0.0) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - missing vital statistics (weight %d, height %d)^n.\r\n", GET_HEIGHT(mob), GET_WEIGHT(mob));
      printed = TRUE;
      issues++;
    }
    
    // Flag mobs with neutral (default) gender
    if (GET_SEX(mob) == SEX_NEUTRAL) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - neutral (default) gender^n (is this intentional?).\r\n");
      printed = TRUE;
      issues++;
    }
    
    // Flag mobs with weird races.
    if (GET_RACE(mob) <= RACE_UNDEFINED || GET_RACE(mob) >= NUM_RACES) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - undefined or unknown race^n.\r\n");
      printed = TRUE;
      issues++;
    }
    
    // Flag mobs with bad strings.
    if (!mob->player.physical_text.name || !*mob->player.physical_text.name || !strcmp(mob->player.physical_text.name, STRING_MOB_NAME_UNFINISHED)) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - missing name^n.\r\n");
      printed = TRUE;
      issues++;
    } else {
      if (ispunct((candidate = get_final_character_from_string(mob->player.physical_text.name)))) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - name ending in punctuation (%c)^n.\r\n", candidate);
        printed = TRUE;
        issues++;
      }
    }
    
    if (!mob->player.physical_text.keywords || !*mob->player.physical_text.keywords || !strcmp(mob->player.physical_text.keywords, STRING_MOB_KEYWORDS_UNFINISHED)) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - missing keywords^n.\r\n");
      printed = TRUE;
      issues++;
    }
    
    if (!mob->player.physical_text.look_desc || !*mob->player.physical_text.look_desc || !strcmp(mob->player.physical_text.look_desc, STRING_MOB_LDESC_UNFINISHED)) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - missing look desc^n.\r\n");
      printed = TRUE;
      issues++;
    }
    
    if (!mob->player.physical_text.room_desc || !*mob->player.physical_text.room_desc || !strcmp(mob->player.physical_text.room_desc, STRING_MOB_RDESC_UNFINISHED)) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - missing room desc^n.\r\n");
      printed = TRUE;
      issues++;
    } else {
      if (!ispunct((candidate = get_final_character_from_string(mob->player.physical_text.room_desc)))) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - room desc not ending in punctuation (%c)^n.\r\n", candidate);
        printed = TRUE;
        issues++;
      }
    }
    
    if (printed)
      send_to_char(ch, "%s\r\n", buf);
  }
    
  return issues;
}

int audit_zone_objects(struct char_data *ch, int zone_num, bool verbose) {
  int issues = 0, real_obj;
  struct obj_data *obj;
  bool printed = FALSE;
  char candidate;
  
  if (verbose)
    send_to_char(ch, "\r\n^WAuditing objects for zone %d...^n\r\n", zone_table[zone_num].number);
    
  for (int i = zone_table[zone_num].number * 100; i <= zone_table[zone_num].top; i++) {
    if ((real_obj = real_object(i)) < 0)
      continue;
      
    obj = &obj_proto[real_obj];
    
    snprintf(buf, sizeof(buf), "^c[%8ld]^n %s %s^n:\r\n",
             GET_OBJ_VNUM(obj),
             vnum_from_non_connected_zone(GET_OBJ_VNUM(obj)) ? " " : "*",
             GET_OBJ_NAME(obj));
    
    printed = FALSE;
    
    // Flag objects with zero cost
    if (GET_OBJ_COST(obj) <= 0) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - cost %d^n.\r\n", GET_OBJ_COST(obj));
      printed = TRUE;
      issues++;
    }
    
    // Call out all objects with affs
    for (int aff_index = 0; aff_index < MAX_OBJ_AFFECT; aff_index++) {
      if (obj->affected[aff_index].modifier) {
        sprinttype(obj->affected[aff_index].location, apply_types, buf2, sizeof(buf2));
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - AFF %s %d^n.\r\n",
                 buf2,
                 obj->affected[aff_index].modifier);
        printed = TRUE;
        issues++;
      }
    }
    
    // Flag objects with zero weight
    if (GET_OBJ_WEIGHT(obj) <= 0) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - no or negative weight^n.\r\n");
      printed = TRUE;
      issues++;
    }
    
    // Flag objects with high weight
    if (GET_OBJ_WEIGHT(obj) >= 100.0) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - high weight %0.2f^n.\r\n", GET_OBJ_WEIGHT(obj));
      printed = TRUE;
      issues++;
    }
    
    // Flag objects that can't be picked up
    if (!GET_OBJ_WEAR(obj).IsSet(ITEM_WEAR_TAKE)) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - cannot be picked up if dropped^n.\r\n");
      printed = TRUE;
      issues++;
    }
    
    if (!obj->text.name || !*obj->text.name || !strcmp(obj->text.name, STRING_OBJ_SDESC_UNFINISHED)) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - missing name^n.\r\n");
      printed = TRUE;
      issues++;
    } else {
      if (ispunct((candidate = get_final_character_from_string(obj->text.name)))) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - name ending in punctuation (%c)^n.\r\n", candidate);
        printed = TRUE;
        issues++;
      }
    }
    
    if (!obj->text.keywords || !*obj->text.keywords || !strcmp(obj->text.room_desc, STRING_OBJ_NAME_UNFINISHED)) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - missing keywords^n.\r\n");
      printed = TRUE;
      issues++;
    }
    
    if (!obj->text.look_desc || !*obj->text.look_desc || !strcmp(obj->text.look_desc, STRING_OBJ_LDESC_UNFINISHED)) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - missing look desc^n.\r\n");
      printed = TRUE;
      issues++;
    }
    
    if (!obj->text.room_desc || !*obj->text.room_desc || !strcmp(obj->text.room_desc, STRING_OBJ_RDESC_UNFINISHED)) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - missing room desc^n.\r\n");
      printed = TRUE;
      issues++;
    }
    else {
      if (!ispunct((candidate = get_final_character_from_string(obj->text.room_desc)))) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - room desc not ending in punctuation (%c)^n.\r\n", candidate);
        printed = TRUE;
        issues++;
      }
    }
    
    if (printed)
      send_to_char(ch, "%s\r\n", buf);
  }  
  // TODO: Make sure they've got all their strings set.
    
  return issues;
}

int audit_zone_quests(struct char_data *ch, int zone_num, bool verbose) {
  int issues = 0, real_qst;
  struct quest_data *quest;  // qwest?  (^_^) 0={=====>
  bool printed = FALSE;
  
  if (verbose)
    send_to_char(ch, "\r\n^WAuditing quests for zone %d...^n\r\n", zone_table[zone_num].number);
    
  for (int i = zone_table[zone_num].number * 100; i <= zone_table[zone_num].top; i++) {
    if ((real_qst = real_quest(i)) < 0)
      continue;
      
    quest = &quest_table[real_qst];
    
    snprintf(buf, sizeof(buf), "^c[%8ld]^n:\r\n", quest->vnum);
    
    printed = FALSE;
             
    // Flag invalid Johnsons
    if (quest->johnson <= 0 || real_mobile(quest->johnson) <= 0) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - invalid Johnson %ld^n.\r\n", quest->johnson);
      printed = TRUE;
      issues++;
    }
    
    // Flag invalid strings
    if (!quest->intro || !*quest->intro) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - no intro string^n.\r\n");
      printed = TRUE;
      issues++;
    }
    
    if (!quest->decline || !*quest->decline) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - no decline string^n.\r\n");
      printed = TRUE;
      issues++;
    }
    
    if (!quest->finish || !*quest->finish) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - no finish string^n.\r\n");
      printed = TRUE;
      issues++;
    }
    
    if (!quest->info || !*quest->info) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - no info string^n.\r\n");
      printed = TRUE;
      issues++;
    }
    
    if (!quest->quit || !*quest->quit) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - no quit string^n.\r\n");
      printed = TRUE;
      issues++;
    }
    
    if (!quest->done || !*quest->done) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - no done string^n.\r\n");
      printed = TRUE;
      issues++;
    }
    
#ifdef USE_QUEST_LOCATION_CODE
    if (!quest->location || !*quest->location) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - no location string^n.\r\n");
      printed = TRUE;
      issues++;
    }
#endif
    
    if (printed)
      send_to_char(ch, "%s\r\n", buf);
  } 
    
  return issues;
}

int audit_zone_shops(struct char_data *ch, int zone_num, bool verbose) {
  int issues = 0, real_shp;
  struct shop_data *shop;
  bool printed = FALSE;
  
  if (verbose)
    send_to_char(ch, "\r\n^WAuditing shops for zone %d...^n\r\n", zone_table[zone_num].number);
    
  for (int i = zone_table[zone_num].number * 100; i <= zone_table[zone_num].top; i++) {
    if ((real_shp = real_shop(i)) < 0)
      continue;
      
    shop = &shop_table[real_shp];
    
    snprintf(buf, sizeof(buf), "^c[%8ld]^n:\r\n", shop->vnum);
    
    printed = FALSE;
             
    // Flag invalid sell multipliers
    if (shop->profit_buy < 1.0) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - too-low buy profit %0.2f < 1.0^n.\r\n", shop->profit_buy);
      printed = TRUE;
      issues++;
    }
    
    // Flag invalid strings
    if (shop->profit_sell > 0.100001) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "  - too-high sell profit %0.2f > 0.1^n.\r\n", shop->profit_sell);
      printed = TRUE;
      issues++;
    }
    
    if (printed)
      send_to_char(ch, "%s\r\n", buf);
  } 
    
  // TODO: Make sure they've got all their strings set.
    
  return issues;
}

int audit_zone_vehicles(struct char_data *ch, int zone_num, bool verbose) {
  int issues = 0, real_veh;
  struct veh_data *veh;
  bool printed = FALSE;
  
  if (verbose)
    send_to_char(ch, "\r\n^WAuditing vehicles for zone %d...^n\r\n", zone_table[zone_num].number);
    
  for (int i = zone_table[zone_num].number * 100; i <= zone_table[zone_num].top; i++) {
    if ((real_veh = real_vehicle(i)) < 0)
      continue;
      
    veh = &veh_proto[real_veh];
    
    snprintf(buf, sizeof(buf), "^c[%8ld]^n:\r\n", veh->veh_number);
    
    printed = FALSE;
    
    /*       
    // Flag invalid sell multipliers
    if (shop->profit_buy < 1.0) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " has too-low buy profit %0.2f < 1.0^n", shop->profit_buy);
      printed = TRUE;
      issues++;
    }
    
    // Flag invalid strings
    if (shop->profit_sell > 0.1) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s too-high sell profit %0.2f > 0.1^n", printed ? ";" : " has", shop->profit_sell);
      printed = TRUE;
      issues++;
    }
    */
    
    if (printed)
      send_to_char(ch, "%s\r\n", buf);
  } 
    
  // TODO: Make sure they've got all their strings set.
    
  return issues;
}

int audit_zone_hosts(struct char_data *ch, int zone_num, bool verbose) {
  int issues = 0, real_hst;
  struct host_data *host;
  bool printed = FALSE;
  
  if (verbose)
    send_to_char(ch, "\r\n^WAuditing hosts for zone %d...^n\r\n", zone_table[zone_num].number);
    
  for (int i = zone_table[zone_num].number * 100; i <= zone_table[zone_num].top; i++) {
    if ((real_hst = real_host(i)) < 0)
      continue;
      
    host = &matrix[real_hst];
    
    snprintf(buf, sizeof(buf), "[%8ld] ^n", host->vnum);
    
    printed = FALSE;
    
    /*       
    // Flag invalid sell multipliers
    if (shop->profit_buy < 1.0) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " has too-low buy profit %0.2f < 1.0^n", shop->profit_buy);
      printed = TRUE;
      issues++;
    }
    
    // Flag invalid strings
    if (shop->profit_sell > 0.1) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s too-high sell profit %0.2f > 0.1^n", printed ? ";" : " has", shop->profit_sell);
      printed = TRUE;
      issues++;
    }
    */
    
    if (printed)
      send_to_char(ch, "%s\r\n", buf);
  } 
    
  // TODO: Make sure they've got all their strings set.
    
  return issues;
}

int audit_zone_ics(struct char_data *ch, int zone_num, bool verbose) {
  int issues = 0, real_num;
  struct matrix_icon *ic;
  bool printed = FALSE;
  
  if (verbose)
    send_to_char(ch, "\r\n^WAuditing ICs for zone %d...^n\r\n", zone_table[zone_num].number);
    
  for (int i = zone_table[zone_num].number * 100; i <= zone_table[zone_num].top; i++) {
    if ((real_num = real_ic(i)) < 0)
      continue;
      
    ic = &ic_proto[real_num];
    
    snprintf(buf, sizeof(buf), "^c[%8d]^n (%s^n)\r\n", ic->idnum, ic->name);
    
    printed = FALSE;
    
    /*       
    // Flag invalid sell multipliers
    if (shop->profit_buy < 1.0) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " has too-low buy profit %0.2f < 1.0^n", shop->profit_buy);
      printed = TRUE;
      issues++;
    }
    
    // Flag invalid strings
    if (shop->profit_sell > 0.1) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s too-high sell profit %0.2f > 0.1^n", printed ? ";" : " has", shop->profit_sell);
      printed = TRUE;
      issues++;
    }
    */
    
    if (printed)
      send_to_char(ch, "%s\r\n", buf);
  } 
    
  // TODO: Make sure they've got all their strings set.
    
  return issues;
}

int audit_zone_commands(struct char_data *ch, int zone_num, bool verbose) {
  int issues = 0;
  
  if (verbose)
    send_to_char(ch, "\r\n^WAuditing zone commands for zone %d...^n\r\n", zone_table[zone_num].number);
  
  for (int cmd_no = 0; cmd_no < zone_table[zone_num].num_cmds; cmd_no++) {
    
  }
    
  return issues;
}

ACMD(do_audit) {
  int number, zonenum;
  char arg1[MAX_INPUT_LENGTH];
  
  // we just wanna check the zone number they want to switch to
  any_one_arg(argument, arg1);

  // convert the arg to an int
  number = atoi(arg1);
  // find the real zone number
  zonenum = real_zone(number);
  
  if (zonenum == 0) {
    send_to_char("That's not a valid zone number.\r\n", ch);
    return;
  }

  // and see if they can edit it
  if (!can_edit_zone(ch, zonenum)) {
    send_to_char("Sorry, you don't have access to audit this zone.\r\n", ch);
    return;
  }
  
  // Perform auditing of the zone.
  int issues = 0;
  issues += audit_zone_rooms(ch, zonenum, TRUE);
  issues += audit_zone_mobs(ch, zonenum, TRUE);
  issues += audit_zone_objects(ch, zonenum, TRUE);
  issues += audit_zone_quests(ch, zonenum, TRUE);
  issues += audit_zone_shops(ch, zonenum, TRUE);
  issues += audit_zone_vehicles(ch, zonenum, TRUE);
  issues += audit_zone_hosts(ch, zonenum, TRUE);
  issues += audit_zone_ics(ch, zonenum, TRUE);
  issues += audit_zone_commands(ch, zonenum, TRUE);
  
  send_to_char(ch, "\r\nDone. Found a total of %d potential issue%s. Note that something being in this list does not disqualify the zone from approval-- it just requires extra scrutiny. Conversely, something not being flagged here doesn't mean it's kosher, it just means we didn't write a coded check for it yet.\r\n", 
               issues, issues != 1 ? "s" : "");
}
