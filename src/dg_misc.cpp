/**************************************************************************
*  File: dg_misc.cpp                                                      *
*  Usage: Target validation and script-inflicted damage.                  *
*                                                                         *
*  Death's Gate MUD is based on CircleMUD, Copyright (C) 1993, 94.        *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
*                                                                         *
*  $Author: Mark A. Heilpern/egreen/Welcor $                              *
*  $Date: 2004/10/11 12:07:00$                                            *
*  $Revision: 1.0.14 $                                                    *
*                                                                         *
*  Ported to AwakeMUD CE by Fizban.                                       *
*                                                                         *
*  Two stock DG commands are absent here, and deliberately so:            *
*                                                                         *
*  %cast% built on call_magic(level, spellnum), which assumes a spell     *
*  table indexed by level. Awake's magic is force and drain based, cast   *
*  through a caster who owns the spell, so there is nothing to hand it.   *
*                                                                         *
*  %dg_affect% built on a per-character list of timed affected_type       *
*  entries, expired by affect_update(). Awake has no such list -- its     *
*  modifiers come from cyberware, sustained spells and equipment, all     *
*  recomputed rather than ticked down. Giving scripts timed affects means *
*  designing that subsystem first, which is a decision for the codebase   *
*  owners rather than something a port should smuggle in.                 *
**************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "structs.hpp"
#include "awake.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "db.hpp"
#include "handler.hpp"
#include "interpreter.hpp"
#include "constants.hpp"
#include "dg_scripts.hpp"

extern bool damage(struct char_data *ch, struct char_data *victim, int dam, int attacktype, bool is_physical);

/* Used throughout the dg_*cmd files to check whether a char may be targetted.
 * DG_ALLOW_GODS is off for %force%, on for the likes of %teleport%. */
int valid_dg_target(struct char_data *ch, int bitvector)
{
  /* Purged but not yet taken off the lists: as far as the rest of the game is
   * concerned this one is already gone. */
  if (dg_extraction_is_pending(ch))
    return FALSE;

  if (IS_NPC(ch))
    return TRUE;  /* all NPCs are allowed as targets */

  if (ch->desc && (STATE(ch->desc) != CON_PLAYING))
    return FALSE; /* only PCs who are actually playing can be targetted */

  if (!IS_SENATOR(ch))
    return TRUE;  /* as well as all mortals */

  if (!IS_SET(bitvector, DG_ALLOW_GODS) && access_level(ch, LVL_ADMIN))
    return FALSE; /* but not the highest staff */

  if (!PRF_FLAGGED(ch, PRF_NOHASSLE))
    return TRUE;  /* the ones in between, so long as nohassle is off */

  return FALSE;   /* the rest are staff with nohassle on */
}

/* Damage dealt by %damage%. The figure is in Awake's damage boxes, not in the
 * hit points tbaMUD's version subtracted, and it runs through damage() so that
 * armor, bioware, docwagon and death are all handled the same way a punch is.
 * The victim is passed as its own attacker, as drug and poison damage are. */
void script_damage(struct char_data *vict, int dam)
{
  if (!vict)
    return;

  if (dam > 0 && access_level(vict, LVL_ADMIN)) {
    send_to_char("Being the staff member you are, you sidestep a trap obviously placed to kill you.\r\n", vict);
    return;
  }

  if (GET_POS(vict) == POS_DEAD)
    return;

  if (dam < 0) {
    /* Negative damage heals, which damage() has no way to express. */
    GET_PHYSICAL(vict) = MIN(GET_MAX_PHYSICAL(vict), GET_PHYSICAL(vict) - dam * 100);
    update_pos(vict);
    return;
  }

  damage(vict, vict, MIN(dam, convert_damage(DEADLY)), TYPE_SCRIPT, TRUE);
}
