#include "structs.hpp"
#include "interpreter.hpp"
#include "handler.hpp"
#include "constants.hpp"
#include "newdb.hpp"
#include "db.hpp"
#include "chipjacks.hpp"
#include "matrix_storage.hpp"

ACMD_DECLARE(do_reload);

/* Chipjack Expert Drivers (.1 ess/rating, 5k nuyen/rating, avail 4/48, SI 1, Legal):
   - give you a Task Pool equal to its rating to be used with the skill on the chip
   - must be purchased separately for each chipjack

   Use a command to link / unlink an expert driver to your skills.
   If you have an unlinked driver and a skill calls for one, auto-link it.
   If you unjack a skill, unlink the driver.
*/
// bool has_expert_driver(struct char_data *ch) {}

/* Chipjack: Default allows datasoft and knowsoft usage. Add skillwires to use activesofts. One chip per jack, jack count unlimited. */

/* Knowsoft Link: This link gives the user mental access to any knowsofts downloaded into headware memory or piped through a datajack. */

/* Skillwires (.2 ess/rating, MP * Rating * 500 nuyen, rating/10d, 1, legal):
   - Enable activesofts
   - Skillwire rating = max skillsoft rating
   - Cost determined by skillwire rating and max skill MP size
   - Controls TOTAL rating and TOTAL size that you can have active at once (rating 6 can hold one rating 6, or 2 rating 3s, etc)
*/

/* Skillsofts: 
   - Suppress any natural skill in this field the character has
   - Does not provide associated dice pools (no combat pool etc)
   - Can be coupled with expert driver to get task pool
   - Can be copied to memory and discarded
   
   KNOWSOFT: Direct chipjack slot, or from memory via a knowsoft link

   ACTIVESOFT:
   - Requires skillwire system with at least enough spare rating and size to activate
   - Can be loaded from chipjack, datajack, or headware memory
   - No magic skills (sorcery, conjuring, etc)
*/

/* Skillsoft Jukebox:
   A portable computer that you slot skillsofts into. Connect via cable to datajack/chipjack.
   Switches between skillsofts quickly (or at least quicker than unjack/rejack).
*/

bool check_chipdriver_and_expert_compat(struct obj_data *jack, struct obj_data *driver) {
#ifdef DIES_IRAE
  // We require that the chipjack and expert driver have the same slot count.
  if (!jack || !driver || GET_CYBERWARE_TYPE(jack) != CYB_CHIPJACK || GET_CYBERWARE_TYPE(driver) != CYB_CHIPJACKEXPERT) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Expected a chipjack and expert driver to c_c_a_e_c(), got %s (%ld) and %s (%ld) instead",
                    GET_OBJ_NAME(jack),
                    GET_OBJ_VNUM(jack),
                    GET_OBJ_NAME(driver),
                    GET_OBJ_VNUM(driver));
    return FALSE;
  }

  return GET_CYBERWARE_FLAGS(jack) == GET_CYBERWARE_FLAGS(driver);
#else
  // They're just compatible. Ezpz.
  return TRUE;
#endif
}

