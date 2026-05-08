#include "structs.hpp"
#include "awake.hpp"
#include "interpreter.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "handler.hpp"

/* Consume a NERP-flagged item in your inventory, printing to the room. */
ACMD(do_usenerps) {
  skip_spaces(&argument);

  FAILURE_CASE(!*argument, "To consume (delete) one item and inform GMs in the room about it, do ^WUSENERPS <item name>^n");

  struct obj_data *obj = get_obj_in_list_vis(ch, argument, ch->carrying);

  FAILURE_CASE_PRINTF(!obj, "You don't seem to be carrying anything called '%s'.", argument);
  FAILURE_CASE_PRINTF(!IS_OBJ_STAT(obj, ITEM_EXTRA_NERPS), "%s isn't a NERP item.", CAP(GET_OBJ_NAME(obj)));
  FAILURE_CASE_PRINTF(IS_OBJ_STAT(obj, ITEM_EXTRA_KEPT), "You'll have to UNKEEP %s first.", decapitalize_a_an(GET_OBJ_NAME(obj)));
  FAILURE_CASE_PRINTF(obj->contains, "You'll have to empty %s first.", decapitalize_a_an(GET_OBJ_NAME(obj)));

  // If there were no questors or staff around to see it happen, you can't do it.
  {
    struct char_data *viewer;

    // Find valid viewers.
    for (viewer = (ch->in_room ? ch->in_room->people : ch->in_veh->people);
         viewer;
         viewer = (ch->in_room ? viewer->next_in_room : viewer->next_in_veh)) 
    {
      if (viewer == ch)
        continue;

      if (PRF_FLAGGED(viewer, PRF_QUESTOR) || (IS_SENATOR(viewer) && CAN_SEE(ch, viewer)))
        break;
    }

    FAILURE_CASE_PRINTF(!viewer, "No GMs are around to see you use %s, so you hold off.", decapitalize_a_an(GET_OBJ_NAME(obj)));
  }

  // Success. Send a message to GMs in the room.
  char msg_buf[1000];
  snprintf(msg_buf, sizeof(msg_buf), "1 '%s' is used by $n.", decapitalize_a_an(GET_OBJ_NAME(obj)));

  for (struct char_data *viewer = viewer = (ch->in_room ? ch->in_room->people : ch->in_veh->people);
        viewer;
        viewer = (ch->in_room ? viewer->next_in_room : viewer->next_in_veh)) 
  {
    if (viewer == ch)
      continue;
      
    if (PRF_FLAGGED(viewer, PRF_QUESTOR) || IS_SENATOR(viewer)) {
      act(msg_buf, FALSE, ch, NULL, viewer, TO_VICT);
    }
  }

  send_to_char(ch, "You discreetly notify the GM that you've spent '%s'.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));

  // Consume it.
  extract_obj(obj);
}

void display_nerpsheet(struct char_data *viewer, struct char_data *vict) {
  // to check: not null, not npc, not staff
  // todo: log if viewer != vict (misclog for non-staff, wizlog for staff)
}

int get_nerpsheet_skill_level(struct char_data *vict, const char *skill_name) {
  return 0;
}

// Validate the skill name; if they don't already have it, ask for confirmation.
void set_nerpsheet_skill_level(struct char_data *actor, struct char_data *vict, const char *skill_name, int level, bool is_active, bool confirmation_given) {

}

int get_nerpsheet_spell_level(struct char_data *vict, const char *spell_name) {
  return 0;
}

// Validate the spell name; if they don't already have it, ask for confirmation.
void set_nerpsheet_spell_level(struct char_data *actor, struct char_data *vict, const char *spell_name, int level, bool confirmation_given, bool for_free) {
  struct char_data *ch = actor; // Enables use of the FAILURE_CASE class of macros.

  FAILURE_CASE(for_free && !access_level(actor, LVL_ADMIN), "Sorry, you're not high-enough level to set values for free.");
  
  FAILURE_CASE_PRINTF(level > 0 && get_nerpsheet_spell_level(vict, spell_name) <= 0 && !confirmation_given,
                      "You don't have a spell named '%s' to increase. If you'd like to buy the first level in it, use NERPSHEET PURCHASE SPELL \"%s\" CONFIRM.\r\n",
                      spell_name);
}

#define NERPSHEET_SYNTAX "syntax string here"
ACMD(do_nerpsheet) {
  // no argument: view your own
  if (!*argument) {
    display_nerpsheet(ch, ch);
    return;
  }

  auto parsed_args = argparse(argument, {}, ch);

  FAILURE_CASE(!parsed_args || parsed_args.value().empty() || parsed_args.value().size() < 2, NERPSHEET_SYNTAX);
  const char *mode = parsed_args.value().at(0);

  if (is_abbrev(mode, "purchase")) {
    const char *skill_or_spell = parsed_args.value().at(1);

    // nerpsheet purchase skill "skillname" <KNOWLEDGE|ACTIVE> [CONFIRM]: increments to next level
    if (is_abbrev(skill_or_spell, "skill")) {
      FAILURE_CASE(parsed_args.value().size() < 4, "Syntax: NERPSHEET PURCHASE SKILL \"Skill Name In Double Quotes\" <KNOWLEDGE|ACTIVE> [CONFIRM]"
                                                   "\r\nExample: NERPSHEET PURCHASE SKILL \"History (Middle Ages)\" KNOWLEDGE CONFIRM");
      const char *skill_name = parsed_args.value().at(2);
      const char *knowledge_or_active = parsed_args.value().at(3);
      bool confirm = (parsed_args.value().size() >= 5 ? is_abbrev(parsed_args.value().at(4), "confirm") : false);

      bool is_knowledge = is_abbrev(knowledge_or_active, "knowledge");
      bool is_active = is_abbrev(knowledge_or_active, "active");
      FAILURE_CASE_PRINTF(!is_knowledge && !is_active, "Syntax: NERPSHEET PURCHASE SKILL \"%s\" <KNOWLEDGE|ACTIVE> %s", skill_name, confirm ? "CONFIRM" : "[CONFIRM]");

      set_nerpsheet_skill_level(ch, ch, skill_name, get_nerpsheet_skill_level(ch, skill_name), is_active, confirm);
      return;
    }

    // nerpsheet purchase spell "spellname" [CONFIRM]: increments to next force
    else if (is_abbrev(skill_or_spell, "spell")) {
      FAILURE_CASE(parsed_args.value().size() < 3, "Syntax: NERPSHEET PURCHASE SPELL \"Spell Name In Double Quotes\" [CONFIRM]"
                                                   "\r\nExample: NERPSHEET PURCHASE SPELL \"Manabolt\" CONFIRM");
      const char *spell_name = parsed_args.value().at(2);
      bool confirm = (parsed_args.value().size() >= 4 ? is_abbrev(parsed_args.value().at(3), "confirm") : false);
      set_nerpsheet_spell_level(ch, ch, spell_name, get_nerpsheet_spell_level(ch, spell_name), confirm, false);
      return;
    }

    send_to_char("Syntax: NERPSHEET PURCHASE <SKILL|SPELL> \"name of skill or spell\" ...\r\n", ch);
    return;
  }

  // Past this point, we know there are at least two arguments, and all commands use a victim as the second argument.
  const char *vict_name = parsed_args.value().at(1);
  struct char_data *vict = get_player_vis(ch, vict_name, true);
  FAILURE_CASE_PRINTF(!vict, "You don't see anyone named '%s' here.", vict_name);

  if (is_abbrev(mode, "show")) {
    // nerpsheet show <name>: view theirs IFF one of following (staff | in a nerpcorp room | questor or rpe or hired viewing a hired)
    if (access_level(ch, LVL_BUILDER)
        || (ch->in_room && VNUM_IS_NERPCORPOLIS(GET_ROOM_VNUM(ch->in_room)))
        || ((PLR_FLAGGED(ch, PLR_RPE) || PRF_FLAGGED(ch, PRF_QUESTOR) || PRF_FLAGGED(ch, PRF_HIRED)) && PRF_FLAGGED(vict, PRF_HIRED)))
    {
      display_nerpsheet(ch, vict);
      return;
    }

    send_to_char(ch, "You can't view %s's nerpsheet-- you need to be in a Nerpcorpolis room, or you both need to be set as Hired.\r\n", GET_CHAR_NAME(vict));
    return;
  }

  if (access_level(ch, LVL_BUILDER)) {  
    if (is_abbrev(mode, "refund")) {
      FAILURE_CASE(parsed_args.value().size() < 4, "Syntax: NERPSHEET REFUND <name> (SKILL|SPELL) \"skill or spell name in quotes\"");
      const char *skill_or_spell = parsed_args.value().at(2);
      const char *skill_or_spell_name = parsed_args.value().at(3);

      // nerpsheet refund <name> skill "skillname": refund it to zero
      if (is_abbrev(skill_or_spell, "skill")) {
        FAILURE_CASE_PRINTF(get_nerpsheet_skill_level(vict, skill_or_spell_name) <= 0, "%s doesn't have a '%s' NERP skill to refund.", GET_CHAR_NAME(vict), skill_or_spell_name);
        set_nerpsheet_skill_level(ch, vict, skill_or_spell_name, 0, true, false);
        return;
      }

      // nerpsheet refund <name> spell "skillname": refund it to zero
      if (is_abbrev(skill_or_spell, "spell")) {
        FAILURE_CASE_PRINTF(get_nerpsheet_spell_level(vict, skill_or_spell_name) <= 0, "%s doesn't have a '%s' NERP spell to refund.", GET_CHAR_NAME(vict), skill_or_spell_name);
        set_nerpsheet_spell_level(ch, vict, skill_or_spell_name, 0, true, false);
        return;
      }

      send_to_char("Syntax: NERPSHEET REFUND <name> (SKILL|SPELL) \"skill or spell name in quotes\"\r\n", ch);
      return;
    }

    if (!str_cmp(mode, "setforfree")) {
      FAILURE_CASE(parsed_args.value().size() < 5, "Syntax: NERPSHEET SETFORFREE <name> (SKILL|SPELL) \"skill or spell name in quotes\" ...");
      const char *skill_or_spell = parsed_args.value().at(2);
      const char *skill_or_spell_name = parsed_args.value().at(3);

      // nerpsheet setforfree <name> skill "skillname" <knowledge|active> <level>: set it for free
      if (is_abbrev(skill_or_spell, "skill")) {
        FAILURE_CASE(parsed_args.value().size() < 6, "Syntax: NERPSHEET SETFORFREE <target> SKILL \"Skill Name In Double Quotes\" <KNOWLEDGE|ACTIVE> <level>"
                                                    "\r\nExample: NERPSHEET SETFORFREE <target> SKILL \"History (Middle Ages)\" KNOWLEDGE 5");

        const char *knowledge_or_active = parsed_args.value().at(4);
        bool is_knowledge = is_abbrev(knowledge_or_active, "knowledge");
        bool is_active = is_abbrev(knowledge_or_active, "active");
        FAILURE_CASE_PRINTF(!is_knowledge && !is_active, "Syntax: NERPSHEET SETFORFREE <target> SKILL \"%s\" <KNOWLEDGE|ACTIVE> <level>; got '%s' for knowledge/active", skill_or_spell_name, knowledge_or_active);

        int level = atoi(parsed_args.value().at(5));
        FAILURE_CASE(level < 0 || level > 12, "Must choose a range between 0 and 12.");
        FAILURE_CASE_PRINTF(level == get_nerpsheet_skill_level(vict, skill_or_spell_name), "%s's '%s' is already at level %d.", GET_CHAR_NAME(vict), skill_or_spell_name, level);

        set_nerpsheet_skill_level(ch, vict, skill_or_spell_name, level, is_active, true);
        return;
      }

      // nerpsheet setforfree <name> spell "skillname" <level>: set it for free
      else if (is_abbrev(skill_or_spell, "spell")) {
        FAILURE_CASE(parsed_args.value().size() < 5, "Syntax: NERPSHEET SETFORFREE <target> SPELL \"Spell Name In Double Quotes\" <level>"
                                                    "\r\nExample: NERPSHEET SETFORFREE <target> SPELL \"Manabolt\" 4");

        int level = atoi(parsed_args.value().at(4));
        FAILURE_CASE(level < 0 || level > 12, "Must choose a range between 0 and 12.");
        FAILURE_CASE_PRINTF(level == get_nerpsheet_skill_level(vict, skill_or_spell_name), "%s's '%s' is already at level %d.", GET_CHAR_NAME(vict), skill_or_spell_name, level);
        
        set_nerpsheet_spell_level(ch, ch, skill_or_spell_name, level, true, true);
        return;
      }

      send_to_char("Syntax: NERPSHEET SETFORFREE <name> (SKILL|SPELL) \"skill or spell name in quotes\" ...\r\n", ch);
      return;
    }

    send_to_char("Syntax: NERPSHEET <REFUND|SETFORFREE> <SKILL|SPELL> \"name of skill or spell\" ...\r\n", ch);
  }

  send_to_char(NERPSHEET_SYNTAX, ch);
}