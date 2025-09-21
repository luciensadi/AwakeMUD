/*
************************************************************************
*   File: act.obj.c                                    Part of CircleMUD  *
*  Usage: object handling routines                                        *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "structs.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "interpreter.hpp"
#include "handler.hpp"
#include "db.hpp"
#include "awake.hpp"
#include "constants.hpp"
#include "newmatrix.hpp"
#include "newdb.hpp"
#include "limits.hpp"
#include "ignore_system.hpp"
#include "newmagic.hpp"
#include "newhouse.hpp"
#include "invis_resistance_tests.hpp"
#include "quest.hpp"
#include "redit.hpp"
#include "vehicles.hpp"
#include "pets.hpp"
#include "bullet_pants.hpp"

/* extern variables */
extern int drink_aff[][3];
extern PCIndex playerDB;

ACMD_DECLARE(do_bond);

// extern funcs
extern char *get_token(char *, char *);
extern int modify_target(struct char_data *ch);
extern int return_general(int skill_num);
extern bool check_quest_delivery(struct char_data *ch, struct char_data *mob, struct obj_data *obj);
extern void check_quest_delivery(struct char_data *ch, struct obj_data *obj);
extern void check_quest_destroy(struct char_data *ch, struct obj_data *obj);
extern void dominator_mode_switch(struct char_data *ch, struct obj_data *obj, int mode);
extern float get_bulletpants_weight(struct char_data *ch);
extern int calculate_vehicle_weight(struct veh_data *veh);
extern void die(struct char_data *ch, idnum_t cause_of_death_idnum);
extern bool obj_can_be_stowed(struct char_data *ch, struct obj_data *obj, bool send_messages);
extern bool raw_stow_obj(struct char_data *ch, struct obj_data *obj, bool send_messages);

extern SPECIAL(fence);
extern SPECIAL(hacker);
extern SPECIAL(fixer);
extern SPECIAL(mageskill_herbie);

extern void rectify_obj_host(struct obj_data *obj);
extern void rectify_desc_host(struct descriptor_data *desc);
extern bool is_approved_multibox_host(const char *host);

void calc_weight(struct char_data *ch);

SPECIAL(weapon_dominator);

extern WSPEC(monowhip);

int wear_bitvectors[] = {
		ITEM_WEAR_TAKE,
		ITEM_WEAR_HEAD,
		ITEM_WEAR_EYES,
		ITEM_WEAR_EAR,
		ITEM_WEAR_EAR,
		ITEM_WEAR_FACE,
		ITEM_WEAR_NECK,
		ITEM_WEAR_NECK,
		ITEM_WEAR_BACK,
		ITEM_WEAR_ABOUT,
		ITEM_WEAR_BODY,
		ITEM_WEAR_UNDER,
		ITEM_WEAR_ARMS,
		ITEM_WEAR_ARM,
		ITEM_WEAR_ARM,
		ITEM_WEAR_WRIST,
		ITEM_WEAR_WRIST,
		ITEM_WEAR_HANDS,
		ITEM_WEAR_WIELD,
		ITEM_WEAR_HOLD,
		ITEM_WEAR_SHIELD,
		ITEM_WEAR_FINGER,
		ITEM_WEAR_FINGER,
		ITEM_WEAR_FINGER,
		ITEM_WEAR_FINGER,
		ITEM_WEAR_FINGER,
		ITEM_WEAR_FINGER,
		ITEM_WEAR_FINGER,
		ITEM_WEAR_FINGER,
		ITEM_WEAR_BELLY,
		ITEM_WEAR_WAIST,
		ITEM_WEAR_THIGH,
		ITEM_WEAR_THIGH,
		ITEM_WEAR_LEGS,
		ITEM_WEAR_ANKLE,
		ITEM_WEAR_ANKLE,
		ITEM_WEAR_SOCK,
		ITEM_WEAR_FEET,
		ITEM_WEAR_FEET /* Technically should be PATCH but we're not doing that here */,
		ITEM_WEAR_UNDERWEAR,
		ITEM_WEAR_CHEST,
		ITEM_WEAR_LAPEL};

bool search_cyberdeck(struct obj_data *cyberdeck, struct obj_data *program)
{
	struct obj_data *temp;

	for (temp = cyberdeck->contains; temp; temp = temp->next_content)
		if ((GET_OBJ_TYPE(temp) == ITEM_PROGRAM && GET_OBJ_TYPE(program) == ITEM_PROGRAM && GET_OBJ_VAL(temp, 0) == GET_OBJ_VAL(program, 0)) ||
				(GET_OBJ_TYPE(temp) == ITEM_DECK_ACCESSORY && GET_OBJ_TYPE(program) == ITEM_DECK_ACCESSORY && GET_OBJ_VAL(temp, 1) == GET_OBJ_VAL(program, 1)))
			return TRUE;

	return FALSE;
}