// Given a character, find their skillwires, chipjacks, and headware memory, then initialize skills from that.
void initialize_chipjack_and_memory_for_character(struct char_data *ch) {
  if (!ch) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Invalid parameters to initialize_chipjack_and_memory_for_character(%s)!", GET_CHAR_NAME(ch));
    return;
  }

  // Wipe out all current chipjack skills.
  for (int skill_idx = 0; skill_idx < MAX_SKILLS; skill_idx++) {
    GET_CHIPJACKED_SKILL(ch, skill_idx) = 0;
  }

  // Calculate skillwire maximum for activesofts.
  // HOUSERULE: To prevent people from having to deactivate and switch softs constantly, the total rating limit doesn't apply during gameplay. It still applies in pruns etc.
  struct obj_data *skillwires = find_cyberware(ch, CYB_SKILLWIRE);
  int skillwire_rating = (skillwires ? GET_CYBERWARE_RATING(skillwires) : 0);

  struct obj_data *chipjack = find_cyberware(ch, CYB_CHIPJACK);
  if (chipjack) {
    // Apply contained 'softs, constrained by skillwire rating if it's an active skill.
    for (struct obj_data *chip = chipjack->contains; chip; chip = chip->next_content) {
      set_skill_from_chip(ch, chip->files, FALSE, skillwire_rating);
    }
  }

  struct obj_data *memory = find_cyberware(ch, CYB_MEMORY);
  if (memory) {
    // Clear and reset their memory.
    GET_CYBERWARE_MEMORY_USED(memory) = 0;

    for (struct obj_data *chip = memory->contains; chip; chip = chip->next_content) {
      if (GET_OBJ_TYPE(chip) == ITEM_CHIP) {          
        // Load in any linked skills.
        if (GET_CHIP_LINKED(chip))
          set_skill_from_chip(ch, chip->files, FALSE, skillwire_rating);
        
        GET_CYBERWARE_MEMORY_USED(memory) += GET_CHIP_SIZE(chip) - GET_CHIP_COMPRESSION_FACTOR(chip);
      }
      // TODO: If any other items can be stored in memory, add their memory here.
      else if (GET_OBJ_TYPE(chip) == ITEM_DECK_ACCESSORY) {
        GET_CYBERWARE_MEMORY_USED(memory) += GET_DECK_ACCESSORY_FILE_SIZE(chip);
      }
    }
  }
}

void set_skill_from_chip(struct char_data *ch, struct matrix_file *skillsoft_file, bool send_message, int skillwire_rating) {
  if (!ch || !skillsoft_file) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Invalid parameters to set_skill_from_chip(%s, %s, %s, %d): null item!", GET_CHAR_NAME(ch), skillsoft_file->name, send_message ? "TRUE" : "FALSE", skillwire_rating);
    return;
  }

  if (skillsoft_file->file_type != MATRIX_FILE_SKILLSOFT) {
    mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Invalid parameters to set_skill_from_chip(%s, %s, %s, %d): bad chip!", GET_CHAR_NAME(ch), skillsoft_file->name, send_message ? "TRUE" : "FALSE", skillwire_rating);
    return;
  }

  if (!skillwire_rating) {
    struct obj_data *skillwires = find_cyberware(ch, CYB_SKILLWIRE);
    skillwire_rating = (skillwires ? GET_CYBERWARE_RATING(skillwires) : 0);
  }

  int chip_rating = skillsoft_file->rating;

  // HOUSERULE: If chip rating exceeds skillwires rating, chip is capped to skillwires instead of deactivated.
  if (!skills[skillsoft_file->skill].is_knowledge_skill) {
    chip_rating = MIN(chip_rating, skillwire_rating);
  }

  GET_CHIPJACKED_SKILL(ch, skillsoft_file->skill) = chip_rating;
  skillsoft_file->linked = ch->unique_id;
  if (send_message) {
    send_to_char(ch, "You gain a sudden understanding of %s%s.\r\n",
                  skills[skillsoft_file->skill].name,
                  chip_rating != skillsoft_file->rating ? ", although it's limited by your skillwires." : "");
  }
}

void deactivate_single_skillsoft(struct matrix_file *skillsoft_file, struct char_data *victim, bool send_message) {
  if (!victim || !skillsoft_file) {
    mudlog_vfprintf(victim, LOG_SYSLOG, "SYSERR: Invalid parameters to deactivate_single_skillsoft(%s, %s, %s): null item!", skillsoft_file->name, GET_CHAR_NAME(victim), send_message ? "TRUE" : "FALSE");
    return;
  }

  if (skillsoft_file->file_type != MATRIX_FILE_SKILLSOFT) {
    mudlog_vfprintf(victim, LOG_SYSLOG, "SYSERR: Invalid parameters to deactivate_single_skillsoft(%s, %s, %s): bad chip!", skillsoft_file->name, GET_CHAR_NAME(victim), send_message ? "TRUE" : "FALSE");
    return;
  }

  if (GET_CHIPJACKED_SKILL(victim, skillsoft_file->skill)) {
    GET_CHIPJACKED_SKILL(victim, skillsoft_file->skill) = 0;
    skillsoft_file->linked = 0;
    if (send_message) {
      send_to_char(victim, "Your %sSoft-linked knowledge of %s fades away.\r\n",
                    skills[skillsoft_file->skill].is_knowledge_skill ? "Know" : "Active",
                    skills[skillsoft_file->skill].name);
    }
  }
}

