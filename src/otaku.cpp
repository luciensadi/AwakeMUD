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
#include "newmatrix.hpp"

extern struct otaku_echo echoes[];

int complex_form_programs[COMPLEX_FORM_TYPES] = {
  SOFT_ARMOR,
  SOFT_ATTACK,
  SOFT_BATTLETEC,
  SOFT_CLOAK,
  SOFT_COMPRESSOR,
  SOFT_LOCKON,
  SOFT_SLEAZE,
  SOFT_SLOW,  
  SOFT_TRACK,
  SOFT_SHIELD,
  SOFT_RADIO
};

int get_otaku_cha(struct char_data *ch) {
  int cha_stat = GET_REAL_CHA(ch);

  /* Handling Drugs */
  int detox_force = affected_by_spell(ch, SPELL_DETOX);
  if (GET_DRUG_STAGE(ch, DRUG_NOVACOKE) == DRUG_STAGE_ONSET && !IS_DRUG_DETOX(DRUG_NOVACOKE, detox_force))
    cha_stat++;
  if (GET_DRUG_STAGE(ch, DRUG_NOVACOKE) == DRUG_STAGE_COMEDOWN && !IS_DRUG_DETOX(DRUG_NOVACOKE, detox_force))
    cha_stat = 1;

  return cha_stat;
}

int get_otaku_wil(struct char_data *ch) {
  int wil_stat = GET_REAL_WIL(ch);

  /* Handling Cyberware/Bioware */
  struct obj_data *pain_editor = find_bioware(ch, BIO_PAINEDITOR);
  if (pain_editor && GET_BIOWARE_IS_ACTIVATED(pain_editor))
    wil_stat++;

  /* Handling Drugs */
  int detox_force = affected_by_spell(ch, SPELL_DETOX);
  if (GET_DRUG_STAGE(ch, DRUG_KAMIKAZE) == DRUG_STAGE_ONSET && !IS_DRUG_DETOX(DRUG_KAMIKAZE, detox_force))
    wil_stat++;
  if (GET_DRUG_STAGE(ch, DRUG_NITRO) == DRUG_STAGE_ONSET && !IS_DRUG_DETOX(DRUG_NITRO, detox_force))
    wil_stat += 2;
  if (GET_DRUG_STAGE(ch, DRUG_ZEN) == DRUG_STAGE_ONSET && !IS_DRUG_DETOX(DRUG_ZEN, detox_force))
    wil_stat++;
  
  if (GET_DRUG_STAGE(ch, DRUG_KAMIKAZE) == DRUG_STAGE_COMEDOWN && !IS_DRUG_DETOX(DRUG_KAMIKAZE, detox_force))
    wil_stat--;
  if (GET_DRUG_STAGE(ch, DRUG_NOVACOKE) == DRUG_STAGE_COMEDOWN && !IS_DRUG_DETOX(DRUG_NOVACOKE, detox_force))
    wil_stat /= 2;

  return wil_stat;
}

int get_otaku_int(struct char_data *ch) {
  int int_stat = GET_REAL_INT(ch);

  /* Handling Cyberware/Bioware */
  struct obj_data *booster = find_bioware(ch, BIO_CEREBRALBOOSTER);
  if (booster)
    int_stat += GET_BIOWARE_RATING(booster);

  struct obj_data *pain_editor = find_bioware(ch, BIO_PAINEDITOR);
  if (pain_editor && GET_BIOWARE_IS_ACTIVATED(pain_editor))
    int_stat--;

  /* Handling Drugs */
  int detox_force = affected_by_spell(ch, SPELL_DETOX);
  if (GET_DRUG_STAGE(ch, DRUG_PSYCHE) == DRUG_STAGE_ONSET && !IS_DRUG_DETOX(DRUG_PSYCHE, detox_force))
    int_stat++;

  return int_stat;
}