void perform_put(struct char_data *ch, struct obj_data *obj, struct obj_data *cont)
{
	FAILURE_CASE(obj == ch->char_specials.programming, "You can't put something you are working on inside something.");
	FAILURE_CASE_PRINTF(GET_OBJ_TYPE(obj) == ITEM_PET, "%s refuses to go inside anything.", CAP(GET_OBJ_NAME(obj)));

	if (cont->in_room || cont->in_veh)
	{
		FAILURE_CASE_PRINTF(IS_OBJ_STAT(obj, ITEM_EXTRA_NODROP) && !IS_SENATOR(ch),
												"You can't let go of %s! You can JUNK it if you want to get rid of it.", decapitalize_a_an(obj));
		FAILURE_CASE_PRINTF(obj_contains_items_with_flag(obj, ITEM_EXTRA_NODROP) && !IS_SENATOR(ch),
												"Action blocked: %s contains at least one no-drop item. You can JUNK that item if you want.", GET_OBJ_NAME(obj));
	}

	FAILURE_CASE_PRINTF(cont->in_veh && cont->in_veh->usedload + GET_OBJ_WEIGHT(obj) > cont->in_veh->load,
											"%s would overload %s's carrying capacity.", CAP(GET_OBJ_NAME(obj)), decapitalize_a_an(cont->in_veh));

  FAILURE_CASE(IS_OBJ_STAT(cont, ITEM_EXTRA_CORPSE), "You can't put anything on corpses or in belongings.");

	if (GET_OBJ_TYPE(cont) == ITEM_WORN)
	{
		if (GET_OBJ_TYPE(obj) == ITEM_SPELL_FORMULA || GET_OBJ_TYPE(obj) == ITEM_DESIGN || GET_OBJ_TYPE(obj) == ITEM_PART)
		{
			act("You can't store spell formulas, designs, or parts in $P.", FALSE, ch, obj, cont, TO_CHAR);
			return;
		}

		if (GET_OBJ_TYPE(obj) == ITEM_HOLSTER || GET_OBJ_TYPE(obj) == ITEM_QUIVER)
		{
			if (GET_OBJ_VAL(cont, 0))
			{
				GET_OBJ_VAL(cont, 0)
				--;
				GET_OBJ_TIMER(obj) = 0;
			}
			else
			{
				act("$p won't fit in $P.", FALSE, ch, obj, cont, TO_CHAR);
				return;
			}
		}
		else if (GET_OBJ_TYPE(obj) == ITEM_GUN_MAGAZINE)
		{
			if (GET_OBJ_VAL(cont, 1))
			{
				GET_OBJ_VAL(cont, 1)
				--;
				GET_OBJ_TIMER(obj) = 1;
			}
			else if (GET_OBJ_VAL(cont, 4))
			{
				GET_OBJ_VAL(cont, 4)
				--;
				GET_OBJ_TIMER(obj) = 4;
			}
			else
			{
				act("$p won't fit in $P.", FALSE, ch, obj, cont, TO_CHAR);
				return;
			}
		}
		else if (GET_OBJ_TYPE(obj) == ITEM_WEAPON)
		{
			if (GET_WEAPON_ATTACK_TYPE(obj) == WEAP_GRENADE)
			{
				if (GET_OBJ_VAL(cont, 2))
				{
					GET_OBJ_VAL(cont, 2)
					--;
					GET_OBJ_TIMER(obj) = 2;
				}
				else if (GET_OBJ_VAL(cont, 4))
				{
					GET_OBJ_VAL(cont, 4)
					--;
					GET_OBJ_TIMER(obj) = 4;
				}
				else
				{
					act("$p won't fit in $P.", FALSE, ch, obj, cont, TO_CHAR);
					return;
				}
			}
			else if (GET_OBJ_VAL(obj, 3) == TYPE_SHURIKEN)
			{
				if (GET_OBJ_VAL(cont, 3))
				{
					GET_OBJ_VAL(cont, 3)
					--;
					GET_OBJ_TIMER(obj) = 3;
				}
				else if (GET_OBJ_VAL(cont, 4))
				{
					GET_OBJ_VAL(cont, 4)
					--;
					GET_OBJ_TIMER(obj) = 4;
				}
				else
				{
					act("$p won't fit in $P.", FALSE, ch, obj, cont, TO_CHAR);
					return;
				}
			}
			else
			{
				act("$p won't fit in $P.", FALSE, ch, obj, cont, TO_CHAR);
				return;
			}
		}
		else if (GET_OBJ_VAL(cont, 4))
		{
			if (GET_OBJ_WEIGHT(obj) <= 1)
			{
				GET_OBJ_VAL(cont, 4)
				--;
				GET_OBJ_TIMER(obj) = 4;
			}
			else
			{
				act("$p won't fit in $P.", FALSE, ch, obj, cont, TO_CHAR);
				return;
			}
		}
		else
		{
			act("$p won't fit in $P.", FALSE, ch, obj, cont, TO_CHAR);
			return;
		}

		if (obj->in_obj)
			obj_from_obj(obj);
		else
			obj_from_char(obj);
		obj_to_obj(obj, cont);

		act("You put $p in $P.", FALSE, ch, obj, cont, TO_CHAR);
		act("$n puts $p in $P.", FALSE, ch, obj, cont, TO_ROOM);

		set_dropped_by_info(obj, ch);
		return;
	}

	if (GET_OBJ_TYPE(cont) == ITEM_QUIVER)
	{
		if ((GET_OBJ_VAL(cont, 1) == 0 && !(GET_OBJ_TYPE(obj) == ITEM_MISSILE && GET_OBJ_VAL(obj, 0) == 0)) ||
				(GET_OBJ_VAL(cont, 1) == 1 && !(GET_OBJ_TYPE(obj) == ITEM_MISSILE && GET_OBJ_VAL(obj, 0) == 1)) ||
				(GET_OBJ_VAL(cont, 1) == 2 && !(GET_OBJ_TYPE(obj) == ITEM_WEAPON && GET_OBJ_VAL(obj, 3) == TYPE_SHURIKEN)) ||
				(GET_OBJ_VAL(cont, 1) == 3 && !(GET_OBJ_TYPE(obj) == ITEM_WEAPON && GET_OBJ_VAL(obj, 3) == TYPE_THROWING_KNIFE)))
			return;
		if (GET_OBJ_VAL(cont, 2) >= GET_OBJ_VAL(cont, 0))
			act("$p won't fit in $P.", FALSE, ch, obj, cont, TO_CHAR);
		else
		{
			obj_from_char(obj);
			obj_to_obj(obj, cont);

			GET_OBJ_VAL(cont, 2)
			++;
			act("You put $p in $P.", FALSE, ch, obj, cont, TO_CHAR);
			act("$n puts $p in $P.", TRUE, ch, obj, cont, TO_ROOM);

			// Cheatlog it for staffers, or for wizloaded or cheatlog-marked items, as long as it's not inv-to-inv.
			if (((!IS_NPC(ch) && access_level(ch, LVL_BUILDER)) || IS_OBJ_STAT(obj, ITEM_EXTRA_WIZLOAD) || IS_OBJ_STAT(obj, ITEM_EXTRA_CHEATLOG_MARK)) && !(cont->carried_by == ch || cont->worn_by == ch))
			{
				char *representation = generate_new_loggable_representation(obj);
				snprintf(buf, sizeof(buf), "%s puts in (%ld) %s [restring: %s]: %s", GET_CHAR_NAME(ch),
								 GET_OBJ_VNUM(cont), cont->text.name, cont->restring ? cont->restring : "none",
								 representation);
				mudlog(buf, ch, IS_OBJ_STAT(obj, ITEM_EXTRA_WIZLOAD) ? LOG_WIZITEMLOG : LOG_CHEATLOG, TRUE);
				delete[] representation;
			}

			set_dropped_by_info(obj, ch);
		}
		return;
	}

	if (GET_OBJ_TYPE(cont) == ITEM_KEYRING)
	{
		if (GET_OBJ_TYPE(obj) != ITEM_KEY)
		{
			act("You can only put keys on $P.", FALSE, ch, obj, cont, TO_CHAR);
			return;
		}

		// Rule out some of the weird keys.
		switch (GET_OBJ_VNUM(obj))
		{
		case OBJ_ENCYCLOPEDIA_LABELED_O:
		case OBJ_OPTICAL_CHIP_KEY:
		case OBJ_FIBEROPTIC_CRYSTAL:
		case OBJ_UNFINISHED_EQUATION:
		case OBJ_INVITATION_TO_SMITHS_PUB:
		case OBJ_ARES_PERSONAL_INVITATION_SLIP:
		case OBJ_SCANEYE:
			send_to_char(ch, "You stare blankly at %s, unable to figure out how to thread it onto a keyring without putting holes in it.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));
			return;
		case OBJ_EYEBALL_KEY:
		case OBJ_ELECTRONIC_EYEBALL_KEY:
			send_to_char(ch, "%s squishes unpleasantly between your fingers as you try to wrap the remnants of the optic nerve around the keyring.\r\n", capitalize(GET_OBJ_NAME(obj)));
			return;
		}

		// Previously, we weight-limited the keyring, but that's no fun.
		/*
		if (GET_OBJ_WEIGHT(cont) + GET_OBJ_WEIGHT(obj) > MAX_KEYRING_WEIGHT) {
			act("$P's ring isn't strong enough to hold everything you're trying to put on it.", FALSE, ch, obj, cont, TO_CHAR);
			return;
		}
		*/
	}
	else if (GET_OBJ_WEIGHT(cont) + GET_OBJ_WEIGHT(obj) > (GET_OBJ_VAL(cont, 0) + get_proto_weight(cont)))
	{
		act("$p won't fit in $P.", FALSE, ch, obj, cont, TO_CHAR);
		return;
	}

	if (obj->in_obj)
		obj_from_obj(obj);
	else
		obj_from_char(obj);
	obj_to_obj(obj, cont);

	act("You put $p in $P.", FALSE, ch, obj, cont, TO_CHAR);
	act("$n puts $p in $P.", TRUE, ch, obj, cont, TO_ROOM);
	// Cheatlog it for staffers, or for wizloaded or cheatlog-marked items, as long as it's not inv-to-inv.
	if (((!IS_NPC(ch) && access_level(ch, LVL_BUILDER)) || IS_OBJ_STAT(obj, ITEM_EXTRA_WIZLOAD) || IS_OBJ_STAT(obj, ITEM_EXTRA_CHEATLOG_MARK)) && !(cont->carried_by == ch || cont->worn_by == ch))
	{
		char *representation = generate_new_loggable_representation(obj);
		snprintf(buf, sizeof(buf), "%s puts in (%ld) %s [restring: %s]: %s", GET_CHAR_NAME(ch),
						 GET_OBJ_VNUM(cont), cont->text.name, cont->restring ? cont->restring : "none",
						 representation);
		mudlog(buf, ch, IS_OBJ_STAT(obj, ITEM_EXTRA_WIZLOAD) ? LOG_WIZITEMLOG : LOG_CHEATLOG, TRUE);
		delete[] representation;
	}

	set_dropped_by_info(obj, ch);
}

void perform_put_cyberdeck(struct char_data *ch, struct obj_data *obj,
													 struct obj_data *cont)
{
	int space_required = 0;

	if (GET_OBJ_TYPE(cont) == ITEM_DECK_ACCESSORY)
	{
		if (GET_DECK_ACCESSORY_TYPE(cont) != TYPE_COMPUTER)
		{
			send_to_char(ch, "You can't install anything into %s^n.\r\n", GET_OBJ_NAME(cont));
			return;
		}

		if (cont->carried_by)
		{
			send_to_char(ch, "%s won't work while carried, you'll have to drop it.\r\n", capitalize(GET_OBJ_NAME(cont)));
			return;
		}

		if (GET_OBJ_TYPE(obj) != ITEM_DESIGN && GET_OBJ_TYPE(obj) != ITEM_PROGRAM && !(GET_OBJ_TYPE(obj) == ITEM_PROGRAM && GET_OBJ_TIMER(obj)))
		{
			send_to_char(ch, "You can't install %s onto a personal computer; you can only install uncooked programs and designs.\r\n", GET_OBJ_NAME(obj));
			return;
		}

		if (GET_OBJ_TYPE(obj) == ITEM_PROGRAM)
		{
			space_required = GET_PROGRAM_SIZE(obj);
		}
		else
		{
			space_required = (int)GET_DESIGN_SIZE(obj) * 1.1;
		}
		int free_space = GET_DECK_ACCESSORY_COMPUTER_MAX_MEMORY(cont) - GET_DECK_ACCESSORY_COMPUTER_USED_MEMORY(cont);

		if (free_space < space_required)
		{
			send_to_char(ch, "%s doesn't seem to fit on %s-- it takes %d megapulses, but there are only %d available.\r\n",
									 capitalize(GET_OBJ_NAME(obj)),
									 decapitalize_a_an(GET_OBJ_NAME(cont)),
									 space_required,
									 free_space);
			return;
		}

		GET_DECK_ACCESSORY_COMPUTER_USED_MEMORY(cont) += space_required;

		obj_from_char(obj);
		obj_to_obj(obj, cont);
		send_to_char(ch, "You install %s^n onto %s^n.\r\n", GET_OBJ_NAME(obj), GET_OBJ_NAME(cont));
		act("$n installs $p on $P.", TRUE, ch, obj, cont, TO_ROOM);
		return;
	}
	else if (GET_OBJ_TYPE(obj) == ITEM_DECK_ACCESSORY)
	{
		switch (GET_DECK_ACCESSORY_TYPE(obj))
		{
		case TYPE_FILE:
			if (GET_OBJ_VAL(cont, 5) + GET_DECK_ACCESSORY_FILE_SIZE(obj) > GET_OBJ_VAL(cont, 3))
			{
				act("$p^n takes up too much memory to be uploaded into $P^n.", FALSE, ch, obj, cont, TO_CHAR);
				return;
			}
			obj_from_char(obj);
			obj_to_obj(obj, cont);
			GET_OBJ_VAL(cont, 5) += GET_DECK_ACCESSORY_FILE_SIZE(obj);
			act("You upload $p in $P.", FALSE, ch, obj, cont, TO_CHAR);
			return;
		case TYPE_UPGRADE:
			if (GET_OBJ_VAL(obj, 1) != 3)
			{
				send_to_char(ch, "You can't seem to fit %s^n into %s^n.\r\n", GET_OBJ_NAME(obj), GET_OBJ_NAME(cont));
				return;
			}
			obj_from_char(obj);
			obj_to_obj(obj, cont);
			act("You fit $p^n into a FUP in $P^n.", FALSE, ch, obj, cont, TO_CHAR);
			return;
		default:
			send_to_char(ch, "You can't seem to fit %s^n into %s^n.\r\n", GET_OBJ_NAME(obj), GET_OBJ_NAME(cont));
			return;
		}
	}
	// Prevent installing persona firmware into a store-bought deck.
	if (GET_OBJ_VNUM(obj) == OBJ_BLANK_PROGRAM)
	{
		switch (GET_PROGRAM_TYPE(obj))
		{
		case SOFT_BOD:
		case SOFT_SENSOR:
		case SOFT_MASKING:
		case SOFT_EVASION:
		case SOFT_ASIST_COLD:
		case SOFT_ASIST_HOT:
		case SOFT_HARDENING:
		case SOFT_ICCM:
		case SOFT_ICON:
		case SOFT_MPCP:
		case SOFT_REALITY:
		case SOFT_RESPONSE:
			if (GET_OBJ_VNUM(cont) == OBJ_CUSTOM_CYBERDECK_SHELL)
			{
				send_to_char(ch, "%s^n is firmware, you'll have to BUILD it into the deck along with the matching chip.\r\n", CAP(GET_OBJ_NAME(obj)));
			}
			else
			{
				send_to_char(ch, "%s^n is firmware for a custom cyberdeck persona chip. It's not compatible with store-bought decks.\r\n", CAP(GET_OBJ_NAME(obj)));
			}
			return;
		}
	}

	if (!GET_OBJ_TIMER(obj) && GET_OBJ_VNUM(obj) == OBJ_BLANK_PROGRAM)
		send_to_char(ch, "You'll have to cook %s before you can install it.\r\n", GET_OBJ_NAME(obj));
	else if (GET_CYBERDECK_MPCP(cont) == 0 || GET_CYBERDECK_IS_INCOMPLETE(cont))
		display_cyberdeck_issues(ch, cont);
	else if (search_cyberdeck(cont, obj))
		act("You already have a similar program installed in $P.", FALSE, ch, obj, cont, TO_CHAR);
	else
	{
		// Persona programs don't take up storage memory in store-bought decks
		if ((GET_OBJ_TYPE(cont) == ITEM_CYBERDECK) && (GET_PROGRAM_TYPE(obj) <= SOFT_SENSOR))
		{
			space_required = 0;
		}
		else
		{
			space_required = GET_PROGRAM_SIZE(obj);
		}

		// Check to make sure there's room
		if (GET_CYBERDECK_USED_STORAGE(cont) + space_required > GET_CYBERDECK_TOTAL_STORAGE(cont))
		{
			act("$p^n takes up too much memory to be installed into $P^n.", FALSE, ch, obj, cont, TO_CHAR);
		}
		else
		{
			obj_from_char(obj);
			obj_to_obj(obj, cont);
			act("You install $p in $P.", FALSE, ch, obj, cont, TO_CHAR);
			GET_CYBERDECK_USED_STORAGE(cont) += space_required;
		}
	}
}

/* The following put modes are supported by the code below:

				1) put <object> <container>
				2) put all.<object> <container>
				3) put all <container>

				<container> must be in inventory or on ground.
				all objects to be put into container must be in inventory.
*/

ACMD(do_put)
{
	char arg1[MAX_INPUT_LENGTH];
	char arg2[MAX_INPUT_LENGTH];
	struct obj_data *obj, *next_obj, *cont;
	struct char_data *tmp_char;
	int obj_dotmode, found = 0;
	bool cyberdeck = FALSE;
	if (IS_WORKING(ch))
	{
		send_to_char(TOOBUSY, ch);
		return;
	}

	FAILURE_CASE(IS_ASTRAL(ch), "Astral beings can't touch things!");

	two_arguments(argument, arg1, arg2);
	obj_dotmode = find_all_dots(arg1, sizeof(arg1));
	find_all_dots(arg2, sizeof(arg2));

	if (subcmd == SCMD_INSTALL)
		cyberdeck = TRUE;

	FAILURE_CASE_PRINTF(!*arg1, "%s what in what?", (cyberdeck ? "Install" : "Put"));
	FAILURE_CASE_PRINTF(obj_dotmode != FIND_INDIV, "You can only %s at a time.", (cyberdeck ? "install programs into one cyberdeck" : "put things into one container"));
	FAILURE_CASE_PRINTF(!*arg2, "What do you want to %s %s in?", (cyberdeck ? "install" : "put"), ((obj_dotmode == FIND_INDIV) ? "it" : "them"));

	if (!str_cmp(arg2, "finger"))
	{
		FAILURE_CASE(!(cont = find_cyberware(ch, CYB_FINGERTIP)), "You don't have a fingertip compartment.");
		FAILURE_CASE(cont->contains, "Your fingertip compartment is already full.");
		FAILURE_CASE_PRINTF(!(obj = get_obj_in_list_vis(ch, arg1, ch->carrying)), "You aren't carrying %s %s.", AN(arg1), arg1);

		if (GET_OBJ_TYPE(obj) != ITEM_PROGRAM && (GET_OBJ_TYPE(obj) != ITEM_DRUG || (GET_OBJ_DRUG_TYPE(obj) != DRUG_CRAM && GET_OBJ_DRUG_TYPE(obj) != DRUG_PSYCHE)) && !(IS_MONOWHIP(obj)))
		{
			send_to_char(ch, "%s doesn't fit in your fingertip compartment.\r\n", CAP(GET_OBJ_NAME(obj)));
			return;
		}

		obj_from_char(obj);
		obj_to_obj(obj, cont);
		send_to_char(ch, "You slip %s^n into your fingertip compartment.\r\n", decapitalize_a_an(obj));
		act("$n slips $p^n into $s fingertip compartment.\r\n", TRUE, ch, obj, 0, TO_ROOM);
		return;
	}

	if (!str_cmp(arg2, "body"))
	{
		FAILURE_CASE(!(cont = find_cyberware(ch, CYB_BODYCOMPART)), "You don't have a body compartment.");
		FAILURE_CASE(cont->contains, "Your body compartment is already full.");
		FAILURE_CASE_PRINTF(!(obj = get_obj_in_list_vis(ch, arg1, ch->carrying)), "You aren't carrying %s %s.", AN(arg1), arg1);
		FAILURE_CASE_PRINTF(GET_OBJ_WEIGHT(obj) > 1, "%s doesn't fit in your body compartment.", CAP(GET_OBJ_NAME(obj)));

		obj_from_char(obj);
		obj_to_obj(obj, cont);
		send_to_char(ch, "You slip %s^n into your body compartment.\r\n", decapitalize_a_an(obj));
		act("$n slips $p^n into $s body compartment.\r\n", TRUE, ch, 0, obj, TO_ROOM);
		return;
	}

	if (!str_cmp(arg2, "tooth"))
	{
		FAILURE_CASE(!(cont = find_cyberware(ch, CYB_TOOTHCOMPARTMENT)), "You don't have a tooth compartment.");
		FAILURE_CASE(cont->contains, "Your tooth compartment is already full.");
		FAILURE_CASE_PRINTF(!(obj = get_obj_in_list_vis(ch, arg1, ch->carrying)), "You aren't carrying %s %s.", AN(arg1), arg1);

		// You can always fit Cram and Psyche in a compartment. We only evaluate the logic in this block if it's neither of these things.
		if (GET_OBJ_TYPE(obj) != ITEM_DRUG || (GET_OBJ_DRUG_TYPE(obj) != DRUG_CRAM && GET_OBJ_DRUG_TYPE(obj) != DRUG_PSYCHE))
		{
			// Breakables can only take Cram/Psyche.
			FAILURE_CASE(GET_CYBERWARE_FLAGS(cont), "You can only put the drugs Cram and Psyche in breakable tooth compartments.");

			// Non-breakables can also take programs.
			FAILURE_CASE(GET_OBJ_TYPE(obj) != ITEM_PROGRAM, "You can only fit optical chips and the drugs Cram and Psyche in non-breakable tooth compartments.");
		}

		obj_from_char(obj);
		obj_to_obj(obj, cont);
		send_to_char(ch, "You slip %s^n into your tooth compartment.\r\n", decapitalize_a_an(obj));
		act("$n slips $p^n into a tooth compartment.\r\n", TRUE, ch, 0, obj, TO_ROOM);
		return;
	}

	generic_find(arg2, FIND_OBJ_EQUIP | FIND_OBJ_INV | FIND_OBJ_ROOM, ch, &tmp_char, &cont);
	if (!cont)
	{
		struct veh_data *veh = get_veh_list(arg2, ch->in_veh ? ch->in_veh->carriedvehs : ch->in_room->vehicles, ch);
		if (veh)
		{
			send_to_char(ch, "%s is a vehicle-- you'll have to use the UPGRADE command.\r\n", capitalize(GET_VEH_NAME_NOFORMAT(veh)));
		}
		else
		{
			send_to_char(ch, "You don't see anything named '%s' here.\r\n", arg2);
		}
		return;
	}

	// Combine drugs.
	if (GET_OBJ_TYPE(cont) == ITEM_DRUG || GET_OBJ_VNUM(cont) == OBJ_ANTI_DRUG_CHEMS)
	{
		FAILURE_CASE_PRINTF(!(obj = get_obj_in_list_vis(ch, arg1, ch->carrying)), "You aren't carrying %s %s.", AN(arg1), arg1);
		FAILURE_CASE_PRINTF(obj == cont, "You can't combine %s with itself.", GET_OBJ_NAME(obj));

		// Combining chems.
		if (GET_OBJ_VNUM(cont) == OBJ_ANTI_DRUG_CHEMS)
		{
			FAILURE_CASE(GET_OBJ_VNUM(obj) != OBJ_ANTI_DRUG_CHEMS, "You can only combine chems with other chems.");

			send_to_char("You combine the chems.\r\n", ch);
			GET_CHEMS_QTY(cont) += GET_CHEMS_QTY(obj);
			GET_CHEMS_QTY(obj) = 0;
			GET_OBJ_WEIGHT(cont) += GET_OBJ_WEIGHT(obj);
			extract_obj(obj);
			return;
		}

		// Combining mismatching drugs.
		FAILURE_CASE_PRINTF(GET_OBJ_TYPE(obj) != ITEM_DRUG || GET_OBJ_DRUG_TYPE(obj) != GET_OBJ_DRUG_TYPE(cont),
												"You can only combine %s with other doses of %s, and %s doesn't qualify.",
												decapitalize_a_an(GET_OBJ_NAME(cont)),
												drug_types[GET_OBJ_DRUG_TYPE(cont)].name,
												GET_OBJ_NAME(obj));

		combine_drugs(ch, obj, cont, TRUE);
		return;
	}

	// Combine ammo boxes.
	if (GET_OBJ_TYPE(cont) == ITEM_GUN_AMMO)
	{
		FAILURE_CASE_PRINTF(!(obj = get_obj_in_list_vis(ch, arg1, ch->carrying)), "You aren't carrying %s %s.", AN(arg1), arg1);
		FAILURE_CASE_PRINTF(obj == cont, "You cannot combine %s with itself.", decapitalize_a_an(GET_OBJ_NAME(cont)));

		// Restriction: You can't wombo-combo non-ammo into ammo.
		FAILURE_CASE_PRINTF(GET_OBJ_TYPE(obj) != ITEM_GUN_AMMO, "%s will only accept the contents of other ammo boxes, and %s^n doesn't qualify.",
												CAP(GET_OBJ_NAME(cont)),
												decapitalize_a_an(GET_OBJ_NAME(obj)));

		// If it's got a creator set, it's not done yet.
		FAILURE_CASE_PRINTF(GET_AMMOBOX_INTENDED_QUANTITY(cont) > 0, "Your target (%s) still has disassembled rounds in it. It needs to be completed first.", decapitalize_a_an(GET_OBJ_NAME(cont)));
		FAILURE_CASE_PRINTF(GET_AMMOBOX_INTENDED_QUANTITY(obj) > 0, "Your source (%s) still has disassembled rounds in it. It needs to be completed first.", decapitalize_a_an(GET_OBJ_NAME(obj)));

		// If the weapons don't match, no good.
		FAILURE_CASE_PRINTF(GET_AMMOBOX_WEAPON(cont) != GET_AMMOBOX_WEAPON(obj),
												"You can't combine %s ammo with %s ammo.",
												weapon_types[GET_AMMOBOX_WEAPON(cont)],
												weapon_types[GET_AMMOBOX_WEAPON(obj)]);

		// If the ammo types don't match, no good.
		FAILURE_CASE_PRINTF(GET_AMMOBOX_TYPE(cont) != GET_AMMOBOX_TYPE(obj),
												"You can't combine %s ammo with %s ammo.",
												ammo_type[GET_AMMOBOX_TYPE(cont)].name,
												ammo_type[GET_AMMOBOX_TYPE(obj)].name);

		// Combine them. This handles junking of empties, restringing, etc.
		if (!combine_ammo_boxes(ch, obj, cont, TRUE))
		{
			send_to_char("Something went wrong. Please reach out to staff.\r\n", ch);
		}
		return;
	}

	// Combine cyberdeck parts/chips, or combine summoning materials.
	if ((GET_OBJ_TYPE(cont) == ITEM_DECK_ACCESSORY && GET_DECK_ACCESSORY_TYPE(cont) == TYPE_PARTS) ||
			(GET_OBJ_TYPE(cont) == ITEM_MAGIC_TOOL && GET_MAGIC_TOOL_TYPE(cont) == TYPE_SUMMONING))
	{
		// Find the object.
		FAILURE_CASE_PRINTF(!(obj = get_obj_in_list_vis(ch, arg1, ch->carrying)), "You aren't carrying %s %s.\r\n", AN(arg1), arg1);

		// Not the same object as container.
		FAILURE_CASE_PRINTF(obj == cont, "You cannot combine %s with itself.\r\n", GET_OBJ_NAME(obj));

		// Must match types.
		FAILURE_CASE_PRINTF(GET_OBJ_TYPE(cont) != GET_OBJ_TYPE(obj), "You cannot combine %s with %s.\r\n", GET_OBJ_NAME(obj), GET_OBJ_NAME(cont));

		// Must match subtypes.
		switch (GET_OBJ_TYPE(obj))
		{
		case ITEM_DECK_ACCESSORY:
			FAILURE_CASE_PRINTF(GET_DECK_ACCESSORY_TYPE(obj) != GET_DECK_ACCESSORY_TYPE(cont) || GET_DECK_ACCESSORY_PARTS_IS_CHIPS(obj) != GET_DECK_ACCESSORY_PARTS_IS_CHIPS(cont),
													"You cannot combine %s with %s.\r\n", GET_OBJ_NAME(obj), GET_OBJ_NAME(cont));
			break;
		case ITEM_MAGIC_TOOL:
			FAILURE_CASE_PRINTF(GET_MAGIC_TOOL_TYPE(obj) != GET_MAGIC_TOOL_TYPE(cont),
													"You cannot combine %s with %s.\r\n", GET_OBJ_NAME(obj), GET_OBJ_NAME(cont));
			break;
		default:
			mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Received unknown type %d to part/chip/material combine switch case.", GET_OBJ_TYPE(obj));
			return;
		}

		// All preconditions passed: Add value, then extract.
		GET_OBJ_COST(cont) += GET_OBJ_COST(obj);
		send_to_char(ch, "You dump the contents of %s^n into %s^n.\r\n", GET_OBJ_NAME(obj), GET_OBJ_NAME(cont));
		extract_obj(obj);
		return;
	}

	if (cyberdeck)
	{
		if (!(GET_OBJ_TYPE(cont) == ITEM_CYBERDECK || GET_OBJ_TYPE(cont) == ITEM_CUSTOM_DECK || GET_OBJ_TYPE(cont) == ITEM_DECK_ACCESSORY))
		{
			snprintf(buf, sizeof(buf), "$p is not a cyberdeck.");
			act(buf, FALSE, ch, cont, 0, TO_CHAR);
			return;
		}
	}
	else if (GET_OBJ_TYPE(cont) != ITEM_CONTAINER && GET_OBJ_TYPE(cont) != ITEM_QUIVER && GET_OBJ_TYPE(cont) != ITEM_WORN && GET_OBJ_TYPE(cont) != ITEM_KEYRING)
	{
		snprintf(buf, sizeof(buf), "$p is not a container.");
		act(buf, FALSE, ch, cont, 0, TO_CHAR);
		return;
	}

	if (IS_SET(GET_OBJ_VAL(cont, 1), CONT_CLOSED) && (GET_OBJ_TYPE(cont) != ITEM_CYBERDECK && GET_OBJ_TYPE(cont) != ITEM_QUIVER && GET_OBJ_TYPE(cont) != ITEM_WORN && GET_OBJ_TYPE(cont) != ITEM_DECK_ACCESSORY) && GET_OBJ_TYPE(cont) != ITEM_CUSTOM_DECK)
	{
		send_to_char("You'd better open it first!\r\n", ch);
		return;
	}

	if (obj_dotmode == FIND_INDIV)
	{ /* put <obj> <container> */
		if (!(obj = get_obj_in_list_vis(ch, arg1, ch->carrying)))
		{
			send_to_char(ch, "You aren't carrying %s %s.\r\n", AN(arg1), arg1);
			return;
		}

		if (GET_OBJ_VNUM(obj) == OBJ_VEHCONTAINER)
		{
			send_to_char(ch, "You'd better drop %s^n if you want to free up your hands.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));
			return;
		}

		if ((obj == cont) && !cyberdeck)
		{
			send_to_char(ch, "You attempt to fold %s^n into itself, but fail.\r\n", GET_OBJ_NAME(obj));
			return;
		}

		if (cyberdeck)
		{
			// Better messaging for parts.
			if (GET_OBJ_TYPE(obj) == ITEM_PART)
			{
				send_to_char(ch, "Parts aren't plug-and-play; you'll have to BUILD %s^n into your deck instead.\r\n", GET_OBJ_NAME(obj));
				return;
			}

			// You can only install programs, parts, and designs.
			if (GET_OBJ_TYPE(obj) != ITEM_PROGRAM && GET_OBJ_TYPE(obj) != ITEM_DECK_ACCESSORY && GET_OBJ_TYPE(obj) != ITEM_DESIGN)
			{
				send_to_char(ch, "You can't install %s^n into a cyberdeck.\r\n", GET_OBJ_NAME(obj));
				return;
			}

			// You can only install program designs into computers.
			if (GET_OBJ_TYPE(obj) == ITEM_DESIGN && GET_OBJ_TYPE(cont) != ITEM_DECK_ACCESSORY)
			{
				send_to_char("Program designs are just conceptual outlines and can't be installed into cyberdecks.\r\n", ch);
				return;
			}

			perform_put_cyberdeck(ch, obj, cont);
			return;
		}

		if (GET_OBJ_TYPE(cont) == ITEM_QUIVER)
		{
			if (GET_OBJ_VAL(cont, 1) == 0 && !(GET_OBJ_TYPE(obj) == ITEM_MISSILE && GET_OBJ_VAL(obj, 0) == 0))
				send_to_char(ch, "Only arrows may be placed in %s.\r\n", GET_OBJ_NAME(obj));
			else if (GET_OBJ_VAL(cont, 1) == 1 && !(GET_OBJ_TYPE(obj) == ITEM_MISSILE && GET_OBJ_VAL(obj, 0) == 1))
				send_to_char(ch, "Only bolts may be placed in %s.\r\n", GET_OBJ_NAME(obj));
			else if (GET_OBJ_VAL(cont, 1) == 2 && !(GET_OBJ_TYPE(obj) == ITEM_WEAPON && GET_OBJ_VAL(obj, 3) == TYPE_SHURIKEN))
				send_to_char(ch, "Only shurikens can be stored in %s.\r\n", GET_OBJ_NAME(obj));
			else if (GET_OBJ_VAL(cont, 1) == 3 && !(GET_OBJ_TYPE(obj) == ITEM_WEAPON && GET_OBJ_VAL(obj, 3) == TYPE_THROWING_KNIFE))
				send_to_char(ch, "%s is used to hold throwing knives only.\r\n", capitalize(GET_OBJ_NAME(obj)));
			else
			{
				perform_put(ch, obj, cont);
			}
			return;
		}

		perform_put(ch, obj, cont);
	}
	else
	{
		for (obj = ch->carrying; obj; obj = next_obj)
		{
			next_obj = obj->next_content;
			if (GET_OBJ_VNUM(obj) == OBJ_VEHCONTAINER)
				continue;
			if (obj != cont && CAN_SEE_OBJ(ch, obj) &&
					(obj_dotmode == FIND_ALL || isname(arg1, obj->text.keywords)))
			{
				found = 1;
				if (!cyberdeck)
				{
					perform_put(ch, obj, cont);
				}
				else if (GET_OBJ_TYPE(obj) != ITEM_PROGRAM)
					send_to_char(ch, "%s is not a program!\r\n", GET_OBJ_NAME(obj));
				else
					perform_put_cyberdeck(ch, obj, cont);
			}
		}
		if (!found)
		{
			if (obj_dotmode == FIND_ALL)
			{
				send_to_char(ch, "You don't seem to have anything in your inventory to %s in it.\r\n", (cyberdeck ? "install" : "put"));
			}
			else
			{
				send_to_char(ch, "You don't seem to have anything named '%s' in your inventory.\r\n", arg1);
			}
		}
	}
}

bool can_take_obj_from_anywhere(struct char_data *ch, struct obj_data *obj)
{
	FALSE_CASE_PRINTF(IS_CARRYING_N(ch) >= CAN_CARRY_N(ch), "%s: you can't carry that many items.", CAP(GET_OBJ_NAME(obj)));

	FALSE_CASE_PRINTF(!IS_SENATOR(ch) && (IS_CARRYING_W(ch) + GET_OBJ_WEIGHT(obj)) > CAN_CARRY_W(ch), "%s: you can't carry another %.2f kilos.%s",
										CAP(GET_OBJ_NAME(obj)),
										GET_OBJ_WEIGHT(obj),
										GET_OBJ_TYPE(obj) == ITEM_GUN_AMMO ? "(You can still grab rounds out of it with ^WPOCKETS ADD <container>^n though!)" : "");

	if (!(CAN_WEAR(obj, ITEM_WEAR_TAKE)) && !ch->in_veh)
	{
		if (access_level(ch, LVL_PRESIDENT))
		{
			send_to_char(ch, "You bypass the !TAKE flag on %s.\r\n", decapitalize_a_an(obj));
		}
		else
		{
			if (GET_OBJ_TYPE(obj) == ITEM_DESTROYABLE)
			{
				send_to_char(ch, "You can't pick %s up%s\r\n", decapitalize_a_an(obj), IS_RIGGING(ch) ? "." : ", but you're pretty sure you could ##^WDESTROY^n it.");
			}
			else
			{
				send_to_char(ch, "You can't take %s.\r\n", decapitalize_a_an(obj));
			}
			return FALSE;
		}
	}

	// If it's quest-protected and you're not the questor...
	FALSE_CASE_PRINTF(ch_is_blocked_by_quest_protections(ch, obj, FALSE, TRUE), "%s is someone else's quest item.", CAP(GET_OBJ_NAME(obj)));

	// It's a pet and you're not the owner or staff.
	if (GET_OBJ_TYPE(obj) == ITEM_PET)
	{
		if (GET_PET_OWNER_IDNUM(obj) > 0 && GET_IDNUM(ch) != GET_PET_OWNER_IDNUM(obj))
		{
			if (IS_SENATOR(ch))
			{
				// Staff can pick it up.
				send_to_char(ch, "You bypass the owner permissions with your staff powers.\r\n");
			}
			else if (obj->in_room && obj->in_room->apartment && obj->in_room->apartment->has_owner_privs(ch))
			{
				// Apartment owner can pick it up.
			}
			else
			{
				// Otherwise, only the creator can pick it up.
				send_to_char(ch, "%s refuses to be picked up.\r\n", CAP(GET_OBJ_NAME(obj)));
			}
		}
	}

	return 1;
}

// Returns TRUE if extracted.
bool get_check_money(struct char_data *ch, struct obj_data *obj, struct obj_data *from_obj)
{
	int zone;

	// Do nothing if it's not money.
	if (GET_OBJ_TYPE(obj) != ITEM_MONEY)
		return FALSE;

	// Find the zone it belongs to.
	for (zone = 0; zone <= top_of_zone_table; zone++)
		if (!zone_table[zone].approved && GET_OBJ_VNUM(obj) >= (zone_table[zone].number * 100) &&
				GET_OBJ_VNUM(obj) <= zone_table[zone].top)
			break;

	// Confirm that the zone is valid.
	if (zone <= top_of_zone_table)
	{
		act("$p dissolves in your hands!", FALSE, ch, obj, 0, TO_CHAR);
		snprintf(buf3, sizeof(buf3), "ERROR: Non-zone-contained item %ld obtained by player.", GET_OBJ_VNUM(obj));
		mudlog(buf3, ch, LOG_SYSLOG, TRUE);

		extract_obj(obj);
		return TRUE;
	}

	// Confirm that it has money on it.
	if (GET_ITEM_MONEY_VALUE(obj) <= (!GET_ITEM_MONEY_IS_CREDSTICK(obj) ? 0 : -1))
	{
		act("$p dissolves in your hands!", FALSE, ch, obj, 0, TO_CHAR);
		snprintf(buf3, sizeof(buf3), "ERROR: Valueless money item %ld obtained by player.", GET_OBJ_VNUM(obj));
		mudlog(buf3, ch, LOG_SYSLOG, TRUE);

		extract_obj(obj);
		return TRUE;
	}

	// If it's paper money, handle it here.
	if (!GET_ITEM_MONEY_IS_CREDSTICK(obj))
	{
		if (GET_ITEM_MONEY_VALUE(obj) > 1)
			send_to_char(ch, "There were %d nuyen.\r\n", GET_ITEM_MONEY_VALUE(obj));
		else
			send_to_char(ch, "There was 1 nuyen.\r\n");

		if (from_obj && (GET_OBJ_VNUM(from_obj) != OBJ_SPECIAL_PC_CORPSE && IS_OBJ_STAT(from_obj, ITEM_EXTRA_CORPSE)))
		{
			// Income from an NPC corpse is always tracked.
			gain_nuyen(ch, GET_ITEM_MONEY_VALUE(obj), NUYEN_INCOME_LOOTED_FROM_NPCS);
		}
		else
		{
			// Picking up money from a player corpse, or dropped money etc-- not a faucet, came from a PC.
			GET_NUYEN_RAW(ch) += GET_ITEM_MONEY_VALUE(obj);
		}

		extract_obj(obj);
		return TRUE;
	}

	// Credstick? Handle it here.
	else
	{
		// Income from an NPC corpse is always tracked. We don't add it to their cash level though-- credstick.
		if (from_obj && (GET_OBJ_VNUM(from_obj) != OBJ_SPECIAL_PC_CORPSE && IS_OBJ_STAT(from_obj, ITEM_EXTRA_CORPSE)))
		{
			if (IS_SENATOR(ch))
				send_to_char("(credstick from npc corpse)\r\n", ch);
			GET_NUYEN_INCOME_THIS_PLAY_SESSION(ch, NUYEN_INCOME_LOOTED_FROM_NPCS) += GET_ITEM_MONEY_VALUE(obj);
		}
		else
		{
			if (IS_SENATOR(ch))
				send_to_char("(credstick from pc corpse)\r\n", ch);
		}

		// We don't extract the credstick.
		return FALSE;
	}

  return FALSE;
}

void calc_weight(struct char_data *ch)
{
	struct obj_data *obj;
	int i = 0;
	/* first reset the player carry weight*/
	IS_CARRYING_W(ch) = 0;

	// Go through worn equipment.
	for (i = 0; i < NUM_WEARS; i++)
		if (GET_EQ(ch, i))
			IS_CARRYING_W(ch) += GET_OBJ_WEIGHT(GET_EQ(ch, i));

	// Go through carried equipment.
	for (obj = ch->carrying; obj; obj = obj->next_content)
		IS_CARRYING_W(ch) += GET_OBJ_WEIGHT(obj);

	// Add cyberware per SR3 p300.
	for (obj = ch->cyberware; obj; obj = obj->next_content)
		if (GET_OBJ_VAL(obj, 0) == CYB_BONELACING)
			switch (GET_OBJ_VAL(obj, 3))
			{
			case BONE_KEVLAR:
			case BONE_PLASTIC:
				IS_CARRYING_W(ch) += 5;
				break;
			case BONE_ALUMINIUM:
				IS_CARRYING_W(ch) += 10;
				break;
			case BONE_TITANIUM:
			case BONE_CERAMIC:
				IS_CARRYING_W(ch) += 15;
				break;
			}

	// Add bullet pants.
	IS_CARRYING_W(ch) += get_bulletpants_weight(ch);
}

// Returns TRUE if container is extracted.
bool perform_get_from_container(struct char_data *ch, struct obj_data *obj,
																struct obj_data *cont, int mode)
{
	bool cyberdeck = FALSE, computer = FALSE;
	if (IS_WORKING(ch))
	{
		send_to_char(TOOBUSY, ch);
		return FALSE;
	}
	if (GET_OBJ_TYPE(cont) == ITEM_CYBERDECK || GET_OBJ_TYPE(cont) == ITEM_CUSTOM_DECK)
		cyberdeck = TRUE;
	else if (GET_OBJ_TYPE(cont) == ITEM_DECK_ACCESSORY && GET_OBJ_VAL(cont, 0) == TYPE_COMPUTER)
		computer = TRUE;

	if (mode == FIND_OBJ_INV || ch == get_obj_possessor(obj) || can_take_obj_from_anywhere(ch, obj))
	{
		if (IS_CARRYING_N(ch) >= CAN_CARRY_N(ch)) {
			act("$p: you can't hold any more items.", FALSE, ch, obj, 0, TO_CHAR);
      return FALSE;
    }

    bool should_wizlog = IS_OBJ_STAT(obj, ITEM_EXTRA_WIZLOAD);
    bool should_cheatlog = (!IS_NPC(ch) && access_level(ch, LVL_BUILDER)) || (IS_OBJ_STAT(obj, ITEM_EXTRA_CHEATLOG_MARK) || IS_OBJ_STAT(cont, ITEM_EXTRA_CHEATLOG_MARK));
    bool should_gridlog = FALSE;
    bool same_host_warning = FALSE;

    if (cont->obj_flags.extra_flags.IsSet(ITEM_EXTRA_CORPSE) && GET_CORPSE_IS_PC(cont)) {
      if (GET_CORPSE_IDNUM(cont) == GET_IDNUM(ch)) {
        should_gridlog = TRUE;
      }
      else {
        should_cheatlog = TRUE;
      }
    }

    if (!should_cheatlog && obj->dropped_by_char > 0 && obj->dropped_by_char != GET_IDNUM(ch) && ch->desc) {
      // Ensure that the object and character both have valid hosts, and convert them from IP to lookup if possible.
      rectify_obj_host(obj);
      rectify_desc_host(ch->desc);

      if (obj->dropped_by_host && !str_cmp(obj->dropped_by_host, ch->desc->host)) {
        // Log anyone doing this from a multibox host.
        should_cheatlog = TRUE;
        same_host_warning = TRUE;
      }
    }

    if ((should_wizlog || should_cheatlog || should_gridlog) && !(cont->carried_by == ch || cont->worn_by == ch)) {
      char *representation = generate_new_loggable_representation(obj);

      // Compose our log line.
      snprintf(buf, sizeof(buf), "%s gets from (%ld) %s [restring: %s]: %s (cost %d)",
                GET_CHAR_NAME(ch),
                GET_OBJ_VNUM(cont), cont->text.name, cont->restring ? cont->restring : "none",
                representation,
                GET_OBJ_COST(obj));

      if (same_host_warning)
      {
        const char *pname = get_player_name(obj->dropped_by_char);
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), ", which was dropped/donated by %s (%ld) at their same host (%s)!",
                  pname,
                  obj->dropped_by_char,
                  GET_LEVEL(ch) < LVL_PRESIDENT ? (obj->dropped_by_host ? obj->dropped_by_host : "<host missing>") : "<obscured>");
        delete[] pname;
      }

      // Write it.
      if (should_wizlog) {
        mudlog(buf, ch, LOG_WIZITEMLOG, TRUE);
      }
      else if (should_cheatlog) {
        mudlog(buf, ch, LOG_CHEATLOG, TRUE);
      }
      else {
        mudlog(buf, ch, LOG_GRIDLOG, TRUE);
      }
      delete[] representation;
    }

    set_dropped_by_info(obj, NULL);

    if (GET_OBJ_TYPE(cont) == ITEM_QUIVER) {
      GET_OBJ_VAL(cont, 2) = MAX(0, GET_OBJ_VAL(cont, 2) - 1);
    }
    snprintf(buf, sizeof(buf), "You %s $p from $P.", (cyberdeck || computer ? "uninstall" : "get"));

    if (computer) {
      if (ch->in_room) {
        for (struct char_data *vict = ch->in_room->people; vict; vict = vict->next_in_room)
        {
          if ((AFF_FLAGGED(vict, AFF_PROGRAM) || AFF_FLAGGED(vict, AFF_DESIGN)) && vict != ch)
          {
            send_to_char(ch, "You can't uninstall %s while someone is working on it.\r\n", GET_OBJ_NAME(obj));
            return FALSE;
          }
          else if (vict == ch && vict->char_specials.programming == obj)
          {
            send_to_char(ch, "You stop %sing %s.\r\n", AFF_FLAGGED(ch, AFF_PROGRAM) ? "programm" : "design", GET_OBJ_NAME(obj));
            STOP_WORKING(ch);
            break;
          }
        }
      }
      else
      {
        for (struct char_data *vict = ch->in_veh->people; vict; vict = vict->next_in_veh)
        {
          if ((AFF_FLAGGED(vict, AFF_PROGRAM) || AFF_FLAGGED(vict, AFF_DESIGN)) && vict != ch)
          {
            send_to_char(ch, "You can't uninstall %s while someone is working on it.\r\n", GET_OBJ_NAME(obj));
            return FALSE;
          }
          else if (vict == ch && vict->char_specials.programming == obj)
          {
            send_to_char(ch, "You stop %sing %s.\r\n", AFF_FLAGGED(ch, AFF_PROGRAM) ? "programm" : "design", GET_OBJ_NAME(obj));
            STOP_WORKING(ch);
            break;
          }
        }
      }
      if (GET_OBJ_TYPE(obj) == ITEM_PROGRAM)
        GET_CYBERDECK_TOTAL_STORAGE(cont) -= GET_OBJ_VAL(obj, 2);
      else
        GET_CYBERDECK_TOTAL_STORAGE(cont) -= GET_OBJ_VAL(obj, 6) + (GET_OBJ_VAL(obj, 6) / 10);
    }
    else if (cyberdeck)
    {
      if (GET_OBJ_TYPE(obj) == ITEM_PROGRAM && (GET_CYBERDECK_MPCP(cont) == 0 || GET_CYBERDECK_IS_INCOMPLETE(cont)))
      {
        display_cyberdeck_issues(ch, cont);
        return FALSE;
      }

      // Subtract program size from storage, but a persona program on a store-bought deck doesn't use storage
      if (((GET_OBJ_TYPE(obj) == ITEM_PROGRAM) && !((GET_OBJ_TYPE(cont) == ITEM_CYBERDECK) && (GET_PROGRAM_TYPE(obj) <= SOFT_SENSOR))) || (GET_OBJ_TYPE(obj) == ITEM_DECK_ACCESSORY && GET_DECK_ACCESSORY_TYPE(obj) == TYPE_FILE))
      {
        GET_CYBERDECK_USED_STORAGE(cont) -= GET_DECK_ACCESSORY_FILE_SIZE(obj);
      }

      if (GET_OBJ_TYPE(obj) == ITEM_PART)
      {
        if (GET_OBJ_VAL(obj, 0) == PART_STORAGE)
        {
          for (struct obj_data *k = cont->contains; k; k = k->next_content)
            if ((GET_OBJ_TYPE(k) == ITEM_DECK_ACCESSORY && GET_DECK_ACCESSORY_TYPE(k) == TYPE_FILE) ||
                GET_OBJ_TYPE(k) == ITEM_PROGRAM)
            {
              send_to_char(ch, "You cannot uninstall %s while you have files installed.\r\n", GET_OBJ_NAME(obj));
              return FALSE;
            }
          GET_CYBERDECK_USED_STORAGE(cont) = GET_CYBERDECK_TOTAL_STORAGE(cont) = 0;
        }
        switch (GET_OBJ_VAL(obj, 0))
        {
        case PART_MPCP:
          GET_PART_RATING(obj) = GET_CYBERDECK_MPCP(cont);
          GET_PART_TARGET_MPCP(obj) = GET_CYBERDECK_MPCP(cont);
          // fall through
        case PART_ACTIVE:
        case PART_BOD:
        case PART_SENSOR:
        case PART_IO:
        case PART_MATRIX_INTERFACE:
          GET_CYBERDECK_IS_INCOMPLETE(cont) = 1;
        }
      }
    }

    // Only send this message for things that aren't corpses. Corpses get their own messaging later on due to potential autostow.
    if (!cont->obj_flags.extra_flags.IsSet(ITEM_EXTRA_CORPSE)) {
      act(buf, FALSE, ch, obj, cont, TO_CHAR);
    }
    if (GET_OBJ_TYPE(cont) == ITEM_WORN) {
      GET_OBJ_VAL(cont, GET_OBJ_TIMER(obj))++;
    }
    if (!cyberdeck && !computer) {
      act("$n gets $p from $P.", TRUE, ch, obj, cont, TO_ROOM);
    } else {
      act("$n uninstalls $p from $P.", TRUE, ch, obj, cont, TO_ROOM);
    }

    if (cont->obj_flags.extra_flags.IsSet(ITEM_EXTRA_CORPSE)) {
      // Extract PC corpses if they're empty.
      if (GET_CORPSE_IS_PC(cont)) {
        act("You get $p from $P.", FALSE, ch, obj, cont, TO_CHAR);
        if (!cont->contains) {
          if (cont->in_room && ROOM_FLAGGED(cont->in_room, ROOM_CORPSE_SAVE_HACK)) {
            bool should_clear_flag = TRUE;

            // Iterate through items in room, making sure there are no other corpses.
            for (struct obj_data *tmp_obj = cont->in_room->contents; tmp_obj; tmp_obj = tmp_obj->next_content) {
              if (tmp_obj != cont && IS_OBJ_STAT(tmp_obj, ITEM_EXTRA_CORPSE) && GET_OBJ_BARRIER(tmp_obj) == PC_CORPSE_BARRIER) {
                should_clear_flag = FALSE;
                break;
              }
            }

            if (should_clear_flag) {
              snprintf(buf, sizeof(buf), "Removing storage flag from %s (%ld) due to no more player corpses being in it.",
                        GET_ROOM_NAME(cont->in_room),
                        GET_ROOM_VNUM(cont->in_room));
              mudlog(buf, NULL, LOG_SYSLOG, TRUE);

              // No more? Remove storage flag and save.
              cont->in_room->room_flags.RemoveBit(ROOM_CORPSE_SAVE_HACK);
              cont->in_room->room_flags.RemoveBit(ROOM_STORAGE);

              // Save the change.
              for (int counter = 0; counter <= top_of_zone_table; counter++) {
                if ((GET_ROOM_VNUM(cont->in_room) >= (zone_table[counter].number * 100)) && (GET_ROOM_VNUM(cont->in_room) <= (zone_table[counter].top))) {
                  write_world_to_disk(zone_table[counter].number);
                  break;
                }
              }
            }
          }

          act("$n takes the last of the items from $p.", TRUE, ch, cont, NULL, TO_ROOM);
          act("You take the last of the items from $p.", TRUE, ch, cont, NULL, TO_CHAR);
          extract_obj(cont);
          return TRUE;
        }
      }
      // Anything in an NPC corpse that can be stowed: stow it.
      else {
#ifdef USE_HAMMERSPACE
        int ammo_weapontype = -1;
        int ammo_type = -1;
        // If it's a mob gun with 1 round, strip out the ammo before attempting to stow.
        if (GET_OBJ_TYPE(obj) == ITEM_WEAPON
            && WEAPON_IS_GUN(obj)
            && obj->contains
            && GET_OBJ_TYPE(obj->contains) == ITEM_GUN_MAGAZINE
            && GET_MAGAZINE_AMMO_COUNT(obj->contains) == 1)
        {
          struct obj_data *magazine = obj->contains;
          obj_from_obj(magazine);
          if (obj_can_be_stowed(ch, obj, FALSE)) {
            ammo_weapontype = GET_MAGAZINE_BONDED_ATTACKTYPE(magazine);
            ammo_type = GET_MAGAZINE_AMMO_TYPE(magazine);
            update_bulletpants_ammo_quantity(ch, GET_MAGAZINE_BONDED_ATTACKTYPE(magazine), GET_MAGAZINE_AMMO_TYPE(magazine), 1);
            extract_obj(magazine);
          } else {
            // Put it back.
            obj_to_obj(magazine, obj);
          }
        }

        if (obj_can_be_stowed(ch, obj, FALSE)) {
          if (ammo_weapontype > -1) {
            // Swap AV for APDS to prevent players getting it.
            ammo_type = (ammo_type == AMMO_AV ? AMMO_APDS : ammo_type);
            snprintf(buf, sizeof(buf), "You get %s from $P, strip out and pocket %s, then stow the weapon away for fencing.",
                     decapitalize_a_an(GET_OBJ_NAME(obj)),
                     get_ammo_representation(ammo_weapontype, ammo_type, 1, ch));
          } else {
            snprintf(buf, sizeof(buf), "You get %s from $P, then stow it away for fencing.", decapitalize_a_an(GET_OBJ_NAME(obj)));
          }
          
          if (raw_stow_obj(ch, obj, FALSE)) {
            act(buf, FALSE, ch, NULL, cont, TO_CHAR);
            // We return at this point because the object was stowed.
            return TRUE;
          }
        }
#endif
        act("You get $p from $P.", FALSE, ch, obj, cont, TO_CHAR);
      }
    }

    {
      struct obj_data *was_in_obj = obj->in_obj;
      obj_from_obj(obj);
      obj_to_char(obj, ch);
      get_check_money(ch, obj, was_in_obj);
      obj = NULL;
    }
  }

	return FALSE;
}

