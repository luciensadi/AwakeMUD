/* ****************************************************
* File: medit.cc                                      *
* Mob Editor For AwakeOLC, a component of AwakeMUD    *
* (c)2001 The AwakeMUD Consortium, and Andrew Hynek   *
**************************************************** */


#include <stdio.h>
#include <stdlib.h>

#include "structs.h"
#include "awake.h"
#include "comm.h"
#include "utils.h"
#include "interpreter.h"
#include "db.h"
#include "olc.h"
#include "boards.h"
#include "screen.h"
#include "memory.h"
#include "handler.h"
#include "constants.h"

void write_mobs_to_disk(int zone);

// extern vars
extern int calc_karma(struct char_data *ch, struct char_data *vict);
extern int max_ability(int i);

extern const char *position_types[];
extern const char *genders[];
extern const char *attack_types[];

// extern funcs
extern char *cleanup(char *dest, const char *src);

// mem func
extern class memoryClass *Mem;

// local defines
#define MOB d->edit_mob
#define NUM_ATTACK_TYPES       28
#define MAX_REG_DESC_LENGTH    160

void medit_disp_menu(struct descriptor_data *d)
{
  int base = calc_karma(NULL, MOB);

  CLS(CH);
  send_to_char(CH, "Mob number: %s%d%s\r\n", CCCYN(CH, C_CMP), d->edit_number,
               CCNRM(CH, C_CMP));
  send_to_char(CH, "1) Keywords: %s%s%s\r\n", CCCYN(CH, C_CMP),
               MOB->player.physical_text.keywords, CCNRM(CH, C_CMP));
  send_to_char(CH, "2) Name: %s%s%s\r\n", CCCYN(CH, C_CMP),
               MOB->player.physical_text.name, CCNRM(CH, C_CMP));
  send_to_char(CH, "3) Room description:\r\n%s%s%s\r\n", CCCYN(CH, C_CMP),
               MOB->player.physical_text.room_desc,CCNRM(CH, C_CMP));
  send_to_char(CH, "4) Look description:\r\n%s\r\n",
               MOB->player.physical_text.look_desc);
  MOB_FLAGS(MOB).PrintBits(buf1, MAX_STRING_LENGTH, action_bits, MOB_MAX);
  send_to_char(CH, "5) Mob Flags: %s%s%s\r\n", CCCYN(CH, C_CMP), buf1,
               CCNRM(CH, C_CMP));
  MOB->char_specials.saved.affected_by.PrintBits(buf1, MAX_STRING_LENGTH,
      affected_bits, AFF_MAX);
  send_to_char(CH, "6) Affected Flags: %s%s%s\r\n", CCCYN(CH, C_CMP), buf1,
               CCNRM(CH, C_CMP));
  send_to_char(CH, "8) Avg. nuyen: %s%6d%s      Avg. credstick value: %s%6d%s\r\n", CCCYN(CH, C_CMP),
               GET_NUYEN(MOB), CCNRM(CH, C_CMP), CCCYN(CH, C_CMP),
               GET_BANK(MOB), CCNRM(CH, C_CMP));
  send_to_char(CH, "9) Bonus karma points: %s%d%s    (Total karma points: %s%d%s)\r\n",
               CCCYN(CH, C_CMP), GET_KARMA(MOB), CCNRM(CH, C_CMP), CCCYN(CH, C_CMP), base, CCNRM(CH, C_CMP));
  send_to_char(CH, "a) Attributes: B(%s%d%s), Q(%s%d%s), S(%s%d%s), C(%s%d%s), "
               "I(%s%d%s), W(%s%d%s), M(%s%d%s), R(%s%d%s)\r\n",
               CCCYN(CH, C_CMP), GET_REAL_BOD(MOB), CCNRM(CH, C_CMP),
               CCCYN(CH, C_CMP), GET_REAL_QUI(MOB), CCNRM(CH, C_CMP),
               CCCYN(CH, C_CMP), GET_REAL_STR(MOB), CCNRM(CH, C_CMP),
               CCCYN(CH, C_CMP), GET_REAL_CHA(MOB), CCNRM(CH, C_CMP),
               CCCYN(CH, C_CMP), GET_REAL_INT(MOB), CCNRM(CH, C_CMP),
               CCCYN(CH, C_CMP), GET_REAL_WIL(MOB), CCNRM(CH, C_CMP),
               CCCYN(CH, C_CMP), MOB->real_abils.mag / 100, CCNRM(CH, C_CMP),
               CCCYN(CH, C_CMP), GET_REAL_REA(MOB), CCNRM(CH, C_CMP));
  send_to_char(CH, "b) Level: %s%d%s\r\n", CCCYN(CH, C_CMP), GET_LEVEL(MOB),
               CCNRM(CH, C_CMP));
  send_to_char(CH, "c) Ballistic: %s%d%s, ", CCCYN(CH, C_CMP), GET_BALLISTIC(MOB),
               CCNRM(CH, C_CMP));
  send_to_char(CH, "d) Impact: %s%d%s\r\n", CCCYN(CH, C_CMP), GET_IMPACT(MOB),
               CCNRM(CH, C_CMP));
  send_to_char(CH, "e) Max physical points: %s%d%s, f) Max mental points: %s%d%s\r\n", CCCYN(CH, C_CMP),
               (int)(GET_MAX_PHYSICAL(MOB) / 100), CCNRM(CH, C_CMP), CCCYN(CH, C_CMP),
               (int)(GET_MAX_MENTAL(MOB) / 100), CCNRM(CH, C_CMP));
  sprinttype(GET_POS(MOB), position_types, buf1);
  send_to_char(CH, "g) Position: %s%s%s, ", CCCYN(CH, C_CMP), buf1,
               CCNRM(CH, C_CMP));
  sprinttype(GET_DEFAULT_POS(MOB), position_types, buf1);
  send_to_char(CH, "h) Default Position: %s%s%s\r\n", CCCYN(CH, C_CMP), buf1,
               CCNRM(CH, C_CMP));
  sprinttype(GET_SEX(MOB), genders, buf1);
  //  strcpy(buf1, genders[GET_SEX(d->edit_mob)]);
  send_to_char(CH, "i) Gender: %s%s%s, ", CCCYN(CH, C_CMP), buf1,
               CCNRM(CH, C_CMP));
  send_to_char(CH, "j) Weight: %s%d%s, ", CCCYN(CH, C_CMP), GET_WEIGHT(MOB),
               CCNRM(CH, C_CMP));
  send_to_char(CH, "k) Height: %s%d%s\r\n", CCCYN(CH, C_CMP), GET_HEIGHT(MOB),
               CCNRM(CH, C_CMP));
  send_to_char(CH, "l) Race: ^c%s^n\r\n", pc_race_types[(int)GET_RACE(MOB)]);
  // gotta subtract TYPE_HIT to make it work properly
  sprinttype(!(MOB->mob_specials.attack_type) ? 0 :
             (MOB->mob_specials.attack_type - TYPE_HIT), attack_types, buf1);
  send_to_char(CH, "m) Attack Type: %s%s%s\r\n", CCCYN(CH, C_CMP), buf1,
               CCNRM(CH, C_CMP));
  send_to_char("n) Skill menu.\r\n", CH);
  send_to_char(CH, "o) Arrive text: ^c%s^n,  p) Leave text: ^c%s^n\r\n",
               MOB->char_specials.arrive, MOB->char_specials.leave);
  send_to_char("q) Quit and save\r\n", CH);
  send_to_char("x) Exit and abort\r\n", CH);
  send_to_char("Enter your choice:\r\n", CH);
  d->edit_mode = MEDIT_MAIN_MENU;
}