int get_otaku_qui(struct char_data *ch) {
  int qui_stat = GET_REAL_QUI(ch);

  /* Handling Cyberware/Bioware */
  struct obj_data *move_by_wire = find_cyberware(ch, CYB_MOVEBYWIRE);
  if (move_by_wire)
    qui_stat += GET_CYBERWARE_RATING(move_by_wire);
  struct obj_data *suprathyroid = find_bioware(ch, BIO_SUPRATHYROIDGLAND);
  if (suprathyroid)
    qui_stat += 1;

  /* Handling Drugs */
  int detox_force = affected_by_spell(ch, SPELL_DETOX);
  if (GET_DRUG_STAGE(ch, DRUG_JAZZ) == DRUG_STAGE_ONSET && !IS_DRUG_DETOX(DRUG_JAZZ, detox_force))
    qui_stat += 2;
  if (GET_DRUG_STAGE(ch, DRUG_KAMIKAZE) == DRUG_STAGE_ONSET && !IS_DRUG_DETOX(DRUG_KAMIKAZE, detox_force))
    qui_stat++;

  if (GET_DRUG_STAGE(ch, DRUG_JAZZ) == DRUG_STAGE_COMEDOWN && !IS_DRUG_DETOX(DRUG_JAZZ, detox_force))
    qui_stat--;
  if (GET_DRUG_STAGE(ch, DRUG_KAMIKAZE) == DRUG_STAGE_COMEDOWN && !IS_DRUG_DETOX(DRUG_KAMIKAZE, detox_force))
    qui_stat--;

  return qui_stat;
}

int get_otaku_rea(struct char_data *ch) {
  int rea_stat = (get_otaku_int(ch) + get_otaku_qui(ch)) / 2;

  /* Handling Cyberware/Bioware */
  struct obj_data *boosted_reflexes = find_cyberware(ch, CYB_BOOSTEDREFLEXES);
  if (boosted_reflexes)
    rea_stat += MAX(0, GET_CYBERWARE_RATING(boosted_reflexes) - 1);
  struct obj_data *move_by_wire = find_cyberware(ch, CYB_MOVEBYWIRE);
  if (move_by_wire)
    rea_stat += GET_CYBERWARE_RATING(move_by_wire) * 2;
  struct obj_data *reaction_enhancer = find_cyberware(ch, CYB_REACTIONENHANCE);
  if (reaction_enhancer) 
    rea_stat += GET_CYBERWARE_RATING(reaction_enhancer);
  struct obj_data *wired_reflexes = find_cyberware(ch, CYB_WIREDREFLEXES);
  if (wired_reflexes)
    rea_stat += GET_CYBERWARE_RATING(wired_reflexes) * 2;
  struct obj_data *suprathyroid = find_bioware(ch, BIO_SUPRATHYROIDGLAND);
  if (suprathyroid)
    rea_stat += 1;

  int detox_force = affected_by_spell(ch, SPELL_DETOX);
  /* Handling Drugs */
  if (GET_DRUG_STAGE(ch, DRUG_BLISS) == DRUG_STAGE_ONSET && !IS_DRUG_DETOX(DRUG_BLISS, detox_force))
    rea_stat -= 1;
  if (GET_DRUG_STAGE(ch, DRUG_NOVACOKE) == DRUG_STAGE_ONSET && !IS_DRUG_DETOX(DRUG_NOVACOKE, detox_force))
    rea_stat += 1;
  if (GET_DRUG_STAGE(ch, DRUG_ZEN) == DRUG_STAGE_ONSET && !IS_DRUG_DETOX(DRUG_ZEN, detox_force))
    rea_stat -= 2;
  if (GET_DRUG_STAGE(ch, DRUG_CRAM) == DRUG_STAGE_ONSET && !IS_DRUG_DETOX(DRUG_CRAM, detox_force))
    rea_stat += 1;

  return rea_stat;
}