void get_from_container(struct char_data *ch, struct obj_data *cont,
												char *arg, int mode, bool confirmed = FALSE)
{
	struct obj_data *obj, *next_obj;
	int obj_dotmode, found = 0;

	obj_dotmode = find_all_dots(arg, sizeof(arg));

	if (GET_OBJ_TYPE(cont) == ITEM_CYBERDECK || GET_OBJ_TYPE(cont) == ITEM_CUSTOM_DECK)
	{
		FAILURE_CASE(obj_dotmode == FIND_ALL || obj_dotmode == FIND_ALLDOT, "You can't uninstall more than one program at a time.");
		FAILURE_CASE_PRINTF(!(obj = get_obj_in_list_vis(ch, arg, cont->contains)), "There doesn't seem to be %s %s installed in %s.", AN(arg), arg, decapitalize_a_an(cont));
		FAILURE_CASE_PRINTF(GET_OBJ_TYPE(obj) == ITEM_PART && !confirmed,
												"It takes a while to reinstall parts once you've removed them! You'll have to do ^WUNINSTALL <part> <deck> CONFIRM^n to uninstall %s from %s.\r\n",
												GET_OBJ_NAME(obj),
												GET_OBJ_NAME(cont));
		perform_get_from_container(ch, obj, cont, mode);
		return;
	}

	if (GET_OBJ_TYPE(cont) == ITEM_DECK_ACCESSORY && GET_DECK_ACCESSORY_TYPE(cont) == TYPE_COOKER)
	{
		struct obj_data *chip = cont->contains;

		FAILURE_CASE(!chip, "There's no chip in there to take out.");

		if (GET_DECK_ACCESSORY_COOKER_TIME_REMAINING(cont))
		{
			FAILURE_CASE(!access_level(ch, LVL_ADMIN), "You cannot remove a chip while it is being encoded.");

			// Staff override.
			send_to_char("You use your staff powers to forcibly complete the encoding process.\r\n", ch);
			GET_DECK_ACCESSORY_COOKER_TIME_REMAINING(cont) = 0;
			GET_SETTABLE_PROGRAM_IS_COOKED(chip) = 1;
		}

		perform_get_from_container(ch, chip, cont, mode);
		return;
	}

	if (IS_OBJ_STAT(cont, ITEM_EXTRA_CORPSE))
	{
		FAILURE_CASE_PRINTF(GET_CORPSE_IS_PC(cont) && GET_CORPSE_IDNUM(cont) != GET_IDNUM(ch) && !IS_SENATOR(ch), "You cannot loot %s.", decapitalize_a_an(cont));

		if (obj_dotmode == FIND_INDIV)
		{
			FAILURE_CASE_PRINTF(!(obj = get_obj_in_list_vis(ch, arg, cont->contains)), "There doesn't seem to be %s %s in %s.", AN(arg), arg, decapitalize_a_an(cont));
			perform_get_from_container(ch, obj, cont, mode);
			return;
		}

		FAILURE_CASE(obj_dotmode == FIND_ALLDOT && !*arg, "Get all of what?");

		for (obj = cont->contains; obj; obj = next_obj)
		{
			next_obj = obj->next_content;
			if (CAN_SEE_OBJ(ch, obj) && (obj_dotmode == FIND_ALL || isname(arg, obj->text.keywords)))
			{
				found = 1;
				if (perform_get_from_container(ch, obj, cont, mode))
					return;
			}
		}

		FAILURE_CASE_PRINTF(!found && obj_dotmode == FIND_ALL, "%s seems to be empty.", CAP(GET_OBJ_NAME(cont)));
		FAILURE_CASE_PRINTF(!found && obj_dotmode != FIND_ALL, "You can't seem to find any %s in %s.", arg, decapitalize_a_an(cont));
	}

	found = 0;

	if (IS_SET(GET_OBJ_VAL(cont, 1), CONT_CLOSED) && GET_OBJ_TYPE(cont) != ITEM_WORN && GET_OBJ_TYPE(cont) != ITEM_DECK_ACCESSORY)
		act("$p is closed.", FALSE, ch, cont, 0, TO_CHAR);
	else if (obj_dotmode == FIND_INDIV)
	{
		if (!(obj = get_obj_in_list_vis(ch, arg, cont->contains)))
		{
			snprintf(buf, sizeof(buf), "There doesn't seem to be %s %s in $p.", AN(arg), arg);
			act(buf, FALSE, ch, cont, 0, TO_CHAR);
		}
		else if (perform_get_from_container(ch, obj, cont, mode))
			return;
	}
	else
	{
		if (obj_dotmode == FIND_ALLDOT && !*arg)
		{
			send_to_char("Get all of what?\r\n", ch);
			return;
		}
		for (obj = cont->contains; obj; obj = next_obj)
		{
			next_obj = obj->next_content;
			if (CAN_SEE_OBJ(ch, obj) &&
					(obj_dotmode == FIND_ALL || isname(arg, obj->text.keywords)))
			{
				found = 1;
				if (perform_get_from_container(ch, obj, cont, mode))
					return;
			}
		}
		if (!found)
		{
			if (obj_dotmode == FIND_ALL)
				act("$p seems to be empty.", FALSE, ch, cont, 0, TO_CHAR);
			else
			{
				snprintf(buf, sizeof(buf), "You can't seem to find any %ss in $p.", arg);
				act(buf, FALSE, ch, cont, 0, TO_CHAR);
			}
		}
	}
}

bool can_take_obj_from_room(struct char_data *ch, struct obj_data *obj)
{
	if (GET_OBJ_TYPE(obj) == ITEM_DECK_ACCESSORY)
	{
		switch (GET_DECK_ACCESSORY_TYPE(obj))
		{
		case TYPE_COMPUTER:
			// You can only get computers that are not in use.
			for (struct char_data *vict = obj->in_veh ? obj->in_veh->people : obj->in_room->people;
					 vict;
					 vict = vict->in_veh ? vict->next_in_veh : vict->next_in_room)
			{
				if (vict->char_specials.programming && vict->char_specials.programming->in_obj == obj)
				{
					FALSE_CASE_PRINTF(vict == ch, "You are using %s already.", decapitalize_a_an(obj));
					FALSE_CASE_PRINTF(vict != ch, "%s is in use.", CAP(GET_OBJ_NAME(obj)));
				}
			}
			break;
		case TYPE_COOKER:
			// Cookers can't be cooking.
			FALSE_CASE_PRINTF(GET_DECK_ACCESSORY_COOKER_TIME_REMAINING(obj), "%s is in the middle of encoding a chip, leave it alone.", capitalize(GET_OBJ_NAME(obj)));
			break;
		}
	}

	else if (GET_OBJ_TYPE(obj) == ITEM_MAGIC_TOOL)
	{
		switch (GET_MAGIC_TOOL_TYPE(obj))
		{
		case TYPE_LIBRARY_SPELL:
			// Nobody can be spelldesigning.
			for (struct char_data *vict = obj->in_veh ? obj->in_veh->people : obj->in_room->people;
					 vict;
					 vict = vict->in_veh ? vict->next_in_veh : vict->next_in_room)
			{
				if (AFF_FLAGGED(vict, AFF_SPELLDESIGN))
				{
					FALSE_CASE_PRINTF(vict == ch, "You are using %s to design your spell.", decapitalize_a_an(obj));
					FALSE_CASE_PRINTF(vict != ch, "%s is in use.", CAP(GET_OBJ_NAME(obj)));
				}
			}
			break;
		case TYPE_LIBRARY_CONJURE:
			// Nobody can be conjuring.
			for (struct char_data *vict = obj->in_veh ? obj->in_veh->people : obj->in_room->people;
					 vict;
					 vict = vict->in_veh ? vict->next_in_veh : vict->next_in_room)
			{
				if (AFF_FLAGGED(vict, AFF_CONJURE))
				{
					FALSE_CASE_PRINTF(vict == ch, "You are using %s to conjure.", decapitalize_a_an(obj));
					FALSE_CASE_PRINTF(vict != ch, "%s is in use.", CAP(GET_OBJ_NAME(obj)));
				}
			}
			break;
		}
	}

	else if (GET_OBJ_TYPE(obj) == ITEM_WORKSHOP)
	{
		if (GET_WORKSHOP_GRADE(obj) > TYPE_KIT)
		{
			FALSE_CASE_PRINTF(GET_WORKSHOP_IS_SETUP(obj), "You may wish to pack %s up first.", decapitalize_a_an(obj));
			FALSE_CASE_PRINTF(GET_WORKSHOP_UNPACK_TICKS(obj), "%s is in use right now.", CAP(GET_OBJ_NAME(obj)));
		}
	}

	if (!(CAN_WEAR(obj, ITEM_WEAR_TAKE)) && !obj->in_veh)
	{
		if (access_level(ch, LVL_PRESIDENT))
		{
			send_to_char(ch, "You bypass the !TAKE flag on %s.", decapitalize_a_an(obj));
		}
		else
		{
			if (GET_OBJ_TYPE(obj) == ITEM_DESTROYABLE)
			{
				send_to_char(ch, "You can't pick %s up%s", decapitalize_a_an(obj), IS_RIGGING(ch) ? "." : ", but you're pretty sure you could ##^WDESTROY^n it.");
			}
			else
			{
				send_to_char(ch, "You can't take %s.", decapitalize_a_an(obj));
			}
			return FALSE;
		}
	}

	// If it's quest-protected and you're not the questor...
	FALSE_CASE_PRINTF(ch_is_blocked_by_quest_protections(ch, obj, FALSE, TRUE), "%s is someone else's quest item.", CAP(GET_OBJ_NAME(obj)));

	return TRUE;
}