void deactivate_skillsofts_in_headware_memory(struct obj_data *mem, struct char_data *victim, bool send_message) {
  if (!victim || !mem) {
    mudlog_vfprintf(victim, LOG_SYSLOG, "SYSERR: Invalid parameters to deactivate_skillsofts_in_headware_memory(%s, %s, %s): null item!", GET_OBJ_NAME(mem), GET_CHAR_NAME(victim), send_message ? "TRUE" : "FALSE");
    return;
  }

  if (GET_OBJ_TYPE(mem) != ITEM_CYBERWARE || GET_CYBERWARE_TYPE(mem) != CYB_MEMORY) {
    mudlog_vfprintf(victim, LOG_SYSLOG, "SYSERR: Invalid parameters to deactivate_skillsofts_in_headware_memory(%s, %s, %s): bad memory!", GET_OBJ_NAME(mem), GET_CHAR_NAME(victim), send_message ? "TRUE" : "FALSE");
    return;
  }

  for (struct obj_data *chip = mem->contains; chip; chip = chip->next_content) {
    if (GET_CHIP_LINKED(chip)) {
      deactivate_single_skillsoft(chip->files, victim, send_message);
    }
  }
}

ACMD(do_jack)
{
  struct obj_data *chip = NULL, *chipjack;
  int chipnum = 0, chip_idx;

  FAILURE_CASE(!(chipjack = find_cyberware(ch, CYB_CHIPJACK)), "You don't have a chipjack.");

  skip_spaces(&argument);
  for (chip = chipjack->contains; chip; chip = chip->next_content)
    chipnum++;

  FAILURE_CASE(CH_IN_COMBAT(ch), "While fighting? That would be a neat trick.");

  // Unjack.
  if (subcmd) {
    FAILURE_CASE_PRINTF(!chipnum, "You don't have anything installed in %s.", decapitalize_a_an(chipjack));
    FAILURE_CASE_PRINTF((chip_idx = atoi(argument)) > chipnum, "You only have %d chip%s in %s. You can list them with ^WJACK^n",
                         chipnum, chipnum == 1 ? "" : "s", decapitalize_a_an(chipjack));

    // Count down to the correct chip.
    for (chip = chipjack->contains; chip && --chip_idx >= 1; chip = chip->next_content)
      ;

    // Ensure the chip is not currently linked, or if it is, ensure it's loaded into memory at the time of removal.
    // asdf todo

    // Remove the chip and give it to the character.
    obj_from_obj(chip);
    obj_to_char(chip, ch);
    
    send_to_char(ch, "You remove %s from your chipjack.\r\n", decapitalize_a_an(chip));
    deactivate_single_skillsoft(chip->files, ch, TRUE);
    act("$n removes a chip from their chipjack.", TRUE, ch, 0, 0, TO_ROOM);
    return;
  }

  // List chips to user.
  if (!*argument) {
    if (chipnum) {
      int i = 1;
      send_to_char(ch, "Currently installed chips:\r\n");
      for (chip = chipjack->contains; chip; chip = chip->next_content)
        send_to_char(ch, "%d) %20s Rating: %d\r\n", i++, GET_OBJ_NAME(chip), GET_OBJ_VAL(chip, 1));
    } else send_to_char(ch, "Your chipjack is currently empty.\r\n");
    return;
  }

  // Find a chip to jack in.
  chip = get_obj_in_list_vis(ch, argument, ch->carrying);
  if (chipnum >= GET_CYBERWARE_FLAGS(chipjack))
    send_to_char(ch, "You don't have any space left in your chipjack.\r\n");
  else if (!chip)
    send_to_char(ch, "But you don't have that chip.\r\n");
  else if (GET_OBJ_TYPE(chip) != ITEM_CHIP)
    send_to_char(ch, "But that isn't a chip.\r\n");
  else {
    set_skill_from_chip(ch, chip->files, TRUE, 0);
    send_to_char(ch, "You slip %s into your chipjack.\r\n", GET_OBJ_NAME(chip));
    obj_from_char(chip);
    obj_to_obj(chip, chipjack);
    act("$n puts a chip into their chipjack.", TRUE, ch, 0, 0, TO_ROOM);
  }
}