void medit_disp_skills(struct descriptor_data *d)
{
  int c, line = 0;

  CLS(CH);
  for (c = 1; c < MAX_SKILLS; c++)
  {
    line++;
    if ((line % 3) == 1)
      sprintf(buf, "%2d) %-20s ", c, skills[c].name);
    else
      sprintf(buf, "%s%2d) %-20s ", buf, c, skills[c].name);
    if (!(line % 3)) {
      sprintf(buf, "%s\r\n", buf);
      send_to_char(buf, CH);
    }
  }
  if ((line % 3) != 0)
  {
    sprintf(buf, "%s\r\nEnter a skill (0 to quit): ", buf);
    send_to_char(buf, CH);
  }
}

void medit_disp_attack_menu(struct descriptor_data *d)
{
  int c;

  CLS(CH);
  for (c = 0; c < (NUM_ATTACK_TYPES - 1); c+= 2)
    send_to_char(CH, "%2d) %-20s %2d) %-20s\r\n", c+1 , attack_types[c],
                 c+2, c+1 < (NUM_ATTACK_TYPES - 1) ? attack_types[c+1] : "");

  send_to_char("Enter attack type: ", CH);
}

void medit_disp_skill_menu(struct descriptor_data *d)
{

  CLS(CH);
  send_to_char(CH, "1) Skill: %s%s%s (%s%d%s)\r\n", CCCYN(CH, C_CMP),
               skills[MOB->mob_specials.mob_skills[0]].name, CCNRM(CH, C_CMP), CCCYN(CH, C_CMP),
               MOB->mob_specials.mob_skills[1], CCNRM(CH, C_CMP));
  send_to_char(CH, "2) Skill: %s%s%s (%s%d%s)\r\n", CCCYN(CH, C_CMP),
               skills[MOB->mob_specials.mob_skills[2]].name, CCNRM(CH, C_CMP), CCCYN(CH, C_CMP),
               MOB->mob_specials.mob_skills[3], CCNRM(CH, C_CMP));
  send_to_char(CH, "3) Skill: %s%s%s (%s%d%s)\r\n", CCCYN(CH, C_CMP),
               skills[MOB->mob_specials.mob_skills[4]].name, CCNRM(CH, C_CMP), CCCYN(CH, C_CMP),
               MOB->mob_specials.mob_skills[5], CCNRM(CH, C_CMP));
  send_to_char(CH, "4) Skill: %s%s%s (%s%d%s)\r\n", CCCYN(CH, C_CMP),
               skills[MOB->mob_specials.mob_skills[6]].name, CCNRM(CH, C_CMP), CCCYN(CH, C_CMP),
               MOB->mob_specials.mob_skills[7], CCNRM(CH, C_CMP));
  send_to_char(CH, "5) Skill: %s%s%s (%s%d%s)\r\n", CCCYN(CH, C_CMP),
               skills[MOB->mob_specials.mob_skills[8]].name, CCNRM(CH, C_CMP), CCCYN(CH, C_CMP),
               MOB->mob_specials.mob_skills[9], CCNRM(CH, C_CMP));
  send_to_char("0) Quit\r\n", CH);

  send_to_char("Enter your choice: ", CH);
}

void medit_disp_mobflags_menu(struct descriptor_data *d)
{

  int c;

  CLS(CH);
  for (c = 0; c < MOB_MAX; c += 2)
  {
    send_to_char(CH, "%2d) %-20s %2d) %-20s\r\n",
                 c + 1, action_bits[c],
                 c + 2, c + 1 < MOB_MAX ?
                 action_bits[c + 1] : "");
  }
  MOB_FLAGS(MOB).PrintBits(buf1, MAX_STRING_LENGTH, action_bits, MOB_MAX);
  send_to_char(CH, "Mob flags: %s%s%s\r\n"
               "Enter mob flag, 0 to quit:", CCCYN(CH, C_CMP),
               buf1, CCNRM(CH, C_CMP));
}

void medit_disp_affected_menu(struct descriptor_data *d)
{

  int c;

  CLS(CH);
  for (c = 0; c < AFF_MAX; c += 2)
  {
    send_to_char(CH, "%2d) %-20s %2d) %-20s\r\n",
                 c + 1, affected_bits[c],
                 c + 2, c + 1 < AFF_MAX ?
                 affected_bits[c + 1] : "");
  }
  AFF_FLAGS(MOB).PrintBits(buf1, MAX_STRING_LENGTH, affected_bits, AFF_MAX);
  send_to_char(CH, "Affected flags: %s%s%s\r\n"
               "Enter affected flag, 0 to quit:", CCCYN(CH, C_CMP),
               buf1, CCNRM(CH, C_CMP));
}

void medit_disp_pos_menu(struct descriptor_data *d)
{
  int c;

  CLS(CH);
  for (c = 1; c < 9; c++)
    send_to_char(CH, "%2d) %-20s\r\n", c, position_types[c]);
}

void medit_disp_gender_menu(struct descriptor_data *d)
{
  send_to_char("1) Neutral\r\n2) Male\r\n3) Female\r\n"
               "\r\nEnter gender: ", CH);
}

void medit_disp_class_menu(struct descriptor_data *d)
{
  CLS(CH);
  for (int c = 1; c < NUM_RACES; c++)
    send_to_char(CH, "%2d) %-20s\r\n", c, pc_race_types[c]);
  send_to_char(CH, "Mob class: ^c%s^n\r\n"
               "Enter mob class, 0 to quit: ", pc_race_types[(int)GET_RACE(MOB)]);
}