int perform_get_from_room(struct char_data *ch, struct obj_data *obj)
{
	// Error messages sent in function.
	if (!can_take_obj_from_anywhere(ch, obj))
		return FALSE;

	// Error messages sent in function.
	if (!can_take_obj_from_room(ch, obj))
		return FALSE;

	// Anti-bot measure: Small chance to fumble quest objects.
	if (GET_OBJ_QUEST_CHAR_ID(obj) && number(1, 50) == 2)
	{
		send_to_char(ch, "You attempt to pick up %s, but fumble it.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));
		return FALSE;
	}

	if (GET_OBJ_TYPE(obj) == ITEM_WORKSHOP)
	{
		for (struct char_data *tmp = ch->in_veh ? ch->in_veh->people : ch->in_room->people; tmp; tmp = ch->in_veh ? tmp->next_in_veh : tmp->next_in_room)
			if (AFF_FLAGGED(tmp, AFF_PACKING))
			{
				if (tmp == ch)
					send_to_char(ch, "You're already working on %s.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));
				else
					send_to_char(ch, "Someone is working on %s.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));
				return FALSE;
			}
	}

	{
		char *representation = generate_new_loggable_representation(obj);

		if (obj->dropped_by_char > 0 && obj->dropped_by_char != GET_IDNUM(ch) && ch->desc)
		{
			// Ensure that the object and character both have valid hosts, and convert them from IP to lookup if possible.
			rectify_obj_host(obj);
			rectify_desc_host(ch->desc);

			if (obj->dropped_by_host && !str_cmp(obj->dropped_by_host, ch->desc->host))
			{
				// Warn anyone who's not on a multibox host (Grapevine, etc)
				if (!is_approved_multibox_host(ch->desc->host))
				{
					// TODO: Turn this back on once you've gotten account logic improved.
					// send_to_char(ch, "^y(Warning: %s^y was dropped or donated by someone at your same host. Please be certain that you are not accidentally transferring items between your own characters.)^n\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));
				}

				// Log anyone doing this from a multibox host.
				const char *pname = get_player_name(obj->dropped_by_char);
				mudlog_vfprintf(ch, LOG_CHEATLOG, "%s getting from room: %s (cost %d), which was dropped/donated by %s (%ld) at their same host (%s)!",
												GET_CHAR_NAME(ch),
												representation,
												GET_OBJ_COST(obj),
												pname,
												obj->dropped_by_char,
												GET_LEVEL(ch) < LVL_PRESIDENT ? obj->dropped_by_host : "<obscured>");
				delete[] pname;
			}
		}
		else if ((!IS_NPC(ch) && access_level(ch, LVL_BUILDER)) || IS_OBJ_STAT(obj, ITEM_EXTRA_WIZLOAD) || IS_OBJ_STAT(obj, ITEM_EXTRA_CHEATLOG_MARK))
		{
			snprintf(buf, sizeof(buf), "%s gets from room: %s", GET_CHAR_NAME(ch), representation);
			mudlog(buf, ch, IS_OBJ_STAT(obj, ITEM_EXTRA_WIZLOAD) ? LOG_WIZITEMLOG : LOG_CHEATLOG, TRUE);
		}

		set_dropped_by_info(obj, NULL);
		delete[] representation;
	}

	obj_from_room(obj);
	obj_to_char(obj, ch);
	act("You get $p.", FALSE, ch, obj, 0, TO_CHAR);
	if (ch->in_veh)
	{
		snprintf(buf, sizeof(buf), "%s gets %s.\r\n", GET_NAME(ch), decapitalize_a_an(GET_OBJ_NAME(obj)));
		send_to_veh(buf, ch->in_veh, ch, FALSE);
	}
	else
		act("$n gets $p.", FALSE, ch, obj, 0, TO_ROOM);
	get_check_money(ch, obj, NULL);
	affect_total(ch);
	return 1;
}

void get_from_room(struct char_data *ch, char *arg)
{
	struct obj_data *obj, *next_obj;
	int dotmode, found = 0;
	dotmode = find_all_dots(arg, sizeof(arg));

	if (dotmode == FIND_INDIV)
	{
		if (ch->in_veh)
			obj = get_obj_in_list_vis(ch, arg, ch->in_veh->contents);
		else
			obj = get_obj_in_list_vis(ch, arg, ch->in_room->contents);
		if (!obj)
		{
			// Attempt to pick up a vehicle.
			struct veh_data *veh;
			if ((veh = get_veh_list(arg, ch->in_veh ? ch->in_veh->carriedvehs : ch->in_room->vehicles, ch)))
			{
				int vehicle_weight = calculate_vehicle_weight(veh);
				rnum_t vehicle_storage_rnum = real_room(RM_PORTABLE_VEHICLE_STORAGE);

				if (vehicle_storage_rnum < 0)
				{
					send_to_char("Whoops-- looks like this system is offline!\r\n", ch);
					mudlog("SYSERR: Got negative rnum when looking up RM_PORTABLE_VEHICLE_STORAGE!", ch, LOG_SYSLOG, TRUE);
					return;
				}

				// No taking vehicles you can't carry.
				if (vehicle_weight + IS_CARRYING_W(ch) > CAN_CARRY_W(ch))
				{
					send_to_char(ch, "Seeing as how it weighs %dkg, %s is too heavy for you to carry.\r\n", vehicle_weight, GET_VEH_NAME(veh));
					return;
				}

				// Error messages sent in function.
				if (!can_take_veh(ch, veh))
				{
					return;
				}

				// You can carry it. Now comes the REAL bullshit. We bundle the vehicle up by transferring it to
				// a special room and giving you an item with enough values to point to it conclusively.

				// Initialize the vehicle container.
				struct obj_data *container = read_object(OBJ_VEHCONTAINER, VIRTUAL, OBJ_LOAD_REASON_VEHCONT);

				if (!container)
				{
					send_to_char("Something went wrong! Don't worry, the vehicle is untouched. Please use the BUG command and tell us about what happened here.\r\n", ch);
					mudlog("SYSERR: Unable to read_object() on OBJ_VEHCONTAINER, did it not get built?", ch, LOG_SYSLOG, TRUE);
					return;
				}

				// Set the container's weight to match the vehicle.
				GET_OBJ_WEIGHT(container) = vehicle_weight;

				// Set the container's values to help us positively ID the vehicle.
				GET_VEHCONTAINER_VEH_VNUM(container) = GET_VEH_VNUM(veh);
				GET_VEHCONTAINER_VEH_IDNUM(container) = veh->idnum;
				GET_VEHCONTAINER_VEH_OWNER(container) = veh->owner;

				// To avoid having to add another field to the pfiles_inv table, I've done something horrifyingly hacky and co-opted a value.
				// For vehicle containers, value 11 is the weight of the container.
				GET_VEHCONTAINER_WEIGHT(container) = GET_OBJ_WEIGHT(container);

				snprintf(buf, sizeof(buf), "^y%s^n (carried vehicle)", GET_VEH_NAME(veh));
				container->restring = str_dup(buf);

				// Give the object to the character.
				obj_to_char(container, ch);

				// Sanity check: If they're not carrying it for whatever reason, abort.
				if (container->carried_by != ch)
				{
					send_to_char("Whoops, something went wrong. Don't worry, the vehicle is untouched. Please use the BUG command and tell us what happened here.\r\n", ch);
					mudlog("SYSERR: Unable to give container object to character when picking up vehicle-- aborted!", ch, LOG_SYSLOG, TRUE);

					extract_obj(container);
					return;
				}

				// Move the vehicle to the storage room.
				veh_from_room(veh);
				veh_to_room(veh, &world[vehicle_storage_rnum]);

				// Finally, message the room.
				send_to_char(ch, "With a grunt, you lift %s.\r\n", GET_VEH_NAME(veh));
				snprintf(buf, sizeof(buf), "With a grunt, $n picks up %s.", GET_VEH_NAME(veh));
				act(buf, FALSE, ch, 0, 0, TO_ROOM);

				const char *owner = get_player_name(veh->owner);
				snprintf(buf, sizeof(buf), "%s (%ld) picked up vehicle %s (%ld, idnum %ld) belonging to %s (%ld).",
								 GET_CHAR_NAME(ch),
								 GET_IDNUM(ch),
								 GET_VEH_NAME(veh),
								 GET_VEH_VNUM(veh),
								 veh->idnum,
								 owner,
								 veh->owner);
				mudlog(buf, ch, GET_IDNUM(ch) != veh->owner ? LOG_CHEATLOG : LOG_GRIDLOG, TRUE);
				DELETE_ARRAY_IF_EXTANT(owner);

				playerDB.SaveChar(ch);
				save_single_vehicle(veh);
				return;
			}

			// Didn't find a vehicle, either.
			send_to_char(ch, "You don't see anything named '%s' here.\r\n", arg);
		}
		else
		{
			if (CAN_SEE_OBJ(ch, obj))
			{
				if (IS_OBJ_STAT(obj, ITEM_EXTRA_CORPSE) && GET_OBJ_VAL(obj, 4) == 1 && !IS_SENATOR(ch))
				{
					if (GET_OBJ_VAL(obj, 5) != GET_IDNUM(ch))
					{
						send_to_char("It's not yours, chummer... better leave it be.\r\n", ch);
					}
					else
					{
						send_to_char("That's not movable-- use ##^WGET ALL BELONGINGS^n to empty it out!\r\n", ch);
					}
				}
				else
				{
					perform_get_from_room(ch, obj);
				}
				found = 1;
			}
		}
	}
	else
	{
		if (dotmode == FIND_ALLDOT && !*arg)
		{
			send_to_char("Get all of what?\r\n", ch);
			return;
		}

		if (ch->in_veh)
			obj = ch->in_veh->contents;
		else
			obj = ch->in_room->contents;

		for (; obj; obj = next_obj)
		{
			next_obj = obj->next_content;
			if (ch->in_veh && ch->vfront != obj->vfront)
				continue;
			if (CAN_SEE_OBJ(ch, obj) &&
					(dotmode == FIND_ALL || isname(arg, obj->text.keywords)))
			{
				found = 1;
				if (IS_OBJ_STAT(obj, ITEM_EXTRA_CORPSE) && GET_OBJ_VAL(obj, 4) == 1 && GET_OBJ_VAL(obj, 5) != GET_IDNUM(ch) && !access_level(ch, LVL_FIXER))
					send_to_char("It's not yours chummer...better leave it be.\r\n", ch);
				else
				{
					perform_get_from_room(ch, obj);
				}
			}
		}
		if (!found)
		{
			if (dotmode == FIND_ALL)
				send_to_char("There doesn't seem to be anything here.\r\n", ch);
			else
			{
				send_to_char(ch, "You don't see anything named '%s' here.\r\n", arg);
			}
		}
	}
}

void get_while_rigging(struct char_data *ch, char *argument)
{
	struct veh_data *veh;
	char obj_name[MAX_INPUT_LENGTH];
	char container_name[MAX_INPUT_LENGTH];

	RIG_VEH(ch, veh);

	FAILURE_CASE(!veh, "You must be rigging a vehicle to do that.");
	FAILURE_CASE_PRINTF(!get_veh_grabber(veh), "%s is not equipped with a manipulator arm.", CAP(GET_VEH_NAME_NOFORMAT(veh)));
	FAILURE_CASE(!*argument, "Get what?");

	if (!veh->in_veh && !veh->in_room)
	{
		send_to_char(ch, "You have no idea where %s is right now.", GET_VEH_NAME(veh));
		mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: get_while_rigging(%s, %s) encountered veh %s (%ld / %ld) with no room or containing veh!",
										GET_CHAR_NAME(ch),
										argument,
										GET_VEH_NAME(veh),
										GET_VEH_VNUM(veh),
										GET_VEH_IDNUM(veh));
		return;
	}

	two_arguments(argument, obj_name, container_name);

	FAILURE_CASE(!*obj_name, "Syntax: GET <name>; or GET <name> FROM <container>");

	int obj_dotmode = find_all_dots(obj_name, sizeof(obj_name));

	// GET X [from] Y
	if (*container_name)
	{
		struct obj_data *cont;
		int cont_dotmode = find_all_dots(container_name, sizeof(container_name));

		// If the mode is FIND_INDIV, they want to get from one contianer, potentially prefixed with a number dot.
		if (cont_dotmode == FIND_INDIV)
		{
			cont = get_obj_in_list_vis(ch, container_name, veh->in_room ? veh->in_room->contents : veh->in_veh->contents);
			FAILURE_CASE_PRINTF(!cont, "You don't see anything named '%s' around your vehicle.", container_name);
			FAILURE_CASE_PRINTF(!container_is_vehicle_accessible(cont), "You can't get anything from %s with your manipulator arm.", decapitalize_a_an(cont));
			FAILURE_CASE_PRINTF(!cont->contains, "%s is empty.", CAP(GET_OBJ_NAME(cont)));
			// Error messages are sent in veh_get_from_container.
			veh_get_from_container(veh, cont, obj_name, obj_dotmode, ch);
		}
		// Otherwise, they want to get from all containers.
		else
		{
			bool found_containers = FALSE, found_something = FALSE;
			for (struct obj_data *cont = veh->in_room ? veh->in_room->contents : veh->in_veh->contents; cont; cont = cont->next_content)
			{
				if ((cont_dotmode == FIND_ALL || keyword_appears_in_obj(container_name, cont)) && container_is_vehicle_accessible(cont))
				{
					found_containers = TRUE;
					// Error messages are sent in veh_get_from_container.
					found_something |= veh_get_from_container(veh, cont, obj_name, obj_dotmode, ch);
				}
			}
			FAILURE_CASE(!found_containers, "There are no manipulator-accessible containers around your vehicle.");
			FAILURE_CASE_PRINTF(!found_something && obj_dotmode != FIND_ALL, "You don't see anything named '%s' in the manipulator-accessible containers around your vehicle.", obj_name);
		}
		return;
	}

	// GET X
	if (obj_dotmode == FIND_INDIV)
	{
		struct obj_data *obj = get_obj_in_list_vis(ch, obj_name, veh->in_room ? veh->in_room->contents : veh->in_veh->contents);
		if (!obj)
		{
			struct veh_data *target_veh = get_veh_list(obj_name, veh->in_room ? veh->in_room->vehicles : veh->in_veh->carriedvehs, ch);
			FAILURE_CASE_PRINTF(!target_veh, "You don't see anything named '%s' around your vehicle.", obj_name);
			veh_get_from_room(veh, target_veh, ch);
		}
		else
		{
			// Error messages are sent in veh_get_from_room.
			veh_get_from_room(veh, obj, ch);
		}
	}
	else
	{
		bool found_something = FALSE;
		for (struct obj_data *obj = veh->in_room ? veh->in_room->contents : veh->in_veh->contents, *next_obj; obj; obj = next_obj)
		{
			next_obj = obj->next_content;
			if (obj_dotmode == FIND_ALL || keyword_appears_in_obj(obj_name, obj))
			{
				found_something = TRUE;
				// Error messages are sent in veh_get_from_room.
				veh_get_from_room(veh, obj, ch);
			}
		}
		for (struct veh_data *target_veh = veh->in_room ? veh->in_room->vehicles : veh->in_veh->carriedvehs, *next_veh; target_veh; target_veh = next_veh)
		{
			next_veh = target_veh->next_veh;
			if (target_veh == veh)
				continue;

			if (obj_dotmode == FIND_ALL || keyword_appears_in_veh(obj_name, target_veh))
			{
				found_something = TRUE;
				// Error messages are sent in veh_get_from_room.
				veh_get_from_room(veh, target_veh, ch);
			}
		}
		FAILURE_CASE_PRINTF(!found_something, "You don't see anything named '%s' around your vehicle.", obj_name);
	}
	return;
}

ACMD(do_get)
{
	char arg1[MAX_INPUT_LENGTH];
	char arg2[MAX_INPUT_LENGTH];

	int cont_dotmode, found = 0, mode, skill = 0, target = 0, kit = 0;
	struct obj_data *cont, *obj, *temp, *shop = NULL;
	struct veh_data *veh = NULL;
	struct char_data *tmp_char;
	bool cyberdeck = (subcmd == SCMD_UNINSTALL);

	FAILURE_CASE(IS_ASTRAL(ch), "You cannot grasp physical objects.");

	if (IS_RIGGING(ch))
	{
		get_while_rigging(ch, argument);
		return;
	}

	const char *remainder = two_arguments(argument, arg1, arg2);

	FAILURE_CASE(IS_CARRYING_N(ch) >= CAN_CARRY_N(ch), "Your arms are already full! Try putting something in a container first.");
	FAILURE_CASE_PRINTF(!*arg1, "%s what?", (cyberdeck ? "Uninstall" : "Get"));

	// Cyberware compartments.
	bool is_tooth = !str_cmp(arg1, "tooth");
	bool is_fingertip = !str_cmp(arg1, "finger");
	bool is_body = !str_cmp(arg1, "body");
	if (is_tooth || is_fingertip || is_body)
	{
		if (is_tooth)
		{
			FAILURE_CASE(!(cont = find_cyberware(ch, CYB_TOOTHCOMPARTMENT)), "You don't have a tooth compartment.");
			FAILURE_CASE(!cont->contains, "Your tooth compartment is empty.");
		}
		else if (is_fingertip)
		{
			FAILURE_CASE(!(cont = find_cyberware(ch, CYB_FINGERTIP)), "You don't have a fingertip compartment.");
			FAILURE_CASE(!cont->contains, "Your fingertip compartment is empty.");
		}
		else
		{
			FAILURE_CASE(!(cont = find_cyberware(ch, CYB_BODYCOMPART)), "You don't have a body compartment.");
			FAILURE_CASE(!cont->contains, "Your body compartment is empty.");
		}

		obj = cont->contains;
		obj_from_obj(obj);
		obj_to_char(obj, ch);

		if (is_tooth)
		{
			send_to_char(ch, "You remove %s from your tooth compartment.\r\n", decapitalize_a_an(obj));
			act("$n reaches into $s mouth and removes $p^n.", TRUE, ch, obj, obj, TO_ROOM);
		}
		else if (is_fingertip)
		{
			send_to_char(ch, "You remove %s from your fingertip compartment.\r\n", decapitalize_a_an(obj));
			act("$n removes $p from a fingertip compartment.", TRUE, ch, obj, obj, TO_ROOM);
		}
		else
		{
			send_to_char(ch, "You remove %s from your body compartment.\r\n", decapitalize_a_an(obj));
			act("$n removes $p from a body compartment.", TRUE, ch, obj, obj, TO_ROOM);
		}
		return;
	}

	// GET <name>.
	if (!*arg2)
	{
		// Prevent ambiguous 'take drug' command.
		if (subcmd == SCMD_TAKE)
		{
			for (int i = MIN_DRUG; i < NUM_DRUGS; i++)
			{
				if (!str_cmp(drug_types[i].name, arg1))
				{
					send_to_char("Command ambiguous-- please either ^WGET^n or ^WUSE^n drugs.\r\n", ch);
					return;
				}
			}
		}
		get_from_room(ch, arg1);
		return;
	}

	cont_dotmode = find_all_dots(arg2, sizeof(arg2));
	if (cont_dotmode == FIND_INDIV)
	{
		mode = generic_find(arg2, FIND_OBJ_EQUIP | FIND_OBJ_INV | FIND_OBJ_ROOM, ch, &tmp_char, &cont);
		if (!ch->in_veh || (ch->in_veh->flags.IsSet(VFLAG_WORKSHOP) && !ch->vfront))
			veh = get_veh_list(arg2, ch->in_veh ? ch->in_veh->carriedvehs : ch->in_room->vehicles, ch);
		if (cyberdeck && veh)
		{
			cont = NULL;
			if (!veh->owner || (veh->locked && veh->owner != GET_IDNUM(ch)))
			{
				snprintf(buf, sizeof(buf), "%s's anti-theft measures beep loudly.\r\n", capitalize(GET_VEH_NAME_NOFORMAT(veh)));
				act(buf, FALSE, ch, 0, 0, TO_ROOM);
				send_to_char(buf, ch);
				return;
			}
			if (veh->cspeed > SPEED_OFF)
			{
				send_to_char("It's moving a little fast...\r\n", ch);
				return;
			}
			for (found = 0; found < NUM_MODS; found++)
			{
				if (GET_MOD(veh, found))
				{
					if (isname(arg1, GET_MOD(veh, found)->text.name))
					{
						cont = GET_MOD(veh, found);
						break;
					}
				}
			}
			if (!cont && veh->mount)
			{
				for (obj = veh->mount; obj; obj = obj->next_content)
					if (isname(arg1, obj->text.name))
					{
						cont = obj;
						cont_dotmode = 1;
						break;
					}
			}
			if (!cont)
			{
				send_to_char(ch, "There doesn't seem to be %s %s installed on %s.\r\n",
										 (is_abbrev(arg1, "autonav") || is_abbrev(arg1, "gridguide")) ? "an aftermarket" : AN(arg1),
										 arg1,
										 GET_VEH_NAME(veh));
				return;
			}
			else
			{
				if (!IS_NPC(ch))
				{
					skill = get_br_skill_for_veh(veh);

					switch (GET_VEHICLE_MOD_TYPE(cont))
					{
					case TYPE_ENGINECUST:
						target = 6;
						break;
					case TYPE_TURBOCHARGER:
						target = 2 + GET_OBJ_VAL(cont, 2);
						break;
					case TYPE_AUTONAV:
						target = 8 - veh->handling;
						break;
					case TYPE_CMC:
					case TYPE_DRIVEBYWIRE:
						target = 10 - veh->handling;
						break;
					case TYPE_ARMOR:
					case TYPE_CONCEALEDARMOR:
						target = (int)(GET_OBJ_VAL(cont, 2) / 3);
						break;
					case TYPE_ROLLBARS:
					case TYPE_TIRES:
					case TYPE_MISC:
					case TYPE_POKEYSTICK:
						target = 3;
						break;
					case TYPE_AUTOPILOT:
						target = 8 - veh->handling;
						break;
					default:
						target = 4;
						break;
					}
					skill = get_skill(ch, skill, target);
					target += modify_target(ch);
					kit = has_kit(ch, TYPE_VEHICLE);
					if ((shop = find_workshop(ch, TYPE_VEHICLE)))
						kit = GET_OBJ_VAL(shop, 0);
					if (!kit && !shop)
					{
						send_to_char("You don't have any tools here for working on vehicles.\r\n", ch);
						return;
					}
					if (kit < mod_types[GET_OBJ_VAL(cont, 0)].tools)
					{
						send_to_char(ch, "You don't have the right tools for that job.\r\n");
						return;
					}
					else if (mod_types[GET_OBJ_VAL(cont, 0)].tools == TYPE_KIT)
					{
						if (kit == TYPE_WORKSHOP)
							target--;
						else if (kit == TYPE_FACILITY)
							target -= 3;
					}
					else if (mod_types[GET_OBJ_VAL(cont, 0)].tools == TYPE_WORKSHOP && kit == TYPE_FACILITY)
						target--;
					if (GET_OBJ_VAL(cont, 0) == TYPE_ENGINECUST)
						veh->engine = 0;
					if (success_test(skill, target) < 1)
					{
						send_to_char(ch, "You can't figure out how to uninstall %s. \r\n", GET_OBJ_NAME(cont));
						return;
					}
				}
				if (GET_VEHICLE_MOD_TYPE(cont) == TYPE_MOUNT)
				{
					// Check to see if anyone is manning it.
					if (cont->worn_by)
					{
						send_to_char(ch, "Someone is manning %s.\r\n", GET_OBJ_NAME(cont));
						return;
					}
					// Make sure it's empty.
					if (cont->contains)
					{
						send_to_char(ch, "You'll have to remove the weapon from %s first.\r\n", GET_OBJ_NAME(cont));
						return;
					}
				}
				snprintf(buf, sizeof(buf), "$n goes to work on %s.", GET_VEH_NAME(veh));
				send_to_char(ch, "You go to work on %s and remove %s.\r\n", GET_VEH_NAME(veh), GET_OBJ_NAME(cont));
				act(buf, TRUE, ch, 0, 0, TO_ROOM);
				if (found == MOD_SEAT && cont->affected[0].modifier > 1)
				{
					cont->affected[0].modifier--;
					affect_veh(veh, cont->affected[0].location, -1);
					obj = read_object(GET_OBJ_VNUM(cont), VIRTUAL, OBJ_LOAD_REASON_UNINSTALL);
					cont = obj;
				}
				else if (cont_dotmode)
				{
					REMOVE_FROM_LIST(cont, veh->mount, next_content)
					veh->sig += get_mount_signature_penalty(cont);
					veh->usedload -= get_obj_vehicle_load_usage(cont, TRUE);
				}
				else
				{
					if (GET_VEHICLE_MOD_TYPE(cont) == TYPE_AUTONAV)
					{
						veh->autonav -= GET_VEHICLE_MOD_RATING(cont);
					}
					veh->usedload -= get_obj_vehicle_load_usage(cont, TRUE);
					GET_MOD(veh, found) = NULL;
					int rnum = real_vehicle(GET_VEH_VNUM(veh));
					if (rnum <= -1)
						send_to_char(ch, "Bro, your vehicle is _fucked_. Contact staff.\r\n");

					for (found = 0; found < MAX_OBJ_AFFECT; found++)
					{
						affect_veh(veh, cont->affected[found].location, -(cont->affected[found].modifier));

						switch (cont->affected[found].location)
						{
						case VAFF_SEN:
							if (veh->sensor <= 0)
								affect_veh(veh, VAFF_SEN, rnum >= 0 ? veh_proto[rnum].sensor : 0);
							break;
						case VAFF_AUTO:
							if (veh->autonav <= 0)
								affect_veh(veh, VAFF_AUTO, rnum >= 0 ? veh_proto[rnum].autonav : 0);
							break;
						case VAFF_PILOT:
							if (veh->pilot <= 0)
								affect_veh(veh, VAFF_PILOT, rnum >= 0 ? veh_proto[rnum].pilot : 0);
							break;
						}
					}
				}
				obj_to_char(cont, ch);
				return;
			}
		}
		else if (!cont)
		{
			send_to_char(ch, "You don't have %s %s.\r\n", AN(arg2), arg2);
		}
		else if ((!cyberdeck && !(GET_OBJ_TYPE(cont) == ITEM_CONTAINER || GET_OBJ_TYPE(cont) == ITEM_KEYRING || GET_OBJ_TYPE(cont) == ITEM_QUIVER || GET_OBJ_TYPE(cont) == ITEM_HOLSTER || GET_OBJ_TYPE(cont) == ITEM_WORN)) || (cyberdeck && !(GET_OBJ_TYPE(cont) == ITEM_CYBERDECK || GET_OBJ_TYPE(cont) == ITEM_CUSTOM_DECK || GET_OBJ_TYPE(cont) == ITEM_DECK_ACCESSORY)))
		{
			if (!cyberdeck && GET_OBJ_TYPE(cont) == ITEM_DECK_ACCESSORY && GET_DECK_ACCESSORY_TYPE(cont) == TYPE_COOKER)
			{
				// QOL: Treat it as uninstall.
				get_from_container(ch, cont, arg1, mode, is_abbrev("confirm", remainder));
			}
			else
			{
				snprintf(buf, sizeof(buf), "$p is not a %s.", (!cyberdeck ? "container" : "cyberdeck"));
				act(buf, FALSE, ch, cont, 0, TO_CHAR);

				if (access_level(ch, LVL_ADMIN) && !str_cmp(arg1, "force-all"))
				{
					send_to_char("Hoping you know what you're doing, you forcibly remove its contents anyways.\r\n", ch);
					struct obj_data *next;
					for (struct obj_data *contained = cont->contains; contained; contained = next)
					{
						next = contained->next_content;
						obj_from_obj(contained);
						obj_to_char(contained, ch);
						send_to_char(ch, "You retrieve %s from %s.\r\n", GET_OBJ_NAME(contained), GET_OBJ_NAME(cont));
					}
				}
			}
		}
		else
		{
			get_from_container(ch, cont, arg1, mode, is_abbrev("confirm", remainder));
		}
	}
	else
	{
		if (cont_dotmode == FIND_ALLDOT && !*arg2)
		{
			send_to_char("Get from all of what?\r\n", ch);
			return;
		}
		for (cont = ch->carrying; cont; cont = cont->next_content)
			if (CAN_SEE_OBJ(ch, cont) &&
					(cont_dotmode == FIND_ALL || isname(arg2, cont->text.keywords)))
			{
				if ((!cyberdeck && (GET_OBJ_TYPE(cont) == ITEM_CONTAINER || GET_OBJ_TYPE(cont) == ITEM_KEYRING)) ||
						(cyberdeck && (GET_OBJ_TYPE(cont) == ITEM_CYBERDECK || GET_OBJ_TYPE(cont) == ITEM_CUSTOM_DECK)))
				{
					found = 1;
					get_from_container(ch, cont, arg1, FIND_OBJ_INV);
				}
				else if (cont_dotmode == FIND_ALLDOT)
				{
					found = 1;
					snprintf(buf, sizeof(buf), "$p is not a %s", (!cyberdeck ? "container" : "cyberdeck"));
					act(buf, FALSE, ch, cont, 0, TO_CHAR);
				}
			}
		FOR_ITEMS_AROUND_CH(ch, cont)
		{
			if (CAN_SEE_OBJ(ch, cont) &&
					(cont_dotmode == FIND_ALL || isname(arg2, cont->text.keywords)))
			{
				if (GET_OBJ_TYPE(cont) == ITEM_CONTAINER || GET_OBJ_TYPE(cont) == ITEM_KEYRING)
				{
					get_from_container(ch, cont, arg1, FIND_OBJ_ROOM);
					found = 1;
				}
				else if (cont_dotmode == FIND_ALLDOT)
				{
					snprintf(buf, sizeof(buf), "$p is not a %s", (!cyberdeck ? "container" : "cyberdeck"));
					act(buf, FALSE, ch, cont, 0, TO_CHAR);
					found = 1;
				}
			}
		}
		if (!found)
		{
			if (cont_dotmode == FIND_ALL)
			{
				send_to_char(ch, "You can't seem to find any %s.\r\n", (!cyberdeck ? "containers" : "cyberdeck"));
			}
			else
			{
				send_to_char(ch, "You can't seem to find any %ss here.\r\n", arg2);
			}
		}
	}
}

void perform_drop_gold(struct char_data *ch, int amount, byte mode, struct room_data *random_donation_room)
{
	struct obj_data *obj;

	FAILURE_CASE(mode != SCMD_DROP, "You can't do that!");
	FAILURE_CASE(amount < 1, "You can't drop less than one nuyen.");
	FAILURE_CASE(amount > GET_NUYEN(ch), "You don't even have that much!");
	FAILURE_CASE(GET_LEVEL(ch) > LVL_MORTAL && !access_level(ch, LVL_PRESIDENT),
							 "Staff can't drop nuyen. Use the PAYOUT command to award a character.");

	obj = read_object(OBJ_ROLL_OF_NUYEN, VIRTUAL, OBJ_LOAD_REASON_DROP_GOLD);
	GET_OBJ_VAL(obj, 0) = amount;

	if (!IS_NPC(ch) && (access_level(ch, LVL_BUILDER) || IS_NPC(ch)))
		obj->obj_flags.extra_flags.SetBit(ITEM_EXTRA_WIZLOAD);

	obj->obj_flags.extra_flags.SetBit(ITEM_EXTRA_CHEATLOG_MARK);

	// Dropping money is not a sink.
	GET_NUYEN_RAW(ch) -= amount;
	act("You drop $p.", FALSE, ch, obj, 0, TO_CHAR);
	act("$n drops $p.", TRUE, ch, obj, 0, TO_ROOM);
	affect_total(ch);

	if (ch->in_veh)
	{
		obj_to_veh(obj, ch->in_veh);
		obj->vfront = ch->vfront;
	}
	else
		obj_to_room(obj, ch->in_room);

	snprintf(buf, sizeof(buf), "%s drops: %d nuyen *", GET_CHAR_NAME(ch), amount);
	mudlog(buf, ch, amount > 5 ? LOG_CHEATLOG : LOG_GRIDLOG, TRUE);
	set_dropped_by_info(obj, ch);

	return;
}

#define VANISH(mode) ((mode == SCMD_DONATE || mode == SCMD_JUNK) ? "  It vanishes into a recycling bin!" : "")

int perform_drop(struct char_data *ch, struct obj_data *obj, byte mode,
								 const char *sname, struct room_data *random_donation_room)
{
	int value;

	struct room_data *in_room = get_ch_in_room(ch);

	FALSE_CASE(mode == SCMD_DROP && ROOM_FLAGGED(in_room, ROOM_NO_DROP), "The game's administration requests that you do not drop anything here.");
	FALSE_CASE_PRINTF(mode != SCMD_JUNK && IS_OBJ_STAT(obj, ITEM_EXTRA_NODROP) && !IS_SENATOR(ch), "You can't %s %s, but you can always JUNK it.", sname, decapitalize_a_an(obj));
	FALSE_CASE_PRINTF(mode != SCMD_JUNK && obj_contains_items_with_flag(obj, ITEM_EXTRA_NODROP) && !IS_SENATOR(ch),
										"Action blocked: %s contains at least one no-drop item. You can JUNK that item if you want.", decapitalize_a_an(obj));

	FALSE_CASE_PRINTF(GET_OBJ_TYPE(obj) == ITEM_PET && (ch->in_veh || !in_room->apartment),
										mode == SCMD_DROP ? "%s will only deign to be set down in an apartment." : "%s just gives you a LOOK.",
										CAP(GET_OBJ_NAME(obj)));

	FALSE_CASE_PRINTF(IS_OBJ_STAT(obj, ITEM_EXTRA_KEPT) && !IS_SENATOR(ch), "You'll have to use the KEEP command on %s before you can %s it.", decapitalize_a_an(obj), sname);
	FALSE_CASE_PRINTF(obj == ch->char_specials.programming, "You can't %s something you are working on.", sname);
	FALSE_CASE_PRINTF(obj_contains_items_with_flag(obj, ITEM_EXTRA_KEPT) && !IS_SENATOR(ch),
										"Action blocked: %s contains at least one kept item. Pull it out and UNKEEP it.", decapitalize_a_an(obj));

	// We don't allow donation anymore for a variety of reasons, so we convert it to junking.
	bool mask_junk_messages_as_donate = (mode == SCMD_DONATE);
	if (mode == SCMD_DONATE)
		mode = SCMD_JUNK;

	// You must be in an apartment or vehicle in order to drop or donate economically-sensitive items.
	bool is_in_apt_veh_or_storage = ((ch->in_room && (ch->in_room->apartment || ROOM_FLAGGED(ch->in_room, ROOM_STORAGE))) || ch->in_veh);
	if (mode == SCMD_DONATE || (mode != SCMD_JUNK && !is_in_apt_veh_or_storage))
	{
		bool action_blocked = FALSE;
		struct room_data *target_room = get_ch_in_room(ch);

		if ((action_blocked |= obj_is_apartment_only_drop_item(obj, target_room)))
		{
			act("Action blocked: $p can only be dropped in apartments and vehicles.", FALSE, ch, obj, 0, TO_CHAR);
		}
		else if ((action_blocked |= obj_contains_apartment_only_drop_items(obj, target_room)))
		{
			act("Action blocked: $p contains at least one item that can only be dropped in apartments and vehicles.", FALSE, ch, obj, 0, TO_CHAR);
		}

		if (action_blocked)
		{
			if (IS_SENATOR(ch))
			{
				send_to_char("...As staff, you bypass the restriction and drop it anyways.\r\n", ch);
			}
			else
			{
				// Hardcoded start and end of the Neophyte Guild area. You can drop things you've picked up here, but only as a newbie.
				// Note that bypassing these checks by handing something to a newbie to drop in the donation area is considered exploiting.
				if (mode == SCMD_DONATE || (GET_TKE(ch) >= NEWBIE_KARMA_THRESHOLD && GET_ROOM_VNUM(target_room) >= 60500 && GET_ROOM_VNUM(target_room) <= 60699))
				{
					send_to_char(ch, "^L(OOC note: Please avoid giving newbies free cyberware / bioware / etc! It's a kind gesture, but it undercuts their economic balance and leads to less overall player retention.)^n\r\n");
				}

				return 0;
			}
		}
	}

	if (mode == SCMD_DONATE)
	{
		// No donating wizloaded or !sell / !rent items.
		FALSE_CASE_PRINTF(IS_OBJ_STAT(obj, ITEM_EXTRA_WIZLOAD), "You can't donate %s: It was given to you by staff.", decapitalize_a_an(obj));
		FALSE_CASE_PRINTF(IS_OBJ_STAT(obj, ITEM_EXTRA_NOSELL) || IS_OBJ_STAT(obj, ITEM_EXTRA_NORENT),
											"You can't donate %s: It's flagged !SELL or !RENT.", decapitalize_a_an(obj));

		// Check contents too.
		for (struct obj_data *contained = obj->contains; contained; contained = contained->next_content)
		{
			FALSE_CASE_PRINTF(IS_OBJ_STAT(contained, ITEM_EXTRA_WIZLOAD), "You can't donate %s: It was given to you by staff.", decapitalize_a_an(obj));
			FALSE_CASE_PRINTF(IS_OBJ_STAT(contained, ITEM_EXTRA_NOSELL) || IS_OBJ_STAT(obj, ITEM_EXTRA_NORENT),
												"You can't donate %s: It's flagged !SELL or !RENT.", decapitalize_a_an(obj));
		}
	}

	// Special handling: Vehicle containers.
	if (GET_OBJ_VNUM(obj) == OBJ_VEHCONTAINER)
	{
		FALSE_CASE_PRINTF(mode != SCMD_DROP, "You can't %s vehicles.", sname);
		FALSE_CASE(ch->in_veh, "You'll have to step out of your current vehicle to do that.");

    // Load vehicles owned by the identified owner to reduce cases of stuck containers.
    load_vehicles_for_idnum(GET_VEHCONTAINER_VEH_OWNER(obj));

		// Find the veh storage room.
		rnum_t vehicle_storage_rnum = real_room(RM_PORTABLE_VEHICLE_STORAGE);
		if (vehicle_storage_rnum < 0)
		{
			send_to_char("Whoops-- looks like this system is offline!\r\n", ch);
			mudlog("SYSERR: Got negative rnum when looking up RM_PORTABLE_VEHICLE_STORAGE!", ch, LOG_SYSLOG, TRUE);
			return 0;
		}

		// Search it for our vehicle.
		for (struct veh_data *veh = world[vehicle_storage_rnum].vehicles; veh; veh = veh->next_veh)
		{
			if (GET_VEH_VNUM(veh) == GET_VEHCONTAINER_VEH_VNUM(obj) && veh->idnum == GET_VEHCONTAINER_VEH_IDNUM(obj) && veh->owner == GET_VEHCONTAINER_VEH_OWNER(obj))
			{
				// Found it! Validate preconditions.

				// It'd be great if we could allow drones and bikes to be dropped anywhere not flagged !BIKE, but this
				// would cause issues with the current world-- the !bike flag is placed at entrances to zones, not
				// spread throughout the whole thing. People would just carry their bikes in, drop them, and do drivebys.
				bool can_be_dropped_here = FALSE;
				if (ch->in_veh)
				{
					if (ch->in_veh->usedload + calculate_vehicle_entry_load(veh) > ch->in_veh->load)
					{
						send_to_char(ch, "%s won't fit in here.\r\n", CAP(GET_VEH_NAME_NOFORMAT(veh)));
						return 0;
					}
					can_be_dropped_here = TRUE;
				}
				else if (ROOM_FLAGGED(in_room, ROOM_GARAGE) || ROOM_FLAGGED(in_room, ROOM_ALL_VEHICLE_ACCESS) || veh->damage >= VEH_DAM_THRESHOLD_DESTROYED)
				{
					can_be_dropped_here = TRUE;
				}
				else if (veh_can_traverse_land(veh) && ROOM_FLAGGED(in_room, ROOM_ROAD))
				{
					can_be_dropped_here = TRUE;
				}
				else if (veh_can_traverse_water(veh) && IS_WATER(in_room))
				{
					can_be_dropped_here = TRUE;
				}
				else if (veh_can_traverse_air(veh) && (ROOM_FLAGGED(in_room, ROOM_AIRCRAFT_CAN_DRIVE_HERE) || GET_ROOM_FLIGHT_CODE(in_room)))
				{
					can_be_dropped_here = TRUE;
				}

				if (!can_be_dropped_here)
				{
					// Exception: You can drop water vehicles on
					send_to_char(ch, "%s can't be set down here.\r\n", CAP(GET_VEH_NAME_NOFORMAT(veh)));
					return 0;
				}

				// Proceed to drop.
				veh_from_room(veh);
				veh_to_room(veh, ch->in_room);
				send_to_char(ch, "You set %s down with a sigh of relief.\r\n", GET_VEH_NAME(veh));
				snprintf(buf, sizeof(buf), "$n sets %s down with a sigh of relief.", GET_VEH_NAME(veh));
				act(buf, FALSE, ch, 0, 0, TO_ROOM);

				const char *owner = get_player_name(veh->owner);
				snprintf(buf, sizeof(buf), "%s (%ld) dropped vehicle %s (%ld, idnum %ld) belonging to %s (%ld) in %s.",
								 GET_CHAR_NAME(ch),
								 GET_IDNUM(ch),
								 GET_VEH_NAME(veh),
								 GET_VEH_VNUM(veh),
								 veh->idnum,
								 owner,
								 veh->owner,
								 GET_ROOM_NAME(veh->in_room));
				mudlog(buf, ch, LOG_CHEATLOG, TRUE);
				DELETE_ARRAY_IF_EXTANT(owner);

				// Clear the values to prevent bug logging, then remove the object from inventory.
				GET_VEHCONTAINER_VEH_VNUM(obj) = GET_VEHCONTAINER_VEH_IDNUM(obj) = GET_VEHCONTAINER_VEH_OWNER(obj) = 0;
				extract_obj(obj);

				playerDB.SaveChar(ch);
				save_single_vehicle(veh);
				return 0;
			}
		}

		send_to_char(ch, "Error: we couldn't find a matching vehicle for %s! Please alert staff.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));
		snprintf(buf, sizeof(buf), "SYSERR: Failed to find matching vehicle for container %s!", decapitalize_a_an(GET_OBJ_NAME(obj)));
		mudlog(buf, ch, LOG_SYSLOG, TRUE);
		return 0;
	}

	// A portion of all donated items are extracted instead.
	if (mode == SCMD_DONATE && !number(0, 6))
		mode = SCMD_JUNK;

	if (mode == SCMD_DONATE || mode == SCMD_JUNK)
	{
		if (GET_OBJ_VNUM(obj) == OBJ_NEOPHYTE_SUBSIDY_CARD && GET_OBJ_VAL(obj, 1) > 0)
		{
			// TODO: Make it so you can use partial amounts for rent payments- this will suck with 1 nuyen left.
			send_to_char(ch, "You can't %s a subsidy card that still has nuyen on it!\r\n", sname);
			return 0;
		}

		switch (GET_OBJ_TYPE(obj))
		{
		case ITEM_CUSTOM_DECK:
		case ITEM_CYBERDECK:
			if (mode == SCMD_DONATE)
			{
				send_to_char("You can't donate cyberdecks!\r\n", ch);
				return 0;
			}
			if (obj->contains && mode == SCMD_JUNK)
			{
				send_to_char("You can't junk a cyberdeck that has components installed!\r\n", ch);
				return 0;
			}
			break;
		case ITEM_CONTAINER:
		case ITEM_KEYRING:
		case ITEM_HOLSTER:
		case ITEM_WORN:
		case ITEM_QUIVER:
			if (obj->contains)
			{
				send_to_char(ch, "You'll have to empty %s before you can %s it.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)), sname);
				return 0;
			}
			break;
		case ITEM_MONEY:
			if (GET_ITEM_MONEY_VALUE(obj) >= 200)
			{
				send_to_char(ch, "There's too much nuyen on %s to %s it.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)), sname);
				return 0;
			}
			break;
		case ITEM_VEHCONTAINER:
			send_to_char(ch, "You struggle to find a receptacle big enough to fit %s.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));
			return 0;
		case ITEM_SHOPCONTAINER:
			if (mode == SCMD_DONATE)
			{
				send_to_char(ch, "That's very kind of you, but you can't donate %s.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));
				return 0;
			}
			break;
		}
	}

	if (ch->in_veh)
	{
		if (mode != SCMD_DONATE && mode != SCMD_JUNK)
		{
			if (ch->in_veh->usedload + GET_OBJ_WEIGHT(obj) > ch->in_veh->load)
			{
				send_to_char("There is too much in the vehicle already!\r\n", ch);
				return 0;
			}
			if (ch->vfront && ch->in_veh->seating[SEATING_REAR] > 0 && ch->in_veh->usedload + GET_OBJ_WEIGHT(obj) > ch->in_veh->load / 10)
			{
				send_to_char("There is too much in the front of the vehicle! Try using the ^WSWITCH^n command.\r\n", ch);
				return 0;
			}
		}

		snprintf(buf, sizeof(buf), "%s %ss %s.%s\r\n", GET_NAME(ch), sname, GET_OBJ_NAME(obj), VANISH(mode));
		send_to_veh(buf, ch->in_veh, ch, FALSE);
	}
	else
	{
		// If you're dropping something in the donation area, zero its cost.
		if (mode == SCMD_DROP)
		{
			// Pull from config.cpp
			extern vnum_t donation_room_1;
			extern vnum_t donation_room_2;
			extern vnum_t donation_room_3;

			// If you're in a donation room, zero it.
			if (GET_ROOM_VNUM(ch->in_room) == donation_room_1 || GET_ROOM_VNUM(ch->in_room) == donation_room_2 || GET_ROOM_VNUM(ch->in_room) == donation_room_3)
			{
				zero_cost_of_obj_and_contents(obj);
			}
		}
		snprintf(buf, sizeof(buf), "$n %ss $p.%s", sname, VANISH(mode));
		act(buf, TRUE, ch, obj, 0, TO_ROOM);
	}

	snprintf(buf, sizeof(buf), "You %s $p.%s", sname, VANISH(mode));
	act(buf, FALSE, ch, obj, 0, TO_CHAR);
	if (obj->in_obj)
		obj_from_obj(obj);
	else
		obj_from_char(obj);
	affect_total(ch);

	if ((mode == SCMD_DONATE) && IS_OBJ_STAT(obj, ITEM_EXTRA_NODONATE))
		mode = SCMD_JUNK;

	if ((!IS_NPC(ch) && access_level(ch, LVL_BUILDER)) || IS_OBJ_STAT(obj, ITEM_EXTRA_WIZLOAD) || IS_OBJ_STAT(obj, ITEM_EXTRA_CHEATLOG_MARK))
	{
		char *representation = generate_new_loggable_representation(obj);
		snprintf(buf, sizeof(buf), "%s %ss: %s", GET_CHAR_NAME(ch),
						 mask_junk_messages_as_donate ? "donate-junk" : sname,
						 representation);
		mudlog(buf, ch, IS_OBJ_STAT(obj, ITEM_EXTRA_WIZLOAD) ? LOG_WIZITEMLOG : LOG_CHEATLOG, TRUE);
		delete[] representation;
	}

	switch (mode)
	{
	case SCMD_DROP:
		if (ch->in_veh)
		{
			obj_to_veh(obj, ch->in_veh);
			obj->vfront = ch->vfront;
		}
		else
			obj_to_room(obj, ch->in_room);
		if (COULD_BE_ON_QUEST(ch))
			check_quest_delivery(ch, obj);
		break;
	case SCMD_DONATE:
		// Wipe its value with the make_newbie() function to prevent reselling.
		zero_cost_of_obj_and_contents(obj);
		// Move it to the donation room.
		obj_to_room(obj, random_donation_room);
		if (random_donation_room->people)
			act("You notice $p exposed beneath the junk.", FALSE, random_donation_room->people, obj, 0, TO_ROOM);
		break;
	case SCMD_JUNK:
		value = MAX(1, MIN(200, GET_OBJ_COST(obj) >> 4));
		if (COULD_BE_ON_QUEST(ch))
			check_quest_destroy(ch, obj);
		extract_obj(obj);
		return value;
	default:
		log("SYSERR: Incorrect argument passed to perform_drop");
		break;
	}

	set_dropped_by_info(obj, ch);

	return 0;
}

void drop_while_rigging(struct char_data *ch, char *argument)
{
	struct veh_data *veh;
	char obj_name[MAX_INPUT_LENGTH];

	RIG_VEH(ch, veh);

	FAILURE_CASE(!veh, "You must be rigging a vehicle to do that.");
	FAILURE_CASE_PRINTF(!get_veh_grabber(veh), "%s is not equipped with a manipulator arm.", CAP(GET_VEH_NAME_NOFORMAT(veh)));
	FAILURE_CASE(!*argument, "Get what?");

	if (!veh->in_veh && !veh->in_room)
	{
		send_to_char(ch, "You have no idea where %s is right now.", GET_VEH_NAME(veh));
		mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: get_while_rigging(%s, %s) encountered veh %s (%ld / %ld) with no room or containing veh!",
										GET_CHAR_NAME(ch),
										argument,
										GET_VEH_NAME(veh),
										GET_VEH_VNUM(veh),
										GET_VEH_IDNUM(veh));
		return;
	}

	one_argument(argument, obj_name);

	FAILURE_CASE(!*obj_name, "Syntax: DROP <name>");

	int obj_dotmode = find_all_dots(obj_name, sizeof(obj_name));

	// DROP X
	if (obj_dotmode == FIND_INDIV)
	{
		// Put us in the back of the vehicle, then search it.
		bool old_vfront = ch->vfront;
		ch->vfront = FALSE;
		struct obj_data *obj = get_obj_in_list_vis(ch, obj_name, veh->contents);
		ch->vfront = old_vfront;

		if (!obj)
		{
			struct veh_data *carried_veh = get_veh_list(obj_name, veh->carriedvehs, ch);
			FAILURE_CASE_PRINTF(!carried_veh, "You don't see anything named '%s' in your vehicle.", obj_name);
			// Error messages are sent in veh_drop_veh.
			veh_drop_veh(veh, carried_veh, ch);
		}
		else
		{
			// Error messages are sent in veh_drop_obj.
			veh_drop_obj(veh, obj, ch);
		}
	}
	else
	{
		FAILURE_CASE(obj_dotmode == FIND_ALL && !veh->contents, "There's nothing in your vehicle.");

		bool found_something = FALSE;
		for (struct obj_data *obj = veh->contents, *next_obj; obj; obj = next_obj)
		{
			next_obj = obj->next_content;
			if (obj_dotmode == FIND_ALL || keyword_appears_in_obj(obj_name, obj))
			{
				found_something = TRUE;
				// Error messages are sent in veh_drop_obj.
				veh_drop_obj(veh, obj, ch);
			}
		}
		for (struct veh_data *carried_veh = veh->carriedvehs, *next_veh; carried_veh; carried_veh = next_veh)
		{
			next_veh = carried_veh->next_veh;
			if (carried_veh == veh)
				continue;

			if (obj_dotmode == FIND_ALL || keyword_appears_in_veh(obj_name, carried_veh))
			{
				found_something = TRUE;
				// Error messages are sent in veh_drop_veh.
				veh_drop_veh(veh, carried_veh, ch);
			}
		}
		FAILURE_CASE_PRINTF(!found_something, "You don't see anything named '%s' in your vehicle.", obj_name);
	}
	return;
}

ACMD(do_drop)
{
	extern vnum_t donation_room_1;
	extern vnum_t donation_room_2; /* uncomment if needed! */
	extern vnum_t donation_room_3; /* uncomment if needed! */

	if (IS_RIGGING(ch))
	{
		drop_while_rigging(ch, argument);
		return;
	}

	FAILURE_CASE(IS_ASTRAL(ch), "Astral projections can't touch things!");
	// FAILURE_CASE(AFF_FLAGGED(ch, AFF_PILOT), "While driving? Now that would be a good trick!");
	FAILURE_CASE(PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED) && (subcmd == SCMD_DROP || subcmd == SCMD_DONATE),
							 "You cannot drop or donate items until you complete character generation.");
	FAILURE_CASE(IS_WORKING(ch), "You're too busy to do that.");

	struct obj_data *obj, *next_obj;
	struct room_data *random_donation_room = NULL;
	byte mode = SCMD_DROP;
	int dotmode, amount = 0;
	const char *sname;

	switch (subcmd)
	{
	case SCMD_JUNK:
		sname = "junk";
		mode = SCMD_JUNK;
		break;
	case SCMD_DONATE:
		sname = "donate";
		mode = SCMD_DONATE;
		/*
				switch (number(0, 2)) {
				case 0:
					random_donation_room = &world[real_room(donation_room_1)];
					break;
				case 1:
					random_donation_room = &world[real_room(donation_room_3)];
					break;
				case 2:
					random_donation_room = &world[real_room(donation_room_2)];
					break;
				}
				if (!random_donation_room && mode != SCMD_JUNK) {
					send_to_char("Sorry, you can't donate anything right now.\r\n", ch);
					return;
				}
		*/
		break;
	default:
		sname = "drop";
		break;
	}

	argument = one_argument(argument, arg);

	FAILURE_CASE_PRINTF(!*arg, "What do you want to %s?", sname);

	if (is_number(arg))
	{
		amount = atoi(arg);
		argument = one_argument(argument, arg);
		if (!str_cmp("nuyen", arg))
		{
			send_to_char("Nuyen can't be dropped. You can ^WGIVE^n it to another player to pay them, or store it in your bank account at an ATM.\r\n", ch);
			// perform_drop_gold(ch, amount, mode, random_donation_room);
		}
		else
		{
			/* code to drop multiple items.  anyone want to write it? -je */
			send_to_char("Sorry, you can't do that to more than one item at a time.\r\n", ch);
		}
		return;
	}
	else
	{
		dotmode = find_all_dots(arg, sizeof(arg));

		/* Can't junk or donate all */
		if ((dotmode == FIND_ALL) && (subcmd == SCMD_JUNK || subcmd == SCMD_DONATE))
		{
			if (subcmd == SCMD_JUNK)
				send_to_char("Go to the dump if you want to junk EVERYTHING!\r\n", ch);
			else
				send_to_char("Go to the junkyard if you want to donate EVERYTHING!\r\n", ch);
			return;
		}
		if (dotmode == FIND_ALL)
		{
			if (!ch->carrying)
				send_to_char("You don't seem to be carrying anything.\r\n", ch);
			else
				for (obj = ch->carrying; obj; obj = next_obj)
				{
					next_obj = obj->next_content;
					amount += perform_drop(ch, obj, mode, sname, random_donation_room);
				}
		}
		else if (dotmode == FIND_ALLDOT)
		{
			if (!*arg)
			{
				send_to_char(ch, "What do you want to %s all of?\r\n", sname);
				return;
			}
			if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying)))
			{
				send_to_char(ch, "You don't seem to have anything named '%s' in your inventory.\r\n", arg);
			}
			while (obj)
			{
				next_obj = get_obj_in_list_vis(ch, arg, obj->next_content);
				amount += perform_drop(ch, obj, mode, sname, random_donation_room);
				obj = next_obj;
			}
		}
		else
		{
			if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying)))
			{
				send_to_char(ch, "You don't seem to have anything named '%s' in your inventory.\r\n", arg);
			}
			else
				amount += perform_drop(ch, obj, mode, sname, random_donation_room);
		}
	}
	if (amount && (subcmd == SCMD_JUNK || subcmd == SCMD_DONATE) && !PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED))
	{
		int payout = amount >> 4;
		if (payout > 0)
		{
			send_to_char(ch, "You receive %d nuyen for recycling.\r\n", payout);
			gain_nuyen(ch, payout, NUYEN_INCOME_JUNKING);
		}
	}
}

