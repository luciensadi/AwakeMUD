#include <stdlib.h>
#include <time.h>

#include "otaku.hpp"
#include "structs.hpp"
#include "awake.hpp"
#include "db.hpp"
#include "comm.hpp"
#include "interpreter.hpp"
#include "handler.hpp"
#include "utils.hpp"
#include "screen.hpp"
#include "constants.hpp"
#include "olc.hpp"
#include "config.hpp"
#include "ignore_system.hpp"
#include "newdb.hpp"
#include "playerdoc.hpp"
#include "newhouse.hpp"
#include "quest.hpp"

extern struct otaku_echo echoes[];

long calculate_sub_nuyen_cost(int desired_grade) {
  if (desired_grade >= 6)
    return 825000;
  else
    return 25000 + (25000 * (1 << (desired_grade - 1)));
}

bool submersion_cost(struct char_data *ch, bool spend)
{
  int desired_grade = GET_GRADE(ch) + 1;
  long karmacost = (desired_grade + 5) * 300;
  long nuyencost = calculate_sub_nuyen_cost(desired_grade);

  if (karmacost < 0) {
    send_to_char("Congratulations, you've hit the ceiling! You can't increase submersion until the code is fixed to allow someone at your rank to do so.\r\n", ch);
    mudlog_vfprintf(ch, LOG_SYSLOG, "lol init capped at %d for %s, karma overflowed... this game was not designed for that init level", GET_GRADE(ch), GET_CHAR_NAME(ch));
    return FALSE;
  }

  long tke = 0;
  if (karmacost > GET_KARMA(ch) || (GET_KARMA(ch) - karmacost) > GET_KARMA(ch)) {
    send_to_char(ch, "You do not have enough karma to increase submersion. It will cost you %.2f karma.\r\n", ((float) karmacost / 100));
    return FALSE;
  }
  if (nuyencost > GET_NUYEN(ch) || (GET_NUYEN(ch) - nuyencost) > GET_NUYEN(ch)) {
    send_to_char(ch, "You do not have enough nuyen to increase submersion. It will cost you %ld nuyen.\r\n", nuyencost);
    return FALSE;
  }

  switch (desired_grade) {
    case 1:
      tke = 0;
      break;
    case 2:
      tke = 100;
      break;
    case 3:
      tke = 200;
      break;
    default:
      tke = 200 + ((GET_GRADE(ch)-2)*200);
      break;
  }
  if (tke > GET_TKE(ch)) {
    send_to_char(ch, "You do not have high enough TKE to initiate. You need %ld TKE.\r\n", tke);
    return FALSE;
  }
  if (spend) {
    lose_nuyen(ch, nuyencost, NUYEN_OUTFLOW_INITIATION);
    GET_KARMA(ch) -= karmacost;
  }
  return TRUE;
}

void disp_submersion_menu(struct descriptor_data *d)
{
  CLS(CH);
  send_to_char("1) Increase submersion grade\r\n"
               "2) Learn a new echo\r\n"
               "3) Return to game\r\n"
               "Enter initiation option: ", CH);
  d->edit_mode = SUBMERSION_MAIN;
}

ACMD(do_submerse)
{
  FAILURE_CASE(!IS_OTAKU(ch), "Sorry, submersions are for otaku characters only.");

  skip_spaces(&argument);

  if (subcmd == SCMD_SUBMERSE && submersion_cost(ch, FALSE)) {
    // Enforce grade restrictions. We can't do this init_cost since it's used elsewhere.
    if ((GET_GRADE(ch) + 1) > SUBMERSION_CAP) {
      send_to_char("Congratulations, you've reached the submersion cap! You're not able to advance further.\r\n", ch);
      return;
    }

    // Check to see that they can afford it. This sends its own message.
    if (!submersion_cost(ch, FALSE)) {
      return;
    }
    STATE(ch->desc) = CON_SUBMERSION;
    PLR_FLAGS(ch).SetBit(PLR_SUBMERSION);
    disp_submersion_menu(ch->desc);
    return;
  }
}

bool can_select_echo(struct char_data *ch, int i)
{
  // Count our grades vs selected echos so we can't learn more than we have grades
  int available_echoes = GET_GRADE(ch);
  for (int i = 1; i < ECHO_MAX; i++) {
    available_echoes -= GET_ECHO(ch, i);
  }
  if (available_echoes <= 0)
    return FALSE;
  // incremental echoes can be selected an arbitrary number of times
  if (echoes[i].incremental)
    return TRUE;
  // otherwise static echoes only once
  if (GET_ECHO(ch, i))
    return FALSE;
  return TRUE;
}

void disp_echo_menu(struct descriptor_data *d)
{
  CLS(CH);
  for (int i = 1; i < ECHO_MAX; i++) {
    if (PRF_FLAGGED(CH, PRF_SCREENREADER)) {
      send_to_char(CH, "%d) %s%s^n\r\n", i, echoes[i].name, can_select_echo(CH, i) ? " (can learn)" : " (cannot learn)");
    } else {
      send_to_char(CH, "%d) %s%s^n\r\n", i, can_select_echo(CH, i) ? "" : "^r", echoes[i].name);
    }
  }

  send_to_char("q) Quit \r\nSelect echo to learn: ", CH);
  d->edit_mode = SUBMERSION_ECHO;
}

void submersion_parse(struct descriptor_data *d, char *arg)
{
  struct obj_data *obj = NULL;
  int number, i;
  switch (d->edit_mode)
  {
    case SUBMERSION_MAIN:
      switch (*arg)
      {
        case '1':
          submersion_cost(CH, FALSE);
          send_to_char("Are you sure you want to increase your submersion? Type 'y' to continue, anything else to abort.\r\n", CH);
          d->edit_mode = SUBMERSION_CONFIRM;
          break;
        case '2':
          disp_echo_menu(d);
          break;
        case '3':
          STATE(d) = CON_PLAYING;
          PLR_FLAGS(CH).RemoveBit(PLR_INITIATE);
          send_to_char("Submersion cancelled.\r\n", CH);
          break;
        default:
          disp_submersion_menu(d);
          break;
      }
      break;
    case SUBMERSION_CONFIRM:
      if (*arg == 'y') {
        GET_GRADE(CH)++;
        send_to_char(CH, "You feel yourself grow closer to the resonance, opening new echoes for you to learn.\r\n");
        STATE(d) = CON_PLAYING;
        PLR_FLAGS(CH).RemoveBit(PLR_SUBMERSION);
      } else {
        disp_submersion_menu(d);
      }
      break;   
    case SUBMERSION_ECHO:
      // learning a new echo here
      number = atoi(arg);
      if (number >= ECHO_MAX) 
        send_to_char("Invalid Response. Select another echo to unlock: ", CH);
      else if (!can_select_echo(CH, number))
        send_to_char("You aren't able to learn that echo at this time. Select another echo to learn: ", CH);
      else {
        SET_ECHO(CH, number, GET_ECHO(CH, number) + 1);
        send_to_char(CH, "New ways to manipulate the resonance open up before you as you learn %s.\r\n", echoes[number].name);
        STATE(d) = CON_PLAYING;
        PLR_FLAGS(CH).RemoveBit(PLR_SUBMERSION);
      }
      break;   
  }
}