ACMD(do_chipload)
{
  skip_spaces(&argument);
  FAILURE_CASE(!*argument, "Syntax: LOAD <knowsoft chip in your chipjack>");

  struct obj_data *chipjack = find_cyberware(ch, CYB_CHIPJACK);
  struct obj_data *memory = find_cyberware(ch, CYB_MEMORY);

  if (!memory || !chipjack) {
    // Check to see if they were just trying to load a gun.
    struct obj_data *weapon = NULL;
    if ((weapon = get_obj_in_list_vis(ch, argument, ch->carrying))
        || ((weapon = GET_EQ(ch, WEAR_WIELD))
             && (isname(argument, weapon->text.keywords)
                 || isname(argument, weapon->text.name)
                 || (weapon->restring && isname(argument, weapon->restring))))) {
      char cmd_buf[MAX_INPUT_LENGTH + 10];
      snprintf(cmd_buf, sizeof(cmd_buf), " %s", argument);
      do_reload(ch, argument, 0, 0);
      return;
    }

    // Otherwise, they were probably messing up the prereqs for the chipload command.
    send_to_char("You need a chipjack and headware memory to load skillsofts into.\r\n", ch);
    return;
  }

  struct obj_data *chip = get_obj_in_list_vis(ch, argument, chipjack->contains);
  FAILURE_CASE(!chip, "You don't have any chips in your chipjack.");

  FAILURE_CASE(CH_IN_COMBAT(ch), "While fighting? That would be a neat trick.");

  FAILURE_CASE_PRINTF(GET_CYBERWARE_MEMORY_FREE(memory) < GET_CHIP_SIZE(chip),
                      "You don't have enough headware memory free. You need at least %d MP.",
                      GET_CHIP_SIZE(chip));

  struct obj_data *newchip = read_object(GET_OBJ_RNUM(chip), REAL, OBJ_LOAD_REASON_CHIPLOAD);
  obj_to_obj(newchip, memory);
  GET_CYBERWARE_MEMORY_USED(memory) += GET_CHIP_SIZE(chip);
  send_to_char(ch, "You upload %s to your headware memory.\r\n", decapitalize_a_an(chip));
}