void _ch_gives_obj_to_vict(struct char_data *ch, struct obj_data *obj, struct char_data *vict)
{
	if (!ch || !obj || !vict)
	{
		mudlog_vfprintf(ch, LOG_SYSLOG, "SYSERR: Invalid invocation of _ch_gives_obj_to_vict(%s, %s, %s)!",
										GET_CHAR_NAME(ch),
										GET_OBJ_NAME(obj),
										GET_CHAR_NAME(vict));
		return;
	}

	obj_from_char(obj);
	obj_to_char(obj, vict);
	act("You give $p to $N.", FALSE, ch, obj, vict, TO_CHAR);
	act("$n gives you $p.", FALSE, ch, obj, vict, TO_VICT);
	if (ch->in_veh)
	{
		snprintf(buf, sizeof(buf), "%s gives %s to %s.\r\n", GET_NAME(ch), GET_OBJ_NAME(obj), GET_NAME(vict));
		send_to_veh(buf, ch->in_veh, ch, vict, FALSE);
	}
	act("$n gives $p to $N.", TRUE, ch, obj, vict, TO_NOTVICT);

	// Always log the transfer of vehicle containers.
	if (GET_OBJ_VNUM(obj) == OBJ_VEHCONTAINER)
	{
		const char *owner = get_player_name(GET_VEHCONTAINER_VEH_OWNER(obj));
		snprintf(buf, sizeof(buf), "%s (%ld) gave veh-container %s (%d, idnum %d), belonging to %s (%d), to %s (%ld).",
						 GET_CHAR_NAME(ch),
						 GET_IDNUM(ch),
						 GET_OBJ_NAME(obj),
						 GET_VEHCONTAINER_VEH_VNUM(obj),
						 GET_VEHCONTAINER_VEH_IDNUM(obj),
						 owner,
						 GET_VEHCONTAINER_VEH_OWNER(obj),
						 GET_CHAR_NAME(vict),
						 GET_IDNUM(vict));
		mudlog(buf, ch, LOG_CHEATLOG, TRUE);
		DELETE_ARRAY_IF_EXTANT(owner);
	}

	// Always log staff giving things away and/or wizloaded/cheat-marked items being given away.
	if ((!IS_NPC(ch) && IS_SENATOR(ch)) || IS_OBJ_STAT(obj, ITEM_EXTRA_WIZLOAD) || IS_OBJ_STAT(obj, ITEM_EXTRA_CHEATLOG_MARK))
	{
		// Default/preliminary logging message; this is appended to where necessary.
		char *representation = generate_new_loggable_representation(obj);
		snprintf(buf, sizeof(buf), "%s gives %s: %s", GET_CHAR_NAME(ch), GET_CHAR_NAME(vict), representation);
		mudlog(buf, ch, IS_OBJ_STAT(obj, ITEM_EXTRA_WIZLOAD) ? LOG_WIZITEMLOG : LOG_CHEATLOG, TRUE);
		delete[] representation;
	}
}

bool perform_give(struct char_data *ch, struct char_data *vict, struct obj_data *obj)
{
	FALSE_CASE_PRINTF(IS_ASTRAL(vict), "Astral beings can't touch %s.", decapitalize_a_an(obj));
	FALSE_CASE_PRINTF(IS_OBJ_STAT(obj, ITEM_EXTRA_NODROP) && !IS_SENATOR(ch), "You can't let go of %s! You can JUNK it if you want to get rid of it.", decapitalize_a_an(obj));
	FALSE_CASE_PRINTF(GET_OBJ_TYPE(obj) == ITEM_PET, "%s would be so sad... :(", CAP(GET_OBJ_NAME(obj)));
	FALSE_CASE_PRINTF(IS_OBJ_STAT(obj, ITEM_EXTRA_KEPT) && !IS_SENATOR(ch), "You'll have to use the ^WKEEP^n command on %s before you can give it away.",
										decapitalize_a_an(obj));

	FALSE_CASE_PRINTF(obj_contains_items_with_flag(obj, ITEM_EXTRA_KEPT) && !IS_SENATOR(ch), "Action blocked: %s contains at least one kept item.", GET_OBJ_NAME(obj));
	FALSE_CASE_PRINTF(obj_contains_items_with_flag(obj, ITEM_EXTRA_NODROP) && !IS_SENATOR(ch),
										"Action blocked: %s contains at least one no-drop item. You can JUNK that item if you want.", GET_OBJ_NAME(obj));

	FALSE_CASE_PRINTF(!IS_NPC(vict) && ch_is_blocked_by_quest_protections(vict, obj, FALSE, TRUE), "%s isn't participating in your job.", GET_CHAR_NAME(vict));

	if (IS_CARRYING_N(vict) >= CAN_CARRY_N(vict))
	{
		act("$N seems to have $S hands full.", FALSE, ch, 0, vict, TO_CHAR);
		return 0;
	}
	if (GET_OBJ_WEIGHT(obj) + IS_CARRYING_W(vict) > CAN_CARRY_W(vict))
	{
		act("$E can't carry that much weight.", FALSE, ch, 0, vict, TO_CHAR);
		return 0;
	}
	if (obj == ch->char_specials.programming)
	{
		send_to_char(ch, "You cannot give away something you are working on.\r\n");
		return 0;
	}
	if (GET_OBJ_VNUM(obj) == OBJ_VEHCONTAINER && IS_NPC(vict))
	{
		send_to_char("You can't give NPCs vehicles.\r\n", ch);
		return 0;
	}

	// player giving something to an NPC: check quest stuff
	if (!IS_NPC(ch) && IS_NPC(vict))
	{
		// Quest item delivery checks.
		if (COULD_BE_ON_QUEST(ch))
		{
			// Successful delivery of quest item.
			if (check_quest_delivery(ch, vict, obj))
			{
				// Give it to them now.
				_ch_gives_obj_to_vict(ch, obj, vict);
				if (MOB_FLAGGED(vict, MOB_INANIMATE))
				{
					act("$n beeps at $N and retains $p.", TRUE, vict, obj, ch, TO_ROOM);
				}
				else
				{
					act("$n nods slightly to $N and tucks $p away.", TRUE, vict, obj, ch, TO_ROOM);
				}
				extract_obj(obj);
				return 1;
			}

			// If it's a quest item, refuse to hand it off.
			if (GET_OBJ_QUEST_CHAR_ID(obj))
			{
				send_to_char("You're pretty sure your Johnson wouldn't be happy with you if you did that.\r\n", ch);
				return 0;
			}
		}

		// Not a quest item. Give succeeds.
		_ch_gives_obj_to_vict(ch, obj, vict);

		if (GET_MOB_SPEC(vict) || GET_MOB_SPEC2(vict))
		{
			// These specs handle objects, so don't mess with them by having the NPC drop the thing.
			if (GET_MOB_SPEC(vict) == fence || GET_MOB_SPEC(vict) == hacker || GET_MOB_SPEC(vict) == fixer || GET_MOB_SPEC(vict) == mageskill_herbie)
				return 1;
			if (GET_MOB_SPEC2(vict) == fence || GET_MOB_SPEC2(vict) == hacker || GET_MOB_SPEC2(vict) == fixer || GET_MOB_SPEC2(vict) == mageskill_herbie)
				return 1;
		}

		// NPC doesn't want it: Drop the thing.
		if (MOB_FLAGGED(vict, MOB_INANIMATE))
		{
			act("$n blats at $N and ejects $p.", TRUE, vict, obj, ch, TO_ROOM);
		}
		else
		{
			act("$n glances at $p, then lets it fall from $s hand.", TRUE, vict, obj, 0, TO_ROOM);
		}
		obj_from_char(obj);
		if (vict->in_room)
			obj_to_room(obj, vict->in_room);
		else
			obj_to_veh(obj, vict->in_veh);

		set_dropped_by_info(obj, ch);
	}
	else
	{
		// Log same-host handoffs.
		if (is_same_host(ch, vict))
		{
			// Log same-host transfers that are not on Grapevine.
			if (!is_approved_multibox_host(ch->desc->host))
			{
				char *representation = generate_new_loggable_representation(obj);
				mudlog_vfprintf(ch, LOG_CHEATLOG, "%s is giving same-host character %s '%s' (host: %s).",
												GET_CHAR_NAME(ch),
												GET_CHAR_NAME(vict),
												representation,
												(GET_LEVEL(ch) < LVL_PRESIDENT && GET_LEVEL(vict) < LVL_PRESIDENT) ? ch->desc->host : "<obscured>");
				delete[] representation;
			}
		}

		// All other cases (pc -> pc, npc -> npc): succeed without further checks

		_ch_gives_obj_to_vict(ch, obj, vict);
	}

	return 1;
}

/* utility function for give */
struct char_data *give_find_vict(struct char_data *ch, char *arg)
{
	SPECIAL(fixer);
	struct char_data *vict;

	if (!*arg)
	{
		send_to_char("To who?\r\n", ch);
		return NULL;
	}

	if (!(vict = get_char_room_vis(ch, arg)))
	{
		send_to_char(ch, "You don't see anyone named '%s' here.\r\n", arg);
		return NULL;
	}

	if (vict == ch)
	{
		send_to_char("What's the point of giving it to yourself?\r\n", ch);
		return NULL;
	}

	if (IS_NPC(vict))
	{
		if (MOB_HAS_SPEC(vict, fixer))
		{
			act("Do you really want to give $M stuff for free?! (Use the 'repair' command here.)", FALSE, ch, 0, vict, TO_CHAR);
			return NULL;
		}
		if (MOB_HAS_SPEC(vict, fence))
		{
			act("Do you really want to give $M stuff for free?! (Use the 'sell' command here.)", FALSE, ch, 0, vict, TO_CHAR);
			return NULL;
		}
	}

	return vict;
}

void perform_give_gold(struct char_data *ch, struct char_data *vict, int amount)
{
	FAILURE_CASE(amount <= 0, "You must specify a positive quantity, like GIVE 30 NUYEN MAN.");
	FAILURE_CASE(IS_ASTRAL(vict), "You can't hand astral beings anything.");
	FAILURE_CASE((GET_NUYEN(ch) < amount) && (IS_NPC(ch) || (!access_level(ch, LVL_VICEPRES))), "You don't have that much!");
	FAILURE_CASE(IS_SENATOR(ch) && !access_level(ch, LVL_PRESIDENT) && !IS_SENATOR(vict) && !IS_NPC(vict), "Staff must use the PAYOUT command instead.");

	bool ch_is_npc = IS_NPC(ch);
	bool vict_is_npc = IS_NPC(vict);

	mudlog_vfprintf(ch, LOG_CHEATLOG, "%s%s (%ld) gave %s%s%s (%ld) %d nuyen.",
									ch_is_npc ? "NPC " : "",
									GET_CHAR_NAME(ch),
									ch_is_npc ? GET_MOB_VNUM(ch) : GET_IDNUM(ch),
									is_same_host(ch, vict) ? "same-host " : "",
									vict_is_npc ? "NPC " : "",
									GET_CHAR_NAME(vict),
									vict_is_npc ? GET_MOB_VNUM(vict) : GET_IDNUM(vict),
									amount);

	if (vict_is_npc)
	{
		AFF_FLAGS(vict).SetBit(AFF_CHEATLOG_MARK);
	}

	snprintf(buf, sizeof(buf), "You give $N %d nuyen.", amount);
	act(buf, FALSE, ch, 0, vict, TO_CHAR);
	snprintf(buf, sizeof(buf), "$n gives you %d nuyen.", amount);
	act(buf, FALSE, ch, 0, vict, TO_VICT);
	if (ch->in_veh)
	{
		snprintf(buf, sizeof(buf), "%s gives some nuyen to %s.", GET_NAME(ch), GET_NAME(vict));
		send_to_veh(buf, ch->in_veh, ch, vict, FALSE);
	}
	else
	{
		snprintf(buf, sizeof(buf), "$n gives some nuyen to $N.");
		act(buf, TRUE, ch, 0, vict, TO_NOTVICT);
	}

	// Giving nuyen. NPCs and non-staff PCs lose the amount they give, staff do not.
	if (IS_NPC(ch) || !access_level(ch, LVL_VICEPRES))
		GET_NUYEN_RAW(ch) -= amount;
	GET_NUYEN_RAW(vict) += amount;

	snprintf(buf, sizeof(buf), "%s gives %s: %d nuyen *",
					 GET_CHAR_NAME(ch), GET_CHAR_NAME(vict), amount);
	mudlog(buf, ch, LOG_GRIDLOG, TRUE);
}

ACMD(do_give)
{
	int amount, dotmode;
	struct char_data *vict;
	struct obj_data *obj, *next_obj;

	argument = one_argument(argument, arg);

	if (IS_ASTRAL(ch))
	{
		send_to_char("Astral projections can't touch anything.\r\n", ch);
		return;
	}
	if (PLR_FLAGGED(ch, PLR_NOT_YET_AUTHED))
	{
		send_to_char("Sorry, you can't do that during character generation.\r\n", ch);
		return;
	}
	if (IS_WORKING(ch))
	{
		send_to_char(TOOBUSY, ch);
		return;
	}
	if (!*arg)
		send_to_char("Give what to who?\r\n", ch);
	else if (is_number(arg))
	{
		amount = atoi(arg);
		argument = one_argument(argument, arg);
		if (!str_cmp("nuyen", arg))
		{
			argument = one_argument(argument, arg);

			if (ch->in_veh)
			{
				if (!(vict = get_char_veh(ch, arg, ch->in_veh)) || IS_IGNORING(vict, is_blocking_ic_interaction_from, ch) || vict == ch)
				{
					send_to_char(ch, "You don't see anyone named '%s' here.\r\n", arg);
				}
				else
				{
					perform_give_gold(ch, vict, amount);
				}
				return;
			}

			if (!(vict = give_find_vict(ch, arg)) || IS_IGNORING(vict, is_blocking_ic_interaction_from, ch) || vict == ch)
			{
				send_to_char(ch, "You don't see anyone named '%s' here.\r\n", arg);
			}
			else
			{
				perform_give_gold(ch, vict, amount);
			}
			return;
		}
		else
		{
			/* code to give multiple items.  anyone want to write it? -je */
			send_to_char("You can't give more than one item at a time.\r\n", ch);
			return;
		}
	}
	else
	{
		one_argument(argument, buf1);
		if (ch->in_veh)
		{
			if (!(vict = get_char_veh(ch, buf1, ch->in_veh)) || vict == ch)
			{
				send_to_char(ch, "You don't see anyone named '%s' here.\r\n", buf1);
				return;
			}
		}
		else if (!(vict = give_find_vict(ch, buf1)) || vict == ch)
			return;

		if (IS_IGNORING(vict, is_blocking_ic_interaction_from, ch))
		{
			send_to_char(ch, "You don't see anyone named '%s' here.\r\n", buf1);
			return;
		}

		dotmode = find_all_dots(arg, sizeof(arg));
		if (dotmode == FIND_INDIV)
		{
			if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying)))
			{
				send_to_char(ch, "You don't seem to have %s %s in your inventory.\r\n", AN(arg), arg);
			}
			else
				perform_give(ch, vict, obj);
		}
		else
		{
			if (dotmode == FIND_ALLDOT && !*arg)
			{
				send_to_char("All of what?\r\n", ch);
				return;
			}
			if (!ch->carrying)
				send_to_char("You don't seem to be holding anything.\r\n", ch);
			else
				for (obj = ch->carrying; obj; obj = next_obj)
				{
					next_obj = obj->next_content;
					if (CAN_SEE_OBJ(ch, obj) &&
							((dotmode == FIND_ALL || isname(arg, obj->text.keywords))))
						perform_give(ch, vict, obj);
				}
		}
	}
}

/* Everything from here down is what was formerly act.obj2.c */

void weight_change_object(struct obj_data *obj, float weight)
{
	struct obj_data *tmp_obj = NULL;
	struct char_data *tmp_ch = NULL;
	struct veh_data *tmp_veh = NULL;
	int worn_on = -1;

	// Remove it from its container (subtracting its current weight from the container's values).
	if ((tmp_ch = obj->carried_by))
		obj_from_char(obj);
	else if ((tmp_obj = obj->in_obj))
		obj_from_obj(obj);
	else if ((tmp_veh = obj->in_veh))
		obj_from_room(obj);
	else if (obj->worn_by && (worn_on = obj->worn_on) >= 0)
		unequip_char((tmp_ch = obj->worn_by), obj->worn_on, TRUE);

	// If none of the above are true, then this object is either in a room or is being juggled by the code somewhere (ex: zoneloading). Either way, no parent containers need updating.

	// Rectify weights so that the object's weight can never be negative.
	GET_OBJ_WEIGHT(obj) = MAX(0, GET_OBJ_WEIGHT(obj) + weight);

	// Return it to its container, re-adding its weight.
	if (tmp_ch)
	{
		if (worn_on >= 0)
			equip_char(tmp_ch, obj, worn_on);
		else
			obj_to_char(obj, tmp_ch);
	}
	else if (tmp_obj)
		obj_to_obj(obj, tmp_obj);
	else if (tmp_veh)
		obj_to_veh(obj, tmp_veh);
}

void name_from_drinkcon(struct obj_data *obj)
{
	char token[80], *temp; // hopefully this will be enough, hehehe
	extern struct obj_data *obj_proto;

	temp = get_token(obj->text.keywords, token);

	buf2[0] = '\0'; // so strcats will start at the beginning

	int i = 0;
	while (*token && strcasecmp(token, drinks[GET_OBJ_VAL(obj, 2)]))
	{
		strlcat(buf2, token, sizeof(buf2));
		strlcat(buf2, " ", sizeof(buf2));
		temp = get_token(temp, token);
		++i;
	}

	// we do this in case there's only one word in the name list and it
	// is the name of the liquid
	if (i < 1)
		return;

	// now, we copy the remaining string onto the end of buf2
	if (temp)
		strlcat(buf2, temp, sizeof(buf2));

	buf2[strlen(buf2)] = '\0'; // remove the trailing space

	// only delete the object name if this object has a unique name
	if (GET_OBJ_RNUM(obj) < 0 || obj->text.keywords != obj_proto[GET_OBJ_RNUM(obj)].text.keywords)
		DELETE_AND_NULL_ARRAY(obj->text.keywords);
	// otherwise it just gets pointed to the new namelist
	obj->text.keywords = str_dup(buf2);
}

void name_to_drinkcon(struct obj_data *obj, int type)
{
	char *new_name;

	if (!obj || (GET_OBJ_TYPE(obj) != ITEM_DRINKCON && GET_OBJ_TYPE(obj) != ITEM_FOUNTAIN))
		return;

	size_t length = strlen(obj->text.keywords) + strlen(drinknames[type]) + strlen(drinks[type]) + 10;
	new_name = new char[length];
	snprintf(new_name, length, "%s %s %s", obj->text.keywords, drinknames[type], drinks[type]);

	if (GET_OBJ_RNUM(obj) == NOTHING ||
			obj->text.keywords != obj_proto[GET_OBJ_RNUM(obj)].text.keywords)
		DELETE_AND_NULL_ARRAY(obj->text.keywords);

	obj->text.keywords = new_name;
}

ACMD(do_drink)
{
	struct obj_data *temp;
	int amount, on_ground = 0;
	float weight;

	one_argument(argument, arg);

	if (IS_ASTRAL(ch))
	{
		send_to_char("Astral forms can't retain fluid.\r\n", ch);
		return;
	}

	if (!*arg)
	{
		send_to_char("Drink from what?\r\n", ch);
		return;
	}
	if (!(temp = get_obj_in_list_vis(ch, arg, ch->carrying)))
	{
		if (!ch->in_room || !(temp = get_obj_in_list_vis(ch, arg, ch->in_room->contents)))
		{
			send_to_char(ch, "You don't see anything named '%s' here.\r\n", arg);
			return;
		}
		else
			on_ground = 1;
	}
	if ((GET_OBJ_TYPE(temp) != ITEM_DRINKCON) &&
			(GET_OBJ_TYPE(temp) != ITEM_FOUNTAIN))
	{
		send_to_char(ch, "You can't drink from %s.\r\n", GET_OBJ_NAME(temp));
		return;
	}
	if (on_ground && (GET_OBJ_TYPE(temp) == ITEM_DRINKCON))
	{
		send_to_char(ch, "You have to be holding %s to drink from it.\r\n", GET_OBJ_NAME(temp));
		return;
	}
	if (GET_COND(ch, COND_DRUNK) > MAX_DRUNK && !affected_by_spell(ch, SPELL_DETOX))
	{
		send_to_char("You can't seem to get close enough to your mouth.\r\n", ch);
		act("$n tries to drink but misses $s mouth!", TRUE, ch, 0, 0, TO_ROOM);
		return;
	}
#ifdef ENABLE_HUNGER
	if ((GET_COND(ch, COND_FULL) > MAX_FULLNESS) && (GET_COND(ch, COND_THIRST) > MIN_QUENCHED))
	{
		send_to_char("Your stomach can't contain anymore!\r\n", ch);
		return;
	}
#endif
	if (GET_DRINKCON_AMOUNT(temp) <= 0)
	{
		send_to_char("It's empty.\r\n", ch);
		return;
	}
	if (subcmd == SCMD_DRINK)
	{
		act("$n drinks from $p.", TRUE, ch, temp, 0, TO_ROOM);

		send_to_char(ch, "You drink the %s.\r\n", drinks[GET_DRINKCON_LIQ_TYPE(temp)]);
		amount = 3;
	}
	else
	{
		act("$n sips from $p.", TRUE, ch, temp, 0, TO_ROOM);
		send_to_char(ch, "It tastes like %s.\r\n", drinks[GET_DRINKCON_LIQ_TYPE(temp)]);
		amount = 1;
	}

	amount = MIN(amount, GET_DRINKCON_AMOUNT(temp));

	/* You can't subtract more than the object weighs */
	weight = (float)(MIN(amount * 100, (int)(GET_OBJ_WEIGHT(temp) * 100)) / 100);

	weight_change_object(temp, -weight); /* Subtract amount */

	gain_condition(ch, COND_DRUNK,
								 (int)((int)drink_aff[GET_DRINKCON_LIQ_TYPE(temp)][COND_DRUNK] * amount) / 4);

#ifdef ENABLE_HUNGER
	gain_condition(ch, COND_FULL,
								 (int)((int)drink_aff[GET_DRINKCON_LIQ_TYPE(temp)][COND_FULL] * amount) / 4);

	gain_condition(ch, COND_THIRST,
								 (int)((int)drink_aff[GET_DRINKCON_LIQ_TYPE(temp)][COND_THIRST] * amount) / 4);
#endif

	if (GET_COND(ch, COND_DRUNK) > MAX_DRUNK && !affected_by_spell(ch, SPELL_DETOX))
		send_to_char("You feel drunk.\r\n", ch);

#ifdef ENABLE_HUNGER
	if (GET_COND(ch, COND_THIRST) > MAX_QUENCHED)
		send_to_char("You don't feel thirsty any more.\r\n", ch);

	if (GET_COND(ch, COND_FULL) > MAX_FULLNESS)
		send_to_char("You are full.\r\n", ch);
#endif

	/* empty the container, and no longer poison. */
	GET_DRINKCON_AMOUNT(temp) -= amount;
	if (GET_DRINKCON_AMOUNT(temp) <= 0)
	{ /* The last bit */
		// name_from_drinkcon(temp); // do this first
		GET_DRINKCON_LIQ_TYPE(temp) = LIQ_WATER;
	}

	// Deal poison damage.
	int poison_rating = MAX(0, GET_DRINKCON_POISON_RATING(temp));

	switch (GET_DRINKCON_LIQ_TYPE(temp))
	{
	case LIQ_CLEANER:
	case LIQ_BATTERY_ACID:
		poison_rating = MAX(poison_rating, DEADLY);
		break;
	case LIQ_COOLANT:
	case LIQ_MOTOR_OIL:
		poison_rating = MAX(poison_rating, SERIOUS);
		break;
	}

	poison_rating = MIN(poison_rating, DEADLY);

	if (poison_rating > 0)
	{
		int damage_resist_dice = GET_BOD(ch);
		int tn = 6 + amount;
		int successes = success_test(damage_resist_dice, tn);
		int staged_damage = stage(-successes, poison_rating);
		int dam_total = convert_damage(staged_damage);
		char rollbuf[500];
		snprintf(rollbuf, sizeof(rollbuf), "Poison resistance test: %d dice vs TN %d gave %d successes to stage down physical damage.",
						 damage_resist_dice,
						 tn,
						 successes);
		act(rollbuf, FALSE, ch, 0, 0, TO_ROLLS);

		if (damage(ch, ch, dam_total, TYPE_POISON, TRUE))
			return;
	}
	return;
}