extern struct obj_data *make_new_finished_part(int part_type, int mpcp, int rating=0);
struct obj_data *make_otaku_deck(struct char_data *ch) {
  if (!ch) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "Tried to create an otaku deck, but provided invalid character.");
    return NULL;
  }
  struct obj_data *asist = find_cyberware(ch, CYB_ASIST);
  if (!asist) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "Tried to create an otaku deck, but otaku somehow didn't have an asist interface.");
    return NULL;
  }

  struct obj_data *new_deck = read_object(OBJ_CUSTOM_CYBERDECK_SHELL, VIRTUAL, OBJ_LOAD_REASON_OTAKU_RESONANCE);

  // Add parts.
  int mpcp = GET_OTAKU_MPCP(ch);

  // Real values are assigned in update_otaku_deck()
  obj_to_obj(make_new_finished_part(PART_MPCP, mpcp, mpcp), new_deck);
  obj_to_obj(make_new_finished_part(PART_BOD, mpcp, 1), new_deck);
  obj_to_obj(make_new_finished_part(PART_EVASION, mpcp, 1), new_deck);
  obj_to_obj(make_new_finished_part(PART_SENSOR, mpcp, 1), new_deck);
  obj_to_obj(make_new_finished_part(PART_MASKING, mpcp, 1), new_deck);
  obj_to_obj(make_new_finished_part(PART_ASIST_HOT, mpcp), new_deck);
  obj_to_obj(make_new_finished_part(PART_RAS_OVERRIDE, mpcp), new_deck);
  if (GET_ECHO(ch, ECHO_NEUROFILTER)) {
    obj_to_obj(make_new_finished_part(PART_ICCM, mpcp), new_deck);
  }

  update_otaku_deck(ch, new_deck);
  GET_CYBERDECK_IS_INCOMPLETE(new_deck) = FALSE;

  new_deck->obj_flags.extra_flags.SetBit(ITEM_EXTRA_NOSELL);
  new_deck->obj_flags.extra_flags.SetBit(ITEM_EXTRA_CONCEALED_IN_EQ);
  new_deck->obj_flags.extra_flags.SetBit(ITEM_EXTRA_OTAKU_RESONANCE);
  
  // Enumerating by counter is wasteful but will ensure we only lead one of each type of complex form
  for (int counter = 0; counter < COMPLEX_FORM_TYPES; counter++) {
    struct obj_data *best_form = NULL;
    // This for loop checks to see if we have a complex form for the counter
    // then it also checks if it's the BEST form we have
    for (struct obj_data *form = asist->contains; form; form = form->next_content) {
      if (GET_OBJ_TYPE(form) != ITEM_COMPLEX_FORM) continue;
      if (GET_COMPLEX_FORM_PROGRAM(form) != complex_form_programs[counter]) continue; // Not the correct program
      if (GET_COMPLEX_FORM_LEARNING_TICKS_LEFT(form) > 0) continue; // The complex form is in unfinished.
      if (GET_COMPLEX_FORM_RATING(form) > mpcp) continue; // Can't load complex forms greater than mpcp rating.

      if (best_form && GET_COMPLEX_FORM_RATING(form) < GET_COMPLEX_FORM_RATING(best_form))
        continue;
      best_form = form;
    }

    if (!best_form) continue; // We don't have this form
    struct obj_data *active = read_object(OBJ_BLANK_PROGRAM, VIRTUAL, OBJ_LOAD_REASON_OTAKU_RESONANCE);
    GET_PROGRAM_TYPE(active) = GET_COMPLEX_FORM_PROGRAM(best_form);
    GET_PROGRAM_SIZE(active) = 0; // Complex forms don't take up memory.
    GET_PROGRAM_ATTACK_DAMAGE(active) = GET_COMPLEX_FORM_WOUND_LEVEL(best_form);
    GET_PROGRAM_IS_DEFAULTED(active) = TRUE;
    GET_OBJ_TIMER(active) = 1;

    GET_PROGRAM_RATING(active) = GET_COMPLEX_FORM_RATING(best_form);

    // Cyberadepts get +1 to Complex Forms
    if (GET_OTAKU_PATH(ch) == OTAKU_PATH_CYBERADEPT) GET_PROGRAM_RATING(active) += 1;

    active->obj_flags.extra_flags.SetBit(ITEM_EXTRA_OTAKU_RESONANCE);
    active->obj_flags.extra_flags.SetBit(ITEM_EXTRA_NOSELL);

    active->restring = str_dup(GET_OBJ_NAME(best_form));
    obj_to_obj(active, new_deck);
  }

  char restring[500];
  snprintf(restring, sizeof(restring), "a living persona cyberdeck");
  new_deck->restring = str_dup(restring);

  return new_deck;
}