ACMD(do_link)
{
  skip_spaces(&argument);
  FAILURE_CASE_PRINTF(!*argument, "Which chip do you wish to %slink?\r\n", subcmd ? "un" : "");
  
  struct obj_data *link = find_cyberware(ch, CYB_KNOWSOFTLINK);
  struct obj_data *skillwires = find_cyberware(ch, CYB_SKILLWIRE);

  FAILURE_CASE(CH_IN_COMBAT(ch), "While fighting? That would be a neat trick.");

  std::vector<struct obj_data*> memory = get_internal_storage_devices(ch);
  FAILURE_CASE(memory.size() <= 0, "You don't have any internal storage mediums connected to your nervous system.");

  struct matrix_file *skillsoft = NULL;
  for (obj_data *device : memory) {
    if ((skillsoft = get_matrix_file_in_list_vis(ch, argument, device->files)))
      break;
  }
  
  FAILURE_CASE_PRINTF(!skillsoft, "You don't have a file named '%s' in your internal storage media.", argument);
  FAILURE_CASE_PRINTF(skillsoft->file_type != MATRIX_FILE_SKILLSOFT, "%s is not a skillsoft file.", skillsoft->name);
  FAILURE_CASE(skills[skillsoft->skill].is_knowledge_skill && !link, "You need a knowsoft link to link knowsofts from headware memory.");
  FAILURE_CASE(!skills[skillsoft->skill].is_knowledge_skill && !skillwires, "You need skillwires to link activesofts from headware memory.");
  FAILURE_CASE(skillsoft->compression_factor, "You must decompress this skillsoft before you link it.");
  FAILURE_CASE_PRINTF((skillsoft->linked && !subcmd) || (!skillsoft->linked && subcmd), "That program is already %slinked.\r\n", subcmd ? "un" : "");

  if (subcmd) {
    skillsoft->linked = FALSE;
    send_to_char(ch, "You unlink %s.\r\n", skillsoft->name);
    deactivate_single_skillsoft(skillsoft, ch, TRUE);
  } else {
    skillsoft->linked = TRUE;
    send_to_char(ch, "You link %s to your %s.\r\n", skillsoft->name, skills[skillsoft->skill].is_knowledge_skill ? "knowsoft link" : "skillwires");
    set_skill_from_chip(ch, skillsoft, TRUE, skillwires ? GET_CYBERWARE_RATING(skillwires) : 0);
  }
}

ACMD(do_compact)
{
  struct obj_data *mem = find_cyberware(ch, CYB_MEMORY);
  struct obj_data *compact = find_cyberware(ch, CYB_DATACOMPACT);
  struct obj_data *obj;

  FAILURE_CASE_PRINTF(!mem || !compact, "You need headware memory and a data compactor to %scompress data.", subcmd ? "de" : "");

  FAILURE_CASE(CH_IN_COMBAT(ch), "While fighting? That would be a neat trick.");
  
  skip_spaces(&argument);
  FAILURE_CASE_PRINTF(!*argument, "What do you wish to %scompress?", subcmd ? "de" : "");

  int i = atoi(argument);
  for (obj = mem->contains; obj; obj = obj->next_content)
    if (!--i)
      break;
  if (!obj) {
    send_to_char("You don't have that file.\r\n", ch);
    return;
  }

  FAILURE_CASE_PRINTF(GET_OBJ_TYPE(obj) != ITEM_CHIP, "You can only %scompress skillsofts.", subcmd ? "de" : "");
  FAILURE_CASE_PRINTF(GET_CHIP_LINKED(obj), "You cannot %scompress a file that is in use.", subcmd ? "de" : "");
  FAILURE_CASE_PRINTF(subcmd && !GET_CHIP_COMPRESSION_FACTOR(obj), "%s isn't compressed.", CAP(GET_OBJ_NAME(obj)));
  FAILURE_CASE_PRINTF(!subcmd && GET_CHIP_COMPRESSION_FACTOR(obj), "%s is already compressed.", CAP(GET_OBJ_NAME(obj)));

  if (subcmd) {
    FAILURE_CASE_PRINTF(GET_CYBERWARE_MEMORY_FREE(mem) - GET_CHIP_COMPRESSION_FACTOR(obj) < 0,
                        "You don't have enough free memory to decompress %s. You need at least %d MP.",
                        decapitalize_a_an(obj),
                        GET_CHIP_COMPRESSION_FACTOR(obj));

    send_to_char(ch, "You decompress %s.\r\n", GET_OBJ_NAME(obj));
    GET_CYBERWARE_MEMORY_USED(mem) += GET_CHIP_COMPRESSION_FACTOR(obj);
    GET_CHIP_COMPRESSION_FACTOR(obj) = 0;
  } else {
    int size = (int)(((float)GET_CHIP_SIZE(obj) / 100) * (GET_CYBERWARE_RATING(compact) * 20));
    GET_CHIP_COMPRESSION_FACTOR(obj) = size;
    GET_CYBERWARE_MEMORY_USED(mem) -= GET_CHIP_COMPRESSION_FACTOR(obj);
    send_to_char(ch, "You compress %s, saving %d MP of space.\r\n", decapitalize_a_an(obj), size);
  }
}