ACMD(do_eat)
{
	SPECIAL(anticoagulant);
	struct obj_data *food, *cyber;
	int amount;

	if (IS_ASTRAL(ch))
	{
		send_to_char("Eat?!  You don't even have a stomach!.\r\n", ch);
		return;
	}
	one_argument(argument, arg);

	if (!*arg)
	{
		send_to_char("Eat what?\r\n", ch);
		return;
	}
	if (!(food = get_obj_in_list_vis(ch, arg, ch->carrying)))
	{
		send_to_char(ch, "You don't seem to have anything named '%s' in your inventory.\r\n", arg);
		return;
	}
	if (subcmd == SCMD_TASTE && ((GET_OBJ_TYPE(food) == ITEM_DRINKCON) ||
															 (GET_OBJ_TYPE(food) == ITEM_FOUNTAIN)))
	{
		do_drink(ch, argument, 0, SCMD_SIP);
		return;
	}
	if ((GET_OBJ_TYPE(food) != ITEM_FOOD) && !access_level(ch, LVL_ADMIN))
	{
		send_to_char(ch, "You can't eat %s!\r\n", GET_OBJ_NAME(food));
		return;
	}
#ifdef ENABLE_HUNGER
	if (GET_COND(ch, COND_FULL) > MAX_FULLNESS)
	{ /* Stomach full */
		act("You are too full to eat more!", FALSE, ch, 0, 0, TO_CHAR);
		return;
	}
#endif

	if (GET_OBJ_TYPE(food) == ITEM_PET)
	{
		send_to_char(ch, "^RA sudden, crushing sense of disapproval hammers into you from all sides! You barely have time to scream before imploding into a tasty ball that %s promptly monches up. Turnabout is fair play?\r\n", GET_OBJ_NAME(food));
		act("$n raises $p to $s mouth, then suddenly gives a piercing scream before imploding under Lucien's disapproval!", FALSE, ch, food, 0, TO_ROOM);
		die(ch, GET_IDNUM(ch));
		return;
	}

	if (subcmd == SCMD_EAT)
	{
		act("You eat $p.", FALSE, ch, food, 0, TO_CHAR);
		act("$n eats $p.", TRUE, ch, food, 0, TO_ROOM);
		if (GET_OBJ_SPEC(food) && GET_OBJ_SPEC(food) == anticoagulant)
		{
			for (cyber = ch->bioware; cyber; cyber = cyber->next_content)
				if (GET_OBJ_VAL(cyber, 0) == BIO_PLATELETFACTORY)
				{
					GET_OBJ_VAL(cyber, 5) = 36;
					GET_OBJ_VAL(cyber, 6) = 0;
					send_to_char("You relax as your platelet factory calms down.\r\n", ch);
					break;
				}
		}
	}
	else
	{
		act("You nibble a little bit of the $o.", FALSE, ch, food, 0, TO_CHAR);
		act("$n tastes a little bit of $p.", TRUE, ch, food, 0, TO_ROOM);
	}

	amount = (subcmd == SCMD_EAT ? GET_OBJ_VAL(food, 0) : 1);

	gain_condition(ch, COND_FULL, amount);

	if (GET_COND(ch, COND_FULL) > 20)
		act("You are full.", FALSE, ch, 0, 0, TO_CHAR);

	if (subcmd == SCMD_EAT)
		extract_obj(food);
	else
	{
		if (!(--GET_OBJ_VAL(food, 0)))
		{
			send_to_char("There's nothing left now.\r\n", ch);
			extract_obj(food);
		}
	}
}

ACMD(do_pour)
{
	char arg1[MAX_INPUT_LENGTH];
	char arg2[MAX_INPUT_LENGTH];
	struct obj_data *from_obj = NULL, *to_obj = NULL;
	int amount;

	two_arguments(argument, arg1, arg2);

	if (subcmd == SCMD_POUR)
	{
		if (!*arg1)
		{ /* No arguments */
			act("What do you want to pour from?", FALSE, ch, 0, 0, TO_CHAR);
			return;
		}
		if (!(from_obj = get_obj_in_list_vis(ch, arg1, ch->carrying)))
		{
			send_to_char(ch, "You don't see anything named '%s' here.\r\n", arg1);
			return;
		}
		if (GET_OBJ_TYPE(from_obj) != ITEM_DRINKCON)
		{
			act("You can't pour from $p.", FALSE, ch, from_obj, 0, TO_CHAR);
			return;
		}
	}
	if (subcmd == SCMD_FILL)
	{
		if (!*arg1)
		{ /* no arguments */
			send_to_char("What do you want to fill?  And what are you filling it from?\r\n", ch);
			return;
		}
		if (!(to_obj = get_obj_in_list_vis(ch, arg1, ch->carrying)))
		{
			send_to_char(ch, "You don't see anything named '%s' here.\r\n", arg1);
			return;
		}
		if (GET_OBJ_TYPE(to_obj) != ITEM_DRINKCON)
		{
			act("You can't fill $p!", FALSE, ch, to_obj, 0, TO_CHAR);
			return;
		}
		if (!*arg2)
		{ /* no 2nd argument */
			act("What do you want to fill $p from?", FALSE, ch, to_obj, 0, TO_CHAR);
			return;
		}
		if (!(from_obj = get_obj_in_list_vis(ch, arg2, ch->in_veh ? ch->in_veh->contents : ch->in_room->contents)))
		{
			send_to_char(ch, "There doesn't seem to be %s %s here.\r\n", AN(arg2), arg2);
			return;
		}
		if (GET_OBJ_TYPE(from_obj) != ITEM_FOUNTAIN)
		{
			act("You can't fill something from $p.", FALSE, ch, from_obj, 0, TO_CHAR);
			return;
		}
	}
	if (GET_OBJ_VAL(from_obj, 1) == 0)
	{
		act("The $p is empty.", FALSE, ch, from_obj, 0, TO_CHAR);
		return;
	}
	if (subcmd == SCMD_POUR)
	{ /* pour */
		if (!*arg2)
		{
			act("Where do you want it?  Out or in what?", FALSE, ch, 0, 0, TO_CHAR);
			return;
		}
		if (!str_cmp(arg2, "out"))
		{
			act("$n empties $p.", TRUE, ch, from_obj, 0, TO_ROOM);
			act("You empty $p.", FALSE, ch, from_obj, 0, TO_CHAR);

			weight_change_object(from_obj, -GET_OBJ_VAL(from_obj, 1)); /* Empty */

			GET_OBJ_VAL(from_obj, 1) = 0;
			GET_OBJ_VAL(from_obj, 2) = 0;
			GET_OBJ_VAL(from_obj, 3) = 0;
			name_from_drinkcon(from_obj);

			return;
		}
		if (!(to_obj = get_obj_in_list_vis(ch, arg2, ch->carrying)))
		{
			send_to_char(ch, "You don't see anything named '%s' here.\r\n", arg2);
			return;
		}
		if ((GET_OBJ_TYPE(to_obj) != ITEM_DRINKCON) &&
				(GET_OBJ_TYPE(to_obj) != ITEM_FOUNTAIN))
		{
			act("You can't pour anything into $p^n.", FALSE, ch, to_obj, 0, TO_CHAR);
			return;
		}
	}
	if (to_obj == from_obj)
	{
		act("A most unproductive effort.", FALSE, ch, 0, 0, TO_CHAR);
		return;
	}
	if ((GET_OBJ_VAL(to_obj, 1) != 0) &&
			(GET_OBJ_VAL(to_obj, 2) != GET_OBJ_VAL(from_obj, 2)))
	{
		act("There is already another liquid in it!", FALSE, ch, 0, 0, TO_CHAR);
		return;
	}
	if (!(GET_OBJ_VAL(to_obj, 1) < GET_OBJ_VAL(to_obj, 0)))
	{
		act("There is no room for more.", FALSE, ch, 0, 0, TO_CHAR);
		return;
	}
	if (subcmd == SCMD_POUR)
	{
		snprintf(buf, sizeof(buf), "You pour the %s into the %s^n.",
						 drinknames[GET_OBJ_VAL(from_obj, 2)], arg2);
		send_to_char(buf, ch);
	}
	if (subcmd == SCMD_FILL)
	{
		act("You gently fill $p from $P.", FALSE, ch, to_obj, from_obj, TO_CHAR);
		act("$n gently fills $p from $P.", TRUE, ch, to_obj, from_obj, TO_ROOM);
	}
	/* New alias */
	if (GET_OBJ_VAL(to_obj, 1) == 0)
		name_to_drinkcon(to_obj, GET_OBJ_VAL(from_obj, 2));

	/* First same type liq. */
	GET_OBJ_VAL(to_obj, 2) = GET_OBJ_VAL(from_obj, 2);

	/* Then how much to pour */
	GET_OBJ_VAL(from_obj, 1) -= (amount =
																	 (GET_OBJ_VAL(to_obj, 0) - GET_OBJ_VAL(to_obj, 1)));

	GET_OBJ_VAL(to_obj, 1) = GET_OBJ_VAL(to_obj, 0);

	if (GET_OBJ_VAL(from_obj, 1) < 0)
	{ /* There was too little */
		GET_OBJ_VAL(to_obj, 1) += GET_OBJ_VAL(from_obj, 1);
		amount += GET_OBJ_VAL(from_obj, 1);
		GET_OBJ_VAL(from_obj, 1) = 0;
		GET_OBJ_VAL(from_obj, 2) = 0;
		GET_OBJ_VAL(from_obj, 3) = 0;
		name_from_drinkcon(from_obj);
	}
	/* Then the poison boogie */
	GET_OBJ_VAL(to_obj, 3) =
			(GET_OBJ_VAL(to_obj, 3) || GET_OBJ_VAL(from_obj, 3));

	/* And the weight boogie */
	weight_change_object(from_obj, -(int)(amount / 5));
	weight_change_object(to_obj, (int)(amount / 5)); /* Add weight */

	return;
}

void wear_message(struct char_data *ch, struct obj_data *obj, int where)
{
	const char *wear_messages[][2] = {
			{"$n equips and activates $p.",
			 "You equip and activate $p."},

			{"$n wears $p on $s head.",
			 "You wear $p on your head."},

			{"$n wears $p on $s eyes.",
			 "You wear $p on your eyes."},

			{"$n wears $p in $s ear.",
			 "You wear $p in your ear."},

			{"$n wears $p in $s ear.",
			 "You wear $p in your ear."},

			{"$n wears $p over $s face.",
			 "You wear $p over your face."},

			{"$n wears $p around $s neck.",
			 "You wear $p around your neck."},

			{"$n wears $p around $s neck.",
			 "You wear $p around your neck."},

			{"$n casually slings $p over $s shoulder.",
			 "You sling $p over your shoulder."},

			{"$n wears $p about $s body.",
			 "You wear $p around your body."},

			{
					"$n wears $p on $s body.",
					"You wear $p on your body.",
			},

			{"$n wears $p underneath $s clothes.",
			 "You wear $p under your clothes."},

			{"$n wears $p on $s arms.",
			 "You wear $p on your arms."},

			{"$n slings $p under $s left arm.",
			 "You sling $p under your left arm."},

			{"$n slings $p under $s right arm.",
			 "You sling $p under your right arm."},

			{"$n puts $p on around $s right wrist.",
			 "You put $p on around your right wrist."},

			{"$n puts $p on around $s left wrist.",
			 "You put $p on around your left wrist."},

			{"$n puts $p on $s hands.",
			 "You put $p on your hands."},

			{"$n wields $p.",
			 "You wield $p."},

			{"$n holds $p.",
			 "You hold $p."},

			{"$n straps $p around $s arm as a shield.",
			 "You start to use $p as a shield."},

			{"$n slides $p on to $s right ring finger.",
			 "You slide $p on to your right ring finger."},

			{"$n slides $p on to $s left ring finger.",
			 "You slide $p on to your left ring finger."},

			{"$n slides $p onto $s right index finger.",
			 "You slide $p onto your right index finger."},

			{"$n slides $p onto $s left index finger.",
			 "You slide $p onto your left index finger."},

			{"$n slides $p onto $s right middle finger.",
			 "You slide $p onto your right middle finger."},

			{"$n slides $p onto $s left middle finger.",
			 "You slide $p onto your left middle finger."},

			{"$n slides $p onto $s right pinkie finger.",
			 "You slide $p onto your right pinkie finger."},

			{"$n slides $p onto $s left pinkie finger.",
			 "You slide $p onto your left pinkie finger."},

			{"$n wears $p in $s belly button.",
			 "You put $p in your belly button."},

			{"$n wears $p around $s waist.",
			 "You wear $p around your waist."},

			{"$n puts $p around $s thigh.",
			 "You put $p around your thigh."},

			{"$n puts $p around $s thigh.",
			 "You put $p around your thigh."},

			{"$n puts $p on $s legs.",
			 "You put $p on your legs."},

			{"$n puts $p around $s left ankle.",
			 "You put $p around your left ankle."},

			{"$n puts $p around $s right ankle.",
			 "You put $p around your right ankle."},

			{"$n wears $p on $s feet.",
			 "You wear $p on your feet."},

			{"$n wears $p on $s feet.",
			 "You put $p on your feet."},

			// Patch
			{"$n wears $p in an ERRONEOUS LOCATION.",
			 "You put $p on an ERRONEOUS LOCATION."},

			// Underwear
			{"$n slips into $p.",
			 "You slip into $p (u)."},

			// Chest
			{"$n slips into $p.",
			 "You slip into $p (c)."},

			// Lapel
			{"$n pins $p to $s lapel area.",
			 "You pin $p to your lapel area."}

			/*
				{"$n sticks $p in $s mouth.",
				"You stick $p in your mouth."}*/
	};

	/* Should we add move waer types?*/
	if (where == WEAR_WIELD || where == WEAR_HOLD)
	{
		if (GET_OBJ_TYPE(obj) == ITEM_WEAPON)
		{
			act(wear_messages[WEAR_WIELD][0], TRUE, ch, obj, 0, TO_ROOM);
			act(wear_messages[WEAR_WIELD][1], FALSE, ch, obj, 0, TO_CHAR);
		}
		else
		{
			act(wear_messages[WEAR_HOLD][0], TRUE, ch, obj, 0, TO_ROOM);
			act(wear_messages[WEAR_HOLD][1], FALSE, ch, obj, 0, TO_CHAR);
		}
	}
	else
	{
		act(wear_messages[where][0], TRUE, ch, obj, 0, TO_ROOM);
		act(wear_messages[where][1], FALSE, ch, obj, 0, TO_CHAR);
	}
}

int can_wield_both(struct char_data *ch, struct obj_data *one, struct obj_data *two)
{
	if (!one || !two)
		return TRUE;
	if (GET_OBJ_TYPE(one) != ITEM_WEAPON || GET_OBJ_TYPE(two) != ITEM_WEAPON)
		return TRUE;
	if ((WEAPON_IS_GUN(one) && !WEAPON_IS_GUN(two)) ||
			(WEAPON_IS_GUN(two) && !WEAPON_IS_GUN(one)))
		return FALSE;
	else if (!WEAPON_IS_GUN(one) && !WEAPON_IS_GUN(two))
		return FALSE;
	else if (IS_OBJ_STAT(one, ITEM_EXTRA_TWOHANDS) || IS_OBJ_STAT(two, ITEM_EXTRA_TWOHANDS))
		return FALSE;

	return TRUE;
}

void perform_wear(struct char_data *ch, struct obj_data *obj, int where, bool print_messages)
{
	struct obj_data *wielded = GET_EQ(ch, WEAR_WIELD);

	const char *already_wearing[] = {
			"You're already using a light.\r\n",
			"You're already wearing something on your head.\r\n",
			"You're already wearing something on your eyes.\r\n",
			"YOU SHOULD NEVER SEE THIS MESSAGE (#1).  PLEASE REPORT.\r\n",
			"You can't wear anything else in your ears.\r\n",
			"There is already something covering your face.\r\n",
			"YOU SHOULD NEVER SEE THIS MESSAGE (#2).  PLEASE REPORT.\r\n",
			"You can't wear anything else around your neck.\r\n",
			"You already have something slung over your back.\r\n",
			"You're already wearing something about your body.\r\n",
			"You're already wearing something on your body.\r\n",
			"You're already wearing something under your clothes.\r\n",
			"You're already wearing something on your arms.\r\n",
			"YOU SHOULD NEVER SEE THIS MESSAGE (#3).  PLEASE REPORT.\r\n",
			"You have something under both of your arms.\r\n",
			"YOU SHOULD NEVER SEE THIS MESSAGE (#4).  PLEASE REPORT.\r\n",
			"You're already wearing something around both of your wrists.\r\n",
			"You're already wearing something on your hands.\r\n",
			"You're already wielding a weapon.\r\n",
			"You're already holding something.\r\n",
			"You're already using a shield.\r\n",
			"YOU SHOULD NEVER SEE THIS MESSAGE (#4).  PLEASE REPORT.\r\n",
			"YOU SHOULD NEVER SEE THIS MESSAGE (#5).  PLEASE REPORT.\r\n",
			"YOU SHOULD NEVER SEE THIS MESSAGE (#6).  PLEASE REPORT.\r\n",
			"YOU SHOULD NEVER SEE THIS MESSAGE (#7).  PLEASE REPORT.\r\n",
			"YOU SHOULD NEVER SEE THIS MESSAGE (#8).  PLEASE REPORT.\r\n",
			"YOU SHOULD NEVER SEE THIS MESSAGE (#9).  PLEASE REPORT.\r\n",
			"YOU SHOULD NEVER SEE THIS MESSAGE (#10).  PLEASE REPORT.\r\n",
			"Who do you think you are? Mr. T?\r\n",
			"You already have something in your belly button.\r\n",
			"You already have something around your waist.\r\n",
			"YOU SHOULD NEVER SEE THIS MESSAGE (#11).  PLEASE REPORT.\r\n",
			"You are already wearing something around both of your thighs.\r\n",
			"You're already wearing something on your legs.\r\n",
			"YOU SHOULD NEVER SEE THIS MESSAGE (#12).  PLEASE REPORT.\r\n",
			"You already have something on each of your ankles.\r\n",
			"You are already wearing something on your feet.\r\n",
			"You're already wearing something on your feet.\r\n",
			"YOU SHOULD NEVER SEE THIS MESSAGE (#13).  PLEASE REPORT.\r\n",
			"You're already wearing underwear.\r\n",
			"You already have something on your chest.\r\n",
			"You already have a lapel pin.\r\n"};

	if (OBJ_IS_FULLY_DAMAGED(obj))
	{
		if (print_messages)
			act("You can't use $p: it's almost destroyed! Take it in for repairs.", FALSE, ch, obj, 0, TO_CHAR);
		return;
	}

	/* first, make sure that the wear position is valid. */
	if (!CAN_WEAR(obj, wear_bitvectors[where]))
	{
		if (print_messages)
			act("You can't wear $p there.", FALSE, ch, obj, 0, TO_CHAR);
		return;
	}

	switch (GET_RACE(ch))
	{
	case RACE_WAKYAMBI:
	case RACE_DRYAD:
	case RACE_ELF:
	case RACE_NIGHTONE:
	case RACE_GHOUL_ELF:
	case RACE_DRAKE_ELF:
		if (IS_OBJ_STAT(obj, ITEM_EXTRA_NOELF))
		{
			if (print_messages)
				send_to_char(ch, "%s isn't sized right for elves.\r\n", capitalize(GET_OBJ_NAME(obj)));
			return;
		}
		break;
	case RACE_DWARF:
	case RACE_MENEHUNE:
	case RACE_GNOME:
	case RACE_KOBOROKURU:
	case RACE_GHOUL_DWARF:
	case RACE_DRAKE_DWARF:
		if (IS_OBJ_STAT(obj, ITEM_EXTRA_NODWARF))
		{
			if (print_messages)
				send_to_char(ch, "%s isn't sized right for dwarfs.\r\n", capitalize(GET_OBJ_NAME(obj)));
			return;
		}
		break;
	case RACE_TROLL:
	case RACE_MINOTAUR:
	case RACE_GIANT:
	case RACE_FOMORI:
	case RACE_CYCLOPS:
	case RACE_GHOUL_TROLL:
	case RACE_DRAKE_TROLL:
		if (IS_OBJ_STAT(obj, ITEM_EXTRA_NOTROLL))
		{
			if (print_messages)
				send_to_char(ch, "%s isn't sized right for trolls.\r\n", capitalize(GET_OBJ_NAME(obj)));
			return;
		}
		break;
	case RACE_HUMAN:
	case RACE_GHOUL_HUMAN:
	case RACE_DRAKE_HUMAN:
		if (IS_OBJ_STAT(obj, ITEM_EXTRA_NOHUMAN))
		{
			if (print_messages)
				send_to_char(ch, "%s isn't sized right for humans.\r\n", capitalize(GET_OBJ_NAME(obj)));
			return;
		}
		break;
	case RACE_ONI:
	case RACE_ORK:
	case RACE_OGRE:
	case RACE_HOBGOBLIN:
	case RACE_GHOUL_ORK:
	case RACE_DRAKE_ORK:
		if (IS_OBJ_STAT(obj, ITEM_EXTRA_NOORK))
		{
			if (print_messages)
				send_to_char(ch, "%s isn't sized right for orks.\r\n", capitalize(GET_OBJ_NAME(obj)));
			return;
		}
		break;
	}
	/* for neck wrist ankles and ears, fingers moved to next case */
	if ((where == WEAR_NECK_1) || (where == WEAR_WRIST_R) || (where == WEAR_LARM) ||
			(where == WEAR_LANKLE) || (where == WEAR_EAR) || (where == WEAR_THIGH_R))
		if (GET_EQ(ch, where))
			where++;

	if (where == WEAR_FINGER_R)
	{
		while ((GET_EQ(ch, where)) && (where < WEAR_FINGER8))
			if (where == WEAR_FINGER_L)
				where = WEAR_FINGER3;
			else
				where++;
	}

	if ((where == WEAR_WIELD || where == WEAR_HOLD) && IS_OBJ_STAT(obj, ITEM_EXTRA_TWOHANDS) &&
			(GET_EQ(ch, WEAR_SHIELD) || GET_EQ(ch, WEAR_HOLD) || GET_EQ(ch, WEAR_WIELD)))
	{
		if (print_messages)
			act("$p requires two free hands.", FALSE, ch, obj, 0, TO_CHAR);
		return;
	}

	if (where == WEAR_WIELD)
	{
		if (!wielded)
		{
			if (!can_wield_both(ch, obj, GET_EQ(ch, WEAR_HOLD)))
			{
				if (print_messages)
				{
					snprintf(buf, sizeof(buf), "You'll have a hard time wielding %s along with $p.", GET_OBJ_NAME(obj));
					act(buf, FALSE, ch, GET_EQ(ch, WEAR_HOLD), 0, TO_CHAR);
				}
				return;
			}
		}
		else
		{
			/* if attempting to wield a second weapon... */
			if (GET_EQ(ch, WEAR_HOLD) || GET_EQ(ch, WEAR_SHIELD))
			{
				if (print_messages)
					send_to_char("Your hands are already full!\r\n", ch);
				return;
			}
			where = WEAR_HOLD;
			if (!can_wield_both(ch, wielded, obj))
			{
				if (print_messages)
				{
					snprintf(buf, sizeof(buf), "You'll have a hard time wielding %s along with $p.", GET_OBJ_NAME(obj));
					act(buf, FALSE, ch, wielded, 0, TO_CHAR);
				}
				return;
			}
		}
	}
	else if (GET_EQ(ch, where))
	{
		send_to_char(already_wearing[where], ch);
		return;
	}

	if (GET_EQ(ch, WEAR_HOLD) && where == WEAR_SHIELD)
	{
		if (print_messages)
			act("$p requires at least one free hand.", FALSE, ch, obj, 0, TO_CHAR);
		return;
	}

	if (GET_EQ(ch, WEAR_WIELD) && IS_OBJ_STAT(GET_EQ(ch, WEAR_WIELD), ITEM_EXTRA_TWOHANDS) &&
			(where == WEAR_HOLD || where == WEAR_SHIELD))
	{
		if (print_messages)
			act("$p requires two free hands.", FALSE, ch, GET_EQ(ch, WEAR_WIELD), 0, TO_CHAR);
		return;
	}
	else if (GET_EQ(ch, WEAR_HOLD) && IS_OBJ_STAT(GET_EQ(ch, WEAR_HOLD), ITEM_EXTRA_TWOHANDS) &&
					 (where == WEAR_WIELD || where == WEAR_SHIELD))
	{
		if (print_messages)
			act("$p requires two free hands.", FALSE, ch, GET_EQ(ch, WEAR_HOLD), 0, TO_CHAR);
		return;
	}

	// Iterate through what they're wearing and check for compatibility-- but only if this is not a helmet.
	if (where != WEAR_HEAD)
	{
		struct obj_data *worn_item = NULL;
		for (int wearloc = 0; wearloc < NUM_WEARS; wearloc++)
		{
			// Helmets never conflict.
			if (wearloc == WEAR_HEAD)
				continue;

			// Not wearing anything here? Skip.
			if (!(worn_item = GET_EQ(ch, wearloc)))
				continue;

			// If this item can't be worn with other armors, check to make sure we meet that restriction.
			if ((IS_OBJ_STAT(obj, ITEM_EXTRA_BLOCKS_ARMOR) || IS_OBJ_STAT(obj, ITEM_EXTRA_HARDENED_ARMOR)) &&
					(GET_OBJ_TYPE(worn_item) == ITEM_WORN && (GET_WORN_IMPACT(worn_item) || GET_WORN_BALLISTIC(worn_item))))
			{
				if (print_messages)
					send_to_char(ch, "You can't wear %s with %s.\r\n", GET_OBJ_NAME(obj), GET_OBJ_NAME(worn_item));
				return;
			}

			// If what they're wearing blocks other armors, and this item is armored, fail.
			if ((IS_OBJ_STAT(worn_item, ITEM_EXTRA_BLOCKS_ARMOR) || IS_OBJ_STAT(worn_item, ITEM_EXTRA_HARDENED_ARMOR)) &&
					(GET_OBJ_TYPE(obj) == ITEM_WORN && (GET_WORN_IMPACT(obj) || GET_WORN_BALLISTIC(obj))))
			{
				if (print_messages)
					send_to_char(ch, "You can't wear %s with %s.\r\n", GET_OBJ_NAME(obj), GET_OBJ_NAME(worn_item));
				return;
			}
		}
	}

	// Gyros take a bit to get into.
	if (GET_OBJ_TYPE(obj) == ITEM_GYRO)
	{
		FAILURE_CASE(CH_IN_COMBAT(ch), "While fighting?? That would be a neat trick.");
		send_to_char("It takes you a moment to get strapped in.\r\n", ch);
		WAIT_STATE(ch, 3 RL_SEC);
	}

	// If it's hardened armor, it's customized to a specific person.
	if (!IS_NPC(ch) && IS_OBJ_STAT(obj, ITEM_EXTRA_HARDENED_ARMOR))
	{
		FAILURE_CASE(CH_IN_COMBAT(ch), "While fighting?? That would be a neat trick.");

		// -1 means it's not yet customized, you can wear it and it molds to you.
		if (GET_WORN_HARDENED_ARMOR_CUSTOMIZED_FOR(obj) == -1)
		{
			send_to_char(ch, "You take a moment to check %s over, making sure the armorer really customized it to fit you.\r\n", GET_OBJ_NAME(obj));
			GET_WORN_HARDENED_ARMOR_CUSTOMIZED_FOR(obj) = GET_IDNUM(ch);
		}
		else if (GET_WORN_HARDENED_ARMOR_CUSTOMIZED_FOR(obj) == GET_IDNUM(ch))
		{
			send_to_char("It takes you a moment to get suited up.\r\n", ch);
		}
		else
		{
			send_to_char(ch, "%s has been customized for someone else.\r\n", capitalize(GET_OBJ_NAME(obj)));
			return;
		}
		// Takes a bit to put it on.
		WAIT_STATE(ch, 4 RL_SEC);
	}

	// House rule: Swapping in and out of ruthenium takes time. This addresses potential cheese.
	{
		SPECIAL(toggled_invis);
		if (GET_OBJ_SPEC(obj) == toggled_invis || obj->obj_flags.bitvector.IsSet(AFF_RUTHENIUM))
		{
			send_to_char("You carefully shrug into the high-tech garment.\r\n", ch);
			WAIT_STATE(ch, 3 RL_SEC);

			// If the invis is already on, they spot you as invis.
			if (obj->obj_flags.bitvector.IsSet(AFF_RUTHENIUM))
			{
				// NPCs around you become alert/alarmed depending on disposition.
				for (struct char_data *viewer = ch->in_veh ? ch->in_veh->people : ch->in_room->people;
						 viewer;
						 viewer = ch->in_veh ? viewer->next_in_veh : viewer->next_in_room)
				{
					if (IS_NPC(viewer) && CAN_SEE(viewer, ch))
					{
						process_spotted_invis(viewer, ch);
					}
				}
			}
		}
	}

	if (print_messages)
		wear_message(ch, obj, where);
	if (obj->in_obj)
		obj_from_obj(obj);
	else
		obj_from_char(obj);
	equip_char(ch, obj, where, true, true);

	if (print_messages)
	{
		switch (get_armor_penalty_grade(ch))
		{
		case ARMOR_PENALTY_TOTAL:
			send_to_char("You are wearing so much armor that you can't move!\r\n", ch);
			break;
		case ARMOR_PENALTY_HEAVY:
			send_to_char("Your movement is severely restricted by your armor.\r\n", ch);
			break;
		case ARMOR_PENALTY_MEDIUM:
			send_to_char("Your movement is restricted by your armor.\r\n", ch);
			break;
		case ARMOR_PENALTY_LIGHT:
			send_to_char("Your movement is mildly restricted by your armor.\r\n", ch);
			break;
		}
	}
}

int find_eq_pos(struct char_data *ch, struct obj_data *obj, char *arg)
{
	int where = -1;

	static const char *keywords[] =
			{
					"!RESERVED!",
					"head",
					"eyes",
					"ear",
					"!RESERVED!",
					"face",
					"neck",
					"!RESERVED!",
					"back",
					"about",
					"body",
					"underneath",
					"arms",
					"underarm",
					"!RESERVED!",
					"wrist",
					"!RESERVED!",
					"hands",
					"!RESERVED!",
					"!RESERVED!",
					"shield",
					"finger",
					"!RESERVED!",
					"!RESERVED!",
					"!RESERVED!",
					"!RESERVED!",
					"!RESERVED!",
					"!RESERVED!",
					"!RESERVED!",
					"belly",
					"waist",
					"thigh",
					"thigh",
					"legs",
					"ankle",
					"!RESERVED!",
					"sock",
					"feet",
					"!RESERVED!",
					"underwear",
					"chest",
					"lapel",
					"\n"};

	if (arg && *arg)
	{
		if ((where = search_block(arg, keywords, FALSE)) >= 0)
			return where;
		else
			send_to_char(ch, "'%s' isn't a part of your body, so you decide to improvise.\r\n", arg);
	}

	if (CAN_WEAR(obj, ITEM_WEAR_FINGER))
		where = WEAR_FINGER_R;
	if (CAN_WEAR(obj, ITEM_WEAR_NECK))
		where = WEAR_NECK_1;
	if (CAN_WEAR(obj, ITEM_WEAR_BODY))
		where = WEAR_BODY;
	if (CAN_WEAR(obj, ITEM_WEAR_HEAD))
		where = WEAR_HEAD;
	if (CAN_WEAR(obj, ITEM_WEAR_LEGS))
		where = WEAR_LEGS;
	if (CAN_WEAR(obj, ITEM_WEAR_FEET))
		where = WEAR_FEET;
	if (CAN_WEAR(obj, ITEM_WEAR_HANDS))
		where = WEAR_HANDS;
	if (CAN_WEAR(obj, ITEM_WEAR_ARMS))
		where = WEAR_ARMS;
	if (CAN_WEAR(obj, ITEM_WEAR_SHIELD))
		where = WEAR_SHIELD;
	if (CAN_WEAR(obj, ITEM_WEAR_ABOUT))
		where = WEAR_ABOUT;
	if (CAN_WEAR(obj, ITEM_WEAR_WAIST))
		where = WEAR_WAIST;
	if (CAN_WEAR(obj, ITEM_WEAR_WRIST))
		where = WEAR_WRIST_R;
	if (CAN_WEAR(obj, ITEM_WEAR_EYES))
		where = WEAR_EYES;
	if (CAN_WEAR(obj, ITEM_WEAR_EAR))
		where = WEAR_EAR;
	if (CAN_WEAR(obj, ITEM_WEAR_UNDER))
		where = WEAR_UNDER;
	if (CAN_WEAR(obj, ITEM_WEAR_ANKLE))
		where = WEAR_LANKLE;
	if (CAN_WEAR(obj, ITEM_WEAR_SOCK))
		where = WEAR_SOCK;
	if (CAN_WEAR(obj, ITEM_WEAR_BACK))
		where = WEAR_BACK;
	if (CAN_WEAR(obj, ITEM_WEAR_BELLY))
		where = WEAR_BELLY;
	if (CAN_WEAR(obj, ITEM_WEAR_ARM))
		where = WEAR_LARM;
	if (CAN_WEAR(obj, ITEM_WEAR_FACE))
		where = WEAR_FACE;
	if (CAN_WEAR(obj, ITEM_WEAR_THIGH))
		where = WEAR_THIGH_R;
	if (CAN_WEAR(obj, ITEM_WEAR_UNDERWEAR))
		where = WEAR_UNDERWEAR;
	if (CAN_WEAR(obj, ITEM_WEAR_CHEST))
		where = WEAR_CHEST;
	if (CAN_WEAR(obj, ITEM_WEAR_LAPEL))
		where = WEAR_LAPEL;
	/*
		if (CAN_WEAR(obj, ITEM_WEAR_MOUTH))
			where = WEAR_MOUTH;
	*/

	return where;
}