void _update_living_persona(struct char_data *ch, struct obj_data *cyberdeck, int mpcp) {
  if (!ch->persona || !ch->persona->decker) return;
  if (ch->persona->type != ICON_LIVING_PERSONA) return;

  ch->persona->decker->mpcp = mpcp;
  ch->persona->decker->hardening = GET_CYBERDECK_HARDENING(cyberdeck);
  ch->persona->decker->response = GET_CYBERDECK_RESPONSE_INCREASE(cyberdeck);

  for (struct obj_data *part = cyberdeck->contains; part; part = part->next_content) {
    if (GET_OBJ_TYPE(part) != ITEM_PART) continue;
    switch (GET_OBJ_VAL(part, 0)) {
      case PART_MPCP:
        GET_PART_RATING(part) = mpcp;
        break;
      case PART_BOD:
        ch->persona->decker->bod = MIN(mpcp * 1.5, GET_PART_RATING(part) + GET_ECHO(ch, ECHO_PERSONA_BOD));
        break;
      case PART_EVASION:
        ch->persona->decker->evasion = MIN(mpcp * 1.5, GET_PART_RATING(part) + GET_ECHO(ch, ECHO_PERSONA_EVAS));
        break;
      case PART_SENSOR:
        ch->persona->decker->sensor = MIN(mpcp * 1.5, GET_PART_RATING(part) + GET_ECHO(ch, ECHO_PERSONA_SENS));
        break;
      case PART_MASKING:
        ch->persona->decker->masking = MIN(mpcp * 1.5, GET_PART_RATING(part) + GET_ECHO(ch, ECHO_PERSONA_MASK));
        break;
    }
  }
}

void update_otaku_deck(struct char_data *ch, struct obj_data *cyberdeck) {
  int mpcp = GET_OTAKU_MPCP(ch);

  GET_CYBERDECK_MPCP(cyberdeck) = MIN(12, mpcp);
  GET_CYBERDECK_HARDENING(cyberdeck) = (get_otaku_wil(ch) + 1) / 2 + GET_ECHO(ch, ECHO_IMPROVED_HARD);
  GET_CYBERDECK_HARDENING(cyberdeck) = MIN(get_otaku_wil(ch), GET_CYBERDECK_HARDENING(cyberdeck));
  GET_CYBERDECK_ACTIVE_MEMORY(cyberdeck) = 0; // Otaku do not have active memory.
  GET_CYBERDECK_TOTAL_STORAGE(cyberdeck) = 0;
  // Otaku don't have response increase, so use this for matrix reaction instead
  GET_CYBERDECK_RESPONSE_INCREASE(cyberdeck) = get_otaku_rea(ch) + GET_ECHO(ch, ECHO_IMPROVED_REA);
  GET_CYBERDECK_RESPONSE_INCREASE(cyberdeck) = MIN(mpcp * 1.5, GET_CYBERDECK_RESPONSE_INCREASE(cyberdeck));
  GET_CYBERDECK_IO_RATING(cyberdeck) = (get_otaku_int(ch) * 100) + (GET_ECHO(ch, ECHO_IMPROVED_IO) * 100);

  for (struct obj_data *part = cyberdeck->contains; part; part = part->next_content) {
    if (GET_OBJ_TYPE(part) != ITEM_PART) continue;
    GET_PART_TARGET_MPCP(part) = mpcp;
    switch (GET_OBJ_VAL(part, 0)) {
      case PART_MPCP:
        GET_PART_RATING(part) = mpcp;
        break;
      case PART_BOD:
        GET_PART_RATING(part) = get_otaku_wil(ch);
        break;
      case PART_EVASION:
        GET_PART_RATING(part) = get_otaku_int(ch);
        break;
      case PART_SENSOR:
        GET_PART_RATING(part) = get_otaku_int(ch);
        break;
      case PART_MASKING:
        GET_PART_RATING(part) = (get_otaku_wil(ch) + get_otaku_cha(ch) + 1) / 2;
        break;
    }
  }

  _update_living_persona(ch, cyberdeck, mpcp);
}

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

  // Enforce grade restrictions. We can't do this init_cost since it's used elsewhere.
  if ((GET_GRADE(ch) + 1) > SUBMERSION_CAP) {
    send_to_char("Congratulations, you've reached the submersion cap! You're not able to advance further.\r\n", ch);
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

int get_free_echoes(struct char_data *ch)
{
  int available_echoes = GET_GRADE(ch);
  for (int i = 1; i < ECHO_MAX; i++) {
    available_echoes -= GET_ECHO(ch, i);
  }
  return available_echoes;
}

void disp_submersion_menu(struct descriptor_data *d)
{
  CLS(CH);
  send_to_char(CH, "Deep in the resonance you focus on becoming closer to the matrix:\r\n"
               " 1) Increase submersion grade (%s)\r\n"
               " 2) Learn a new echo (%d echoes available)\r\n"
               " 3) Return to reality\r\n"
               "Enter submersion option: \r\r\r\n",
               submersion_cost(CH, FALSE) ? "^gavailable^n" : "^runavailable^n",
               get_free_echoes(CH));
  d->edit_mode = SUBMERSION_MAIN;
}

ACMD(do_submerse)
{
  FAILURE_CASE(!IS_OTAKU(ch), "Sorry, submersions are for otaku characters only.");

  STATE(ch->desc) = CON_SUBMERSION;
  PLR_FLAGS(ch).SetBit(PLR_SUBMERSION);
  disp_submersion_menu(ch->desc);
}

bool can_select_echo(struct char_data *ch, int i)
{
  // Count our grades vs selected echos so we can't learn more than we have grades
  int available_echoes = get_free_echoes(ch);
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
      send_to_char(CH, "%d) %s%s%s^n\r\n", i, echoes[i].name, can_select_echo(CH, i) ? " (can learn)" : " (cannot learn)", echoes[i].nerps ? " ^Y(NERPS)^n" : "");
    } else {
      send_to_char(CH, "%d) %s%s%s^n\r\n", i, can_select_echo(CH, i) ? "" : "^r", echoes[i].name, echoes[i].nerps ? " ^Y(NERPS)^n" : "");
    }
  }

  send_to_char(CH, "q) Quit \r\nYou have ^w%d^n echoes available to allocate. Select echo to learn: ", get_free_echoes(CH));
  d->edit_mode = SUBMERSION_ECHO;
}