void medit_disp_att_menu(struct descriptor_data *d)
{

  CLS(CH);
  send_to_char(CH, "1) Body:       %s%d%s\r\n", CCCYN(CH, C_CMP), GET_REAL_BOD(MOB), CCNRM(CH, C_CMP));
  send_to_char(CH, "2) Quickness:  %s%d%s\r\n", CCCYN(CH, C_CMP), GET_REAL_QUI(MOB), CCNRM(CH, C_CMP));
  send_to_char(CH, "3) Strength:   %s%d%s\r\n", CCCYN(CH, C_CMP), GET_REAL_STR(MOB), CCNRM(CH, C_CMP));
  send_to_char(CH, "4) Charisma:   %s%d%s\r\n", CCCYN(CH, C_CMP), GET_REAL_CHA(MOB), CCNRM(CH, C_CMP));
  send_to_char(CH, "5) Intel.:     %s%d%s\r\n", CCCYN(CH, C_CMP), GET_REAL_INT(MOB), CCNRM(CH, C_CMP));
  send_to_char(CH, "6) Willpower:  %s%d%s\r\n", CCCYN(CH, C_CMP), GET_REAL_WIL(MOB), CCNRM(CH, C_CMP));
  send_to_char(CH, "7) Magic:      %s%d%s\r\n", CCCYN(CH, C_CMP), MOB->real_abils.mag / 100,
               CCNRM(CH, C_CMP));
  send_to_char(CH, "q) Quit\r\n");

  send_to_char("\r\nEnter your choice:\r\n", CH);
  d->edit_mode = MEDIT_ATTRIBUTES;
}


// **************************************************************************
// *  Main Loop                                                            *
// ***************************************************************************