ACMD(do_wear)
{
	char arg1[MAX_INPUT_LENGTH];
	char arg2[MAX_INPUT_LENGTH];
	struct obj_data *obj, *next_obj;
	int where = -1, dotmode, items_worn = 0;

	two_arguments(argument, arg1, arg2);

	if (AFF_FLAGGED(ch, AFF_PILOT))
	{
		send_to_char("While driving? Now that would be a good trick!\r\n", ch);
		return;
	}

	if (!*arg1)
	{
		send_to_char("Wear what?\r\n", ch);
		return;
	}
	dotmode = find_all_dots(arg1, sizeof(arg1));

	if (*arg2 && (dotmode != FIND_INDIV))
	{
		send_to_char("You can't specify the same body location for more than one item!\r\n", ch);
		return;
	}
	if (dotmode == FIND_ALL)
	{
		for (obj = ch->carrying; obj; obj = next_obj)
		{
			next_obj = obj->next_content;
			if (CAN_SEE_OBJ(ch, obj) && (where = find_eq_pos(ch, obj, 0)) >= 0)
			{
				items_worn++;
				perform_wear(ch, obj, where, TRUE);
			}
		}
		if (!items_worn)
			send_to_char("You don't seem to have anything wearable in your inventory.\r\n", ch);
	}
	else if (dotmode == FIND_ALLDOT)
	{
		if (!*arg1)
		{
			send_to_char("Wear all of what?\r\n", ch);
			return;
		}
		if (!(obj = get_obj_in_list_vis(ch, arg1, ch->carrying)))
		{
			send_to_char(ch, "You don't seem to have anything named '%s' in your inventory.\r\n", arg1);
		}
		else
			while (obj)
			{
				next_obj = get_obj_in_list_vis(ch, arg1, obj->next_content);
				if ((where = find_eq_pos(ch, obj, 0)) >= 0)
					perform_wear(ch, obj, where, TRUE);
				else
					act("You can't wear $p.", FALSE, ch, obj, 0, TO_CHAR);
				obj = next_obj;
			}
	}
	else
	{
		if (!(obj = get_obj_in_list_vis(ch, arg1, ch->carrying)))
		{
			send_to_char(ch, "You don't seem to have anything named '%s' in your inventory.\r\n", arg1);
		}
		else
		{
			if ((where = find_eq_pos(ch, obj, arg2)) >= 0)
				perform_wear(ch, obj, where, TRUE);
			else if (!*arg2)
				act("You can't wear $p.", FALSE, ch, obj, 0, TO_CHAR);
		}
	}
}

ACMD(do_wield)
{
	struct obj_data *obj, *attach;
	rnum_t rnum;
	one_argument(argument, arg);

	if (!*arg)
		send_to_char("Wield what?\r\n", ch);
	else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying)))
	{
		send_to_char(ch, "You don't seem to have anything named '%s' in your inventory.\r\n", arg);
	}
	else
	{
		if (!CAN_WEAR(obj, ITEM_WEAR_WIELD))
			send_to_char(ch, "You can't wield %s.\r\n", GET_OBJ_NAME(obj));
		else if (GET_OBJ_TYPE(obj) == ITEM_WEAPON && !WEAPON_IS_GUN(obj) && GET_WEAPON_FOCUS_BONDED_BY(obj) == GET_IDNUM(ch) && GET_MAG(ch) * 2 < GET_WEAPON_FOCUS_RATING(obj))
		{
			send_to_char(ch, "%s is too powerful for you to wield!\r\n", capitalize(GET_OBJ_NAME(obj)));
			return;
		}
		else if (GET_OBJ_VAL(obj, 4) >= SKILL_MACHINE_GUNS && GET_OBJ_VAL(obj, 4) <= SKILL_ASSAULT_CANNON &&
						 (GET_STR(ch) < 8 || GET_BOD(ch) < 8))
		{
			bool found = FALSE;
			for (int i = 0; i < (NUM_WEARS - 1); i++)
				if (GET_EQ(ch, i) && GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_GYRO)
					found = TRUE;
			if (GET_OBJ_VAL(obj, 9) && (rnum = real_object(GET_OBJ_VAL(obj, 9))) > -1 &&
					(attach = &obj_proto[rnum]) && GET_OBJ_TYPE(attach) == ITEM_GUN_ACCESSORY && (GET_OBJ_VAL(attach, 1) == ACCESS_BIPOD || GET_OBJ_VAL(attach, 1) == ACCESS_TRIPOD))
				found = TRUE;

			if (!found)
				send_to_char(ch, "Warning: %s is too heavy for you to wield effectively in combat unless you're ^WPRONE^n.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));

			perform_wear(ch, obj, WEAR_WIELD, TRUE);
		}
		else
			perform_wear(ch, obj, WEAR_WIELD, TRUE);
	}
}

ACMD(do_grab)
{
	struct obj_data *obj;

	one_argument(argument, arg);

	if (!*arg)
		send_to_char("Hold what?\r\n", ch);
	else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying)))
	{
		send_to_char(ch, "You don't seem to have anything named '%s' in your inventory.\r\n", arg);
	}
	else
	{
		if (GET_OBJ_TYPE(obj) == ITEM_LIGHT)
			perform_wear(ch, obj, WEAR_LIGHT, TRUE);

		// Auto-wield if it's not holdable but is wieldable.
		else if (!CAN_WEAR(obj, wear_bitvectors[WEAR_HOLD]) && CAN_WEAR(obj, wear_bitvectors[WEAR_WIELD]))
			perform_wear(ch, obj, WEAR_WIELD, TRUE);

		// Hold.
		else
			perform_wear(ch, obj, WEAR_HOLD, TRUE);
	}
}

void perform_remove(struct char_data *ch, int pos)
{
	struct obj_data *obj;

	if (!(obj = ch->equipment[pos]))
	{
		log("Error in perform_remove: bad pos passed.");
		return;
	}
	if (IS_CARRYING_N(ch) >= CAN_CARRY_N(ch))
	{
		act("$p: you can't carry that many items!", FALSE, ch, obj, 0, TO_CHAR);
		return;
	}

	int was_worn_on = obj->worn_on;
	int previous_armor_penalty = get_armor_penalty_grade(ch);

	// Gyros take a bit to get out of.
	if (GET_OBJ_TYPE(obj) == ITEM_GYRO)
	{
		FAILURE_CASE(CH_IN_COMBAT(ch), "While fighting?? That would be a neat trick.");
		send_to_char("It takes you a moment to get unstrapped.\r\n", ch);
		WAIT_STATE(ch, 3 RL_SEC);
	}

	// Taking off hardened armor takes time.
	if (IS_OBJ_STAT(obj, ITEM_EXTRA_HARDENED_ARMOR))
	{
		FAILURE_CASE(CH_IN_COMBAT(ch), "While fighting?? That would be a neat trick.");
		send_to_char("It takes you a moment to step out of the heavily-customized armor.\r\n", ch);
		WAIT_STATE(ch, 4 RL_SEC);
	}

	// House rule: Swapping in and out of ruthenium takes time. This addresses potential cheese.
	{
		SPECIAL(toggled_invis);
		if (GET_OBJ_SPEC(obj) == toggled_invis || obj->obj_flags.bitvector.IsSet(AFF_RUTHENIUM))
		{
			send_to_char("You carefully remove the high-tech garment.\r\n", ch);
			WAIT_STATE(ch, 3 RL_SEC);

			// If the invis is already on, they spot you as someone who has just revealed themselves as previously invis.
			if (obj->obj_flags.bitvector.IsSet(AFF_RUTHENIUM))
			{
				// NPCs around you become alert/alarmed depending on disposition.
				for (struct char_data *viewer = ch->in_veh ? ch->in_veh->people : ch->in_room->people;
						 viewer;
						 viewer = ch->in_veh ? viewer->next_in_veh : viewer->next_in_room)
				{
					if (IS_NPC(viewer) && CAN_SEE(viewer, ch))
					{
						process_spotted_invis(viewer, ch);
					}
				}
			}
		}
	}

	if (was_worn_on == WEAR_HOLD)
	{
		act("You stop holding $p up.", FALSE, ch, obj, 0, TO_CHAR);
		act("$n stops holding $p up.", TRUE, ch, obj, 0, TO_ROOM);
	}
	else
	{
		act("You stop using $p.", FALSE, ch, obj, 0, TO_CHAR);
		act("$n stops using $p.", TRUE, ch, obj, 0, TO_ROOM);
	}

	obj_to_char(unequip_char(ch, pos, true, true, true), ch);

	if (previous_armor_penalty && !get_armor_penalty_grade(ch))
		send_to_char("You can move freely again.\r\n", ch);

	// Unready the holster.
	if (GET_OBJ_TYPE(obj) == ITEM_HOLSTER)
		GET_HOLSTER_READY_STATUS(obj) = 0;
	/* add damage back from stim patches */
	else if (GET_OBJ_TYPE(obj) == ITEM_PATCH && GET_PATCH_TYPE(obj) == PATCH_STIM)
	{
		GET_MENTAL(ch) = MAX(0, GET_MENTAL(ch) - (GET_PATCH_RATING(obj) * 100));
	}

	return;
}

ACMD(do_remove)
{
	struct obj_data *obj;
	int i, dotmode, found;

	one_argument(argument, arg);

	if (AFF_FLAGGED(ch, AFF_PILOT))
	{
		send_to_char("While driving? Now that would be a good trick!\r\n", ch);
		return;
	}

	if (!*arg)
	{
		send_to_char("Remove what?\r\n", ch);
		return;
	}

	dotmode = find_all_dots(arg, sizeof(arg));

	if (dotmode == FIND_ALL)
	{
		found = 0;
		for (i = 0; i < NUM_WEARS; i++)
			if (GET_EQ(ch, i))
			{
				perform_remove(ch, i);
				found = 1;
			}
		if (!found)
			send_to_char("You're not using anything.\r\n", ch);
	}
	else if (dotmode == FIND_ALLDOT)
	{
		if (!*arg)
			send_to_char("Remove all of what?\r\n", ch);
		else
		{
			found = 0;
			for (i = 0; i < NUM_WEARS; i++)
				if (GET_EQ(ch, i) && CAN_SEE_OBJ(ch, GET_EQ(ch, i)) &&
						isname(arg, GET_EQ(ch, i)->text.keywords))
				{
					if (GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_GYRO)
					{
						if (GET_EQ(ch, WEAR_WIELD))
							perform_remove(ch, WEAR_WIELD);
						if (GET_EQ(ch, WEAR_HOLD))
							perform_remove(ch, WEAR_HOLD);
					}
					perform_remove(ch, i);
					found = 1;
				}
			if (!found)
			{
				send_to_char(ch, "You don't seem to be using anything named '%s'.\r\n", arg);
			}
		}
	}
	else
	{
		if (!(obj = get_object_in_equip_vis(ch, arg, ch->equipment, &i)))
		{
			send_to_char(ch, "You don't seem to be using anything named '%s'.\r\n", arg);
		}
		else
		{
			if (GET_OBJ_TYPE(obj) == ITEM_GYRO)
			{
				if (GET_EQ(ch, WEAR_WIELD))
					perform_remove(ch, WEAR_WIELD);
				if (GET_EQ(ch, WEAR_HOLD))
					perform_remove(ch, WEAR_HOLD);
			}
			perform_remove(ch, i);
		}
	}
}

ACMD(do_activate)
{
	struct obj_data *obj;
	int i;

	if (!*argument)
	{
		send_to_char("What do you want to activate?\r\n", ch);
		return;
	}

	if (GET_TRADITION(ch) == TRAD_ADEPT && !str_str(argument, "pain editor") && !str_str(argument, "editor") && !str_str(argument, "voice modulator"))
	{
		char name[120], tokens[MAX_STRING_LENGTH], *s;
		extern int ability_cost(int abil, int level);
		int desired_level;
		strncpy(tokens, argument, sizeof(tokens) - 1);
		if ((strtok(tokens, "\"") && (s = strtok(NULL, "\""))) || (strtok(tokens, "'") && (s = strtok(NULL, "'"))))
		{
			strncpy(name, s, sizeof(name) - 1);
			if ((s = strtok(NULL, "\0")))
			{
				skip_spaces(&s);
				strncpy(buf1, s, sizeof(buf1) - 1);
			}
			else
				*buf1 = '\0';
			one_argument(argument, buf);
			desired_level = atoi(buf);
		}
		else
		{
			half_chop(argument, buf, buf1, sizeof(buf1));
			if (!(desired_level = atoi(buf)))
			{
				strncpy(name, buf, sizeof(name) - 1);
			}
			else
			{
				char remainder[MAX_INPUT_LENGTH];
				half_chop(buf1, buf2, remainder, sizeof(remainder));
				strlcpy(name, buf2, sizeof(name));
				strlcpy(buf1, remainder, sizeof(buf1));
			}
		}

		if (desired_level <= 0) {
			send_to_char("Power level must be greater than zero!", ch);
			return;
		}

		// Find powers they have skill in first.
		for (i = 0; i < ADEPT_NUMPOWER; i++)
			if (GET_POWER_TOTAL(ch, i) && is_abbrev(name, adept_powers[i]))
				break;
		// Find powers they don't have skill in.
		if (i >= ADEPT_NUMPOWER)
			for (i = 0; i < ADEPT_NUMPOWER; i++)
				if (is_abbrev(name, adept_powers[i]))
					if (!GET_POWER_TOTAL(ch, i))
					{
						send_to_char(ch, "You haven't learned the %s power yet.\r\n", adept_powers[i]);
						return;
					}

		// Prevent activation of incompatible powers.
		// Penetrating strike can't be conbined with distance strike (SotA 2064, pg 67).
		if ((i == ADEPT_DISTANCE_STRIKE) && GET_POWER(ch, ADEPT_PENETRATINGSTRIKE))
		{
			send_to_char("Distance strike is not compatible with penetrating strike.\r\n", ch);
			return;
		}
		if ((i == ADEPT_PENETRATINGSTRIKE) && GET_POWER(ch, ADEPT_DISTANCE_STRIKE))
		{
			send_to_char("Penetrating strike is not compatible with distance strike.\r\n", ch);
			return;
		}
		// Improved reflexes isn't checked/blocked here. Its incompatibility (as written in
		// SR3, pg 169) can be interpreted as "does not stack" and so highest applies.
		// This is also how its handled in handler.cpp: affect_total(struct char_data * ch)

		if (i < ADEPT_NUMPOWER)
		{
			if (desired_level == 0)
				desired_level = GET_POWER_TOTAL(ch, i);
			else
				desired_level = MIN(desired_level, GET_POWER_TOTAL(ch, i));
			int total = 0;

			for (int q = desired_level > GET_POWER_ACT(ch, i) ? desired_level : GET_POWER_ACT(ch, i);
					 desired_level > GET_POWER_ACT(ch, i) ? GET_POWER_ACT(ch, i) < q : desired_level < q;
					 q--)
			{
				total += ability_cost(i, q);
			}

			if (desired_level < GET_POWER_ACT(ch, i))
				total *= -1;

			int remaining_pp = ((int)(GET_REAL_MAG(ch) / 100) * 100) - GET_POWER_POINTS(ch);
			if (total > remaining_pp)
				send_to_char(ch, "That costs %.2f points to activate, but you only have %.2f free.\r\n", ((float)total / 100), ((float)remaining_pp / 100));
			else if (GET_POWER_ACT(ch, i) == desired_level)
			{
				send_to_char(ch, "%s is already active at rank %d.\r\n", CAP(adept_powers[i]), desired_level);
				return;
			}
			else
			{
				GET_POWER_ACT(ch, i) = desired_level;
				GET_POWER_POINTS(ch) += total;
				if (i == ADEPT_BOOST_BOD || i == ADEPT_BOOST_QUI || i == ADEPT_BOOST_STR)
				{
					send_to_char(ch, "You activate %s. You'll need to use the ^WBOOST^n command to engage it.\r\n", adept_powers[i]);
				}
				else if (i == ADEPT_LIVINGFOCUS)
				{
					send_to_char(ch, "You activate %s. You'll need to use the ^WFOCUS^n command to engage it.\r\n", adept_powers[i]);
				}
				else
				{
					send_to_char(ch, "You activate %s.\r\n", adept_powers[i]);
				}
				affect_total(ch);
			}
			return;
		}
	}

	any_one_arg(argument, arg);

	if (is_abbrev(arg, "pain editor") || is_abbrev(arg, "editor"))
	{
		for (obj = ch->bioware; obj; obj = obj->next_content)
			if (GET_BIOWARE_TYPE(obj) == BIO_PAINEDITOR)
			{
				if (GET_BIOWARE_IS_ACTIVATED(obj))
					send_to_char("You have already activated your Pain Editor.\r\n", ch);
				else
				{
					GET_BIOWARE_IS_ACTIVATED(obj) = 1;
					send_to_char("You lose all tactile perception.\r\n", ch);
				}
				return;
			}
	}
	if (is_abbrev(arg, "voice modulator"))
	{
		for (obj = ch->cyberware; obj; obj = obj->next_content)
			if (GET_CYBERWARE_TYPE(obj) == CYB_VOICEMOD)
			{
				if (GET_CYBERWARE_FLAGS(obj))
					send_to_char("You have already activated your Voice Modulator.\r\n", ch);
				else
				{
					GET_CYBERWARE_FLAGS(obj) = 1;
					send_to_char("You begin to speak like Stephen Hawking.\r\n", ch);
				}
				return;
			}
	}

	if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying)))
	{
		if (!(obj = get_object_in_equip_vis(ch, arg, ch->equipment, &i)))
		{
			if (!(obj = get_obj_in_list_vis(ch, arg, ch->cyberware)))
			{
				send_to_char(ch, "You don't have %s %s.\r\n", AN(arg), arg);
				return;
			}
		}
	}

	if (GET_OBJ_TYPE(obj) == ITEM_CYBERWARE)
	{
		switch (GET_CYBERWARE_TYPE(obj))
		{
		case CYB_ARMS:
			if (IS_SET(GET_CYBERWARE_FLAGS(obj), ARMS_MOD_GYROMOUNT))
			{
				if (GET_CYBERWARE_IS_DISABLED(obj))
				{
					send_to_char(ch, "You activate the gyromount on %s.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));
				}
				else
				{
					send_to_char(ch, "%s's gyromount was already activated.\r\n", CAP(GET_OBJ_NAME(obj)));
				}
				GET_CYBERWARE_IS_DISABLED(obj) = FALSE;
				return;
			}
			send_to_char(ch, "%s doesn't have a gyromount to activate.\r\n", CAP(GET_OBJ_NAME(obj)));
			return;
		}
		send_to_char(ch, "You can't activate %s.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));
		return;
	}

	if (GET_OBJ_TYPE(obj) == ITEM_FOCUS && GET_IDNUM(ch) == GET_FOCUS_BONDED_TO(obj))
	{
		if (obj->worn_on == NOWHERE)
		{
			send_to_char("You have to be wearing or holding a focus to activate it.\r\n", ch);
			return;
		}
		if (GET_FOCUS_BOND_TIME_REMAINING(obj) > 0)
		{
			send_to_char(ch, "You haven't finished bonding %s yet.\r\n", GET_OBJ_NAME(obj));
			return;
		}
		if (GET_FOCUS_TYPE(obj) == FOCI_SUSTAINED)
		{
			send_to_char("This focus is automatically activated when you cast the bonded spell on it.\r\n", ch);
			return;
		}
#ifdef DIES_IRAE
		if (GET_FOCI(ch) >= GET_INT(ch))
		{
			send_to_char(ch, "You have too many foci activated already. You're limited to %s's intelligence (%d).\r\n", GET_CHAR_NAME(ch), GET_INT(ch));
			return;
		}
#endif
		if (GET_FOCUS_ACTIVATED(obj))
		{
			send_to_char(ch, "%s is already activated.\r\n", CAP(GET_OBJ_NAME(obj)));
			return;
		}
		if (GET_FOCUS_TYPE(obj) == FOCI_POWER)
			for (int x = 0; x < NUM_WEARS; x++)
				if (GET_EQ(ch, x) && GET_OBJ_TYPE(GET_EQ(ch, x)) == ITEM_FOCUS && GET_FOCUS_TYPE(GET_EQ(ch, x)) == FOCI_POWER && GET_FOCUS_ACTIVATED(GET_EQ(ch, x)))
				{
					send_to_char("You can only activate one power focus at a time.\r\n", ch);
					return;
				}
		send_to_char(ch, "You activate %s.\r\n", GET_OBJ_NAME(obj));
		GET_FOCUS_ACTIVATED(obj) = 1;
		GET_FOCI(ch)
		++;
		affect_total(ch);
		return;
	}
	else if (GET_OBJ_TYPE(obj) == ITEM_WEAPON && WEAPON_IS_FOCUS(obj))
	{
		send_to_char(ch, "There's no need to activate or deactivate %s. Just ^WWIELD^n it when you want to use it.\r\n", GET_OBJ_NAME(obj));
		return;
	}
	else if (GET_OBJ_TYPE(obj) == ITEM_LIGHT)
	{
		send_to_char(ch, "There's no need to activate or deactivate %s. Just ^WHOLD^n it when you want to use it.\r\n", GET_OBJ_NAME(obj));
		return;
	}
	else if (GET_OBJ_TYPE(obj) == ITEM_DOCWAGON)
	{
		bool bonded = (GET_DOCWAGON_BONDED_IDNUM(obj) == GET_IDNUM(ch));
		bool bonded_to_someone_else = GET_DOCWAGON_BONDED_IDNUM(obj) && !bonded;
		bool worn = (obj->worn_by == ch);

		char tmp_arg[MAX_INPUT_LENGTH];
		snprintf(tmp_arg, sizeof(tmp_arg), " %s", arg);

		if (bonded && worn)
		{
			send_to_char(ch, "%s is already bonded and worn: You're all set.\r\n", CAP(GET_OBJ_NAME(obj)));
		}
		else if (bonded_to_someone_else)
		{
			send_to_char(ch, "%s is bonded to someone else, so it won't work for you.\r\n", CAP(GET_OBJ_NAME(obj)));
		}
		else if (bonded)
		{
			send_to_char(ch, "%s is already bonded, so you just need to ^WWEAR^n it. Trying that now...\r\n", CAP(GET_OBJ_NAME(obj)));
			do_wear(ch, tmp_arg, 0, 0);
		}
		else if (worn)
		{
			send_to_char(ch, "%s hasn't been bonded yet, so you need to ^WREMOVE^n it and ^WBOND^n it, then ^WWEAR^n it again. Trying that now...\r\n", CAP(GET_OBJ_NAME(obj)));
			obj_to_char(unequip_char(ch, obj->worn_on, FALSE, TRUE, true), ch);
			do_bond(ch, tmp_arg, 0, 0);
			do_wear(ch, tmp_arg, 0, 0);
		}
		else
		{
			do_bond(ch, tmp_arg, 0, 0);
			do_wear(ch, tmp_arg, 0, 0);
		}
		return;
	}
	else if (GET_OBJ_TYPE(obj) != ITEM_MONEY || !GET_OBJ_VAL(obj, 1))
	{
		send_to_char(ch, "You can't activate %s.\r\n", GET_OBJ_NAME(obj));
		return;
	}
	// Credsticks from here down.
	else if (GET_ITEM_MONEY_CREDSTICK_OWNER_ID(obj) != 0)
	{
		send_to_char(ch, "But %s has already been activated!\r\n", GET_OBJ_NAME(obj));
		return;
	}

	if (!IS_NPC(ch))
	{
		GET_ITEM_MONEY_CREDSTICK_IS_PC_OWNED(obj) = 1;
		GET_ITEM_MONEY_CREDSTICK_OWNER_ID(obj) = GET_IDNUM(ch);
	}
	else
	{
		GET_ITEM_MONEY_CREDSTICK_IS_PC_OWNED(obj) = 0;
		GET_ITEM_MONEY_CREDSTICK_OWNER_ID(obj) = ch->nr;
	}

	switch (GET_ITEM_MONEY_CREDSTICK_GRADE(obj))
	{
	case 1:
		GET_ITEM_MONEY_CREDSTICK_LOCKCODE(obj) = number(100000, 999999);
		send_to_char(ch, "You key %d as the passcode and the display flashes green.\r\n", GET_ITEM_MONEY_CREDSTICK_LOCKCODE(obj));
		break;
	case 2:
		send_to_char("You press your thumb against the pad and the display flashes green.\r\n", ch);
		break;
	case 3:
		act("$p scans your retina and the display flashes green.", FALSE, ch, obj, 0, TO_CHAR);
		break;
	}

	// Credsticks, when activated by a player, dump their contents to their bank account immediately.
	transfer_credstick_contents_to_bank(obj, ch);
}

ACMD(do_type)
{
	struct obj_data *obj;
	int i;

	any_one_arg(any_one_arg(argument, arg), buf);

	if (!*arg || !*buf)
	{
		send_to_char("Type what into what?\r\n", ch);
		return;
	}

	if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying)))
		if (!(obj = get_object_in_equip_vis(ch, arg, ch->equipment, &i)))
		{
			send_to_char(ch, "You don't have %s %s.\r\n", AN(arg), arg);
			return;
		}

	if (GET_OBJ_TYPE(obj) != ITEM_MONEY || !GET_OBJ_VAL(obj, 1) || GET_OBJ_VAL(obj, 2) != 1)
	{
		send_to_char(ch, "You can't type a code into %s^n.\r\n", GET_OBJ_NAME(obj));
		return;
	}

	i = atoi(buf);

	if (i < 100000 || i > 999999)
		send_to_char("Code must be 6-digits!\r\n", ch);
	else if (i != GET_OBJ_VAL(obj, 5))
	{
		send_to_char("The display flashes red and beeps annoyingly.\r\n", ch);
		WAIT_STATE(ch, 2 RL_SEC);
	}
	else
	{
		send_to_char("The display flashes green.\r\n", ch);
		GET_OBJ_VAL(obj, 3) = 0;
		GET_OBJ_VAL(obj, 4) = 0;
		GET_OBJ_VAL(obj, 5) = 0;
	}
}

ACMD(do_crack)
{
	int skill = SKILL_ELECTRONICS, mod = 0, rating;
	struct obj_data *obj, *shop = NULL;
	if (GET_POS(ch) == POS_FIGHTING)
	{
		send_to_char("Good luck...\r\n", ch);
		return;
	}
	skill = get_skill(ch, skill, mod);

	any_one_arg(argument, buf);
	if (IS_WORKING(ch))
	{
		send_to_char(TOOBUSY, ch);
		return;
	}
	if (!*buf)
	{
		send_to_char("What do you want to crack?\r\n", ch);
		return;
	}

	if (!(obj = get_obj_in_list_vis(ch, buf, ch->carrying)))
	{
		send_to_char(ch, "You don't have %s %s.\r\n", AN(buf), buf);
		return;
	}

	if (GET_OBJ_TYPE(obj) != ITEM_MONEY || !GET_OBJ_VAL(obj, 1) || GET_OBJ_VAL(obj, 2) > 3 || !GET_OBJ_VAL(obj, 2))
	{
		send_to_char(ch, "%s isn't someone else's activated credstick, not much cracking to be done there.\r\n", capitalize(GET_OBJ_NAME(obj)));
		return;
	}
	else if (!GET_OBJ_VAL(obj, 4))
	{
		act("But $p's not even activated!", FALSE, ch, obj, 0, TO_CHAR);
		return;
	}

	rating = GET_OBJ_VAL(obj, 2) * 3 + 6 + mod + (GET_OBJ_ATTEMPT(obj) * 2);
	if ((shop = find_workshop(ch, TYPE_ELECTRONIC)))
		rating -= 2;

	if (!shop && !has_kit(ch, TYPE_ELECTRONIC))
	{
		send_to_char("You need at least an electronics kit to crack a credstick.\r\n", ch);
		return;
	}

	if (1)
	{
		char rbuf[MAX_STRING_LENGTH];
		snprintf(rbuf, sizeof(rbuf), "Crack: Skill %d, Rating %d, Modify Target %d.\n",
						 skill, rating, modify_target(ch));
		act(rbuf, FALSE, ch, NULL, NULL, TO_ROLLS);
	}

	// If you've attempted this so much that your TN is 100+, just stop. This prevents DB failure on attempts > 255.
	if (rating >= 100)
	{
		act("$p's locked itself out from too many attempts! Try again later.", FALSE, ch, obj, 0, TO_CHAR);
		return;
	}

	if (success_test(skill, rating + modify_target(ch)) < 1)
	{
		act("$p sounds a series of beeps, and flashes red.", FALSE, ch, obj, 0, TO_CHAR);
		GET_OBJ_ATTEMPT(obj)
		++;
	}
	else
	{
		act("$p hums silently, and its display flashes green.", FALSE, ch, obj, 0, TO_CHAR);
		GET_OBJ_TIMER(obj) = GET_OBJ_VAL(obj, 3) = GET_OBJ_VAL(obj, 4) = GET_OBJ_VAL(obj, 5) = 0;
	}
}