void submersion_parse(struct descriptor_data *d, char *arg)
{
  int number;
  switch (d->edit_mode)
  {
    case SUBMERSION_MAIN:
      switch (*arg)
      {
        case '1':
          send_to_char("Are you sure you want to increase your submersion? Type 'y' to continue, anything else to abort.\r\n", CH);
          d->edit_mode = SUBMERSION_CONFIRM;
          break;
        case '2':
          disp_echo_menu(d);
          break;
        case '3':
          STATE(d) = CON_PLAYING;
          PLR_FLAGS(CH).RemoveBit(PLR_SUBMERSION);
          send_to_char("Submersion cancelled.\r\n", CH);
          break;
        default:
          disp_submersion_menu(d);
          break;
      }
      break;
    case SUBMERSION_CONFIRM:
      if (*arg == 'y') {
        if (!submersion_cost(CH, TRUE)) { // Actually spends the points for submersion
          send_to_char("Returning to main menu.", CH);
          d->edit_mode = SUBMERSION_MAIN;
          return;
        }
        GET_GRADE(CH)++;
        send_to_char(CH, "You feel yourself grow closer to the resonance, opening new echoes for you to learn.\r\n");
        send_to_char("Returning to main menu.", CH);
        d->edit_mode = SUBMERSION_MAIN;
      } else {
        disp_submersion_menu(d);
      }
      break;   
    case SUBMERSION_ECHO:
      // learning a new echo here
      number = atoi(arg);
      if (*arg == 'q') {
        send_to_char("Returning to main menu.", CH);
        d->edit_mode = SUBMERSION_MAIN;
      }
      else if (number >= ECHO_MAX) 
        send_to_char("Invalid Response. Select another echo to unlock: ", CH);
      else if (!can_select_echo(CH, number))
        send_to_char("You aren't able to learn that echo at this time. Select another echo to learn: ", CH);
      else {
        SET_ECHO(CH, number, GET_ECHO(CH, number) + 1);
        send_to_char(CH, "New ways to manipulate the resonance open up before you as you learn %s.\r\n", echoes[number].name);
        send_to_char("Returning to main menu.", CH);
        d->edit_mode = SUBMERSION_MAIN;
      }
      break;   
  }
}