void medit_parse(struct descriptor_data *d, const char *arg)
{

  int number;
  int mob_number;  // the RNUM

  switch(d->edit_mode)
  {
  case MEDIT_CONFIRM_EDIT:
    /* if player hits 'Y' then edit mob */
    switch (*arg) {
    case 'y':
    case 'Y':
      medit_disp_menu(d);
      break;
    case 'n':
    case 'N':
      STATE(d) = CON_PLAYING;
      /* free up the editing mob */
      if (d->edit_mob)
        Mem->DeleteCh(d->edit_mob);
      d->edit_mob = NULL;
      d->edit_number = 0;
      PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
      break;
    default:
      send_to_char("That's not a valid choice!\r\n", CH);
      send_to_char("Do you wish to edit it?\r\n", CH);
      break;
    }
    break;                      /* end of MEDIT_CONFIRM_EDIT */

  case MEDIT_CONFIRM_SAVESTRING:
    switch(*arg) {
    case 'y':
    case 'Y':
      // first write to the internal tables
      if (!from_ip_zone(d->edit_number)) {
        sprintf(buf,"%s wrote new mob #%ld",
                GET_CHAR_NAME(d->character), d->edit_number);
        mudlog(buf, d->character, LOG_WIZLOG, TRUE);
      }
      mob_number = real_mobile(d->edit_number);
      if (mob_number > 0) {
        // first we go through the entire list of mobs to find out
        // which are pointing to this prototype; if it is, it gets
        // replaced
        struct char_data *i, *temp;
        int c;
        for (i = character_list; i; i = i->next) {
          if (mob_number == i->nr) {
            // alloc a temp mobile
            temp = Mem->GetCh();
            *temp = *i;
            *i = *d->edit_mob;
            /* copy game-time dependent vars over */
            i->in_room = temp->in_room;
            i->nr = mob_number;
            i->carrying = temp->carrying;
            i->cyberware = temp->cyberware;
            i->bioware = temp->bioware;
            // any eq worn
            for (c = 0; c < NUM_WEARS; ++c)
              i->equipment[c] = temp->equipment[c];
            i->next_in_room = temp->next_in_room;
            i->next = temp->next;
            i->next_fighting = temp->next_fighting;
            i->followers = temp->followers;
            i->master = temp->master;
            i->char_specials.fighting = temp->char_specials.fighting;
            i->char_specials.hunting = temp->char_specials.hunting;
            i->mob_specials.last_direction = temp->mob_specials.last_direction;
            i->mob_specials.memory = temp->mob_specials.memory;
            i->mob_specials.wait_state = temp->mob_specials.wait_state;
            Mem->ClearCh(temp);
          } // end if statement

        } // end for loop

        // now you can free the old prototype and put in the new one
        DELETE_ARRAY_IF_EXTANT(mob_proto[mob_number].player.physical_text.keywords);
        DELETE_ARRAY_IF_EXTANT(mob_proto[mob_number].player.title);
        DELETE_ARRAY_IF_EXTANT(mob_proto[mob_number].player.physical_text.name);
        DELETE_ARRAY_IF_EXTANT(mob_proto[mob_number].player.physical_text.room_desc);
        DELETE_ARRAY_IF_EXTANT(mob_proto[mob_number].player.physical_text.look_desc);
        DELETE_ARRAY_IF_EXTANT(mob_proto[mob_number].char_specials.arrive);
        DELETE_ARRAY_IF_EXTANT(mob_proto[mob_number].char_specials.leave);

        mob_proto[mob_number] = *d->edit_mob;
        mob_proto[mob_number].nr = mob_number;

      } else {  // if not, we need to make a new spot in the list
        int             counter;
        int             found = FALSE;

        struct char_data *new_mob_proto;
        struct index_data *new_mob_index;
        struct char_data *temp_mob;

        // two because you are adding one and you need one over
        new_mob_index = new struct index_data[top_of_mobt + 2];
        new_mob_proto = new struct char_data[top_of_mobt + 2];
        // count through the tables
        for (counter = 0; counter <= top_of_mobt; counter++) {
          /* if we haven't found it */
          if (!found) {
            /* check if current virtual is bigger than our virtual */
            if (MOB_VNUM_RNUM(counter) > d->edit_number) {
              /* eureka. insert now */
              /*---------*/
              new_mob_index[counter].vnum = d->edit_number;
              new_mob_index[counter].number = 0;
              new_mob_index[counter].func = NULL;
              /*---------*/
              new_mob_proto[counter] = *(d->edit_mob);
              new_mob_proto[counter].in_room = NULL;
              /* it is now safe (and necessary!) to assign real number to
               * the edit_mob, which has been -1 all this time */
              d->edit_mob->nr = counter;
              /* and assign to prototype as well */
              new_mob_proto[counter].nr = counter;
              found = TRUE;
              /* insert the other proto at this point */
              new_mob_index[counter + 1] = mob_index[counter];
              new_mob_proto[counter + 1] = mob_proto[counter];
              new_mob_proto[counter + 1].nr = counter + 1;
            } else {
              /* just copy from old to new, no num change */
              new_mob_proto[counter] = mob_proto[counter];
              new_mob_index[counter] = mob_index[counter];
            }

          } else { // if !found else
            /* we HAVE already found it.. therefore copy to mobile + 1 */
            new_mob_index[counter + 1] = mob_index[counter];
            new_mob_proto[counter + 1] = mob_proto[counter];
            new_mob_proto[counter + 1].nr = counter + 1;
          }
        } // for loop through list


        /* if we STILL haven't found it, means the mobile was > than all
        * the other mobs.. so insert at end */
        if (!found) {
          new_mob_index[top_of_mobt + 1].vnum = d->edit_number;
          new_mob_index[top_of_mobt + 1].number = 0;
          new_mob_index[top_of_mobt + 1].func = NULL;

          clear_char(new_mob_proto + top_of_mobt + 1);
          new_mob_proto[top_of_mobt + 1] = *(d->edit_mob);
          new_mob_proto[top_of_mobt + 1].in_room = NULL;
          new_mob_proto[top_of_mobt + 1].nr = top_of_mobt + 1;
          /* it is now safe (and necessary!) to assign real number to
           * the edit_mob, which has been -1 all this time */
          d->edit_mob->nr = top_of_mobt + 1;
        }
        top_of_mobt++;

        /* we also have to renumber all the mobiles currently    *
        *  existing in the world. This is because when I start   *
        * extracting mobiles, bad things will happen!           */
        for (temp_mob = character_list; temp_mob; temp_mob = temp_mob->next)
          if (GET_MOB_RNUM (temp_mob) >= d->edit_mob->nr)
            GET_MOB_RNUM (temp_mob)++;

        /* free and replace old tables */
        delete [] mob_proto;
        delete [] mob_index;
        mob_proto = new_mob_proto;
        mob_index = new_mob_index;

        /* RENUMBER ZONE TABLES HERE, only              *
        *   because I ADDED a mobile!                   *
        *   This code shamelessly ripped off from db.c */

        int zone, cmd_no;
        for (zone = 0; zone <= top_of_zone_table; zone++)
          for (cmd_no = 0; cmd_no < zone_table[zone].num_cmds; cmd_no++) {
            switch (ZCMD.command) {
            case 'M':
              ZCMD.arg1 = (ZCMD.arg1 >= d->edit_mob->nr ? ZCMD.arg1 + 1 : ZCMD.arg1);
              break;
            }

          }

      } // finally done putting the mobile into memory

      send_to_char("Writing mobile to disk...", CH);
      write_mobs_to_disk(d->character->player_specials->saved.zonenum);

      // again, here we don't delete it cause we don't want to end up
      // deleting the strings from the prototypes
      if (d->edit_mob)
        Mem->ClearCh(d->edit_mob);
      d->edit_mob = NULL;
      STATE(d) = CON_PLAYING;
      PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
      send_to_char("Done.\r\n", d->character);
      break;
    case 'n':
    case 'N':
      send_to_char("Mobile not saved, aborting.\r\n", d->character);
      STATE(d) = CON_PLAYING;
      // complete nuke
      if (d->edit_mob)
        Mem->DeleteCh(d->edit_mob);
      d->edit_mob = NULL;
      d->edit_number = 0;
      d->edit_zone = 0;
      PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
      break;
    default:
      send_to_char("Invalid choice!\r\n", d->character);
      send_to_char("Do you wish to save this mobile internally?\r\n", d->character);
      break;
    }

    break;

  case MEDIT_MAIN_MENU:
    switch (*arg) {
    case 'q':
    case 'Q':
      d->edit_mode = MEDIT_CONFIRM_SAVESTRING;
      medit_parse(d, "y");
      break;
    case 'x':
    case 'X':
      d->edit_mode = MEDIT_CONFIRM_SAVESTRING;
      medit_parse(d, "n");
      break;
    case '1':
      send_to_char("Enter keywords:", CH);
      d->edit_mode = MEDIT_EDIT_NAMELIST;
      break;
    case '2':
      send_to_char("Enter name:", CH);
      d->edit_mode = MEDIT_SHORT_DESCR;
      break;
    case '3':
      send_to_char("Enter room description:", CH);
      d->edit_mode = MEDIT_REG_DESCR;
      DELETE_D_STR_IF_EXTANT(d);
      INITIALIZE_NEW_D_STR(d);
      d->max_str = MAX_REG_DESC_LENGTH;
      d->mail_to = 0;
      break;
    case '4':
      // go to modify.cc
      send_to_char("Enter look description:\r\n", CH);
      d->edit_mode = MEDIT_LONG_DESCR;
      DELETE_D_STR_IF_EXTANT(d);
      INITIALIZE_NEW_D_STR(d);
      d->max_str = MAX_MESSAGE_LENGTH;
      d->mail_to = 0;
      break;
    case '5':
      medit_disp_mobflags_menu(d);
      d->edit_mode = MEDIT_MOB_FLAGS;
      break;
    case '6':
      medit_disp_affected_menu(d);
      d->edit_mode = MEDIT_AFF_FLAGS;
      break;
    case '8':
      send_to_char("Enter average nuyen: ", CH);
      d->edit_mode = MEDIT_NUYEN;
      break;
    case '9':
      send_to_char("Enter bonus karma points: ", CH);
      d->edit_mode = MEDIT_EXPERIENCE;
      break;
    case 'a':
      medit_disp_att_menu(d);
      d->edit_mode = MEDIT_ATTRIBUTES;
      break;
    case 'b':
      send_to_char("Enter level: ", CH);
      d->edit_mode = MEDIT_LEVEL;
      break;
    case 'c':
      send_to_char("Enter ballistic armor points: ", CH);
      d->edit_mode = MEDIT_BALLISTIC;
      break;
    case 'd':
      send_to_char("Enter impact armor points: ", CH);
      d->edit_mode = MEDIT_IMPACT;
      break;
    case 'e':
      send_to_char("Enter max physical points: ", CH);
      d->edit_mode = MEDIT_PHYSICAL;
      break;
    case 'f':
      send_to_char("Enter max mental points: ", CH);
      d->edit_mode = MEDIT_MENTAL;
      break;
    case 'g':
      medit_disp_pos_menu(d);
      send_to_char("Enter position: ", CH);
      d->edit_mode = MEDIT_POSITION;
      break;
    case 'h':
      medit_disp_pos_menu(d);
      send_to_char("Enter default position: ", CH);
      d->edit_mode = MEDIT_DEFAULT_POSITION;
      break;
    case 'i':
      medit_disp_gender_menu(d);
      d->edit_mode = MEDIT_GENDER;
      break;
    case 'j':
      send_to_char("Enter weight (in kilograms): ", CH);
      d->edit_mode = MEDIT_WEIGHT;
      break;
    case 'k':
      send_to_char("Enter height (in centimeters): ", CH);
      d->edit_mode = MEDIT_HEIGHT;
      break;
    case 'l':
      medit_disp_class_menu(d);
      d->edit_mode = MEDIT_CLASS;
      break;
    case 'm':
      medit_disp_attack_menu(d);
      d->edit_mode = MEDIT_ATTACK_TYPE;
      break;
    case 'n':
      medit_disp_skill_menu(d);
      d->edit_mode = MEDIT_SKILLS;
      break;
    case 'o':
      send_to_char("Enter arrive text: ", CH);
      d->edit_mode = MEDIT_ARRIVE_MSG;
      break;
    case 'p':
      send_to_char("Enter leave text: ", CH);
      d->edit_mode = MEDIT_LEAVE_MSG;
      break;

    default:
      medit_disp_menu(d);
      break;
    }
    break;

  case MEDIT_EDIT_NAMELIST:
    DELETE_ARRAY_IF_EXTANT(MOB->player.physical_text.keywords);
    MOB->player.physical_text.keywords = str_dup(arg);
    medit_disp_menu(d);
    break;

  case MEDIT_SHORT_DESCR:
    DELETE_ARRAY_IF_EXTANT(MOB->player.physical_text.name);
    MOB->player.physical_text.name = str_dup(arg);
    medit_disp_menu(d);
    break;

  case MEDIT_ARRIVE_MSG:
    DELETE_ARRAY_IF_EXTANT(MOB->char_specials.arrive);
    MOB->char_specials.arrive = str_dup(arg);
    medit_disp_menu(d);
    break;

  case MEDIT_LEAVE_MSG:
    DELETE_ARRAY_IF_EXTANT(MOB->char_specials.leave);
    MOB->char_specials.leave = str_dup(arg);
    medit_disp_menu(d);
    break;

  case MEDIT_REG_DESCR:
  case MEDIT_LONG_DESCR:
    break;

  case MEDIT_MOB_FLAGS:
    number = atoi(arg);
    if ((number < 0) || (number > MOB_MAX))
      medit_disp_mobflags_menu(d);
    else {
      if (number == 0) // 0 = quit
        medit_disp_menu(d);
      else {
        MOB_FLAGS(MOB).ToggleBit(number-1);
        medit_disp_mobflags_menu(d);
      }
    }
    break;

  case MEDIT_AFF_FLAGS:
    number = atoi(arg);
    if ((number < 0) || (number > AFF_MAX))
      medit_disp_affected_menu(d);
    else {
      if (number == 0) // 0 = quit
        medit_disp_menu(d);
      else {
        AFF_FLAGS(MOB).ToggleBit(number - 1);
        medit_disp_affected_menu(d);
      }
    }
    break;

  case MEDIT_NUYEN:
    number = atoi(arg);
    if ((number < 0) || (number > 999999)) {
      send_to_char("Value must range between 0 and 999999.\r\n", CH);
      send_to_char("Enter average nuyen: ", CH);
    } else {
      GET_NUYEN(MOB) = number;
      send_to_char("Enter average credstick value: ", CH);
      d->edit_mode = MEDIT_CREDSTICK;
    }
    break;

  case MEDIT_CREDSTICK:
    number = atoi(arg);
    if ((number < 0) || (number > 999999)) {
      send_to_char("Value must range between 0 and 999999.\r\n", CH);
      send_to_char("Enter average credstick value: ", CH);
    } else {
      GET_BANK(MOB) = number;
      medit_disp_menu(d);
    }
    break;

  case MEDIT_EXPERIENCE:
    number = atoi(arg);
    if ((number < 0) || (number > 7500)) {
      send_to_char("Value must range between 0 and 7500.\r\n", CH);
      send_to_char("Enter bonus karma points: ", CH);
    } else {
      int karma;
      if (number > (karma=calc_karma(NULL, MOB))) {
        send_to_char("Bonus karma may not be higher than actual karma.  Lowering.\r\n", CH);
        number = karma;
      }
      GET_KARMA(MOB) = number;
      medit_disp_menu(d);
    }
    break;

  case MEDIT_SKILLS:
    switch (*arg) {
    case '0':
      medit_disp_menu(d);
      break;
    case '1':
      medit_disp_skills(d);
      send_to_char("Enter a skill (0 to quit): ", CH);
      d->edit_mode = MEDIT_SKILL1;
      break;
    case '2':
      medit_disp_skills(d);
      send_to_char("Enter a skill (0 to quit): ", CH);
      d->edit_mode = MEDIT_SKILL2;
      break;
    case '3':
      medit_disp_skills(d);
      send_to_char("Enter a skill (0 to quit): ", CH);
      d->edit_mode = MEDIT_SKILL3;
      break;
    case '4':
      medit_disp_skills(d);
      send_to_char("Enter a skill (0 to quit): ", CH);
      d->edit_mode = MEDIT_SKILL4;
      break;
    case '5':
      medit_disp_skills(d);
      send_to_char("Enter a skill (0 to quit): ", CH);
      d->edit_mode = MEDIT_SKILL5;
      break;

    default:
      medit_disp_skill_menu(d);
      break;
    }
    break; // end of MEDIT_SKILLS

  case MEDIT_SKILL1:
    number = atoi(arg);
    if ((number < 0) || (number > MAX_SKILLS)) {
      medit_disp_skills(d);
      send_to_char(CH, "Value must range between 1 and %d.\r\n", MAX_SKILLS);
      send_to_char("Enter a skill (0 to quit): ", CH);
    } else if (number == 0) { // 0 = quit
      medit_disp_skill_menu(d);
      d->edit_mode = MEDIT_SKILLS;
    } else {
      SET_SKILL(MOB, MOB->mob_specials.mob_skills[0], 0)
      MOB->mob_specials.mob_skills[0] = number; // to adjust it
      send_to_char("Enter skill level (0 to quit): ", CH);
      d->edit_mode = MEDIT_SKILL1_VAL;
    }
    break;

  case MEDIT_SKILL2:
    number = atoi(arg);
    if ((number < 0) || (number > MAX_SKILLS)) {
      medit_disp_skills(d);
      send_to_char(CH, "Value must range between 1 and %d.\r\n", MAX_SKILLS);
      send_to_char("Enter a skill (0 to quit): ", CH);
    } else if (number == 0) { // 0 = quit
      medit_disp_skill_menu(d);
      d->edit_mode = MEDIT_SKILLS;
    } else {
      SET_SKILL(MOB, MOB->mob_specials.mob_skills[2], 0)
      MOB->mob_specials.mob_skills[2] = number; // to adjust it
      send_to_char("Enter skill level (0 to quit): ", CH);
      d->edit_mode = MEDIT_SKILL2_VAL;
    }
    break;
  case MEDIT_SKILL3:
    number = atoi(arg);
    if ((number < 0) || (number > MAX_SKILLS)) {
      medit_disp_skills(d);
      send_to_char(CH, "Value must range between 1 and %d.\r\n", MAX_SKILLS);
      send_to_char("Enter a skill (0 to quit): ", CH);
    } else if (number == 0) { // 0 = quit
      medit_disp_skill_menu(d);
      d->edit_mode = MEDIT_SKILLS;
    } else {
      SET_SKILL(MOB, MOB->mob_specials.mob_skills[4], 0)
      MOB->mob_specials.mob_skills[4] = number; // to adjust it
      send_to_char("Enter skill level (0 to quit): ", CH);
      d->edit_mode = MEDIT_SKILL3_VAL;
    }
    break;
  case MEDIT_SKILL4:
    number = atoi(arg);
    if ((number < 0) || (number > MAX_SKILLS)) {
      medit_disp_skills(d);
      send_to_char(CH, "Value must range between 1 and %d.\r\n", MAX_SKILLS);
      send_to_char("Enter a skill (0 to quit): ", CH);
    } else if (number == 0) { // 0 = quit
      medit_disp_skill_menu(d);
      d->edit_mode = MEDIT_SKILLS;
    } else {
      SET_SKILL(MOB, MOB->mob_specials.mob_skills[6], 0)
      MOB->mob_specials.mob_skills[6] = number; // to adjust it
      send_to_char("Enter skill level (0 to quit): ", CH);
      d->edit_mode = MEDIT_SKILL4_VAL;
    }
    break;
  case MEDIT_SKILL5:
    number = atoi(arg);
    if ((number < 0) || (number > MAX_SKILLS)) {
      medit_disp_skills(d);
      send_to_char(CH, "Value must range between 1 and %d.\r\n", MAX_SKILLS);
      send_to_char("Enter a skill (0 to quit): ", CH);
    } else if (number == 0) { // 0 = quit
      medit_disp_skill_menu(d);
      d->edit_mode = MEDIT_SKILLS;
    } else {
      SET_SKILL(MOB, MOB->mob_specials.mob_skills[8], 0)
      MOB->mob_specials.mob_skills[8] = number; // to adjust it
      send_to_char("Enter skill level (0 to quit): ", CH);
      d->edit_mode = MEDIT_SKILL5_VAL;
    }
    break;

  case MEDIT_SKILL1_VAL:
    number = atoi(arg);
    if ((number < 0) || (number > 50)) {
      send_to_char("Skill level must be between 1 and 50.\r\n", CH);
      send_to_char("Enter skill level: ", CH);
    } else if (number == 0) {
      MOB->mob_specials.mob_skills[0] = 0;
      MOB->mob_specials.mob_skills[1] = 0;
      d->edit_mode = MEDIT_SKILLS;
      medit_disp_skill_menu(d);
    } else {
      MOB->mob_specials.mob_skills[1] = number;
      GET_SKILL(MOB, MOB->mob_specials.mob_skills[0]) = MOB->mob_specials.mob_skills[1];
      SET_SKILL(MOB, MOB->mob_specials.mob_skills[0],
                MOB->mob_specials.mob_skills[1]);
      d->edit_mode = MEDIT_SKILLS;
      medit_disp_skill_menu(d);
    }
    break;

  case MEDIT_SKILL2_VAL:
    number = atoi(arg);
    if ((number < 0) || (number > 50)) {
      send_to_char("Skill level must be between 1 and 50.\r\n", CH);
      send_to_char("Enter skill level: ", CH);
    } else if (number == 0) {
      MOB->mob_specials.mob_skills[2] = 0;
      MOB->mob_specials.mob_skills[3] = 0;
      d->edit_mode = MEDIT_SKILLS;
      medit_disp_skill_menu(d);
    } else {
      MOB->mob_specials.mob_skills[3] = number;
      GET_SKILL(MOB, MOB->mob_specials.mob_skills[2]) = MOB->mob_specials.mob_skills[3];
      SET_SKILL(MOB, MOB->mob_specials.mob_skills[2],
                MOB->mob_specials.mob_skills[3]);
      d->edit_mode = MEDIT_SKILLS;
      medit_disp_skill_menu(d);
    }
    break;

  case MEDIT_SKILL3_VAL:
    number = atoi(arg);
    if ((number < 0) || (number > 50)) {
      send_to_char("Skill level must be between 1 and 50.\r\n", CH);
      send_to_char("Enter skill level: ", CH);
    } else if (number == 0) {
      MOB->mob_specials.mob_skills[4] = 0;
      MOB->mob_specials.mob_skills[5] = 0;
      d->edit_mode = MEDIT_SKILLS;
      medit_disp_skill_menu(d);
    } else {
      MOB->mob_specials.mob_skills[5] = number;
      GET_SKILL(MOB, MOB->mob_specials.mob_skills[4]) = MOB->mob_specials.mob_skills[5];
      SET_SKILL(MOB, MOB->mob_specials.mob_skills[4],
                MOB->mob_specials.mob_skills[5]);
      d->edit_mode = MEDIT_SKILLS;
      medit_disp_skill_menu(d);
    }
    break;

  case MEDIT_SKILL4_VAL:
    number = atoi(arg);
    if ((number < 0) || (number > 50)) {
      send_to_char("Skill level must be between 1 and 50.\r\n", CH);
      send_to_char("Enter skill level: ", CH);
    } else if (number == 0) {
      MOB->mob_specials.mob_skills[6] = 0;
      MOB->mob_specials.mob_skills[7] = 0;
      d->edit_mode = MEDIT_SKILLS;
      medit_disp_skill_menu(d);
    } else {
      MOB->mob_specials.mob_skills[7] = number;
      GET_SKILL(MOB, MOB->mob_specials.mob_skills[6]) = MOB->mob_specials.mob_skills[7];
      SET_SKILL(MOB, MOB->mob_specials.mob_skills[6],
                MOB->mob_specials.mob_skills[7]);
      d->edit_mode = MEDIT_SKILLS;
      medit_disp_skill_menu(d);
    }
    break;

  case MEDIT_SKILL5_VAL:
    number = atoi(arg);
    if ((number < 0) || (number > 50)) {
      send_to_char("Skill level must be between 1 and 50.\r\n", CH);
      send_to_char("Enter skill level: ", CH);
    } else if (number == 0) {
      MOB->mob_specials.mob_skills[8] = 0;
      MOB->mob_specials.mob_skills[9] = 0;
      d->edit_mode = MEDIT_SKILLS;
      medit_disp_skill_menu(d);
    } else {
      MOB->mob_specials.mob_skills[9] = number;
      GET_SKILL(MOB, MOB->mob_specials.mob_skills[8]) = MOB->mob_specials.mob_skills[9];
      SET_SKILL(MOB, MOB->mob_specials.mob_skills[8],
                MOB->mob_specials.mob_skills[9]);
      d->edit_mode = MEDIT_SKILLS;
      medit_disp_skill_menu(d);
    }
    break;


  case MEDIT_ATTRIBUTES:
    switch (*arg) {
    case '1':
      send_to_char("Enter body attribute: ", CH);
      d->edit_mode = MEDIT_BOD;
      break;
    case '2':
      send_to_char("Enter quickness attribute: ", CH);
      d->edit_mode = MEDIT_QUI;
      break;
    case '3':
      send_to_char("Enter strength attribute: ", CH);
      d->edit_mode = MEDIT_STR;
      break;
    case '4':
      send_to_char("Enter charisma attribute: ", CH);
      d->edit_mode = MEDIT_CHA;
      break;
    case '5':
      send_to_char("Enter intelligence attribute: ", CH);
      d->edit_mode = MEDIT_INT;
      break;
    case '6':
      send_to_char("Enter willpower attribute: ", CH);
      d->edit_mode = MEDIT_WIL;
      break;
    case '7':
      send_to_char("Enter magic attribute: ", CH);
      d->edit_mode = MEDIT_MAG;
      break;
    case 'q':
    case 'Q': // back to main menu
      medit_disp_menu(d);
      break;
    default:
      medit_disp_att_menu(d);
      break;
    }
    break;

  case MEDIT_BOD:
    number = atoi(arg);
    if ((number < 1) || (number > 50)) {
      send_to_char("Value must range between 1 and 50.\r\n", CH);
      send_to_char("Enter body attribute: ", CH);
    } else {
      GET_REAL_BOD(MOB) = number;
      MOB->real_abils.bod_index = number * 100;
      medit_disp_att_menu(d);
    }
    break;

  case MEDIT_QUI:
    number = atoi(arg);
    if ((number < 1) || (number > 50)) {
      send_to_char("Value must range between 1 and 50.\r\n", CH);
      send_to_char("Enter quickness attribute: ", CH);
    } else {
      GET_REAL_QUI(MOB) = number;
      GET_REAL_REA(MOB) = (number + GET_REAL_INT(MOB)) >> 1;
      medit_disp_att_menu(d);
    }
    break;

  case MEDIT_STR:
    number = atoi(arg);
    if ((number < 1) || (number > 50)) {
      send_to_char("Value must range between 1 and 50.\r\n", CH);
      send_to_char("Enter strength attribute: ", CH);
    } else {
      GET_REAL_STR(MOB) = number;
      medit_disp_att_menu(d);
    }
    break;

  case MEDIT_CHA:
    number = atoi(arg);
    if ((number < 1) || (number > 50)) {
      send_to_char("Value must range between 1 and 50.\r\n", CH);
      send_to_char("Enter charisma attribute: ", CH);
    } else {
      GET_REAL_CHA(MOB) = number;
      medit_disp_att_menu(d);
    }
    break;

  case MEDIT_INT:
    number = atoi(arg);
    if ((number < 1) || (number > 50)) {
      send_to_char("Value must range between 1 and 50.\r\n", CH);
      send_to_char("Enter intelligence attribute: ", CH);
    } else {
      GET_REAL_INT(MOB) = number;
      GET_REAL_REA(MOB) = (number + GET_REAL_QUI(MOB)) >> 1;
      medit_disp_att_menu(d);
    }
    break;

  case MEDIT_WIL:
    number = atoi(arg);
    if ((number < 1) || (number > 50)) {
      send_to_char("Value must range between 1 and 50.\r\n", CH);
      send_to_char("Enter willpower attribute: ", CH);
    } else {
      GET_REAL_WIL(MOB) = number;
      medit_disp_att_menu(d);
    }
    break;

  case MEDIT_MAG:
    number = atoi(arg);
    if ((number < 0) || (number > 50)) {
      send_to_char("Value must range between 0 and 50.\r\n", CH);
      send_to_char("Enter magic attribute: ", CH);
    } else {
      MOB->real_abils.mag = number * 100;
      medit_disp_att_menu(d);
    }
    break;

  case MEDIT_LEVEL:
    number = atoi(arg);
    if (number < 0) {
      send_to_char("Invalid choice.\r\n", CH);
      send_to_char("Enter level: ", CH);
    } else {
      GET_LEVEL(MOB) = number;
      medit_disp_menu(d);
    }
    break;

  case MEDIT_BALLISTIC:
    number = atoi(arg);
    if (number < 0) {
      send_to_char("Value must be greater than 0.\r\n", CH);
      send_to_char("Enter ballistic armor points: ", CH);
    } else {
      GET_BALLISTIC(MOB) = number;
      medit_disp_menu(d);
    }
    break;

  case MEDIT_IMPACT:
    number = atoi(arg);
    if (number < 0) {
      send_to_char("Value must be greater than 0.\r\n", CH);
      send_to_char("Enter impact armor points: ", CH);
    } else {
      GET_IMPACT(MOB) = number;
      medit_disp_menu(d);
    }
    break;

  case MEDIT_PHYSICAL:
    number = atoi(arg);
    if ((number < 0) || (number > 10)) {
      send_to_char("Value must range between 0 and 10.\r\n", CH);
      send_to_char("Enter max physical points: ", CH);
    } else {
      GET_MAX_PHYSICAL(MOB) = number * 100;
      GET_PHYSICAL(MOB) = number * 100;
      medit_disp_menu(d);
    }
    break;

  case MEDIT_MENTAL:
    number = atoi(arg);
    if ((number < 0) || (number > 10)) {
      send_to_char("Value must range between 0 and 10.\r\n", CH);
      send_to_char("Enter max mental points: ", CH);
    } else {
      GET_MAX_MENTAL(MOB) = number * 100;
      GET_MENTAL(MOB) = number * 100;
      medit_disp_menu(d);
    }
    break;

  case MEDIT_WEIGHT:
    number = atoi(arg);
    if (number < 0) {
      send_to_char("Value must be greater than 0.\r\n", CH);
      send_to_char("Enter weight (in kilograms): ", CH);
    } else {
      GET_WEIGHT(MOB) = number;
      medit_disp_menu(d);
    }
    break;

  case MEDIT_HEIGHT:
    number = atoi(arg);
    if (number < 0) {
      send_to_char("Value must be greater than 0.\r\n", CH);
      send_to_char("Enter height (in centimeters): ", CH);
    } else {
      GET_HEIGHT(MOB) = number;
      medit_disp_menu(d);
    }
    break;

  case MEDIT_CLASS:
    number = atoi(arg);
    if ((number < 1) || (number > NUM_RACES))
      medit_disp_class_menu(d);
    else {
      GET_RACE(MOB) = number;
      medit_disp_menu(d);
    }
    break;

  case MEDIT_POSITION:
    number = atoi(arg);
    if ((number < POS_MORTALLYW) || (number > POS_STANDING)) {
      send_to_char("Invalid choice.\r\nEnter position: ", CH);
      medit_disp_pos_menu(d);
    } else {
      GET_POS(MOB) = number;
      medit_disp_menu(d);
    }
    break;

  case MEDIT_DEFAULT_POSITION:
    number = atoi(arg);
    if ((number < POS_MORTALLYW) || (number > POS_STANDING)) {
      send_to_char("Invalid choice.\r\nEnter default position: ", CH);
      medit_disp_pos_menu(d);
    } else {
      GET_DEFAULT_POS(MOB) = number;
      medit_disp_menu(d);
    }
    break;

  case MEDIT_GENDER:
    number = atoi(arg);
    if ((number < 0) || (number > 3)) {
      send_to_char("Invalid choice.\r\n", CH);
      medit_disp_gender_menu(d);
    } else
      if (number != 0) // 0 = quit
        GET_SEX(MOB) = (number - 1);
    medit_disp_menu(d);
    break;

  case MEDIT_ATTACK_TYPE:
    number = atoi(arg);
    if ((number < 0) || (number > NUM_ATTACK_TYPES)) {
      send_to_char("Invalid choice.\r\n", CH);
      medit_disp_attack_menu(d);
    } else
      if (number != 0) // 0 = quit
        MOB->mob_specials.attack_type = number-1 + TYPE_HIT;
    medit_disp_menu(d);
    break;

  }
}