// Draw a weapon from the provided holster. Returns 1 if drawn, 0 if not.
int draw_from_readied_holster(struct char_data *ch, struct obj_data *holster)
{
	struct obj_data *contents = holster->contains;

	if (!contents)
	{
		// Readied holster was empty: un-ready the holster, but continue looking for a valid ready holster.
		GET_HOLSTER_READY_STATUS(holster) = 0;
		return 0;
	}

	if (OBJ_IS_FULLY_DAMAGED(contents))
	{
		act("Draw check: Skipping $p, fully damaged.", FALSE, ch, contents, 0, TO_ROLLS);
		return 0;
	}

	// Check to see if it can be wielded.
	if (!CAN_WEAR(contents, ITEM_WEAR_WIELD))
	{
		act("Draw check: Skipping $p, can't be wielded.", FALSE, ch, contents, 0, TO_ROLLS);
		return 0;
	}

	// Our hands are full.
	if (GET_EQ(ch, WEAR_WIELD) && (GET_EQ(ch, WEAR_HOLD) || GET_EQ(ch, WEAR_SHIELD)))
	{
		act("Draw check: Skipping $p, hands are full.", FALSE, ch, contents, 0, TO_ROLLS);
		return 0;
	}

	// We're wielding a 2H item.
	if (GET_EQ(ch, WEAR_WIELD) && IS_OBJ_STAT(GET_EQ(ch, WEAR_WIELD), ITEM_EXTRA_TWOHANDS))
	{
		act("Draw check: Skipping $p, wielding a 2H item already.", FALSE, ch, contents, 0, TO_ROLLS);
		return 0;
	}

	// We're holding a 2H item.
	if (GET_EQ(ch, WEAR_HOLD) && IS_OBJ_STAT(GET_EQ(ch, WEAR_HOLD), ITEM_EXTRA_TWOHANDS))
	{
		act("Draw check: Skipping $p, holding a 2H item already.", FALSE, ch, contents, 0, TO_ROLLS);
		return 0;
	}

	// We're holding something and drawing a 2H item.
	if ((GET_EQ(ch, WEAR_WIELD) || GET_EQ(ch, WEAR_HOLD) || GET_EQ(ch, WEAR_SHIELD)) && IS_OBJ_STAT(contents, ITEM_EXTRA_TWOHANDS))
	{
		act("Draw check: Skipping $p, have something in hand and drawing 2H item.", FALSE, ch, contents, 0, TO_ROLLS);
		return 0;
	}

	// TODO: What does this check mean? (ed: probably intended to prevent machine guns and assault cannons from being drawn. Nonfunctional.)
	if (GET_OBJ_VAL(holster, 4) >= SKILL_MACHINE_GUNS && GET_OBJ_VAL(holster, 4) <= SKILL_ASSAULT_CANNON)
	{
		act("Draw check: Skipping $p, skill check failure.", FALSE, ch, contents, 0, TO_ROLLS);
		return 0;
	}

	// Refuse to let someone draw a weapon focus that they can't use.
	if (GET_OBJ_TYPE(contents) == ITEM_WEAPON && !WEAPON_IS_GUN(contents) && GET_WEAPON_FOCUS_RATING(contents) > 0 && GET_WEAPON_FOCUS_BONDED_BY(contents) == GET_IDNUM(ch))
	{
		if (GET_MAG(ch) * 2 < GET_WEAPON_FOCUS_RATING(contents))
		{
			act("Draw check: Skipping $p, bonded focus would exceed magic rating focus total.", FALSE, ch, contents, 0, TO_ROLLS);
			return 0;
		}

#ifdef DIES_IRAE
		int total_rating = get_total_active_focus_rating(ch, &equipped_count);

		if (GET_FOCI(ch) + 1 >= GET_INT(ch))
		{
			act("Draw check: Skipping $p, would exceed max focus count.", FALSE, ch, contents, 0, TO_ROLLS);
			return 0;
		}
		if ((total_rating + GET_WEAPON_FOCUS_RATING(contents)) * 100 >= GET_MAG(ch) * 2)
		{
			act("Draw check: Skipping $p, would exceed max focus force.", FALSE, ch, contents, 0, TO_ROLLS);
			return 0;
		}
#endif
	}

	// Apply racial limitations.
	switch (GET_RACE(ch))
	{
	case RACE_WAKYAMBI:
	case RACE_DRYAD:
	case RACE_ELF:
	case RACE_NIGHTONE:
	case RACE_GHOUL_ELF:
	case RACE_DRAKE_ELF:
		if (IS_OBJ_STAT(contents, ITEM_EXTRA_NOELF))
		{
			act("Draw check: Skipping $p, racial check failure (elf).", FALSE, ch, contents, 0, TO_ROLLS);
			return 0;
		}
		break;
	case RACE_DWARF:
	case RACE_MENEHUNE:
	case RACE_GNOME:
	case RACE_KOBOROKURU:
	case RACE_GHOUL_DWARF:
	case RACE_DRAKE_DWARF:
		if (IS_OBJ_STAT(contents, ITEM_EXTRA_NODWARF))
		{
			act("Draw check: Skipping $p, racial check failure (dwarf).", FALSE, ch, contents, 0, TO_ROLLS);
			return 0;
		}
		break;
	case RACE_TROLL:
	case RACE_MINOTAUR:
	case RACE_GIANT:
	case RACE_FOMORI:
	case RACE_CYCLOPS:
	case RACE_GHOUL_TROLL:
	case RACE_DRAKE_TROLL:
		if (IS_OBJ_STAT(contents, ITEM_EXTRA_NOTROLL))
		{
			act("Draw check: Skipping $p, racial check failure (troll).", FALSE, ch, contents, 0, TO_ROLLS);
			return 0;
		}
		break;
	case RACE_HUMAN:
	case RACE_GHOUL_HUMAN:
	case RACE_DRAKE_HUMAN:
		if (IS_OBJ_STAT(contents, ITEM_EXTRA_NOHUMAN))
		{
			act("Draw check: Skipping $p, racial check failure (human).", FALSE, ch, contents, 0, TO_ROLLS);
			return 0;
		}
		break;
	case RACE_ONI:
	case RACE_ORK:
	case RACE_OGRE:
	case RACE_HOBGOBLIN:
	case RACE_GHOUL_ORK:
	case RACE_DRAKE_ORK:
		if (IS_OBJ_STAT(contents, ITEM_EXTRA_NOORK))
		{
			act("Draw check: Skipping $p, racial check failure (ork).", FALSE, ch, contents, 0, TO_ROLLS);
			return 0;
		}
		break;
	}

	// Staff-only limitations.
	if (IS_OBJ_STAT(contents, ITEM_EXTRA_STAFF_ONLY) && !access_level(ch, LVL_BUILDER))
	{
		act("Draw check: Skipping $p, racial check failure (staff limitation).", FALSE, ch, contents, 0, TO_ROLLS);
		mudlog_vfprintf(ch, LOG_SYSLOG, "%s has STAFF-ONLY object %s (%ld) in a holster??", GET_CHAR_NAME(ch), GET_OBJ_NAME(contents), GET_OBJ_VNUM(contents));
		return 0;
	}

	// Sanity check: We have at least one free hand.
	if (GET_EQ(ch, WEAR_WIELD) && GET_EQ(ch, WEAR_HOLD))
	{
		mudlog("SYSERR: Logical constraint failure-- we're wielding two items and trying to draw a third after safeguards!", ch, LOG_SYSLOG, TRUE);
		return 0;
	}

	int where = 0;
	if (!GET_EQ(ch, WEAR_WIELD) && can_wield_both(ch, GET_EQ(ch, WEAR_HOLD), contents))
		where = WEAR_WIELD;
	else if (!GET_EQ(ch, WEAR_HOLD) && can_wield_both(ch, GET_EQ(ch, WEAR_WIELD), contents))
		where = WEAR_HOLD;

	if (where)
	{
		obj_from_obj(contents);
		equip_char(ch, contents, where);
		act("You draw $p from $P.", FALSE, ch, contents, holster, TO_CHAR);
		act("$n draws $p from $P.", TRUE, ch, contents, holster, TO_ROOM);

		if (GET_OBJ_SPEC(contents) == weapon_dominator)
		{
			GET_OBJ_TYPE(contents) = ITEM_OTHER;
			dominator_mode_switch(ch, contents, DOMINATOR_MODE_PARALYZER);
		}

		// We wielded 1 weapon.
		return 1;
	}
	else
	{
		act("Draw check: Unable to draw $p, no viable slot?", FALSE, ch, contents, 0, TO_ROLLS);
		return 0;
	}

	// We wielded 0 weapons.
	return 0;
}

int find_and_draw_weapon(struct char_data *ch)
{
	struct obj_data *potential_holster, *obj;
	int i = 0;

	// Look in fingertip first to get it out of the way.
	// At some point we need to write a mechanism to select what is drawn automatically -- Nodens
	for (potential_holster = ch->cyberware; potential_holster; potential_holster = potential_holster->next_content)
		if (GET_CYBERWARE_TYPE(potential_holster) == CYB_FINGERTIP && GET_HOLSTER_READY_STATUS(potential_holster))
			i += draw_from_readied_holster(ch, potential_holster);

	// Go through all the wearslots, provided that the character is not already wielding & holding things.
	for (int draw_only_if_most_recent = 1; draw_only_if_most_recent >= 0; draw_only_if_most_recent--)
	{
		for (int x = 0; x < NUM_WEARS && (!GET_EQ(ch, WEAR_WIELD) || !GET_EQ(ch, WEAR_HOLD)); x++)
		{
			if ((potential_holster = GET_EQ(ch, x)))
			{
				if (GET_OBJ_TYPE(potential_holster) == ITEM_HOLSTER)
				{
					if (GET_HOLSTER_READY_STATUS(potential_holster) && (!draw_only_if_most_recent || GET_HOLSTER_MOST_RECENT_FLAG(potential_holster)))
					{
						i += draw_from_readied_holster(ch, potential_holster);
					}
				}

				else if (GET_OBJ_TYPE(potential_holster) == ITEM_WORN)
				{
					for (obj = potential_holster->contains; obj; obj = obj->next_content)
					{
						if (GET_OBJ_TYPE(obj) == ITEM_HOLSTER && GET_HOLSTER_READY_STATUS(obj) && (!draw_only_if_most_recent || GET_HOLSTER_MOST_RECENT_FLAG(potential_holster)))
						{
							i += draw_from_readied_holster(ch, obj);
						}
					}
				}
			}
		}
	}

	affect_total(ch);

	return i;
}

bool holster_can_fit(struct obj_data *holster, struct obj_data *weapon)
{
	bool small_weapon = GET_WEAPON_SKILL(weapon) == SKILL_PISTOLS || GET_WEAPON_SKILL(weapon) == SKILL_SMG;
	switch (GET_HOLSTER_TYPE(holster))
	{
	case HOLSTER_TYPE_SMALL_GUNS:
		// Handle standard small guns.
		if (WEAPON_IS_GUN(weapon) && small_weapon)
			return TRUE;

		// Check for ranged tasers.
		return (GET_WEAPON_ATTACK_TYPE(weapon) == WEAP_TASER && GET_WEAPON_SKILL(weapon) == SKILL_TASERS);
	case HOLSTER_TYPE_MELEE_WEAPONS:
		// Handle standard melee weapons.
		if (!WEAPON_IS_GUN(weapon))
			return TRUE;

		// Check for melee tasers.
		return (GET_WEAPON_ATTACK_TYPE(weapon) == WEAP_TASER && GET_WEAPON_SKILL(weapon) != SKILL_TASERS);
	case HOLSTER_TYPE_LARGE_GUNS:
		return WEAPON_IS_GUN(weapon) && !small_weapon;
	}

	return FALSE;
}

struct obj_data *find_holster_that_fits_weapon(struct char_data *ch, struct obj_data *weapon)
{
	// If the weapon is a monowhip see if we have a fingertip compartment and it's not currently full
	if (obj_index[GET_OBJ_RNUM(weapon)].wfunc == monowhip)
	{
		struct obj_data *cont;
		for (cont = ch->cyberware; cont; cont = cont->next_content)
			if (GET_CYBERWARE_TYPE(cont) == CYB_FINGERTIP && !cont->contains)
				return cont;
	}
	// Look at their worn items. We exclude inventory here.
	for (int x = 0; x < NUM_WEARS; x++)
	{
		// Is it a holster?
		if (GET_EQ(ch, x) && GET_OBJ_TYPE(GET_EQ(ch, x)) == ITEM_HOLSTER && !GET_EQ(ch, x)->contains && holster_can_fit(GET_EQ(ch, x), weapon))
		{
			return GET_EQ(ch, x);
		}

		// Does it contain a holster?
		if (GET_EQ(ch, x) && GET_OBJ_TYPE(GET_EQ(ch, x)) == ITEM_WORN && GET_EQ(ch, x)->contains)
		{
			for (struct obj_data *temp = GET_EQ(ch, x)->contains; temp; temp = temp->next_content)
			{
				if (GET_OBJ_TYPE(temp) == ITEM_HOLSTER && !temp->contains && holster_can_fit(temp, weapon))
				{
					return temp;
				}
			}
		}
	}

	return NULL;
}

ACMD(do_holster)
{ // Holster <gun> <holster>
	struct obj_data *obj = NULL, *cont = NULL;
	struct char_data *tmp_char;
	int dontfit = 0;

	if (IS_ASTRAL(ch))
	{
		send_to_char("Astral projections can't touch things.\r\n", ch);
		return;
	}

	two_arguments(argument, buf, buf1);

	if (!argument || !*argument)
	{
		// Holster your wielded weapon.
		if ((obj = GET_EQ(ch, WEAR_WIELD)) || (obj = GET_EQ(ch, WEAR_HOLD)))
		{
			if (GET_OBJ_TYPE(obj) != ITEM_WEAPON)
			{
				send_to_char(ch, "%s isn't a weapon.\r\n", capitalize(GET_OBJ_NAME(obj)));
				return;
			}
		}
		else
		{
			send_to_char("You're not wielding anything.\r\n", ch);
			return;
		}

		// Find a generic holster.
		cont = find_holster_that_fits_weapon(ch, obj);
	}

	// Something was specified.
	else
	{
		// Find weapon.
		if (!generic_find(buf, FIND_OBJ_EQUIP | FIND_OBJ_INV, ch, &tmp_char, &obj))
		{
			send_to_char(ch, "You're not carrying a '%s'.\r\n", buf);
			return;
		}

		// Find holster.
		if (!*buf1 || !generic_find(buf1, FIND_OBJ_EQUIP | FIND_OBJ_INV, ch, &tmp_char, &cont))
		{
			cont = find_holster_that_fits_weapon(ch, obj);
		}
	}

	FAILURE_CASE_PRINTF(!cont, "You don't have any empty %s that will fit %s.",
											!WEAPON_IS_GUN(obj) ? "sheaths" : "holsters", decapitalize_a_an(GET_OBJ_NAME(obj)));

	FAILURE_CASE_PRINTF(cont == obj, "You can't put %s inside itself.", decapitalize_a_an(GET_OBJ_NAME(obj)));

	FAILURE_CASE_PRINTF(GET_OBJ_TYPE(obj) != ITEM_WEAPON, "%s is not a holsterable weapon.", capitalize(GET_OBJ_NAME(obj)));

	FAILURE_CASE_PRINTF(GET_OBJ_TYPE(cont) != ITEM_HOLSTER && (GET_OBJ_TYPE(cont) != ITEM_CYBERWARE || GET_CYBERWARE_TYPE(cont) != CYB_FINGERTIP),
											"%s is not a holster.", capitalize(GET_OBJ_NAME(cont)));

	FAILURE_CASE_PRINTF(cont->contains, "There's already something in %s.", decapitalize_a_an(GET_OBJ_NAME(cont)));

	const char *madefor = "<error, report to staff>";

	if (GET_OBJ_TYPE(cont) == ITEM_HOLSTER && !holster_can_fit(cont, obj))
	{
		dontfit++;

		switch (GET_HOLSTER_TYPE(cont))
		{
		case HOLSTER_TYPE_SMALL_GUNS:
			madefor = "pistols and SMGs";
			break;
		case HOLSTER_TYPE_MELEE_WEAPONS:
			madefor = "melee weapons";
			break;
		case HOLSTER_TYPE_LARGE_GUNS:
			madefor = "rifles and other longarms";
			break;
		}

		FAILURE_CASE_PRINTF(dontfit, "%s is made for %s, so %s won't fit in it.",
												capitalize(GET_OBJ_NAME(cont)), madefor, decapitalize_a_an(GET_OBJ_NAME(obj)));
	}

	if (GET_OBJ_SPEC(obj) == weapon_dominator)
	{
		dominator_mode_switch(ch, obj, DOMINATOR_MODE_PARALYZER);
	}

	if (obj->worn_by)
		obj = unequip_char(ch, obj->worn_on, TRUE);
	else
		obj_from_char(obj);
	obj_to_obj(obj, cont);
	send_to_char(ch, "You slip %s^n into %s^n and ready it for a quick draw.\r\n", GET_OBJ_NAME(obj), decapitalize_a_an(GET_OBJ_NAME(cont)));
	act("$n slips $p into $P.", FALSE, ch, obj, cont, TO_ROOM);
	GET_HOLSTER_READY_STATUS(cont) = 1;

	set_dropped_by_info(obj, ch);

	// Remove last-holstered flag from all holsters.
	for (int wear_idx = 0; wear_idx < NUM_WEARS; wear_idx++)
	{
		struct obj_data *temp_holst = GET_EQ(ch, wear_idx);

		if (!temp_holst)
			continue;

		if (GET_OBJ_TYPE(temp_holst) == ITEM_HOLSTER)
		{
			GET_HOLSTER_MOST_RECENT_FLAG(temp_holst) = FALSE;
		}
		else if (GET_OBJ_TYPE(temp_holst) == ITEM_WORN)
		{
			for (struct obj_data *holst_contents = temp_holst->contains; holst_contents; holst_contents = holst_contents->next_content)
			{
				if (GET_OBJ_TYPE(holst_contents) == ITEM_HOLSTER)
				{
					GET_HOLSTER_MOST_RECENT_FLAG(holst_contents) = FALSE;
				}
			}
		}
	}

	// Set last-holstered flag on this holster.
	GET_HOLSTER_MOST_RECENT_FLAG(cont) = TRUE;
	return;
}

ACMD(do_ready)
{
	struct obj_data *obj, *finger;
	struct char_data *tmp_char;
	int num = 0;

	skip_spaces(&argument);
	one_argument(argument, buf);

	if (!*buf)
	{
		send_to_char(ch, "You have to ready something.\r\n");
		return;
	}

	// If we have a fingertip compartment that matches the argument, set our object to that.
	if ((finger = get_obj_in_list_vis(ch, buf, ch->cyberware)) && GET_CYBERWARE_TYPE(finger) == CYB_FINGERTIP)
		obj = finger;
	else
	{
		if (!(generic_find(buf, FIND_OBJ_EQUIP, ch, &tmp_char, &obj)))
		{
			send_to_char(ch, "You don't seem to be using anything named '%s'.\r\n", argument);
			return;
		}
	}
	if (GET_OBJ_TYPE(obj) != ITEM_HOLSTER && (GET_OBJ_TYPE(obj) != ITEM_CYBERWARE || GET_CYBERWARE_TYPE(obj) != CYB_FINGERTIP))
	{
		send_to_char(ch, "%s is not a weapons holster.\r\n", capitalize(GET_OBJ_NAME(obj)));
		return;
	}
	if (!obj->contains)
	{
		send_to_char(ch, "There is nothing in %s.\r\n", GET_OBJ_NAME(obj));
		return;
	}
	if (GET_HOLSTER_READY_STATUS(obj) > 0)
	{
		act("You unready $p.", FALSE, ch, obj, NULL, TO_CHAR);
		GET_HOLSTER_READY_STATUS(obj) = 0;
		return;
	}
	else
	{
		// Check if we have any fingertip compartments that are readied
		for (struct obj_data *cyber = ch->cyberware; cyber; cyber = cyber->next_content)
			if (GET_CYBERWARE_TYPE(cyber) == CYB_FINGERTIP && GET_HOLSTER_READY_STATUS(cyber))
				num++;

		for (int i = 0; i < NUM_WEARS; i++)
			if (GET_EQ(ch, i))
			{
				if (GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_HOLSTER && GET_HOLSTER_READY_STATUS(GET_EQ(ch, i)) > 0)
					num++;
				else if (GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_WORN && GET_EQ(ch, i)->contains)
					for (struct obj_data *cont = GET_EQ(ch, i)->contains; cont; cont = cont->next_content)
						if (GET_OBJ_TYPE(cont) == ITEM_HOLSTER && GET_HOLSTER_READY_STATUS(cont) > 0)
							num++;
			}
		if (num >= 2)
		{
			send_to_char(ch, "You already have 2 holsters readied.\r\n");
			return;
		}
		act("You ready $p.", FALSE, ch, obj, NULL, TO_CHAR);
		GET_HOLSTER_READY_STATUS(obj) = 1;
		return;
	}
}

ACMD(do_draw)
{
	if (GET_EQ(ch, WEAR_WIELD) && (IS_OBJ_STAT(GET_EQ(ch, WEAR_WIELD), ITEM_EXTRA_TWOHANDS) || GET_EQ(ch, WEAR_HOLD)))
	{
		send_to_char(ch, "Your hands are full.\r\n");
		return;
	}

	skip_spaces(&argument);
	if (*argument)
	{
		struct obj_data *holster = NULL, *finger = NULL;
		struct char_data *tmp_char;

		// Draw from a specific holster.
		if ((finger = get_obj_in_list_vis(ch, argument, ch->cyberware)) && GET_CYBERWARE_TYPE(finger) == CYB_FINGERTIP)
		{
			holster = finger;
		}
		else
		{
			if (!(generic_find(argument, FIND_OBJ_EQUIP, ch, &tmp_char, &holster)))
			{
				/* TODO: Eventually I'll add code to let you draw specific weapons by name.
				// Search all worn holsters for a weapon matching the name.
				for (int wearslot = 0; wearslot < NUM_WEARS; wearslot++) {
					struct obj_data *eq = GET_EQ(ch, wearslot);
					if (GET_OBJ_TYPE(eq) == ITEM_HOLSTER && asdf)
					// problem here is we have to recurse a bit-- worn holsters, and also worn items containing holsters
				}
				*/
				send_to_char(ch, "You don't seem to be wearing a holster named '%s'.\r\n", argument);
				return;
			}

			if (GET_OBJ_TYPE(holster) != ITEM_HOLSTER)
			{
				send_to_char(ch, "%s is not a weapons holster.\r\n", capitalize(GET_OBJ_NAME(holster)));
				return;
			}
		}

		if (!holster->contains)
		{
			send_to_char(ch, "There is nothing in %s.\r\n", GET_OBJ_NAME(holster));
			return;
		}

		if (!draw_from_readied_holster(ch, holster))
		{
			send_to_char(ch, "%s isn't compatible with your current loadout.\r\n", CAP(GET_OBJ_NAME(holster->contains)));
		}
		else
		{
			GET_HOLSTER_MOST_RECENT_FLAG(holster) = FALSE;
		}
		return;
	}

	if (find_and_draw_weapon(ch) == 0)
	{
		send_to_char(ch, "You have nothing to draw, or you're trying to draw a "
										 "two-handed weapon with something in your hands. Make sure "
										 "you're wearing a sheath or holster with a weapon in it, "
										 "and that you've used the ^WREADY^n command on the sheath "
										 "or holster.\r\n");
		return;
	}
}

ACMD(do_break)
{
	skip_spaces(&argument);

	// Mindlink.
	if (is_abbrev(argument, "mindlink"))
	{
		for (struct sustain_data *sust = GET_SUSTAINED(ch); sust; sust = sust->next)
		{
			if (sust->spell == SPELL_MINDLINK)
			{
				send_to_char("You dissolve any mindlinks cast on you.\r\n", ch);
				end_all_sustained_spells_of_type_affecting_ch(SPELL_MINDLINK, 0, ch);
				return;
			}
		}
		send_to_char("You are not currently under a mindlink.\r\n", ch);
		return;
	}

	// Stealth.
	if (is_abbrev(argument, "silence") || is_abbrev(argument, "stealth"))
	{
		for (struct sustain_data *sust = GET_SUSTAINED(ch); sust; sust = sust->next)
		{
			if (sust->spell == SPELL_STEALTH)
			{
				send_to_char("You reject any stealth spells cast on you.\r\n", ch);
				end_all_sustained_spells_of_type_affecting_ch(SPELL_STEALTH, 0, ch);
				return;
			}
		}
		send_to_char("You are not currently under a stealth spell.\r\n", ch);
		return;
	}

	// Tooth compartment.
	if (is_abbrev(argument, "tooth compartment") || is_abbrev(argument, "compartment"))
	{
		struct obj_data *obj = ch->cyberware, *contents = NULL;
		for (; obj; obj = obj->next_content)
			if (GET_OBJ_VAL(obj, 0) == CYB_TOOTHCOMPARTMENT && GET_OBJ_VAL(obj, 3))
				break;

		if (!obj)
			send_to_char("You don't have a breakable tooth compartment.\r\n", ch);
		else if (!(contents = obj->contains))
			send_to_char("Your tooth compartment is empty, so breaking it now would be a waste.\r\n", ch);
		else
		{
			if (GET_OBJ_TYPE(contents) != ITEM_DRUG)
			{
				send_to_char(ch, "Your tooth compartment contains something that isn't drugs! Aborting.\r\n");
				snprintf(buf, sizeof(buf), "%s's tooth compartment contains non-drug item %s!", GET_CHAR_NAME(ch), GET_OBJ_NAME(contents));
				mudlog(buf, ch, LOG_SYSLOG, TRUE);
				return;
			}

			send_to_char("You bite down hard on the tooth compartment, breaking it open.\r\n", ch);
			obj_from_cyberware(obj);
			GET_ESSHOLE(ch) += calculate_ware_essence_or_index_cost(ch, obj);
			obj_from_obj(contents);
			extract_obj(obj);
			if (do_drug_take(ch, contents))
				return;
		}
		return;
	}

	// No matches found.
	if (*argument)
	{
		send_to_char("You can only break mindlinks and tooth compartments. Are you looking for the ^WDESTROY^n or ^WJUNK^n commands?\r\n", ch);
	}
	else
	{
		send_to_char("Syntax: ^WBREAK TOOTH^n or ^WBREAK MINDLINK^n.\r\n", ch);
	}
}

ACMD(do_keep)
{
	if (!argument || !*argument)
	{
		send_to_char("What do you want to flag as undroppable?\r\n", ch);
		return;
	}

	struct obj_data *obj = NULL;
	struct char_data *tmp_char = NULL;

	generic_find(argument, FIND_OBJ_EQUIP | FIND_OBJ_INV, ch, &tmp_char, &obj);

	if (!obj)
	{
		send_to_char(ch, "You're not carrying or wearing anything named '%s'.\r\n", argument);
		return;
	}

	if (IS_OBJ_STAT(obj, ITEM_EXTRA_KEPT))
	{
		send_to_char(ch, "You un-keep %s.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));
		GET_OBJ_EXTRA(obj).RemoveBit(ITEM_EXTRA_KEPT);
	}
	else
	{
		send_to_char(ch, "You set %s as kept. You will be unable to drop, junk, or give it away until you use this command on it again.\r\n", decapitalize_a_an(GET_OBJ_NAME(obj)));
		GET_OBJ_EXTRA(obj).SetBit(ITEM_EXTRA_KEPT);
	}

	playerDB.SaveChar(ch);
}

ACMD(do_conceal_reveal)
{
	skip_spaces(&argument);
	FAILURE_CASE(!*argument, "Syntax: ^WCONCEAL <focus>^n  (to conceal it when other players look at you)\r\n"
													 "        ^WREVEAL <focus>^n   (to show it again)\r\n");

	int dotmode = find_all_dots(argument, sizeof(argument));

	if (dotmode == FIND_ALL || dotmode == FIND_ALLDOT)
	{
		struct obj_data *eq;
		bool took_action = FALSE;

		for (int wear_idx = 0; wear_idx < NUM_WEARS; wear_idx++)
		{
			if ((eq = GET_EQ(ch, wear_idx)) && GET_OBJ_TYPE(eq) == ITEM_FOCUS)
			{
				if (dotmode == FIND_ALL || keyword_appears_in_obj(argument, eq))
				{
					if (subcmd == SCMD_CONCEAL)
					{
						if (!IS_OBJ_STAT(eq, ITEM_EXTRA_CONCEALED_IN_EQ))
						{
							GET_OBJ_EXTRA(eq).SetBit(ITEM_EXTRA_CONCEALED_IN_EQ);
							took_action = TRUE;
						}
					}
					else
					{
						if (IS_OBJ_STAT(eq, ITEM_EXTRA_CONCEALED_IN_EQ))
						{
							GET_OBJ_EXTRA(eq).RemoveBit(ITEM_EXTRA_CONCEALED_IN_EQ);
							took_action = TRUE;
						}
					}
				}
			}
		}

		if (took_action)
		{
			send_to_char("You rearrange your equipment.\r\n", ch);
			act("$n rearranges $s equipment.", TRUE, ch, 0, 0, TO_ROOM);
			playerDB.SaveChar(ch);
		}
		else
		{
			if (dotmode == FIND_ALL)
			{
				send_to_char(ch, "You're not wearing any %sconcealed foci.\r\n", subcmd == SCMD_CONCEAL ? "non-" : "");
			}
			else
			{
				send_to_char(ch, "Couldn't find any %shidden equipped foci named '%s'.\r\n", subcmd == SCMD_CONCEAL ? "non-" : "", argument);
			}
		}
		return;
	}

	struct char_data *dummy = NULL;
	struct obj_data *obj = NULL;
	generic_find(argument, FIND_OBJ_EQUIP, ch, &dummy, &obj);

	FAILURE_CASE_PRINTF(!obj, "Couldn't find any equipped foci named '%s'.", argument);
	FAILURE_CASE_PRINTF(GET_OBJ_TYPE(obj) != ITEM_FOCUS, "%s is not a focus.", decapitalize_a_an(GET_OBJ_NAME(obj)));

	if (subcmd == SCMD_CONCEAL)
	{
		GET_OBJ_EXTRA(obj).SetBit(ITEM_EXTRA_CONCEALED_IN_EQ);
	}
	else
	{
		GET_OBJ_EXTRA(obj).RemoveBit(ITEM_EXTRA_CONCEALED_IN_EQ);
	}

	send_to_char("You rearrange your equipment.\r\n", ch);
	act("$n rearranges $s equipment.", TRUE, ch, 0, 0, TO_ROOM);
	playerDB.SaveChar(ch);
}