void write_mobs_to_disk(int zone)
{
  int counter, realcounter;
  FILE *fp;
  struct char_data *mob;
  zone = real_zone(zone);
  int i;

  // ideally, this would just fill a VTable with vals...maybe one day

  sprintf(buf, "%s/%d.mob", MOB_PREFIX, zone_table[zone].number);
  fp = fopen(buf, "w+");

  /* start running through all mobiles in this zone */
  for (counter = zone_table[zone].number * 100;
       counter <= zone_table[zone].top;
       counter++) {
    /* write mobile to disk */
    realcounter = real_mobile(counter);

    if (realcounter >= 0) {
      mob = mob_proto+realcounter;
      if (!strcmp("an unfinished mob", GET_NAME(mob)))
        continue;
      fprintf(fp, "#%ld\n", GET_MOB_VNUM(mob));

      fprintf(fp,
              "Keywords:\t%s\n"
              "Name:\t%s\n"
              "RoomDesc:$\n%s\n~\n",
              mob->player.physical_text.keywords?
              mob->player.physical_text.keywords : "mob unnamed",
              mob->player.physical_text.name?
              mob->player.physical_text.name : "An unnamed mob",
              cleanup(buf2, mob->player.physical_text.room_desc?
                      mob->player.physical_text.room_desc : "An unnamed mob is here."));
      fprintf(fp, "LookDesc:$\n%s~\n",
              cleanup(buf2,
                      mob->player.physical_text.look_desc?
                      mob->player.physical_text.look_desc
                      : "An unnamed mob is here."));

      if (mob->char_specials.arrive)
        fprintf(fp, "ArriveMsg:\t%s\n", mob->char_specials.arrive);
      if (mob->char_specials.leave)
        fprintf(fp, "LeaveMsg:\t%s\n", mob->char_specials.leave);

      fprintf(fp,
              "MobFlags:\t%s\n"
              "AffFlags:\t%s\n"
              "Race:\t%s\n"
              "Gender:\t%s\n",
              MOB_FLAGS(mob).ToString(),
              AFF_FLAGS(mob).ToString(),
              pc_race_types[(int)mob->player.race],
              genders[(int)mob->player.sex]);

      if (mob->char_specials.position != POS_STANDING)
        fprintf(fp, "Position:\t%s\n",
                position_types[(int)mob->char_specials.position]);
      if (mob->mob_specials.default_pos)
        fprintf(fp, "DefaultPos:\t%s\n",
                position_types[(int)mob->mob_specials.default_pos]);

      if (mob->mob_specials.attack_type != TYPE_HIT)
        fprintf(fp, "AttackType:\t%s\n",
                attack_types[mob->mob_specials.attack_type-TYPE_HIT]);

      fprintf(fp,
              "[ATTRIBUTES]\n"
              "\tBod:\t%d\n"
              "\tQui:\t%d\n"
              "\tStr:\t%d\n"
              "\tCha:\t%d\n"
              "\tInt:\t%d\n"
              "\tWil:\t%d\n"
              "\tMag:\t%d\n",
              GET_REAL_BOD(mob), GET_REAL_QUI(mob), GET_REAL_STR(mob),
              GET_REAL_CHA(mob), GET_REAL_INT(mob), GET_REAL_WIL(mob),
              mob->real_abils.mag / 100);
      fprintf(fp,
              "[POINTS]\n"
              "\tHeight:\t%d\n"
              "\tWeight:\t%d\n",
              mob->player.height, mob->player.weight);

      if (GET_LEVEL(mob) > 0)
        fprintf(fp, "\tLevel:\t%d\n", GET_LEVEL(mob));
      if (int(GET_MAX_PHYSICAL(mob) / 100) != 10)
        fprintf(fp, "\tMaxPhys:\t%d\n", int(GET_MAX_PHYSICAL(mob) / 100));
      if (int(GET_MAX_MENTAL(mob) / 100) != 10)
        fprintf(fp, "\tMaxMent:\t%d\n", int(GET_MAX_MENTAL(mob) / 100));
      if (GET_BALLISTIC(mob) > 0)
        fprintf(fp, "\tBallistic:\t%d\n", GET_BALLISTIC(mob));
      if (GET_IMPACT(mob) > 0)
        fprintf(fp, "\tImpact:\t%d\n", GET_IMPACT(mob));
      if (GET_NUYEN(mob) > 0)
        fprintf(fp, "\tCash:\t%ld\n", GET_NUYEN(mob));
      if (GET_BANK(mob) > 0)
        fprintf(fp, "\tBank:\t%ld\n", GET_BANK(mob));
      if (GET_KARMA(mob) > 0)
        fprintf(fp, "\tKarma:\t%d\n", GET_KARMA(mob));
      fprintf(fp, "[SKILLS]\n");

      for (i = 0; i <= 8; i = i +2)
        if (mob->mob_specials.mob_skills[i])
          fprintf(fp, "\t%s:\t%d\n", skills[mob->mob_specials.mob_skills[i]].name,
                  mob->mob_specials.mob_skills[i+1]);



      fprintf(fp, "BREAK\n");
    } // close if statement
  } // close for loop

  fprintf(fp, "END\n");
  fclose(fp);

  write_index_file("mob");
}

