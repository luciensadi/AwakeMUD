#include <time.h>

#include "structs.hpp"
#include "awake.hpp"
#include "utils.hpp"
#include "comm.hpp"
#include "interpreter.hpp"
#include "handler.hpp"
#include "db.hpp"
#include "dblist.hpp"
#include "constants.hpp"
#include "newmatrix.hpp"
#include "memory.hpp"
#include "quest.hpp"
#include "config.hpp"
#include "ignore_system.hpp"
#include "moderation.hpp"
#include "pets.hpp"
#include "otaku.hpp"
#include "matrix_storage.hpp"
#include "gmcp.hpp"

#ifdef TEMPORARY_COMPILATION_GUARD
extern void create_secret_container(struct char_data *ch);
extern void create_secret_contents(struct char_data *ch);
#endif

#define PERSONA ch->persona
#define PERSONA_CONDITION ch->persona->condition
#define DECKER PERSONA->decker
struct ic_info dummy_ic;

extern struct otaku_echo echoes[];

#define ICON_IS_IC(icon) (!(icon)->decker)

#define HAS_HITCHER_JACK(deck) ([](struct obj_data *d) { \
    for (struct obj_data *hitch = (d)->contains; hitch; hitch = hitch->next_content) \
        if (GET_OBJ_TYPE(hitch) == ITEM_DECK_ACCESSORY && GET_OBJ_VAL(hitch, 0) == 1 && GET_OBJ_VAL(hitch, 1) == 3) \
            return true; \
    return false; \
})(deck)

extern void order_list(struct matrix_icon *start);
extern void create_program(struct char_data *ch);
extern void create_part(struct char_data *ch);
extern void create_deck(struct char_data *ch);
extern void create_spell(struct char_data *ch);
extern void create_ammo(struct char_data *ch);
extern void create_art(struct char_data *ch);
extern void create_complex_form(struct char_data *ch);

ACMD_DECLARE(do_look);

struct matrix_icon *find_icon_by_id(vnum_t idnum);

void gain_matrix_karma(struct matrix_icon *icon, struct matrix_icon *targ);

#define IS_PROACTIVE(IC) ((IC)->ic.type != IC_PROBE && (IC)->ic.type != IC_SCRAMBLE && (IC)->ic.type != IC_TARBABY && (IC)->ic.type != IC_TARPIT && (IC)->ic.type != IC_SCOUT)
#define HOST matrix[host]
void make_seen(struct matrix_icon *icon, int idnum)
{
  struct seen_data *seen;
  for (seen = icon->decker->seen; seen; seen = seen->next)
    if (seen->idnum == idnum)
      return;

  seen = new seen_data;
  seen->idnum = idnum;
  if (icon->decker->seen)
    seen->next = icon->decker->seen;
  icon->decker->seen = seen;
}

void clear_hitcher(struct char_data *ch, bool shouldNotify)
{
    // Safety check: ensure ch is valid and actually hitched to someone
    if (!ch || !ch->hitched_to) {
        return;
    }

    // Notify the hitcher if requested
    if (shouldNotify) {
        send_to_char("Your hitcher has disconnected.\r\n", ch->hitched_to);
    }

    // Clear references
    if (ch->hitched_to->persona && ch->hitched_to->persona->decker)
      ch->hitched_to->persona->decker->hitcher = NULL; // Super safety check. Clear functions should always be safe.
    ch->hitched_to = NULL;

    // Remove matrix flag
    PLR_FLAGS(ch).RemoveBit(PLR_MATRIX);
}

struct word_level {
  int min_security;
  int max_security;
  std::vector<const char*> words;
  word_level(int a, int b, std::vector<const char*> c) : min_security(a), max_security(b), words(c) { } 
};

const word_level adjectives[] = {
  {-99, 4, { "public", "unclassified", "open", "generic", "draft", "preliminary" }},
  {5, 7, { "confidential", "restricted", "internal", "classified" }},
  {8, 10, { "sensitive", "top secret", "forensic", "proprietary" }},
  {10, 99, { "blacklisted", "wetwork directive", "omega-level", "deadly" }}
};

const word_level subjects[] = {
  {-99, 4, { "office memos", "routine logs", "basic records", "casual reports", "reports", "files", "logs", "records" }},
  {5, 7, { "financial records", "contractor dossiers", "research notes", "surveillance logs", "personnel files", "briefings", "summaries", "case files", "studies" }},
  {8, 10, { "incident reports", "breach logs", "market analysis", "corporate agreements", "legal briefings", "private correspondence", "internal audits", "compliance reviews", "strategic outlines", "operational plans" }},
  {10, 99, { "R&D logs", "espionage reports", "communications archives", "cybernetic blueprints", "black ops directives", "encryption keys", "core intelligence" }}
};

const word_level details[] = {
  {-99, 4, { "ver. a12", "backup archive", "non-critical", "dated cache", "outdated" }},
  {5, 7, { "time-sensitive", "pending review", "recovered segment", "audit trail" }},
  {8, 10, { "corrupted data", "incriminating evidence", "access level 5", "govt. oversight" }},
  {10, 99, { "catastrophic exploit", "lethal payload", "redacted", "classified" }}
};

char* generate_paydata_name(int security, int size) {
  snprintf(buf, sizeof(buf), "");
  if ((rand() % 2 == 0)) {
    for (word_level adj : adjectives) {
      if (security < adj.min_security) continue;
      if (security > adj.max_security) continue;
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s ", str_dup(adj.words[rand() % adj.words.size()]));
      break;
    }      
  }

  for (word_level sub : subjects) {
    if (security < sub.min_security) continue;
    if (security > sub.max_security) continue;
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s", str_dup(sub.words[rand() % sub.words.size()]));
    break;
  }      

  if ((rand() % 3 == 0)) {
    for (word_level det : details) {
      if (security < det.min_security) continue;
      if (security > det.max_security) continue;
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " - %s", str_dup(det.words[rand() % det.words.size()]));
      break;
    }    
  }

  return capitalize(buf);
}

struct matrix_file * spawn_paydata(struct matrix_icon *icon) {
  struct matrix_file *paydata = create_matrix_file(NULL, OBJ_LOAD_REASON_SPAWN_PAYDATA);
  paydata->file_type = MATRIX_FILE_PAYDATA;
  paydata->size = (number(1, 6) + number(1, 6)) * MAX(5, (20 - (5 * matrix[icon->in_host].color)));
  paydata->found_by = icon->idnum;
  paydata->name = str_dup(generate_paydata_name(matrix[icon->in_host].security, paydata->size));
  paydata->from_host_vnum = icon->in_host;
  paydata->from_host_color = matrix[icon->in_host].color;

  int defense[5][6] = {{ 0, 0, 0, 1, 1, 1 },
                       { 0, 0, 1, 1, 2, 2 },
                       { 0, 1, 1, 2, 2, 3 },
                       { 1, 1, 2, 2, 3, 3 },
                       { 1, 2, 2, 3, 3, 3 }};
  int def = defense[matrix[icon->in_host].color][number(0, 4)];
  if (def) {
    int rate[4];
    int rating = number(1, 6) + number(1, 6);
    if (rating < 6) {
      rate[0] = 4;
      rate[1] = 5;
      rate[2] = 6;
      rate[3] = 8;
    } else if (rating < 9) {
      rate[0] = 5;
      rate[1] = 7;
      rate[2] = 8;
      rate[3] = 10;
    } else if (rating < 12) {
      rate[0] = 6;
      rate[1] = 9;
      rate[2] = 10;
      rate[3] = 11;
    } else {
      rate[0] = 7;
      rate[1] = 10;
      rate[2] = 12;
      rate[3] = 12;
    }
    if (matrix[icon->in_host].security < 5)
      paydata->rating = rate[0];
    else if (matrix[icon->in_host].security < 8)
      paydata->rating = rate[1];
    else if (matrix[icon->in_host].security < 11)
      paydata->rating = rate[2];
    else
      paydata->rating = rate[3];
    switch (def) {
    case 1:
      paydata->file_protection = FILE_PROTECTION_SCRAMBLED;
      break;
    case 2:
    case 3:
      // Bomb rating.
      if (number(1, 6) > 4)
        paydata->file_protection = 4;
      else
        paydata->file_protection = FILE_PROTECTION_DATABOMB;
      break;
    }
  }

  move_matrix_file_to(paydata, &matrix[icon->in_host]);
  return paydata;
}

// Spawns an IC to the host. Returns TRUE on successful spawn, FALSE otherwise.
bool spawn_ic(struct matrix_icon *target, vnum_t ic_vnum, int triggerstep) {
  rnum_t ic_rnum = real_ic(ic_vnum);

  if (!target || !target->decker) {
    mudlog("SYSERR: Precondition failure-- got an invalid target to spawn_ic()!", NULL, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  if (ic_rnum < 0) {
    vnum_t ic_to_spawn = -1;

    // Find a new one by seeking down the trigger list for this host until we encounter one.
    // Remember that trigger lists are ordered largest / highest first.
    for (struct trigger_step *trig = matrix[target->in_host].trigger; trig; trig = trig->next) {
      // We refuse to spawn anything from a previous step in the security sheaf.
      // We'll have to make do with the latest IC located.
      if (trig->step < target->decker->last_trigger && ic_to_spawn >= 0)
        break;

      // Overwrite the last IC we saw.
      if (trig->ic > 0)
        ic_to_spawn = trig->ic;
    }

    // At this point, ic_to_spawn contains the lowest IC vnum that is at or above their last trigger OR the last IC-loading trigger in the sheaf.

    if (ic_to_spawn < 0 || (ic_rnum = real_ic(ic_to_spawn)) < 0) {
      // In order for us to trigger this code, there needs to have been zero valid ICs at or above that trigger.
      return FALSE;
    }
  }

  // If we've gotten here, it's valid. Spawn it.
  struct matrix_icon *ic = read_ic(ic_rnum, REAL);

  // Sanity check: IC must exist.
  if (!ic) {
    snprintf(buf, sizeof(buf), "SYSERR: Attempted to process trigger for non-existent IC. Read failure caused on host %ld, trigger step %d, IC %ld.", matrix[target->in_host].vnum, triggerstep, ic_vnum);
    mudlog(buf, NULL, LOG_SYSLOG, TRUE);
    return FALSE;
  }

  // Sanity check: IC must have a vnum field, otherwise when it's deleted it'll wipe out proto values.
  if (!ic->vnum) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR while processing host %ld, trigger step %d, IC %ld: IC's datastructure vnum is zero! Forcibly setting it.", matrix[target->in_host].vnum, triggerstep, ic_vnum);
    ic->vnum = ic_vnum;
  }

  ic->ic.target = target->idnum;
  for (struct matrix_icon *icon = matrix[target->in_host].icons; icon; icon = icon->next_in_host) {
    if (icon->decker) {
      // Shortcut: If the icon is the target AND we're in locate-paydata spawn mode, make it visible with no fuss.
      if (icon == target && triggerstep == -1) {
        make_seen(icon, ic->idnum);
      }
      // Otherwise, do the standard 'X is triggered' routine.
      else {
        int tn = ic->ic.rating;
        if (matrix[target->in_host].shutdown)
          tn -= 2;
        int success = success_test(icon->decker->sensor, tn);
        if (success >= 3) {
          send_to_icon(icon, "%s^n is triggered.\r\n", CAP(ic->name));
          make_seen(icon, ic->idnum);
        } else if (success == 2)
          send_to_icon(icon, "%s^n is triggered, but it slips out of view almost immediately.\r\n", CAP(ic->name));
        else if (success == 1)
          send_to_icon(icon, "An IC has entered the host.\r\n");
      }
    }
  }

  icon_to_host(ic, target->in_host);

  return TRUE;
}

void roll_matrix_init(struct matrix_icon *icon)
{
  int init_dice = 1;
  if (icon->decker && icon->decker->ch)
  {
    // Matrix pg 18 & 24, available bonuses are response increase, reality filter, and hot asist
    init_dice += GET_INIT_DICE(icon->decker->ch) + icon->decker->response + (icon->decker->reality ? 1 : 0) + (icon->decker->asist[0] ? 1 : 0);

    if (icon->type == ICON_LIVING_PERSONA) {
      init_dice = MIN(5, init_dice + GET_ECHO(icon->decker->ch, ECHO_OVERCLOCK));
    }

    // Apply Matrix 'trode net cap (max init dice 2d6)
    if (GET_EQ(icon->decker->ch, WEAR_HEAD) && IS_OBJ_STAT(GET_EQ(icon->decker->ch, WEAR_HEAD), ITEM_EXTRA_TRODE_NET)) {
      init_dice = MIN(init_dice, 2);
    }

    icon->initiative = GET_REA(icon->decker->ch) + (icon->decker->response * 2) + (icon->decker->reality ? 2 : 0) + (icon->decker->asist[0] ? 2 : 0);
  } else
  {
    icon->initiative = icon->ic.rating;
    if (matrix[icon->in_host].shutdown)
      icon->initiative--;
    init_dice += matrix[icon->in_host].color;
  }
  icon->initiative += dice(init_dice, 6);
  icon->initiative -= matrix[icon->in_host].pass * 10;
}

void check_trigger(rnum_t host, struct char_data *ch)
{
  if (!PERSONA || !DECKER)
    return;
  for (struct trigger_step *trig= HOST.trigger; trig; trig = trig->next)
    if (trig->step > DECKER->last_trigger && trig->step <= DECKER->tally)
    {
      if (trig->alert && trig->alert > HOST.alert) {
        HOST.alert = trig->alert;
        switch (HOST.alert) {
        case MTX_ALERT_PASSIVE:
          send_to_host(host, "Your view begins to flash orange.\r\n", NULL, FALSE);
          break;
        case MTX_ALERT_ACTIVE:
          send_to_host(host, "Sirens join the flashing lights.\r\n", NULL, FALSE);
          break;
        }
      }

      // Attempt to spawn an IC. If the trigger's IC value is invalid, nothing will happen.
      spawn_ic(PERSONA, trig->ic, trig->step);
    }
  DECKER->last_trigger = DECKER->tally;
}

bool tarbaby(struct matrix_file *prog, struct char_data *ch, struct matrix_icon *ic)
{
  int target = ic->ic.rating;
  if (matrix[ic->in_host].shutdown)
    target -= 2;
  
  int suc = success_test(prog->rating, target);
  suc -= success_test(target, prog->rating);
  if (suc < 0)
  {
    struct matrix_file *temp;
    send_to_icon(PERSONA, "%s^n crashes your %s^n!\r\n", CAP(ic->name), prog->name);
    DECKER->active += prog->size;
    REMOVE_FROM_LIST(prog, DECKER->software, next_file);
    // Otaku complex forms cannot be destroyed from memory, so no problem there.
    if (ic->ic.type == IC_TARPIT 
      && !DECKER->deck->obj_flags.extra_flags.IsSet(ITEM_EXTRA_OTAKU_RESONANCE)
      && success_test(target, DECKER->mpcp + DECKER->hardening) > 0)
      for (struct obj_data *copy = DECKER->deck->contains; copy; copy = copy->next_content) {
        if (!strcmp(GET_OBJ_NAME(copy), prog->name)) {
          send_to_icon(PERSONA, "It destroys all copies in storage memory as well!\r\n");
          GET_OBJ_VAL(DECKER->deck, 5) -= GET_OBJ_VAL(copy, 2);
          extract_obj(copy);
          break;
        }
      }
    extract_matrix_file(prog);
    extract_icon(ic);
    return TRUE;
  }
  return FALSE;
}

bool dumpshock(struct matrix_icon *icon)
{
  if (!icon) return FALSE;

  if (icon->decker && icon->decker->ch)
  {
    send_to_char(icon->decker->ch, "You are dumped from the matrix!\r\n");
    snprintf(buf, sizeof(buf), "%s^n depixelates and vanishes from the host.\r\n", CAP(icon->name));
    send_to_host(icon->in_host, buf, icon, FALSE);

    // Clean up their uploads.
    if (icon->decker->deck) {
      for (struct obj_data *soft = icon->decker->deck->contains; soft; soft = soft->next_content)
        if (GET_OBJ_TYPE(soft) != ITEM_PART && GET_OBJ_VAL(soft, 9) > 0 && GET_OBJ_VAL(soft, 8) == 1)
          GET_OBJ_VAL(soft, 9) = GET_OBJ_VAL(soft, 8) = 0;
    }

    // Clean out downloads involving them.
    for (struct matrix_file *file = matrix[icon->in_host].files; file; file = file->next_file) {
      if (file->file_worker != icon->idnum) continue;
      file->transferring_to = NULL;
      file->found_by = 0;
      file->file_worker = 0;
      file->transfer_remaining = 0;
    }

    int resist = -success_test(GET_WIL(icon->decker->ch), matrix[icon->in_host].security);
    int dam = convert_damage(stage(resist, MIN(matrix[icon->in_host].color + 1, DEADLY)));

    struct obj_data *jack = get_datajack(icon->decker->ch, FALSE);

    if (GET_OBJ_TYPE(jack) == ITEM_CYBERWARE) {
      if (GET_CYBERWARE_TYPE(jack) == CYB_DATAJACK) {
        // Palm induction.
        if (GET_CYBERWARE_FLAGS(jack) == DATA_INDUCTION)
          snprintf(buf, sizeof(buf), "$n's hand suddenly recoils from $s induction pad, electricity arcing between the two surfaces!");
        // Head jack.
        else 
          snprintf(buf, sizeof(buf), "$n suddenly jerks forward and rips the jack out of $s head!");
      } else {
        // Eye jack.
        snprintf(buf, sizeof(buf), "$n suddenly jerks forward and rips the jack out of $s eye!");
      }
    } else {
      // Trode net.
      snprintf(buf, sizeof(buf), "$n suddenly jerks forward and rips the 'trode net off of $s head!");
    }
    act(buf, FALSE, icon->decker->ch, NULL, NULL, TO_ROOM);
    icon->decker->PERSONA = NULL;
    PLR_FLAGS(icon->decker->ch).RemoveBit(PLR_MATRIX);
    if (icon->decker->deck && GET_OBJ_VAL(icon->decker->deck, 0) == 0) {
      act("Smoke emerges from $n's $p.", FALSE, icon->decker->ch, icon->decker->deck, NULL, TO_ROOM);
      act("Smoke emerges from $p.", FALSE, icon->decker->ch, icon->decker->deck, NULL, TO_CHAR);
    }
    struct char_data *ch = icon->decker->ch;
    extract_icon(icon);
    PLR_FLAGS(ch).RemoveBit(PLR_MATRIX);
    SendGMCPMatrixInfo(ch);
    SendGMCPMatrixDeck(ch);

    // No reason to double-damage
    if (!PLR_FLAGGED(ch, PLR_MATRIX))
      return FALSE;
    // If they're stunned or dead, there's no reason to take dumpshock damage.
    if (GET_POS(ch) <= POS_STUNNED)
      return FALSE; 
    if (damage(ch, ch, dam, TYPE_DUMPSHOCK, MENTAL))
      return TRUE;
  } else {
    extract_icon(icon);
  }
  return FALSE;
}

int get_detection_factor(struct char_data *ch)
{
  int detect = 0;
  for (struct matrix_file *soft = DECKER->software; soft; soft = soft->next_file)
    if (soft->program_type == SOFT_SLEAZE)
      detect = soft->rating;
  detect += DECKER->masking + 1; // +1 because we round up
  detect = detect / 2;
  detect -= DECKER->res_det;
  if  (PERSONA->type == ICON_LIVING_PERSONA) {
    detect += 1 + GET_ECHO(ch, ECHO_GHOSTING); // Otaku always get +1 DF
  }
  return detect;
}

int system_test(rnum_t host, struct char_data *ch, int type, int software, int modifier)
{
  int detect = 0;
  struct matrix_file *prog = NULL;

  char rollbuf[5000];

  // You can only successfully run Decrypt and Analyze against encrypted subsystems.
  if (HOST.stats[type][MTX_STAT_ENCRYPTED] && software != SOFT_DECRYPT && software != SOFT_ANALYZE) {
    snprintf(rollbuf, sizeof(rollbuf), "Can't perform test against %s with software %s-- subsystem encrypted.", acifs_strings[type], programs[software].name);
    act(rollbuf, FALSE, ch, 0, 0, TO_ROLLS);
    send_to_char(ch, "The %s subsystem seems to be encrypted.\r\n", mtx_subsystem_names[type]);
    return 0;
  }

  int target = HOST.stats[type][MTX_STAT_RATING];
  if (PERSONA->type == ICON_LIVING_PERSONA) {
    // We lower the TN by the channel rating
    int channel_rating = GET_OTAKU_PATH(ch) == OTAKU_PATH_TECHNOSHAM ? 1 : 0;
    switch(type) {
      case ACIFS_ACCESS:
        channel_rating += GET_SKILL(ch, SKILL_CHANNEL_ACCESS);
        break;
      case ACIFS_CONTROL:
        channel_rating += GET_SKILL(ch, SKILL_CHANNEL_CONTROL);
        break;
      case ACIFS_FILES:
        channel_rating += GET_SKILL(ch, SKILL_CHANNEL_FILES);
        break;
      case ACIFS_INDEX:
        channel_rating += GET_SKILL(ch, SKILL_CHANNEL_INDEX);
        break;
      case ACIFS_SLAVE:
        channel_rating += GET_SKILL(ch, SKILL_CHANNEL_SLAVE);
        break;
    }
    target -= channel_rating;
  } else {
    // ... or by the appropriate decker utility program
    for (struct matrix_file *soft = DECKER->software; soft; soft = soft->next_file) {
      if (soft->program_type == software) {
        target -= soft->rating;
        buf_mod(rollbuf, sizeof(rollbuf), "soft", -soft->rating);
        prog = soft;  // for tarbaby/tarpit
        break;
      }
    }
  }
  snprintf(rollbuf, sizeof(rollbuf), "System test against %s with software %s: Starting TN %d", acifs_strings[type], programs[software].name, target);

  int skill = get_skill(ch, SKILL_COMPUTER, target) + MIN(GET_MAX_HACKING(ch), GET_REM_HACKING(ch));
  GET_REM_HACKING(ch) -= skill - get_skill(ch, SKILL_COMPUTER, detect);
  detect = 0;

  snprintf(ENDOF(rollbuf), sizeof(rollbuf) - strlen(rollbuf), ", after get_skill %d, plus called modifier %d is %d", target, modifier, target + modifier);

  if (modifier) {
    target += modifier;
    snprintf(ENDOF(rollbuf), sizeof(rollbuf) - strlen(rollbuf), ", plus called modifier %d is %d", modifier, target);
  }

  if (HOST.alert) {
    target += 2;
    strlcat(rollbuf, ", +2 for alert", sizeof(rollbuf));
  }
  strlcat(rollbuf, ". Modifiers: ", sizeof(rollbuf));
  target += modify_target_rbuf_raw(ch, rollbuf, sizeof(rollbuf), 8, FALSE) + DECKER->res_test + (DECKER->ras ? 0 : 4);
  detect = get_detection_factor(ch);

  int tally = MAX(0, success_test(HOST.security, detect));
  target = MAX(target, 2);
  int success = success_test(skill, target);

  snprintf(ENDOF(rollbuf), sizeof(rollbuf) - strlen(rollbuf), "\r\nRolled %d successes on %d dice vs TN %d",
           success,
           skill,
           target);

  success -= tally;

  snprintf(ENDOF(rollbuf), sizeof(rollbuf) - strlen(rollbuf), ", then lost %d successes to tally (%d dice vs TN %d).",
           tally,
           HOST.security,
           detect);
  act(rollbuf, FALSE, ch, 0, 0, TO_ROLLS);

  DECKER->tally += tally;
  if (DECKER->located)
    DECKER->tally++;

  struct matrix_icon *temp;
  for (struct matrix_icon *ic = HOST.icons; ic; ic = temp)
  {
    temp = ic->next_in_host;
    target = ic->ic.rating;
    if (HOST.shutdown)
      target -= 2;
    if (ICON_IS_IC(ic) && ic->ic.target == PERSONA->idnum) {
      if (ic->ic.type == IC_PROBE || ic->ic.type == IC_SCOUT)
        tally += MAX(0, success_test(target, detect));
      else if (prog && (ic->ic.type == IC_TARBABY || ic->ic.type == IC_TARPIT) && ic->ic.subtype == 0) {
        // Here's hoping this didn't extract the program they used...
        if (tarbaby(prog, ch, ic))
          return success;
      }
    }
  }
  
  if (DECKER->tally >= 90)
  {
    // House rule: we don't shut down the host per Matrix pg112,
    // Instead we just kill the problematic connection.
    send_to_icon(PERSONA, "The sirens and lights seem to turn towards you!\r\n");
    dumpshock(PERSONA);
    send_to_char(ch, "^y(OOC note: Your security tally hit 80+, so the host disconnected you.)^n\r\n");
    return -1;
  }
  check_trigger(PERSONA->in_host, ch);
  return success;
}
#undef HOST

bool has_spotted(struct matrix_icon *icon, struct matrix_icon *targ)
{
  if (targ->evasion || targ == icon)
    return FALSE;
  for (struct seen_data *seen = icon->decker->seen; seen; seen = seen->next)
    if (seen->idnum == targ->idnum || targ->decker) // You auto-see deckers now.
      return TRUE;
  return FALSE;
}

// Assume that the target character has been deleted from memory if this returns TRUE.
bool do_damage_persona(struct matrix_icon *targ, int dmg)
{
  if (targ->type == ICON_LIVING_PERSONA) {
    // It's an otaku! They get to suffer MENTAL DAMAGE!
    // targ->condition seems to be 1-10 scale, while ch mental wounds seems to be 1-100. Multiply by ten.

    // damage() returns TRUE if the target has been deleted from memory, so when this function returns true, we must bail out of everything immediately.
    if (damage(targ->decker->ch, targ->decker->ch, dmg, TYPE_BLACKIC, MENTAL))
      return TRUE;

    return FALSE;
  }
  targ->condition -= dmg;
  return FALSE;
}

void fry_mpcp(struct matrix_icon *icon, struct matrix_icon *targ, int success)
{
  if (success < 2) return;
  if (!targ->decker->deck) return;

  if (targ->decker->deck->obj_flags.extra_flags.IsSet(ITEM_EXTRA_OTAKU_RESONANCE)) {
    // This is an otaku persona! MPCP damage works slightly different on otaku.
    GET_CYBERDECK_MPCP(targ->decker->deck)--; // Damage the virtual deck.
    
    // MPCP damage always occurs to decker physical
    while (success >= 2 && GET_PHYSICAL(targ->decker->ch) > 0) {
      success -= 2;
      damage(targ->decker->ch, targ->decker->ch, 1, TYPE_BLACKIC, PHYSICAL);
    }
    return;
  }
  
  if (targ->decker->ch && PLR_FLAGGED(targ->decker->ch, PLR_NEWBIE)) {
    send_to_icon(targ, "(OOC message: Be careful with these enemies; your deck would have taken permanent damage if you weren't a newbie!)");
    return;
  }
  snprintf(buf, sizeof(buf), "%s^n uses the opportunity to fry your MPCP!\r\n", CAP(icon->name));
  send_to_icon(targ, buf);
  while (success >= 2 && targ->decker->mpcp > 0) {
    success -= 2;
    targ->decker->mpcp--;
    GET_CYBERDECK_MPCP(targ->decker->deck)--;
  }
  // Damage the MPCP chip, if any.
  for (struct obj_data *part = targ->decker->deck->contains; part; part = part->next_content) {
    if (GET_OBJ_TYPE(part) == ITEM_PART && GET_PART_TYPE(part) == PART_MPCP) {
      GET_PART_RATING(part) = GET_CYBERDECK_MPCP(targ->decker->deck);
      GET_PART_TARGET_MPCP(part) = GET_CYBERDECK_MPCP(targ->decker->deck);
      break;
    }
  }  
}

ACMD(do_fry_self) {
  fry_mpcp(PERSONA, PERSONA, 2);
}

void cascade(struct matrix_icon *icon)
{
  switch (matrix[icon->in_host].color)
  {
  case 0:
    icon->ic.cascade = 1;
    break;
  case 1:
    if (icon->ic.cascade < (int)(icon->ic.rating * .25) || icon->ic.cascade < 2)
      icon->ic.cascade++;
    break;
  case 2:
    if (icon->ic.cascade < (int)(icon->ic.rating * .5) || icon->ic.cascade < 3)
      icon->ic.cascade++;
    break;
  case 3:
  case 4:
    if (icon->ic.cascade < icon->ic.rating || icon->ic.cascade < 4)
      icon->ic.cascade++;
    break;
  }
}

int maneuver_test(struct matrix_icon *icon)
{
  int icon_target = 0, icon_skill, targ_target = 0, targ_skill;
  if (icon->decker)
  {
    for (struct matrix_file *soft = icon->decker->software; soft; soft = soft->next_file)
      if (soft->program_type == SOFT_CLOAK)
        icon_target -= soft->rating;
    targ_target += icon_skill = icon->decker->evasion;
  } else
    targ_target += icon_skill = matrix[icon->in_host].security;
  if (icon->fighting->decker)
  {
    for (struct matrix_file *soft = icon->fighting->decker->software; soft; soft = soft->next_file)
      if (soft->program_type == SOFT_LOCKON)
        targ_target -= soft->rating;
    icon_target += targ_skill = icon->fighting->decker->sensor;
  } else
    icon_target += targ_skill = matrix[icon->in_host].security;
  return resisted_test(icon_skill, icon_target, targ_skill, targ_target);
}

void evade_detection(struct matrix_icon *icon)
{
  if (icon->fighting)
  {
    int success = maneuver_test(icon);
    if (success > 0) {
      send_to_icon(icon, "You maneuver away from %s^n.\r\n", decapitalize_a_an(icon->fighting->name));
      icon->evasion = success;
      icon->parry = 0;
      icon->fighting->parry = 0;
      if (icon->fighting->decker) {
        send_to_icon(icon->fighting, "%s^n vanishes from your sight.\r\n", CAP(icon->name));
      } else {
        icon->fighting->ic.targ_evasion = success;
      }
    } else
      send_to_icon(icon, "You fail to evade %s^n.\r\n", decapitalize_a_an(icon->fighting->name));
    return;
  }
  send_to_icon(icon, "But you're not fighting anyone.\r\n");
}

ACMD(do_evade)
{
  evade_detection(PERSONA);
  WAIT_STATE(ch, (int) (DECKING_WAIT_STATE_TIME));
}

void parry_attack(struct matrix_icon *icon)
{
  if (icon->fighting)
  {
    int success = maneuver_test(icon);
    if (success > 0) {
      icon->parry = success;
      send_to_icon(icon, "You manage to enhance your defenses.\r\n");
    } else
      send_to_icon(icon, "You fail to enhance your defenses.\r\n");
  } else
    send_to_icon(icon, "But you're not fighting anyone.\r\n");
}

ACMD(do_parry)
{
  parry_attack(PERSONA);
  WAIT_STATE(ch, (int) (DECKING_WAIT_STATE_TIME));
}

void position_attack(struct matrix_icon *icon)
{
  if (icon->fighting)
  {
    int success = maneuver_test(icon);
    if (success > 0) {
      send_to_icon(icon, "You maneuver yourself to attack!\r\n");
      if (!icon->evasion)
        send_to_icon(icon->fighting, "%s^n maneuvers themselves into a better position!\r\n", CAP(icon->name));
      icon->position = success;
      icon->fighting->position = 0;
    } else if (success < 0) {
      send_to_icon(icon, "You manage to put yourself in a worse position than before!\r\n");
      if (!icon->evasion)
        send_to_icon(icon->fighting, "%s^n tries to maneuver into a better position but ends up right in your sights!\r\n", CAP(icon->name));
      icon->position = 0;
      icon->fighting->position = -success;
    } else
      send_to_icon(icon, "You fail to put yourself in a better position.\r\n");
  } else
    send_to_icon(icon, "But you're not fighting anyone.\r\n");
}

ACMD(do_matrix_position)
{
  position_attack(PERSONA);
  WAIT_STATE(ch, (int) (DECKING_WAIT_STATE_TIME));
}

bool try_execute_shield_program(struct matrix_icon *icon, struct matrix_icon *targ, int &success)
{
  struct matrix_file *soft = NULL, *temp = NULL;
  if (!targ || !targ->decker) return FALSE; // IC don't have shields

  for (soft = targ->decker->software; soft; soft = soft->next_file) {
    if (soft->program_type == SOFT_SHIELD) {
      int shield_test = success_test(soft->rating, 
        ICON_IS_IC(icon) ? matrix[icon->in_host].security : GET_SKILL(icon->decker->ch, SKILL_COMPUTER));

      if (shield_test > 0) {
        success -= shield_test;
        send_to_icon(targ, "You raise your shield program and deflect some of the attack.\r\n");
      }
      soft->rating--;
      if (soft->rating <= 0) {
        send_to_icon(targ, "Your shield program crashes as the rating is depleted.\r\n");
        REMOVE_FROM_LIST(soft, targ->decker->software, next_file);
        extract_matrix_file(soft);
      }
      return TRUE;
    }
  }
  return FALSE;
}

void matrix_fight(struct matrix_icon *icon, struct matrix_icon *targ)
{
  struct matrix_file *soft = NULL;
  int target = 0, skill, bod = 0, dam = 0, icondam = 0, power, success;
  int iconrating = icon->ic.rating;
  if (!targ)
    return;
  if (!targ->fighting && !(ICON_IS_IC(targ) && (!IS_PROACTIVE(targ) || targ->ic.type == IC_SCOUT)))
  {
    targ->fighting = icon;
    targ->next_fighting = matrix[targ->in_host].fighting;
    matrix[targ->in_host].fighting = targ;
    roll_matrix_init(targ);
    targ->initiative -= matrix[targ->in_host].pass * 10;
    order_list(matrix[targ->in_host].fighting);
  }
  if (targ->decker && !has_spotted(targ, icon))
    make_seen(targ, icon->idnum);

#ifdef MTX_DEBUG
  if (icon->decker)
    send_to_char(icon->decker->ch, "entering matrix_fight(%s, %s)\r\n", GET_CHAR_NAME(icon->decker->ch), targ->name);
#endif

  // Determine attack TNs
  switch (matrix[icon->in_host].color)
  {
  case HOST_COLOR_BLUE:
    if (targ->decker)
      target = 6;
    else
      target = 3;
    break;
  case HOST_COLOR_GREEN:
    if (targ->decker)
      target = 5;
    else
      target = 4;
    break;
  case HOST_COLOR_ORANGE:
    if (targ->decker)
      target = 4;
    else
      target = 5;
    break;
  case HOST_COLOR_RED:
    if (targ->decker)
      target = 3;
    else
      target = 6;
    break;
  case HOST_COLOR_BLACK:
    if (targ->decker)
      target = 2;
    else
      target = 7;
    break;
  }
  target += targ->parry;
  target -= icon->position;
  targ->parry = 0;
  icon->position = 0;

  // Determine skill dice, damage level, and power
  if (icon->decker)
  {
    for (soft = icon->decker->software; soft; soft = soft->next_file)
      if (soft->program_type == SOFT_ATTACK)
        break;
    if (!soft) {
#ifdef MTX_DEBUG
  if (icon->decker)
    send_to_icon(icon, "matrix_fight(): failed to find attack soft, bailing out\r\n");
#endif
      return;
    }
    skill = soft->rating + MIN(GET_MAX_HACKING(icon->decker->ch), GET_REM_HACKING(icon->decker->ch));
    GET_REM_HACKING(icon->decker->ch) -= skill - soft->rating;
    dam = soft->wound_category;
    power = soft->rating;
  } else
  {
    skill = matrix[icon->in_host].security;
    if (icon->ic.options.IsSet(IC_EX_OFFENSE))
      skill += icon->ic.expert;
    else if (icon->ic.options.IsSet(IC_EX_DEFENSE))
      skill -= icon->ic.expert;
    skill += icon->ic.cascade;
    if (targ->decker && targ->decker->scout) {
      skill += targ->decker->scout;
      targ->decker->scout = 0;
    }
    power = iconrating + (matrix[icon->in_host].color == HOST_COLOR_BLACK ? 2 : 0);
    dam = ic_dmg_by_color[matrix[icon->in_host].color];
  }

  // Rules for specific IC/decker combinations
  if (targ->decker)
  {
    // Crippler/ripper IC vs decker
    if (ICON_IS_IC(icon) && (icon->ic.type == IC_CRIPPLER || icon->ic.type == IC_RIPPER)) {
      if (!targ->decker->deck)
        return;
      extern const char *crippler[4];
      if (targ->type == ICON_LIVING_PERSONA) 
        send_to_icon(targ, "%s^n takes a shot at your living persona's %s^n attribute!\r\n", CAP(icon->name), crippler[icon->ic.subtype]);
      else
        send_to_icon(targ, "%s^n takes a shot at your %s^n program!\r\n", CAP(icon->name), crippler[icon->ic.subtype]);
      switch (icon->ic.subtype) {
      case 0:
        bod = targ->decker->bod;
        break;
      case 1:
        bod = targ->decker->evasion;
        break;
      case 2:
        bod = targ->decker->sensor;
        break;
      case 3:
        bod = targ->decker->masking;
        break;
      }
      success = success_test(matrix[targ->in_host].security, bod);
      if (try_execute_shield_program(icon, targ, success) && success <= 0) {
        return;
      }
      int resist = success_test(bod + MIN(GET_MAX_HACKING(targ->decker->ch), GET_REM_HACKING(targ->decker->ch)), iconrating);
      GET_REM_HACKING(targ->decker->ch) = MAX(0, GET_REM_HACKING(targ->decker->ch) - GET_MAX_HACKING(targ->decker->ch));
      success -= resist;

      // Results of crippler/ripper IC vs decker
      if (targ->type == ICON_LIVING_PERSONA) {
        if (success <= 0) {
          return send_to_icon(targ, "It fails to damage your living persona.\r\n");
        }
        // Houserule: handle like Killer IC instead of Bioware Stress
        // Hacking pool was already used to resist with the targeted attribute, so only bod here
        success -= success_test(targ->decker->bod, power);
        if (icondam <= 0) {
          return send_to_icon(targ, "You power through it and nullify the damage.\r\n");
        } else {
          icondam = convert_damage(stage(success, dam));
          send_to_icon(targ, "It tears into your living persona!\r\n");
          if (do_damage_persona(targ, icondam))
            return;
        }
        return;
      } else if (success >= 2) {
        send_to_icon(targ, "It tears into the program!\r\n");
        bod -= success / 2;
        if (icon->ic.type == IC_CRIPPLER) {
          bod = MAX(bod, 1);
        } else if (bod <= 0) {
          bod = 0;
          success = success_test(iconrating, targ->decker->mpcp + targ->decker->hardening);
          fry_mpcp(icon, targ, success);
        }
      } else {
        send_to_icon(targ, "It failed to cause any damage.\r\n");
        return;
      }

      // Only damage attributes to non-otaku personae
      if (targ->type != ICON_LIVING_PERSONA) {
        // Damage was done to the attribute, so reduce the attribute
        switch (icon->ic.subtype) {
          case 0:
            targ->decker->bod = bod;
            break;
          case 1:
            targ->decker->evasion = bod;
            break;
          case 2:
            targ->decker->sensor = bod;
            break;
          case 3:
            targ->decker->masking = bod;
            break;
        }
        if (targ->decker->mpcp == 0)
          dumpshock(targ);
      }

      // Do not proceed further; ripper do not do attacks beyond this
      return;
    }

    // Non-crippler/ripper IC vs decker
    send_to_icon(targ, "%s^n runs an attack program against you.\r\n", CAP(icon->name));
    if (icon->ic.type >= IC_LETHAL_BLACK)
      power -= targ->decker->hardening;
    else
      for (soft = targ->decker->software; soft; soft = soft->next_file)
        if (soft->program_type == SOFT_ARMOR) {
          power -= soft->rating;
          break;
        }

    if (!targ->decker->ras)
      power += 4;

    if (icon->ic.type == IC_TRACE)
      target += targ->decker->redirect;

    // NOTE: results are below (look for "Attack results" comment)

  } else
  { // Decker vs IC
    bod = matrix[targ->in_host].security;
    if (targ->ic.options.IsSet(IC_EX_DEFENSE))
      bod += targ->ic.expert;
    else if (targ->ic.options.IsSet(IC_EX_OFFENSE))
      bod -= targ->ic.expert;
    if (targ->ic.options.IsSet(IC_SHIELD))
      target += 2;
    else if (targ->ic.options.IsSet(IC_SHIFT))
      target += 2;
    if (targ->ic.options.IsSet(IC_ARMOR))
      power -= 2;
    if (!icon->decker->ras)
      target += 4;
  }

  // Attack!
  success = success_test(skill, target);
  if (success <= 0)
  {
    send_to_icon(icon, "Your program fails to run.\r\n");
    send_to_icon(targ, "%s^n's program fails to run.\r\n", CAP(icon->name));
    if (!icon->decker && icon->ic.options.IsSet(IC_CASCADE))
      cascade(icon);
    return;
  }

  // Block!
  if (try_execute_shield_program(icon, targ, success) && success <= 0) {
#ifdef MTX_DEBUG
    if (icon->decker)
      send_to_icon(icon, "matrix_fight(): shield program failed, bailing out\r\n");
#endif
    return;
  }

  // Results of non-damaging attacks
  if (ICON_IS_IC(icon)) {
    if (icon->ic.type == IC_SCOUT)
    {
      if (success && targ->decker->scout < iconrating) {
        send_to_icon(targ, "%s^n locates your memory address.\r\n", CAP(icon->name));
        targ->decker->scout += success;
        if (targ->decker->scout > iconrating)
          targ->decker->scout = iconrating;
      }
      return;
    } else if (icon->ic.type == IC_TRACE)
    {
      success -= success_test(targ->decker->evasion, iconrating);
      if (success > 0) {
        icon->ic.subtype = 10;
        for (struct matrix_file *soft = targ->decker->software; soft; soft = soft->next_file)
          if (soft->program_type == SOFT_CAMO) {
            icon->ic.subtype += soft->rating;
            break;
          }
        icon->ic.subtype += targ->decker->ch->in_room->trace;
        icon->ic.subtype /= success;
        send_to_icon(targ, "%s^n locks onto your datatrail and vanishes from sight.\r\n", CAP(icon->name));
        {
          struct matrix_icon *temp;
          icon->fighting = NULL;
          REMOVE_FROM_LIST(icon, matrix[icon->in_host].fighting, next_fighting);
          if (targ->fighting == icon) {
            targ->fighting = NULL;
            REMOVE_FROM_LIST(targ, matrix[icon->in_host].fighting, next_fighting);
          }
        }
        for (struct seen_data *seen = targ->decker->seen; seen; seen = seen->next)
          if (seen->idnum == icon->idnum) {
            struct seen_data *temp;
            REMOVE_FROM_LIST(seen, targ->decker->seen, next);
            break;
          }
      } else
        send_to_icon(targ, "You manage to avoid %s^n's attack.\r\n", decapitalize_a_an(icon->name));
      return;
    }
  } // else { implementations of slow, etc, would go here }

  if (targ->type == ICON_LIVING_PERSONA && ICON_IS_IC(icon) && icon->ic.type >= IC_LETHAL_BLACK) {
    // For black IC we'll be be doing our ICCM checks up front
    // since we bypass doing a (separate) damage pass from the condition monitor of the persona
    // and instead directly attack the otaku
    int resist = 0;
    if (icon->ic.type == IC_LETHAL_BLACK)
      resist = GET_BOD(targ->decker->ch);
    else
      resist = GET_WIL(targ->decker->ch);

    int wil_test_result = success_test(GET_WIL(targ->decker->ch), power);
    int bod_test_result = success_test(GET_BOD(targ->decker->ch), power);
    success -= targ->decker->iccm ? MAX(wil_test_result, bod_test_result) : success_test(resist, power);
  } else if (targ->decker && targ->decker->ch) {
    bod = targ->decker->bod + MIN(GET_MAX_HACKING(targ->decker->ch), GET_REM_HACKING(targ->decker->ch));
    GET_REM_HACKING(targ->decker->ch) = MAX(0, GET_REM_HACKING(targ->decker->ch) - GET_MAX_HACKING(targ->decker->ch));
    success -= success_test(bod, power);
  } else {
    success -= success_test(bod, power);
  }

  // Results of damaging attacks  
  icondam = convert_damage(stage(success, dam));
  if (icondam <= 0) {
    send_to_icon(targ, "%s^n's attack reflects off you harmlessly!\r\n", CAP(icon->name));
    send_to_icon(icon, "%s^n manages to block your attack.\r\n", CAP(targ->name));
    if (!icon->decker && icon->ic.options.IsSet(IC_CASCADE))
      cascade(icon);
  } else if (icondam <= 1) {
    send_to_icon(targ, "%s^n's attack skims off you.\r\n", CAP(icon->name));
    send_to_icon(icon, "Your attack skims off of %s^n.\r\n", decapitalize_a_an(targ->name));
  } else if (icondam <= 3) {
    send_to_icon(targ, "%s^n's attack sends parts of you flying.\r\n", CAP(icon->name));
    send_to_icon(icon, "Parts fly off of %s^n from your attack.\r\n", decapitalize_a_an(targ->name));
  } else if (icondam <= 6) {
    send_to_icon(targ, "%s^n's attack leaves you reeling.\r\n", CAP(icon->name));
    send_to_icon(icon, "Your attack leaves %s^n reeling.\r\n", decapitalize_a_an(targ->name));
  } else {
    send_to_icon(targ, "%s^n's attack completely obliterates you!\r\n", CAP(icon->name));
    send_to_icon(icon, "You obliterate %s^n.\r\n", decapitalize_a_an(targ->name));
  }

  struct char_data *ch = targ->decker ? targ->decker->ch : NULL;
  if (do_damage_persona(targ, icondam) || (ch && GET_POS(ch) <= POS_STUNNED)) {
    // If do_damage_persona returns true then the icon condition monitor is overloaded,
    // or it's an otaku that has fainted/died from brain bleeding.
    // ~~If it's the latter we check if they're uncon/dead, and then return early.~~
    // We can't tell which, and damage() returning true means the character was extracted, so we assume the targ has been extracted and bail.
    return;
  }

  if (icondam > 0 && ch)
  {
    // Black IC deal damage to the decker as well as their icon
    // Living persona don't take a second round of black IC damage, it was already done above
    if (ICON_IS_IC(icon) && icon->ic.type >= IC_LETHAL_BLACK && targ->type != ICON_LIVING_PERSONA) {
      int resist = 0;
      bool lethal = icon->ic.type == IC_LETHAL_BLACK ? TRUE : FALSE;
      if (!targ->decker->asist[0] && lethal)
        lethal = FALSE;
      if (lethal)
        resist = GET_BOD(ch);
      else
        resist = GET_WIL(ch);

      int wil_test_result = success_test(GET_WIL(ch), power);
      int bod_test_result = success_test(GET_BOD(ch), power);
      success -= targ->decker->iccm ? MAX(wil_test_result, bod_test_result) : success_test(resist, power);
      int meatdam = convert_damage(stage(success, dam));
      send_to_icon(targ, "You smell something burning.\r\n");

      if (damage(ch, ch, meatdam, TYPE_BLACKIC, lethal ? PHYSICAL : MENTAL)) {
        // Oh shit, they died. Guess they don't take MPCP damage, since their struct is zeroed out now.
        return;
      }
      // If they're not still connected to the matrix, bail.
      if (!ch->persona) {
        return;
      }
      if (targ && targ->decker) {
        if (ch && !AWAKE(ch)) {
          success = success_test(iconrating * 2, targ->decker->mpcp + targ->decker->hardening);
          fry_mpcp(icon, targ, success);
          dumpshock(targ);
          return;
        } else if (!ch) {
          success = success_test(iconrating * 2, targ->decker->mpcp + targ->decker->hardening);
          fry_mpcp(icon, targ, success);
          extract_icon(targ);
          return;
        }
      }
    } else {
      // Simsense overload, SR3 pg 226
      switch(dam) {
      case 1:
        power = 2;
        break;
      case 3:
        power = 3;
        break;
      case 6:
        power = 5;
        break;
      }
      if (success_test(GET_WIL(ch), power) < 1) {
        send_to_icon(targ, "Your interface overloads.\r\n");
        if (damage(ch, ch, 1, TYPE_TASER, MENTAL)) {
          return;
        }
      }
    }
  }

  // Process icon destruction
  if (targ->condition < 1)
  {
    if (targ->decker) {
      if (ICON_IS_IC(icon)) {
        switch (icon->ic.type) {
        case IC_SPARKY:
          send_to_icon(targ, "%s^n sends jolts of electricity into your deck!\r\n", CAP(icon->name));
          success = success_test(iconrating, targ->decker->mpcp + targ->decker->hardening + 2);
          fry_mpcp(icon, targ, success);
          success = success - success_test(GET_BOD(targ->decker->ch), iconrating - targ->decker->hardening);
          dam = convert_damage(stage(success, MODERATE));
          if (damage(targ->decker->ch, targ->decker->ch, dam, TYPE_BLACKIC, PHYSICAL)) {
            return;
          }
          break;
        case IC_BLASTER:
          success = success_test(iconrating, targ->decker->mpcp + targ->decker->hardening);
          fry_mpcp(icon, targ, success);
          break;
        }
      }
      if (dumpshock(targ))
        return;
    } else {
      icon->decker->tally += iconrating;
      if (icon->decker->located)
        icon->decker->tally++;
      snprintf(buf, sizeof(buf), "%s^n shatters into a million pieces and vanishes from the node.\r\n", CAP(targ->name));
      send_to_host(icon->in_host, buf, targ, TRUE);
      gain_matrix_karma(icon, targ);
      if (targ->ic.options.IsSet(IC_TRAP) && real_ic(targ->ic.trap) > 0) {
        struct matrix_icon *trap = read_ic(targ->ic.trap, VIRTUAL);
        trap->ic.target = icon->idnum;
        icon_to_host(trap, icon->in_host);
      }

      if (ICON_IS_IC(targ) && matrix[icon->in_host].ic_bound_paydata > 0) {
        if (!(number(0, MAX(0, matrix[icon->in_host].color - HOST_COLOR_ORANGE)))) {
          matrix[icon->in_host].ic_bound_paydata--;
          struct matrix_file *paydata = spawn_paydata(icon);
          send_to_icon(icon, "A mote labeled '%s^n' drifts away from your kill.\r\n", paydata->name);
        } else  {
          send_to_icon(icon, "The paydata you thought your kill was guarding turns out to have been junk.\r\n");
        }
      }

      extract_icon(targ);
      check_trigger(icon->in_host, icon->decker->ch);
      return;
    }
  }
}

// Award karma to icon on destruction of targ.
void gain_matrix_karma(struct matrix_icon *icon, struct matrix_icon *targ) {
  if (!icon || !targ || !icon->in_host) {
    mudlog("SYSERR: Received null icon or target to gain_matrix_karma().", NULL, LOG_SYSLOG, TRUE);
    return;
  }

  // We only hand out karma to PCs for killing NPCs.
  if (!icon->decker || !icon->decker->ch) {
    mudlog("SYSERR: Received icon without decker in gain_matrix_karma.", NULL, LOG_SYSLOG, TRUE);
    return;
  }

  if (targ->decker) {
    mudlog("SYSERR: Receievd targ with decker in gain_matrix_karma.", NULL, LOG_SYSLOG, TRUE);
    return;
  }

  int ic_stats_total = 0;
  ic_stats_total += targ->ic.rating * 2;
  ic_stats_total += targ->ic.cascade;

  switch (targ->ic.type) {
    case IC_CRIPPLER:
      ic_stats_total += 2;
      break;
    case IC_KILLER:
    case IC_PROBE:
    case IC_SCRAMBLE:
      // These are NERP, no bonus.
      break;
    case IC_TARBABY:
      ic_stats_total += 2;
      break;
    case IC_SCOUT:
      ic_stats_total += 1;
      break;
    case IC_TRACE:
      ic_stats_total += 1;
      break;
    case IC_BLASTER:
      ic_stats_total += 10;
      break;
    case IC_RIPPER:
      ic_stats_total += 8;
      break;
    case IC_SPARKY:
      ic_stats_total += 9;
      break;
    case IC_TARPIT:
      ic_stats_total += 5;
      break;
    case IC_LETHAL_BLACK:
      ic_stats_total += 10;
      // fallthrough
    case IC_NON_LETHAL_BLACK:
      // Blue and Green hosts deal Moderate damage.
      ic_stats_total += 10;

      // Orange and up deal Serious damage.
      if (matrix[icon->in_host].color >= HOST_COLOR_ORANGE)
        ic_stats_total += 2;

      // Black hosts additionally deal +2 power.
      if (matrix[icon->in_host].color == HOST_COLOR_BLACK)
        ic_stats_total += 2;
      break;
    default:
      snprintf(buf3, sizeof(buf3), "SYSERR: Unrecognized IC type %d in gain_matrix_karma().", targ->ic.type);
      mudlog(buf3, NULL, LOG_SYSLOG, TRUE);
      break;
  }

  if (targ->ic.options.IsSet(IC_EX_DEFENSE) || targ->ic.options.IsSet(IC_EX_OFFENSE)) {
    ic_stats_total += targ->ic.expert;
  }

  if (targ->ic.options.IsSet(IC_SHIELD))
    ic_stats_total += 2;
  else if (targ->ic.options.IsSet(IC_SHIFT))
    ic_stats_total += 2;

  if (targ->ic.options.IsSet(IC_ARMOR))
    ic_stats_total += 2;

  if (icon->ic.options.IsSet(IC_CASCADE))
    ic_stats_total += 2;


  // Knock it down by their notor. This does mean that deckers have a low ceiling, but they need to branch out.
  ic_stats_total -= (int)(GET_NOT(icon->decker->ch) / 4);
  ic_stats_total /= 2;

  // Randomize it a bit.
  ic_stats_total += ((!number(0,2) ? number(0,3) : 0 - number(0,3)));

  // Cap it at top and bottom.
  ic_stats_total = MAX(MIN(max_exp_gain, ic_stats_total), 1);

  // Suppress rewards for unlinked zones.
  if (vnum_from_non_approved_zone(targ->vnum) && !IS_SENATOR(icon->decker->ch)) {
#ifndef IS_BUILDPORT
    mudlog_vfprintf(icon->decker->ch, LOG_SYSLOG, "BUILD ERROR: %s encountered IC '%s' (%ld) from a non-connected zone!",
                    GET_CHAR_NAME(icon->decker->ch),
                    targ->name,
                    targ->vnum);
#endif
    ic_stats_total = 0;
  }

  int karma_gained = gain_karma(icon->decker->ch, ic_stats_total, FALSE, TRUE, TRUE);

  send_to_icon(icon, "You gain %0.2f karma.\r\n", ((float) karma_gained / 100));

  snprintf(buf3, sizeof(buf3), "Matrix karma gain: %0.2f.", ((float) karma_gained / 100));
  act(buf3, FALSE, icon->decker->ch, 0, 0, TO_ROLLS);
}

const char *get_plaintext_matrix_score_health(struct char_data *ch) {
  if (ch->persona->type == ICON_LIVING_PERSONA)
    snprintf(buf2, sizeof(buf2), "Your Mental Condition: %d / %d\r\n", (int)(GET_MENTAL(ch) / 100), (int)(GET_MAX_MENTAL(ch) / 100));
  else
    snprintf(buf2, sizeof(buf2), "Persona Condition: %d\r\n", PERSONA_CONDITION);
  snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Your Physical Condition: %d / %d\r\n", (int)(GET_PHYSICAL(ch) / 100), (int)(GET_MAX_PHYSICAL(ch) / 100));
  return buf2;
}

const char *get_plaintext_matrix_score_stats(struct char_data *ch, int detect) {
  snprintf(buf2, sizeof(buf2), "TN to detect you: %d\r\n", detect);
  snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Hacking Pool: %d / %d\r\n", MAX(0, GET_REM_HACKING(ch)), GET_HACKING(ch));
  snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Max %d hacking dice usable per action.\r\n", GET_MAX_HACKING(ch));
  snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Persona Programs:\r\nBod: %d\r\nEvasion: %d\r\nMasking: %d\r\nSensors: %d\r\n",
          DECKER->bod, DECKER->evasion, DECKER->masking, DECKER->sensor);
  if (ch->persona->type == ICON_LIVING_PERSONA) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "Your Submersion Echoes: ");
    int echoes_found = 0;
    for (int ci=ECHO_UNDEFINED + 1; ci <= ECHO_MAX;ci++) {
      if (!GET_ECHO(ch, ci)) continue;
      echoes_found++;
      if (echoes[ci].incremental)
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s%s (%d)",
          echoes_found > 0 ? ", " : "",
          echoes[ci].name,
          GET_ECHO(ch, ci));
      else
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s%s",
          echoes_found > 0 ? ", " : "",
          echoes[ci].name);
    }
    if (echoes_found > 0) snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\n");
    else snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " None\r\n");
  }
  return buf2;
}

const char *get_plaintext_matrix_score_deck(struct char_data *ch) {
  snprintf(buf2, sizeof(buf2), "Deck Status:\r\nHardening: %d\r\nMPCP: %d\r\nI/O Speed: %d\r\nResponse Increase: %d\r\n",
          DECKER->hardening, DECKER->mpcp, DECKER->deck ? GET_OBJ_VAL(DECKER->deck, 4) : 0, DECKER->response);
  return buf2;
}

const char *get_plaintext_matrix_score_memory(struct char_data *ch) {
  if (ch->persona->type == ICON_LIVING_PERSONA) return buf2;

  snprintf(buf2, sizeof(buf2), "Active Memory: %d free of %d total\r\n", DECKER->active, GET_CYBERDECK_ACTIVE_MEMORY(DECKER->deck));
  snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "Storage Memory: %d free of %d total\r\n",
          GET_CYBERDECK_FREE_STORAGE(DECKER->deck), GET_CYBERDECK_TOTAL_STORAGE(DECKER->deck));
  return buf2;
}

ACMD(do_programs)
{
  if (!DECKER->software) {
    send_to_icon(PERSONA, "You check your active memory and don't see anything loaded.\r\n\r\n");
    get_plaintext_matrix_score_memory(ch);
    return;
  }

  send_to_icon(PERSONA, "You pull up a console and check your active memory:\r\n");
  for (struct matrix_file *soft = DECKER->software; soft; soft = soft->next_file) {
    char paddingnumberstr[10], formatstr[512];

    snprintf(paddingnumberstr, sizeof(paddingnumberstr), "%d", 40 + count_color_codes_in_string(soft->name));
    snprintf(formatstr, sizeof(formatstr), "%s%s%s", "%-", paddingnumberstr, "s ^c%3d^nMp ");
    send_to_icon(PERSONA, formatstr,
                  soft->name,
                  soft->size);
    send_to_icon(PERSONA, "( R%-2d", soft->rating);
    if (soft->program_type == SOFT_ATTACK) {
      switch (soft->wound_category) {
        case 1:
          send_to_icon(PERSONA, ", ^c%s^n )\r\n", GET_WOUND_NAME(soft->wound_category));
          break;
        case 2:
          send_to_icon(PERSONA, ", ^g%s^n )\r\n", GET_WOUND_NAME(soft->wound_category));
          break;
        case 3:
          send_to_icon(PERSONA, ", ^y%s^n )\r\n", GET_WOUND_NAME(soft->wound_category));
          break;
        case 4:
          send_to_icon(PERSONA, ", ^r%s^n )\r\n", GET_WOUND_NAME(soft->wound_category));
          break;
      }
    } else {
      send_to_icon(PERSONA, " )\r\n");
    }
  }

  std::vector<obj_data *> devices = get_storage_devices(ch);
  for (obj_data *device: devices) {
    for (struct matrix_file *soft = device->files; soft; soft = soft->next_file) {
      if (!soft->transfer_remaining) continue;
      if (soft->transferring_to != DECKER->deck) continue;
      char paddingnumberstr[10], formatstr[512];

      snprintf(paddingnumberstr, sizeof(paddingnumberstr), "%d", 40 + count_color_codes_in_string(soft->name));
      snprintf(formatstr, sizeof(formatstr), "%s%s%s", "%-", paddingnumberstr, "s ^c%3d^nMp ");
      send_to_icon(PERSONA, formatstr,
                    soft->name,
                    soft->size);
      float completion_percentage = (float) (soft->size - soft->transfer_remaining) / MAX(1, soft->size) * 100;
      send_to_icon(PERSONA, "( R%-2d, ^GLoading^n ^c%2.2f%%^n complete )\r\n", soft->rating, completion_percentage);
    }
  }

  send_to_icon(PERSONA, "\r\n");
  get_plaintext_matrix_score_memory(ch);
}

ACMD(do_matrix_score)
{
  if (!PERSONA) {
    send_to_char(ch, "You can't do that while hitching.\r\n");
    return;
  }

  // Calculate detection TN (for others to notice this decker).
  int detect = get_detection_factor(ch);

  if (*argument) {
    skip_spaces(&argument);

    // Find the index of the command the player wants.
    if (!strncmp(argument, "health", strlen(argument))) {
      strlcpy(buf, get_plaintext_matrix_score_health(ch), sizeof(buf));
      send_to_icon(PERSONA, buf);
      return;
    }

    if (!strncmp(argument, "stats", strlen(argument))) {
      strlcpy(buf, get_plaintext_matrix_score_stats(ch, detect), sizeof(buf));
      send_to_icon(PERSONA, buf);
      return;
    }

    if (!strncmp(argument, "deck", strlen(argument))) {
      strlcpy(buf, get_plaintext_matrix_score_deck(ch), sizeof(buf));
      send_to_icon(PERSONA, buf);
      return;
    }

    if (!strncmp(argument, "memory", strlen(argument))) {
      strlcpy(buf, get_plaintext_matrix_score_memory(ch), sizeof(buf));
      send_to_icon(PERSONA, buf);
      return;
    }

    snprintf(buf, sizeof(buf), "Sorry, that's not a valid score subsheet. Your options are HEALTH, STATS, DECK, and MEMORY.\r\n");
    send_to_icon(PERSONA, buf);
    return;
  }

  if (ch->persona->type == ICON_LIVING_PERSONA)
    snprintf(buf, sizeof(buf), "You are communing directly with the matrix using your living persona.\r\n");
  else
    snprintf(buf, sizeof(buf), "You are connected to the matrix with %s.\r\n", GET_OBJ_NAME(DECKER->deck));

  if (PRF_FLAGGED(ch, PRF_SCREENREADER)) {
    strlcat(buf, get_plaintext_matrix_score_health(ch), sizeof(buf));
    strlcat(buf, get_plaintext_matrix_score_stats(ch, detect), sizeof(buf));
    strlcat(buf, get_plaintext_matrix_score_deck(ch), sizeof(buf));
    strlcat(buf, get_plaintext_matrix_score_memory(ch), sizeof(buf));
  } else {
    if (ch->persona->type == ICON_LIVING_PERSONA) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf),
              "     Mental:^B%3d(%2d)^n       Physical:^R%3d(%2d)^n\r\n",
              (int)(GET_MENTAL(ch) / 100), (int)(GET_MAX_MENTAL(ch) / 100), 
              (int)(GET_PHYSICAL(ch) / 100), (int)(GET_MAX_PHYSICAL(ch) / 100));
    } else {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf),
              "  Condition:^B%3d^n           Physical:^R%3d(%2d)^n\r\n",
              PERSONA_CONDITION, (int)(GET_PHYSICAL(ch) / 100), (int)(GET_MAX_PHYSICAL(ch) / 100));
    }
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), 
            "  Detection:^r%3d^n       Hacking Pool:^g%3d/%3d (%2d)^n\r\n",
            detect, MAX(0, GET_REM_HACKING(ch)), GET_HACKING(ch), GET_MAX_HACKING(ch));
    if (ch->persona->type != ICON_LIVING_PERSONA) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), 
            "    Storage:^g%4d^n/%4d (^c%d^n MP free)\r\n",
            GET_CYBERDECK_USED_STORAGE(DECKER->deck), GET_CYBERDECK_TOTAL_STORAGE(DECKER->deck), GET_CYBERDECK_FREE_STORAGE(DECKER->deck));
    }
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf),
            "            ^cPersona Programs:^n\r\n"
            "        Bod:^B%3d^n       Evasion:^B%3d^n\r\n"
            "    Masking:^B%3d^n       Sensors:^B%3d^n\r\n"
            "               ^cDeck Status:^n\r\n"
            "  Hardening:^g%3d^n       MPCP:^g%3d^n\r\n"
            "   IO Speed:^g%4d^n      Response Increase:^g%3d^n\r\n",
            DECKER->bod, DECKER->evasion, DECKER->masking, DECKER->sensor,
            DECKER->hardening, DECKER->mpcp, DECKER->deck ? GET_CYBERDECK_IO_RATING(DECKER->deck) : 0, DECKER->response);
  }

  if (HAS_HITCHER_JACK(DECKER->deck)) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "    Hitcher: %s\r\n", DECKER->hitcher ? "^gconnected^n" : "^rdisconnected^n");
  }

  if (ch->persona->type == ICON_LIVING_PERSONA) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "     Echoes: ");
    int echoes_found = 0;
    for (int ci=ECHO_UNDEFINED + 1; ci <= ECHO_MAX;ci++) {
      if (!GET_ECHO(ch, ci)) continue;
      echoes_found++;
      if (echoes[ci].incremental)
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s%s (%d)",
          echoes_found > 0 ? ", " : "",
          echoes[ci].name,
          GET_ECHO(ch, ci));
      else
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s%s",
          echoes_found > 0 ? ", " : "",
          echoes[ci].name);
    }
    if (echoes_found > 0) snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\n");
    else snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " None\r\n");
  }

  if (DECKER->io < GET_CYBERDECK_IO_RATING(DECKER->deck)) {
    snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^yYour I/O rating is restricted to %d by your jackpoint.^n\r\n", DECKER->io * 10);
  }

  strlcat(buf, "\r\n(Switches available: ^WSCORE HEALTH^n, ^WSTATS^n, ^WDECK^n, ^WMEMORY^n.)\r\n", sizeof(buf));

  send_to_icon(PERSONA, buf);
}

ACMD(do_locate)
{
  if (!PERSONA) {
    send_to_char(ch, "You can't do that while hitching.\r\n");
    return;
  }
  WAIT_STATE(ch, (int) (DECKING_WAIT_STATE_TIME));
  two_arguments(argument, buf, arg);
  int success, i = 0;
  if (is_abbrev(buf, "hosts")) {
    success = system_test(PERSONA->in_host, ch, ACIFS_INDEX, SOFT_BROWSE, 0);
    int x = 0;
    char *name = arg;
    while (*name) {
      x++;
      name++;
    }
    if (x < 3) {
      send_to_icon(PERSONA, "Your search returns %d useless matches. Try a longer search string.\r\n", number(1000, 30000));
      return;
    }
    if (success > 0) {
      for (struct exit_data *exit = matrix[PERSONA->in_host].exit; exit; exit = exit->next)
        if (real_host(exit->host) > 0
            && isname(arg, matrix[real_host(exit->host)].keywords)
            && matrix[real_host(exit->host)].alert <= 2) {
          send_to_icon(PERSONA, "Your search returns the address %s^n.\r\n", fname_allchars(exit->addresses));
          i++;
        }
      if (!i)
        send_to_icon(PERSONA, "This host isn't hooked up to any connections with that keyword.\r\n");
    } else {
      send_to_icon(PERSONA, "You fumble your attempt to locate that host.\r\n");
    }
    return;
  } else if (is_abbrev(buf, "ics")) {
    success = system_test(PERSONA->in_host, ch, ACIFS_INDEX, SOFT_ANALYZE, 0);
    if (success > 0) {
      for (struct matrix_icon *icon = matrix[PERSONA->in_host].icons; icon; icon = icon->next_in_host)
        if (ICON_IS_IC(icon)) {
          if (icon->evasion)
            icon->evasion = 0;
          make_seen(PERSONA, icon->idnum);
          i++;
        }

      if (!i)
        send_to_icon(PERSONA, "There aren't any ICs in the host right now.\r\n");
      else
        send_to_icon(PERSONA, "You notice %d IC in the host.\r\n", i);
    } else {
      send_to_icon(PERSONA, "You don't notice any ICs in the host.\r\n");
    }
    return;
  } else if (is_abbrev(buf, "files")) {
    int x = 0;
    char *name = arg;
    while (*name) {
      x++;
      name++;
    }
    if (x < 3) {
      send_to_icon(PERSONA, "Your search returns %d useless matches. Try a longer search string.\r\n", number(1000, 30000));
      return;
    }
    success = system_test(PERSONA->in_host, ch, ACIFS_INDEX, SOFT_BROWSE, 0);
    if (success <= 0)
      send_to_icon(PERSONA, "You fumble your attempt to locate files.\r\n");
    else {
      if (PERSONA) {
        for (struct matrix_file *file = matrix[PERSONA->in_host].files; file && success > 0; file = file->next_file) {
          // If it has been found by someone else:
          if (file->found_by != PERSONA->idnum) {
             // You can only overwrite claims for quest objects that you're grouped with the questor for. No quest? Skip.
            if (!file->quest_id) continue;

            // Skip it if you're not grouped with the person whose quest it is.
            if (!ch_is_grouped_with_idnum(ch, file->quest_id))
              continue;

            // Otherwise, you can still potentially find it.
            // fall through
          }

          // Skip anything without a keyword match.
          if (!keyword_appears_in_file(arg, file))
            continue;
         
          // Found a match!
          file->found_by = PERSONA->idnum;
          success--;
          i++;
        }

        if (!i)
          send_to_icon(PERSONA, "You fail to return any data on that search.\r\n");
        else
          send_to_icon(PERSONA, "Your search returns %d match%s.\r\n", i, i > 1 ? "es" : "");
      }
    }
    return;
  } else if (is_abbrev(buf, "deckers")) {
    success = system_test(PERSONA->in_host, ch, ACIFS_INDEX, SOFT_SCANNER, 0);
    if (success > 0) {
      int sensor = 0;
      for (int r = DECKER->sensor;r > 0; r--) {
        i = srdice();
        if (i > sensor)
          sensor = i;
      }
      i = 0;
      for (struct matrix_icon *icon = matrix[PERSONA->in_host].icons; icon; icon = icon->next_in_host)
        if (icon->decker && icon != PERSONA) {
          int targ = icon->decker->masking;
          for (struct matrix_file *soft = icon->decker->software; soft; soft = soft->next_file)
            if (soft->program_type == SOFT_SLEAZE)
              targ += soft->rating;
          if (sensor >= targ) {
            make_seen(PERSONA, icon->idnum);
            i++;
          }
        }
      if (!i)
        send_to_icon(PERSONA, "You don't notice any other deckers in the host.\r\n");
      else
        send_to_icon(PERSONA, "You notice %d other decker%s in the host.\r\n", i, (i > 1 ? "s":""));
    } else {
      send_to_icon(PERSONA, "You fumble your attempt to locate other deckers.\r\n");
    }
    return;
  } else if (is_abbrev(buf, "paydata")) {
    {
      // We use in_host here because system_test can result in you getting booted from the Matrix.
      rnum_t in_host = PERSONA->in_host;

      success = system_test(in_host, ch, ACIFS_INDEX, SOFT_EVALUATE, 0);
      if (success <= 0) {
        send_to_icon(PERSONA, "You fumble your attempt to locate paydata.\r\n");

        matrix[in_host].undiscovered_paydata = MAX(matrix[in_host].undiscovered_paydata - 1, 0);
        return;
      }
    }

    if (matrix[PERSONA->in_host].type == HOST_DATASTORE && matrix[PERSONA->in_host].undiscovered_paydata > 0) {
      while (success && matrix[PERSONA->in_host].undiscovered_paydata > 0) {
        matrix[PERSONA->in_host].undiscovered_paydata--;
        success--;
        i++;
        spawn_paydata(PERSONA);
      }
    }

    if (!i) {
      if (matrix[PERSONA->in_host].ic_bound_paydata > 0) {
        // Search the host for existing ICs.
        for (struct matrix_icon *ic_test = matrix[PERSONA->in_host].icons; ic_test; ic_test = ic_test->next_in_host) {
          // If there's already an IC in the host...
          if (!ic_test->decker) {
            send_to_icon(PERSONA, "There's no free-floating paydata left to find, but you're pretty sure there's at least one piece bound up in an IC somewhere.\r\n");
            return;
          }
        }

        // No ICs in the host. Spawn one.
        if (spawn_ic(PERSONA, -1, -1)) {
          send_to_icon(PERSONA, "There's no free-floating paydata left to find, but you're able to locate a suspicious-looking IC.\r\n");
          return;
        }

        // We've failed to spawn an IC-- character hit the end of the sheaf. Log it.
        mudlog_vfprintf(ch, LOG_GRIDLOG, "BUILD ERROR: %s hit end of sheaf with paydata remaining in host %s (%ld).", GET_CHAR_NAME(ch), matrix[PERSONA->in_host].name, matrix[PERSONA->in_host].vnum);

        // Log spam defense: Have a chance of dumping them.
        send_to_icon(PERSONA, "The node fuzzes around you as it tries to load further defenses-- something's gone wrong, and it's destabilizing! Your connection closes abruptly.\r\n");
        snprintf(buf, sizeof(buf), "%s^n abruptly shatters into digital static.\r\n", CAP(PERSONA->name));
        send_to_host(PERSONA->in_host, buf, PERSONA, FALSE);

        // Cleanup of uploads, downloads, etc is handled in icon_from_host, which is called in extract_icon.
        extract_icon(PERSONA);
        PERSONA = NULL;
        PLR_FLAGS(ch).RemoveBit(PLR_MATRIX);
        return;
      }

      // No IC-bound paydata exists, OR we were not able to locate or spawn an IC to carry it.
      send_to_icon(PERSONA, "There's no paydata left to find.\r\n");
      return;
    } else {
      switch (number(0, 100)) {
        case 0:
          send_to_icon(PERSONA, "Your interface highlights %d chunk%s of primo paydata.\r\n", i, i > 1 ? "s": "");
          break;
        case 1:
          send_to_icon(PERSONA, "You find %d potentially-valuable file%s.\r\n", i, i > 1 ? "s": "");
          break;
        case 2:
          send_to_icon(PERSONA, "Your search turns up %d bit%s of useful data.\r\n", i, i > 1 ? "s": "");
          break;
        case 3:
          send_to_icon(PERSONA, "You are able to locate %d discrete unit%s of paydata.\r\n", i, i > 1 ? "s": "");
          break;
        default:
          send_to_icon(PERSONA, "You find %d piece%s of paydata.\r\n", i, i > 1 ? "s": "");
          break;
      }

    }
    return;
  }
  send_to_icon(PERSONA, "You can locate hosts, ICs, files, deckers, or paydata.\r\n");
}

void show_icon_to_persona(struct matrix_icon *ch, struct matrix_icon *icon) {
  if (!ch || !icon) {
    mudlog("SYSERR: Missing ch or icon in show_icon_to_persona!", NULL, LOG_SYSLOG, TRUE);
    return;
  }

  // Start with description.
  snprintf(buf, sizeof(buf), "%s^n\r\n%s^n is ", icon->long_desc, CAP(icon->name));

  // Generate condition.
  if (icon->condition < 2) {
    strcat(buf, "in terrible shape!");
  } else if (icon->condition < 5) {
    strcat(buf, "looking pretty beat-up.");
  } else if (icon->condition < 8) {
    strcat(buf, "damaged.");
  } else if (icon->condition < 10) {
    strcat(buf, "minorly damaged.");
  } else {
    strcat(buf, "intact.");
  }

  send_to_icon(ch, "%s\r\n", buf);
}

ACMD(do_matrix_look)
{
  // Did they supply a target? Look at that instead.
  if (argument && *argument) {
    one_argument(argument, arg);

    // Looking at self? Ezpz.
    if (!str_cmp(arg, "self") || !str_cmp(arg, "me") || !str_cmp(arg, "myself")) {
      show_icon_to_persona(PERSONA, PERSONA);
      return;
    }

    // Find the icon they're looking at.
    for (struct matrix_icon *icon = matrix[PERSONA->in_host].icons; icon; icon = icon->next_in_host) {
      if (has_spotted(PERSONA, icon) && keyword_appears_in_icon(arg, icon, TRUE, FALSE)) {
        show_icon_to_persona(PERSONA, icon);
        return;
      }
    }
    send_to_icon(PERSONA, "You can't see that icon in this host.\r\n");
    return;
  }

  if ((PRF_FLAGGED(ch, PRF_ROOMFLAGS) && GET_REAL_LEVEL(ch) >= LVL_BUILDER)) {
    snprintf(buf, sizeof(buf), "^C[%ld]^n %s^n\r\n%s^n", matrix[PERSONA->in_host].vnum, matrix[PERSONA->in_host].name, matrix[PERSONA->in_host].desc);
  } else {
    snprintf(buf, sizeof(buf), "^C%s^n\r\n%s^n", matrix[PERSONA->in_host].name, matrix[PERSONA->in_host].desc);
  }
  send_to_icon(PERSONA, buf);

  // Set up for sending our host list.
  send_to_icon(PERSONA, "^cObvious hosts:^n\r\n");
  bool sent_any_roomstring = FALSE;

  // Iterate through connected hosts.
  for (struct exit_data *exit = matrix[PERSONA->in_host].exit; exit; exit = exit->next) {
    // Skip exits to hosts that don't exist or exits that are hidden by default.
    if (real_host(exit->host) <= 0 || exit->hidden)
      continue;

    // We're sending something. If they don't have a valid roomstring (legacy exits), craft a generic one.
    if (!exit->roomstring || !*(exit->roomstring))
      snprintf(buf, sizeof(buf), "^W(Obvious Host) ^cA plain-looking icon with the address %s floats here.^n\r\n", fname_allchars(exit->addresses));
    else
      snprintf(buf, sizeof(buf), "^W(Obvious Host) ^c%s^n\r\n", exit->roomstring);

    // Send our composed string and indicate that we've sent something.
    send_to_icon(PERSONA, buf);
    sent_any_roomstring = TRUE;
  }

  // Finish: If we've not sent anything, just send "None"
  if (!sent_any_roomstring)
    send_to_icon(PERSONA, "^cNone.^n\r\n");

  for (struct matrix_icon *icon = matrix[PERSONA->in_host].icons; icon; icon = icon->next_in_host)
    if (has_spotted(PERSONA, icon))
      send_to_icon(PERSONA, "^Y%s^n\r\n", icon->look_desc);

  for (struct matrix_file *file = matrix[PERSONA->in_host].files; file; file = file->next_file) {
    if (file->found_by == PERSONA->idnum)
    {
      if (file->transferring_to) {
        int percent_complete = (int) (100 * ((float) file->size - file->transfer_remaining) / MAX(1, file->size));
        send_to_icon(PERSONA, "^yA file named %s^y floats here (Downloading - %d%%).^n\r\n",
                     file->name, percent_complete);
      } else if (file->file_type == MATRIX_FILE_PAYDATA) {
        send_to_icon(PERSONA, "^yA paydata file named %s - ^g%d^n^yMp floats here.%s^n\r\n", 
                     file->name,
                     file->size,
                     file->quest_id ? (ch_is_grouped_with_idnum(ch, file->quest_id) ? " ^Y(Quest)" : " ^m(Protected)") : "");
      } else {
        send_to_icon(PERSONA, "^yA file named %s^y floats here.%s^n\r\n", 
                     file->name,
                     file->quest_id ? (ch_is_grouped_with_idnum(ch, file->quest_id) ? " ^Y(Quest)" : " ^m(Protected)") : "");
      }
    }

    if (file == file->next_file) {
      mudlog("SYSERR: Infinite loop detected in Matrix object listing. Discarding subsequent objects.", NULL, LOG_SYSLOG, TRUE);
      file->next_file = NULL;
      break;
    }
  }
}

ACMD(do_analyze)
{
  if (!PERSONA) {
    send_to_char(ch, "You can't do that while hitching.\r\n");
    return;
  }
  if (!*argument) {
    send_to_icon(PERSONA, "Analyze What?\r\n");
    return;
  }
  WAIT_STATE(ch, (int) (DECKING_WAIT_STATE_TIME));
  int success;
  one_argument(argument, arg);
  if (is_abbrev(arg, "host")) {
    success = system_test(PERSONA->in_host, ch, ACIFS_CONTROL, SOFT_ANALYZE, 0);
    if (success > 0) {
      snprintf(buf, sizeof(buf), "You analyze the ^W%s^n host.\r\n", host_type[matrix[PERSONA->in_host].type]);
      send_to_icon(PERSONA, buf);
      int found[] = { 0, 0, 0, 0, 0, 0, 0};
      if (success < 7) {
        for (int current;success; success--) {
          current = number(0, 6);
          int start = current;
          while (found[current]) {
            current++;
            if (current > 6)
              current = 0;
            if (current == start)
              break;
          }
          found[current] = 1;
        }

        // Color
        if (found[0])
          snprintf(buf, sizeof(buf), "%s-", host_color[matrix[PERSONA->in_host].color]);
        else
          snprintf(buf, sizeof(buf), "?-");
        // Security
        if (found[1])
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%d ", matrix[PERSONA->in_host].security);
        else
          strcat(buf, "? ");
        // Access
        if (found[2])
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%ld/", matrix[PERSONA->in_host].stats[ACIFS_ACCESS][MTX_STAT_RATING]);
        else
          strcat(buf, "0/");
        // Control
        if (found[3])
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%ld/", matrix[PERSONA->in_host].stats[ACIFS_CONTROL][MTX_STAT_RATING]);
        else
          strcat(buf, "0/");
        // Index
        if (found[4])
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%ld/", matrix[PERSONA->in_host].stats[ACIFS_INDEX][MTX_STAT_RATING]);
        else
          strcat(buf, "0/");
        // Files
        if (found[5])
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%ld/", matrix[PERSONA->in_host].stats[ACIFS_FILES][MTX_STAT_RATING]);
        else
          strcat(buf, "0/");
        // Slave
        if (found[6])
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%ld", matrix[PERSONA->in_host].stats[ACIFS_SLAVE][MTX_STAT_RATING]);
        else
          strcat(buf, "0");

        strcat(buf, "\r\n");
      } else
        snprintf(buf, sizeof(buf), "%s-%d %ld/%ld/%ld/%ld/%ld\r\n", host_color[matrix[PERSONA->in_host].color],
                matrix[PERSONA->in_host].security, matrix[PERSONA->in_host].stats[ACIFS_ACCESS][MTX_STAT_RATING],
                matrix[PERSONA->in_host].stats[ACIFS_CONTROL][MTX_STAT_RATING], matrix[PERSONA->in_host].stats[ACIFS_INDEX][MTX_STAT_RATING],
                matrix[PERSONA->in_host].stats[ACIFS_FILES][MTX_STAT_RATING], matrix[PERSONA->in_host].stats[ACIFS_SLAVE][MTX_STAT_RATING]);
      send_to_char(buf, ch);
    } else
      send_to_icon(PERSONA, "Your program fails to run.\r\n");
    return;
  } else if (is_abbrev(arg, "security")) {
    success = system_test(PERSONA->in_host, ch, ACIFS_CONTROL, SOFT_ANALYZE, 0);
    if (success > 0) {
      send_to_icon(PERSONA, "You analyze the security of the host.\r\n");
      snprintf(buf, sizeof(buf), "%s-%d Tally: %d Alert: %s\r\n", host_color[matrix[PERSONA->in_host].color],
              matrix[PERSONA->in_host].security, DECKER->tally,
              alerts[matrix[PERSONA->in_host].alert]);
      send_to_icon(PERSONA, buf);
    } else
      send_to_icon(PERSONA, "Your program fails to run.\r\n");
    return;
  } else if (is_abbrev(arg, "access") || is_abbrev(arg, "control") || is_abbrev(arg, "index") || is_abbrev(arg, "files")  || is_abbrev(arg, "slave")) {
    int mode = 0;
    if (is_abbrev(arg, "access"))
      mode = ACIFS_ACCESS;
    else if (is_abbrev(arg, "control"))
      mode = ACIFS_CONTROL;
    else if (is_abbrev(arg, "index"))
      mode = ACIFS_INDEX;
    else if (is_abbrev(arg, "files"))
      mode = ACIFS_FILES;
    else if (is_abbrev(arg, "slave"))
      mode = ACIFS_SLAVE;
    int success = system_test(PERSONA->in_host, ch, mode, SOFT_ANALYZE, 0);
    if (success > 0) {
      if (matrix[PERSONA->in_host].stats[mode][MTX_STAT_ENCRYPTED] || matrix[PERSONA->in_host].stats[mode][MTX_STAT_TRAPDOOR]) {
        if (matrix[PERSONA->in_host].stats[mode][MTX_STAT_ENCRYPTED])
          send_to_icon(PERSONA, "The subsystem is protected by Scramble-%ld IC.\r\n", matrix[PERSONA->in_host].stats[mode][MTX_STAT_SCRAMBLE_IC_RATING]);
        if (matrix[PERSONA->in_host].stats[mode][MTX_STAT_TRAPDOOR])
          send_to_icon(PERSONA, "There is a trapdoor located in this subsystem.\r\n");
      } else send_to_icon(PERSONA, "There is nothing out of the ordinary about that subsystem.\r\n");
    } else send_to_icon(PERSONA, "Your program fails to run.\r\n");
    return;
  } else {
    struct matrix_file *file = NULL;
    if ((file = get_matrix_file_in_list_vis(ch, arg, matrix[PERSONA->in_host].files)) && file->found_by == PERSONA->idnum) {
      int success = system_test(PERSONA->in_host, ch, ACIFS_CONTROL, SOFT_ANALYZE, 0);
      if (success > 0) {
        send_to_icon(PERSONA, "You analyze the file:\r\n");
        if (file->file_type == MATRIX_FILE_PAYDATA) {
          send_to_icon(PERSONA, "Paydata %dMp\r\n", file->size);
          switch(file->file_protection) {
            case FILE_PROTECTION_SCRAMBLED:
              send_to_icon(PERSONA, "Protected by: Scramble-%d\r\n", file->rating);
                break;
            case FILE_PROTECTION_DATABOMB:
              send_to_icon(PERSONA, "Protected by: Data Bomb-%d\r\n", file->rating);
                break;
            case FILE_PROTECTION_PAVLOV:
              send_to_icon(PERSONA, "Protected by: Pavlov-%d\r\n", file->rating);
                break;
          }
        } else {
          send_to_icon(PERSONA, "%s^n - %dMp\r\n", file->name, file->size);
          if (file->content) send_to_icon(PERSONA, file->content);
        }
      } else
        send_to_icon(PERSONA, "You fail to get any useful information out of your request.\r\n");
      return;
    }
    for (struct matrix_icon *ic = matrix[PERSONA->in_host].icons; ic; ic = ic->next_in_host)
      if (has_spotted(PERSONA, ic) && keyword_appears_in_icon(arg, ic, TRUE, FALSE)) {
        int success = system_test(PERSONA->in_host, ch, ACIFS_CONTROL, SOFT_ANALYZE, 0);
        if (success > 0 ) {
          show_icon_to_persona(PERSONA, ic);
          if (ICON_IS_IC(ic)) {
            send_to_icon(PERSONA, "%s^n is a %s-%d\r\n", CAP(ic->name), ic_type[ic->ic.type],
                         matrix[PERSONA->in_host].shutdown ? ic->ic.rating - 2 : ic->ic.rating);
            if (ic->ic.options.AreAnySet(IC_ARMOR, IC_CASCADE, IC_EX_OFFENSE, IC_EX_DEFENSE, IC_TRAP, IC_SHIELD, IC_SHIFT, ENDBIT)) {
              ic->ic.options.PrintBits(buf1, MAX_STRING_LENGTH, ic_option_long, IC_TRAP + 1);
              buf1[strlen(buf1) - 2] = '\0';
              send_to_icon(PERSONA, "It has the following options: %s.\r\n", buf1);
            }
          }
        } else
          send_to_icon(PERSONA, "You fail to get any useful information out of your request.\r\n");
        return;
      }
  }
  send_to_icon(PERSONA, "Analyze what?\r\n");
}


ACMD(do_logon)
{
  if (!PERSONA) {
    send_to_char(ch, "You can't do that while hitching.\r\n");
    return;
  }
  skip_spaces(&argument);
  rnum_t target_host = -1, trapdoor_host = -1;
  int subsystem = ACIFS_ACCESS;  // Most logons target ACCESS 

  WAIT_STATE(ch, (int) (DECKING_WAIT_STATE_TIME));
  if (!str_cmp(argument, "LTG")) {
    // Parent is not real, or parent is not LTG/PLTG
    // BUG: && should be ||, but fixing this may prevent backing out of hosts
    // I.e., fixing this would mean we also have to audit every host for backlinks
    if ( ((target_host = real_host(matrix[PERSONA->in_host].parent)) <= 0)
        && !(matrix[target_host].type == HOST_LTG || matrix[target_host].type == HOST_PLTG)) {
      send_to_icon(PERSONA, "This host is not connected to a LTG.\r\n");
      return;
    }
  } else if (!str_cmp(argument, "RTG")) {
    // Current host is not a LTG, or parent is not real
    if (matrix[PERSONA->in_host].type != HOST_LTG ||
        ((target_host = real_host(matrix[PERSONA->in_host].parent)) <= 0) ) {
      send_to_icon(PERSONA, "This host is not connected to a RTG.\r\n");
      return;
    }
  } else {
    // Subsystem trap door? Test against the local subsystem instead of destination ACCESS
    for (int sub = ACIFS_ACCESS; sub < NUM_ACIFS; sub++) {
      if (!str_cmp(argument, mtx_subsystem_names[sub]) && matrix[PERSONA->in_host].stats[sub][MTX_STAT_TRAPDOOR]) {
        target_host = PERSONA->in_host;
        subsystem = sub;
        trapdoor_host = real_host(matrix[PERSONA->in_host].stats[sub][MTX_STAT_TRAPDOOR]);
        break;
      }
    }
    // Check normal exits (if we haven't already found a target)
    for (struct exit_data *exit = matrix[PERSONA->in_host].exit; (target_host <= 0) && exit; exit = exit->next)
      if (isname(argument, exit->addresses))
        target_host = real_host(exit->host);
  }

  if (target_host > 0 && matrix[target_host].alert <= 2) {
    int success = system_test(target_host, ch, subsystem, SOFT_DECEPTION, 0);
    if (success > 0) {
      if (matrix[target_host].type == HOST_RTG && matrix[PERSONA->in_host].type == HOST_RTG)
        DECKER->tally = 0;
      DECKER->last_trigger = 0;
      DECKER->located = FALSE;
      snprintf(buf, sizeof(buf), "%s^n connects to a different host and vanishes from this one.\r\n", CAP(PERSONA->name));
      send_to_host(PERSONA->in_host, buf, PERSONA, TRUE);
      icon_from_host(PERSONA);

      // Send decker to appropriate destination
      if (trapdoor_host > 0) {
        send_to_char(ch, "This hole wasn't made for you, but you squeeze through anyway.\r\n");
        icon_to_host(PERSONA, trapdoor_host);
      } else {
        send_to_icon(PERSONA, "You connect to %s^n.\r\n", matrix[target_host].name);
        icon_to_host(PERSONA, target_host);
      }
      SendGMCPMatrixInfo(ch);
      do_matrix_look(ch, NULL, 0, 0);
      return;
    } else {
      send_to_icon(PERSONA, "You fail to gain access to the host.\r\n");
      return;
    }
  }
  send_to_icon(PERSONA, "You haven't located any hosts with the address '%s'.\r\n", argument);
}

ACMD(do_logoff)
{
  if (!PERSONA) {
    send_to_char(ch, "You yank the plug out and return to the real world.\r\n");
    clear_hitcher(ch, TRUE);
    return;
  }
  if (subcmd) {
    WAIT_STATE(ch, (int) (3 RL_SEC));
    send_to_char(ch, "You yank the plug out and return to the real world.\r\n");
    for (struct matrix_icon *icon = matrix[PERSONA->in_host].icons; PERSONA && icon; icon = icon->next_in_host)
      if (icon->fighting == PERSONA && icon->ic.type >= IC_LETHAL_BLACK) {
        send_to_icon(PERSONA, "The IC takes a final shot.\r\n");
        matrix_fight(icon, PERSONA);
      }
    if (PERSONA)
      dumpshock(PERSONA);
    return;
  } else {
    for (struct matrix_icon *icon = matrix[PERSONA->in_host].icons; icon; icon = icon->next_in_host)
      if (icon->fighting == PERSONA && icon->ic.type >= IC_LETHAL_BLACK) {
        send_to_icon(PERSONA, "You can't log off gracefully while fighting a black IC!\r\n");
        return;
      }
    int success = system_test(PERSONA->in_host, ch, ACIFS_ACCESS, SOFT_DECEPTION, 0);
    if (success <= 0) {
      WAIT_STATE(ch, (int) (DECKING_WAIT_STATE_TIME));
      send_to_icon(PERSONA, "The matrix host's automated procedures detect and block your logoff attempt.\r\n");
      return;
    }
    send_to_icon(PERSONA, "You gracefully log off from the matrix and return to the real world.\r\n");
    WAIT_STATE(ch, (int) (0.5 RL_SEC));
  }
  snprintf(buf, sizeof(buf), "%s^n depixelates and vanishes from the host.\r\n", CAP(PERSONA->name));
  send_to_host(PERSONA->in_host, buf, PERSONA, FALSE);

  // Cleanup of uploads, downloads, etc is handled in icon_from_host, which is called in extract_icon.
  extract_icon(PERSONA);
  PERSONA = NULL;
  PLR_FLAGS(ch).RemoveBit(PLR_MATRIX);
  SendGMCPMatrixInfo(ch);
  SendGMCPMatrixDeck(ch);

  // Make 'em look if they're not screenreaders.
  if (!PRF_FLAGGED(ch, PRF_SCREENREADER)) {
    char emptybuf[10] = { '\0' };
    do_look(ch, emptybuf, 0, 0);
  }
}

extern struct obj_data *make_otaku_deck(struct char_data *ch);
void find_cyberdeck(char_data *ch, obj_data *&cyberdeck)
{
  struct obj_data *cyber = NULL;

  if (IS_OTAKU(ch)) {
    cyberdeck = make_otaku_deck(ch);
  }

  for (cyber = ch->carrying; !cyberdeck && cyber; cyber = cyber->next_content)
    if ((GET_OBJ_TYPE(cyber) == ITEM_CYBERDECK || GET_OBJ_TYPE(cyber) == ITEM_CUSTOM_DECK) && (IS_SENATOR(ch) || !IS_OBJ_STAT(cyber, ITEM_EXTRA_STAFF_ONLY)))
      cyberdeck = cyber;
  for (int i = 0; !cyberdeck && i < NUM_WEARS; i++)
    if (GET_EQ(ch, i) && (GET_OBJ_TYPE(GET_EQ(ch,i )) == ITEM_CYBERDECK || GET_OBJ_TYPE(GET_EQ(ch,i )) == ITEM_CUSTOM_DECK))
      cyberdeck = GET_EQ(ch, i);
}

/**
 * @brief Parses the connect command's arguments and modifies the caller's variables
 *
 * @param cyberdeck A reference to a pointer to cyberdeck, which could be modified
 *                   to point to a new object.
 * @param host For senators, host allows connecting directly to a host
 *
 * @returns whether or not the connect function should return early.
 */
bool parse_connect_args(char_data *ch, char *argument, obj_data *&cyberdeck, rnum_t *host) {
  struct char_data *temp;
  vnum_t host_vnum = 0;

  // Easy guard check; is there even an argument?
  skip_spaces(&argument);
  if (!*argument) return FALSE;

  char targ[MAX_INPUT_LENGTH];
  one_argument(argument, targ);

  if (IS_SENATOR(ch)) {
    host_vnum = atoi(argument);
  }

  if (!PLR_FLAGGED(ch, PLR_MATRIX) && host_vnum <= 0) {
    // This is the hitcher code.
    temp = get_char_room_vis(ch, argument);
    if (!temp) {
      send_to_char(ch, "You don't see anyone named '%s' here.\r\n", argument);
      return TRUE;
    } else if (temp == ch) {
      send_to_char(ch, "Are you trying to divide by zero? You can't connect to yourself.\r\n");
      return TRUE;
    } else if (
        !PLR_FLAGGED(temp, PLR_MATRIX)
        || !temp->persona
        || !temp->persona->decker
        || !temp->persona->decker->deck
        || !HAS_HITCHER_JACK(temp->persona->decker->deck)
        || IS_IGNORING(temp, is_blocking_ic_interaction_from, ch)) {
      send_to_char(ch, "It doesn't look like you can hitch a ride with %s.\r\n", argument);
      return TRUE;
    } else if (temp->persona->decker->hitcher) {
      send_to_char(ch, "The hitcher jack on %s's deck is already in use.\r\n", argument);
      return TRUE;
    }

    act("You slip your jack into $n's hitcher port.", FALSE, temp, 0, ch, TO_VICT);
    send_to_char("Someone has connected to your hitcher port.\r\n", temp);
    PLR_FLAGS(ch).SetBit(PLR_MATRIX);
    temp->persona->decker->hitcher = ch;
    ch->hitched_to = temp;
    return FALSE;
  }

  if (IS_SENATOR(ch)) {
    if (host_vnum <= 0) {
      send_to_char("Invalid syntax! Either CONNECT with no arguments, or CONNECT <vnum> to connect directly to that host.\r\n", ch);
      return TRUE;
    }

    rnum_t host_rnum = real_host(host_vnum);
    if (host_rnum < 0) {
      send_to_char(ch, "%s is not a valid host.\r\n", argument);
      return TRUE;
    }

    send_to_char(ch, "You override your jackpoint to connect to host %ld.\r\n", host_vnum);
    *host = host_rnum;
  }

  return FALSE;
}

ACMD(do_connect)
{
  struct matrix_icon *icon = NULL;
  
  struct obj_data *cyberdeck = NULL, *jack;
  rnum_t host;

  if (!ch->in_room || !ch->in_room->matrix || (host = real_host(ch->in_room->matrix)) < 1) {
    send_to_char("You cannot connect to the matrix from here.\r\n", ch);
    return;
  }
  if (IS_WORKING(ch)) {
    send_to_char(TOOBUSY, ch);
    return;
  }

  WAIT_STATE(ch, (int) (DECKING_WAIT_STATE_TIME));

  // Error message sent in function.
  if (!(jack = get_datajack(ch, FALSE)))
    return;

  // Locate the appropriate cyberdeck objects for the player.
  find_cyberdeck(ch, cyberdeck);

  // Command argument parsing
  if (parse_connect_args(ch, argument, cyberdeck, &host))
      return;

#ifdef JACKPOINTS_ARE_ONE_PERSON_ONLY
      send_to_char("The jackpoint is already in use.\r\n", ch);
      return;
#endif

  if (!cyberdeck) {
    if (access_level(ch, LVL_ADMIN)) {
      // Create a !RENT staff-only deck from whole cloth.
      extern struct obj_data *make_staff_deck_target_mpcp(int mpcp);
      cyberdeck = make_staff_deck_target_mpcp(12);
      obj_to_char(cyberdeck, ch);
      send_to_char(ch, "You pull a deck out of thin air to connect with.\r\n");
    } else {
      send_to_char(ch, "I don't recommend trying to do that without a cyberdeck.\r\n");
      return;
    }
  }

  if (GET_CYBERDECK_MPCP(cyberdeck) == 0) {
    send_to_char("You cannot connect to the matrix with fried MPCP chips!\r\n", ch);
    return;
  }

  if (matrix[host].alert > 2) {
    send_to_char("It seems that host has been shut down.\r\n", ch);
    return;
  }

  if (GET_POS(ch) != POS_SITTING) {
    GET_POS(ch) = POS_SITTING;
    if (cyberdeck->obj_flags.extra_flags.IsSet(ITEM_EXTRA_OTAKU_RESONANCE)) send_to_char(ch, "You find a place to sit down and commune with the matrix.\r\n");
    else send_to_char(ch, "You find a place to sit and work with your deck.\r\n");
  }

  icon = Mem->GetIcon();
  icon->condition = 10;
  if (ch->player.matrix_text.name)
    icon->name = str_dup(ch->player.matrix_text.name);
  else
    icon->name = str_dup("a nondescript persona");
  if (ch->player.matrix_text.room_desc)
    icon->look_desc = str_dup(ch->player.matrix_text.room_desc);
  else
    icon->look_desc = str_dup("A nondescript persona stands idly here.\r\n");
  if (ch->player.matrix_text.look_desc)
    icon->long_desc = str_dup(ch->player.matrix_text.look_desc);
  else
    icon->long_desc = str_dup("A nondescript persona stands idly here.\r\n");
  // If this is an Otaku GhostDeck, then this is a living persona
  if (cyberdeck->obj_flags.extra_flags.IsSet(ITEM_EXTRA_OTAKU_RESONANCE)) {
    icon->type = ICON_LIVING_PERSONA;
  }

  if (GET_OBJ_TYPE(cyberdeck) == ITEM_CUSTOM_DECK && GET_CYBERDECK_IS_INCOMPLETE(cyberdeck)) {
    send_to_char(ch, "That deck is missing some components.\r\n");
    return;
  }
  PERSONA = icon;
  DECKER = new deck_info;
  if (GET_OBJ_VNUM(cyberdeck) == OBJ_CUSTOM_CYBERDECK_SHELL) {
    struct obj_data *parts = cyberdeck->contains;
    for (; parts; parts = parts->next_content)
      if (GET_OBJ_TYPE(parts) == ITEM_PART && (GET_OBJ_VAL(parts, 0) == PART_ASIST_HOT || GET_OBJ_VAL(parts, 0) == PART_ASIST_COLD))
        break;
    if (!parts) {
      struct obj_data* cyber = NULL;
      for (cyber = ch->cyberware; cyber; cyber = cyber->next_content)
        if (GET_OBJ_VAL(cyber, 0) == CYB_ASIST)
          break;
      if (!cyber) {
        send_to_char("You need an ASIST interface to use that.\r\n", ch);
        extract_icon(PERSONA);
        PERSONA = NULL;
        PLR_FLAGS(ch).RemoveBit(PLR_MATRIX);
        return;
      }
      DECKER->asist[1] = 0;
      DECKER->asist[0] = 0;
      GET_MAX_HACKING(ch) = 0;
      DECKER->response = 0;
    }
  }
  DECKER->ch = ch;
  DECKER->phone = new phone_data;
  DECKER->phone->next = phone_list;
  phone_list = DECKER->phone;
  DECKER->phone->persona = PERSONA;
  DECKER->phone->rtg = ch->in_room->rtg;
  DECKER->phone->number = ch->in_room->jacknumber;
  DECKER->mxp = real_room(ch->in_room->number) * DECKER->phone->number / MAX(DECKER->phone->rtg, 1);
  PERSONA->idnum = GET_IDNUM(ch);
  DECKER->deck = cyberdeck;
  DECKER->mpcp = GET_OBJ_VAL(cyberdeck, 0);
  DECKER->hardening = GET_OBJ_VAL(cyberdeck, 1);
  DECKER->active = GET_OBJ_VAL(cyberdeck, 2);
  DECKER->response = GET_OBJ_VAL(cyberdeck, 6);
  DECKER->ras = GET_OBJ_VNUM(cyberdeck) == ITEM_CUSTOM_DECK ? FALSE : TRUE;
  affect_total(ch);
  GET_REM_HACKING(ch) = GET_HACKING(ch);
  GET_MAX_HACKING(ch) = (int)(GET_HACKING(ch) / 3);

  // Cap the IO speed based on the room's I/O rating. 0 is uncapped, -1 is capped by MPCP * 50, all other ratings cap to that rating.
  if (ch->in_room->io == 0) {
    DECKER->io = GET_CYBERDECK_IO_RATING(cyberdeck);
  } else if (ch->in_room->io == -1) {
    DECKER->io = MIN(DECKER->mpcp * 50, GET_CYBERDECK_IO_RATING(cyberdeck));
  } else {
    if (ch->in_room->io <= 100) {
      char warnbuf[1000];
      snprintf(warnbuf, sizeof(warnbuf), "Warning: I/O speed in %s (%ld) is low at %d.", GET_ROOM_NAME(ch->in_room), GET_ROOM_VNUM(ch->in_room), ch->in_room->io);
      mudlog(warnbuf, ch, LOG_SYSLOG, TRUE);
    }
    DECKER->io = MIN(ch->in_room->io, GET_CYBERDECK_IO_RATING(cyberdeck));
  }

  // IO is then divided by 10. I've set it to be a minimum of 1 here.
  DECKER->io = MAX(1, (int)(DECKER->io / 10));

  if (GET_OBJ_VNUM(cyberdeck) != OBJ_CUSTOM_CYBERDECK_SHELL 
    && !cyberdeck->obj_flags.extra_flags.IsSet(ITEM_EXTRA_OTAKU_RESONANCE)) {
    DECKER->asist[1] = 0;
    DECKER->asist[0] = 0;
    GET_MAX_HACKING(ch) = 0;
    DECKER->response = 0;
  }
  for (struct matrix_file *soft = cyberdeck->files; soft; soft = soft->next_file) {
    if (soft->file_type != MATRIX_FILE_FIRMWARE) continue;

    // Handle persona programs
    switch(soft->program_type) {
      case SOFT_BOD:
        DECKER->bod = soft->rating;
        continue;
      case SOFT_SENSOR:
        DECKER->sensor = soft->rating;
        continue;
      case SOFT_MASKING:
        DECKER->masking = soft->rating;
        continue;
      case SOFT_EVASION:
        DECKER->evasion = soft->rating;
        continue;
    }

    // Handle all other programs
    if (soft->size > DECKER->active && !soft->resonant) {
      send_to_char(ch, "%s^n would exceed your deck's active memory, so it failed to load.\r\n", soft->name);
      continue;
    }

    // OTAKU living persona don't need to check this, as all programs are complex forms
    // and we already checked that when we created the deck.
    if (PERSONA->type != ICON_LIVING_PERSONA && soft->rating > DECKER->mpcp) {
      send_to_char(ch, "%s^n is too advanced for your deck's MPCP rating, so it failed to load.\r\n", soft->name);
      continue;
    }

    struct matrix_file *active = copy_matrix_file_to(soft, NULL);
    DECKER->active -= active->size;
    active->next_file = DECKER->software;
    DECKER->software = active;
  }
  
  for (struct obj_data *soft = cyberdeck->contains; soft; soft = soft->next_content) {
    if (GET_OBJ_TYPE(soft) == ITEM_PART) {
      switch (GET_OBJ_VAL(soft, 0)) {
      case PART_BOD:
        DECKER->bod = GET_OBJ_VAL(soft, 1);
        if (PERSONA->type == ICON_LIVING_PERSONA) DECKER->bod = MIN(DECKER->mpcp*1.5, DECKER->bod + GET_ECHO(ch, ECHO_PERSONA_BOD));
        break;
      case PART_SENSOR:
        DECKER->sensor = GET_OBJ_VAL(soft, 1);
        if (PERSONA->type == ICON_LIVING_PERSONA) DECKER->sensor = MIN(DECKER->mpcp*1.5, DECKER->sensor + GET_ECHO(ch, ECHO_PERSONA_SENS));
        break;
      case PART_MASKING:
        DECKER->masking = GET_OBJ_VAL(soft, 1);
        if (PERSONA->type == ICON_LIVING_PERSONA) DECKER->masking = MIN(DECKER->mpcp*1.5, DECKER->masking + GET_ECHO(ch, ECHO_PERSONA_MASK));
        break;
      case PART_EVASION:
        DECKER->evasion = GET_OBJ_VAL(soft, 1);
        if (PERSONA->type == ICON_LIVING_PERSONA) DECKER->evasion = MIN(DECKER->mpcp*1.5, DECKER->evasion + GET_ECHO(ch, ECHO_PERSONA_EVAS));
        break;
      case PART_ASIST_HOT:
        DECKER->asist[1] = 1;
        DECKER->asist[0] = 1;
        break;
      case PART_ASIST_COLD:
        DECKER->asist[1] = 0;
        DECKER->asist[0] = 0;
        GET_MAX_HACKING(ch) = 0;
        DECKER->response = 0;
        break;
      case PART_RAS_OVERRIDE:
        DECKER->ras = TRUE;
        break;
      case PART_REALITY_FILTER:
        DECKER->reality = TRUE;
        break;
      case PART_ICCM:
        DECKER->iccm = TRUE;
        break;
      }
    }
  }

  if (icon_list)
    PERSONA->next = icon_list;
  icon_list = PERSONA;
  icon_to_host(PERSONA, host);
  if (PERSONA->type != ICON_LIVING_PERSONA && (DECKER->bod + DECKER->sensor + DECKER->evasion + DECKER->masking > DECKER->mpcp * 3)) {
    send_to_char(ch, "Your deck overloads on persona programs and crashes. You'll have to keep the combined bod, sensor, evasion, and masking rating less than or equal to %d.\r\n", DECKER->mpcp * 3);
    extract_icon(PERSONA);
    PERSONA = NULL;
    PLR_FLAGS(ch).RemoveBit(PLR_MATRIX);
    return;
  }

  if (DECKER->bod <= 0) {
    send_to_char("You'll have a hard time forming a durable persona with no Body chip.\r\n", ch);
    extract_icon(PERSONA);
    PERSONA = NULL;
    PLR_FLAGS(ch).RemoveBit(PLR_MATRIX);
    return;
  }

  if (DECKER->sensor <= 0) {
    send_to_char("You'll have a hard time receiving Matrix data with no Sensor chip.\r\n", ch);
    extract_icon(PERSONA);
    PERSONA = NULL;
    PLR_FLAGS(ch).RemoveBit(PLR_MATRIX);
    return;
  }


  if (GET_OBJ_TYPE(jack) == ITEM_CYBERWARE) {
    if (GET_CYBERWARE_TYPE(jack) == CYB_DATAJACK) {
      if (GET_CYBERWARE_FLAGS(jack) == DATA_INDUCTION) {
        snprintf(buf, sizeof(buf), "$n places $s hand over $s induction pad as $e connects to %s.",
          cyberdeck->obj_flags.extra_flags.IsSet(ITEM_EXTRA_OTAKU_RESONANCE) ? "the jackpoint" : "$s cyberdeck");
      } else {
        snprintf(buf, sizeof(buf), "$n slides one end of the cable into $s datajack and the other into %s.",
          cyberdeck->obj_flags.extra_flags.IsSet(ITEM_EXTRA_OTAKU_RESONANCE) ? "the jackpoint" : "$s cyberdeck");
      }
    } else {
      snprintf(buf, sizeof(buf), "$n's eye opens up as $e slides %s cable into $s eye datajack.",
        cyberdeck->obj_flags.extra_flags.IsSet(ITEM_EXTRA_OTAKU_RESONANCE) ? "the jackpoint" : "$s cyberdeck");
    }
  } else {
    snprintf(buf, sizeof(buf), "$n plugs the leads of $s 'trode net into $s cyberdeck.");
  }
  act(buf, FALSE, ch, 0, 0, TO_ROOM);
  if (cyberdeck->obj_flags.extra_flags.IsSet(ITEM_EXTRA_OTAKU_RESONANCE)) 
    act("You jack into the matrix to commune with the resonance.", FALSE, ch, 0, 0, TO_CHAR);
  else 
    send_to_char(ch, "You jack into the matrix with your %s.\r\n", GET_OBJ_NAME(cyberdeck));
  PLR_FLAGS(ch).SetBit(PLR_MATRIX);
  SendGMCPMatrixDeck(ch);
  SendGMCPMatrixInfo(ch);
  do_matrix_look(ch, NULL, 0, 0);
}

ACMD(do_load)
{
  if (!*argument) {
    send_to_char(ch, "What do you want to %s?\r\n", subcmd == SCMD_UNLOAD ? "unload" : "upload");
    return;
  }

  skip_spaces(&argument);
  struct matrix_file *file = NULL;

  // We handle unload first because it's simple and easy
  if (subcmd == SCMD_UNLOAD) {
    if (PERSONA->type == ICON_LIVING_PERSONA) {
      send_to_icon(PERSONA, "You don't have active memory to erase things from.\r\n");
      return;
    }

    if (!(file = get_matrix_file_in_list_vis(ch, argument, DECKER->software))) {
      send_to_char(ch, "You search but don't see any file named '%s' loaded in your active memory.\r\n", argument);
      return;
    }


    send_to_icon(PERSONA, "You erase %s^n from active memory.\r\n", file->name);
    DECKER->active += file->size;
    delete_matrix_file(file);
    return;
  }

  std::vector<obj_data *> devices = get_storage_devices(ch, TRUE);
  for (obj_data *device : devices) {
    if ((file = get_matrix_file_in_list_vis(ch, argument, device->files)))
      break;
  }

  if (!file) {
    send_to_char(ch, "You search but don't see any file named '%s' in your storage devices.\r\n", argument);
    return;
  }

  switch (subcmd) {
    case SCMD_SWAP:
      if (file->file_type == MATRIX_FILE_FIRMWARE) {
        send_to_icon(PERSONA, "Persona programs are loaded at time of connection.\r\n");
        return;
      } else if (file->size > DECKER->active) {
        send_to_icon(PERSONA, "You don't have enough active memory to load that program.\r\n");
        return;
      } else if (file->rating > DECKER->mpcp) {
        send_to_icon(PERSONA, "Your deck is not powerful enough to run that program.\r\n");
        return;
      }

      // Slight nerf, programs cannot be loaded twice
      for (struct matrix_file *soft = DECKER->software; soft; soft = soft->next_file) {
        if (soft->file_type != MATRIX_FILE_PROGRAM) continue;
        if (soft->program_type != file->program_type) continue;

        send_to_icon(PERSONA, "You aren't able to load a second %s program. ^WUNLOAD^n %s if you want to load another version.\r\n",
          programs[soft->program_type].name, soft->name);
        return;
      }

      file->file_worker = PERSONA->idnum;
      file->transfer_remaining = file->size;
      file->transferring_to = DECKER->deck;
      send_to_icon(PERSONA, "You begin to load %s into your deck's active memory.\r\n", file->name);
      if (IS_SENATOR(ch)) {
        send_to_icon(PERSONA, "(Upload will take %d ticks.)\r\n", file->transfer_remaining);
      }
      break;
    case SCMD_UPLOAD:
      if (file->file_type == MATRIX_FILE_PAYDATA) {
        send_to_icon(PERSONA, "Action aborted: Re-uploading paydata like %s^n would be a great way to get caught with it!\r\n", file->name);
        return;
      } else if (!program_can_be_copied(file)) {
        send_to_icon(PERSONA, "You try to upload %s, but the copy-protection prevents you.\r\n", file->name);
      }

      file->file_worker = PERSONA->idnum;
      file->transfer_remaining = file->size;
      file->transferring_to_host = &matrix[PERSONA->in_host];
      send_to_icon(PERSONA, "You begin to upload %s to the hose.\r\n", file->name);
      if (IS_SENATOR(ch)) {
        send_to_icon(PERSONA, "(Upload will take %d ticks.)\r\n", file->transfer_remaining);
      }
      break;
  }
}

ACMD(do_redirect)
{
  if (!PERSONA) {
    send_to_char(ch, "You can't do that while hitching.\r\n");
    return;
  }
  if (matrix[PERSONA->in_host].type != HOST_RTG) {
    send_to_icon(PERSONA, "You can only perform this operation on an RTG.\r\n");
    return;
  }
  WAIT_STATE(ch, (int) (DECKING_WAIT_STATE_TIME));
  int success = system_test(PERSONA->in_host, ch, ACIFS_CONTROL, SOFT_CAMO, 0);
  if (success > 0) {
    for (int x = 0; x < DECKER->redirect; x++)
      if (DECKER->redirectedon[x] == PERSONA->in_host) {
        send_to_icon(PERSONA, "You have already redirected on this RTG.\r\n");
        return;
      }
    DECKER->redirect++;
    long *temp = new long[DECKER->redirect];
    for (int x = 0; x < DECKER->redirect - 1; x++)
      temp[x] = DECKER->redirectedon[x];
    temp[DECKER->redirect - 1] = PERSONA->in_host;
    DECKER->redirectedon = temp;
    send_to_icon(PERSONA, "You successfully redirect your datatrail.\r\n");
  } else
    send_to_icon(PERSONA, "Your program fails to run.\r\n");
}

ACMD(do_download)
{
  if (!PERSONA) {
    send_to_char(ch, "You can't do that while hitching.\r\n");
    return;
  }
  if (!DECKER->deck) {
    send_to_char(ch, "You don't have anywhere to download it to!\r\n");
    return;
  }
  if (!*argument) {
    send_to_icon(PERSONA, "Download what?\r\n");
    return;
  }
  WAIT_STATE(ch, (int) (DECKING_WAIT_STATE_TIME));
  struct matrix_file *soft = NULL;
  struct obj_data *target_deck = DECKER->deck;
  
  skip_spaces(&argument);
  // TODO: This might cause conflicts if multiple deckers have paydata on the host.
  if ((soft = get_matrix_file_in_list_vis(ch, argument, matrix[PERSONA->in_host].files)) && soft->found_by == PERSONA->idnum) {
    if (GET_CYBERDECK_FREE_STORAGE(target_deck) < soft->size) {
      send_to_icon(PERSONA, "You don't have enough storage memory to download that file.\r\n");
      return;
    } else {
      int success = system_test(PERSONA->in_host, ch, ACIFS_FILES, SOFT_READ, 0);
      if (soft->file_protection == 4)
        success -= soft->rating;
      if (success > 0) {
        if (soft->file_protection == FILE_PROTECTION_SCRAMBLED)
        {
          send_to_icon(PERSONA, "A Scramble IC blocks your attempts to download %s^n. You'll have to decrypt it first!\r\n", soft->name);
        } else if (soft->file_protection == FILE_PROTECTION_DATABOMB|| soft->file_protection == FILE_PROTECTION_PAVLOV)
        {
          int power = soft->rating;
          for (struct matrix_file *prog = DECKER->software; prog; prog = prog->next_file)
            if (prog->program_type == SOFT_ARMOR) {
              power -= prog->rating;
              break;
            }
          success = -success_test(DECKER->bod + MIN(GET_MAX_HACKING(ch), GET_REM_HACKING(ch)), power);
          GET_REM_HACKING(ch) = MAX(0, GET_REM_HACKING(ch) - GET_MAX_HACKING(ch));
          int dam = convert_damage(stage(success, soft->file_protection == FILE_PROTECTION_DATABOMB ? DEADLY : MODERATE));
          if (!dam)
            send_to_icon(PERSONA, "The %s explodes, but fails to cause damage to you.\r\n", soft->file_protection == FILE_PROTECTION_DATABOMB ? "Data Bomb" : "Pavlov");
          else {
            if (do_damage_persona(PERSONA, dam) || (ch && GET_POS(ch) <= POS_STUNNED)) {
              return;
            }
            if (PERSONA_CONDITION < 1) {
              send_to_icon(PERSONA, "The %s explodes, ripping your icon into junk logic\r\n", soft->file_protection == FILE_PROTECTION_DATABOMB ? "Data Bomb" : "Pavlov");
              dumpshock(PERSONA);
              return;
            } else
              send_to_icon(PERSONA, "The %s explodes, damaging your icon.\r\n", soft->file_protection == FILE_PROTECTION_DATABOMB ? "Data Bomb" : "Pavlov");
          }
          if (soft->file_protection == FILE_PROTECTION_DATABOMB) { 
            soft->file_protection = 0;
            if (PERSONA) {
              DECKER->tally += soft->rating;
              check_trigger(PERSONA->in_host, ch);
            }
          }
        } else {
          soft->transfer_remaining = soft->size;
          soft->transferring_to = target_deck;
          soft->file_worker = PERSONA->idnum;
          soft->dirty_bit = TRUE;
          send_to_icon(PERSONA, "You begin to download %s to %s.\r\n", soft->name, GET_OBJ_NAME(target_deck));
        }
      } else
        send_to_icon(PERSONA, "The file fails to download.\r\n");
      return;
    }
  }
  send_to_icon(PERSONA, "You can't seem to locate that file on this host.\r\n");
}

ACMD(do_run)
{
  if (!PERSONA) {
    send_to_char(ch, "You can't do that while hitching.\r\n");
    return;
  }
  struct matrix_file *soft;
  struct matrix_icon *temp;
  two_arguments(argument, buf, arg);
  for (soft = DECKER->software; soft; soft = soft->next_file)
    if (keyword_appears_in_file(buf, soft))
      break;
  if (soft) {
    WAIT_STATE(ch, (int) (DECKING_WAIT_STATE_TIME));
    switch (soft->program_type) {
      case SOFT_ATTACK:
        {
          struct matrix_icon *icon;

          if (!*arg) {
            send_to_icon(PERSONA, "You'll need to specify a target, like ^WRUN ATTACK DRONE^n.\r\n");
            return;
          }

          for (icon = matrix[PERSONA->in_host].icons; icon; icon = icon->next_in_host)
            if (has_spotted(PERSONA, icon) && keyword_appears_in_icon(arg, icon, TRUE, FALSE))
              break;

          if (!icon) {
            send_to_icon(PERSONA, "You can't see an icon named '%s' in this host.\r\n", arg);
            return;
          }

#ifdef ENABLE_PK
          if (icon->decker && icon->decker->ch && (!PRF_FLAGGED(ch, PRF_PKER) || !PRF_FLAGGED(icon->decker->ch, PRF_PKER))) {
            send_to_icon(PERSONA, "Both you and your target need to be flagged PK for that.\r\n");
            return;
          }
#else
          if (icon->decker && icon->decker->ch) {
            send_to_icon(PERSONA, "You can't perform aggressive actions against player-controlled icons.\r\n");
            return;
          }
#endif

          send_to_icon(PERSONA, "You start running %s^n against %s^n.\r\n", soft->name, decapitalize_a_an(icon->name));
          if (!PERSONA->fighting) {
            PERSONA->next_fighting = matrix[icon->in_host].fighting;
            matrix[icon->in_host].fighting = PERSONA;
            roll_matrix_init(PERSONA);
          }

          PERSONA->fighting = icon;
          if (!icon->fighting && IS_PROACTIVE(icon)) {
            icon->fighting = PERSONA;
            icon->next_fighting = matrix[icon->in_host].fighting;
            matrix[icon->in_host].fighting = icon;
            roll_matrix_init(icon);
          }

          order_list(matrix[icon->in_host].fighting);
          send_to_icon(icon, "%s^n begins to run an attack program aimed at you.\r\n", CAP(PERSONA->name));

          for (struct matrix_icon *ic = matrix[PERSONA->in_host].icons; ic; ic = temp) {
            temp = ic->next_in_host;
            if ((ic->ic.type == IC_TARBABY || ic->ic.type == IC_TARPIT) && ic->ic.subtype == 1) {
              send_to_icon(PERSONA, "%s^n's surface ripples and yawns open, reaching towards you!\r\n", CAP(ic->name));
              // We return after tarbaby succeeds, as it might delete the program we're using.
              if (tarbaby(soft, ch, ic))
                return;
            }
          }
        }
        break;
      case SOFT_MEDIC:
        if (PERSONA->condition == 10) {
          send_to_icon(PERSONA, "You're already at optimal condition.\r\n");
        } else if (soft->rating <= 0) {
          send_to_icon(PERSONA, "That program is no longer usable!\r\n");
        } else {
          for (struct matrix_icon *ic = matrix[PERSONA->in_host].icons; ic; ic = temp) {
            temp = ic->next_in_host;
            if ((ic->ic.type == IC_TARBABY || ic->ic.type == IC_TARPIT) && ic->ic.subtype == 1) {
              send_to_icon(PERSONA, "%s^n's surface ripples and yawns open, reaching towards you!\r\n", CAP(ic->name));
              // We return after tarbaby succeeds, as it might delete the program we're using.
              if (tarbaby(soft, ch, ic)) {
                send_to_icon(PERSONA, "You break off your medic run to regroup.\r\n");
                return;
              }
            }
          }
          send_to_icon(PERSONA, "You run a medic program.\r\n");
          int targ = 0;
          if (PERSONA->condition < 2)
            targ = 6;
          else if (PERSONA->condition < 7)
            targ = 5;
          else if (PERSONA->condition < 9)
            targ = 4;

          int skill = soft->rating + MIN(GET_MAX_HACKING(ch), GET_REM_HACKING(ch));
          int success = success_test(skill, targ);
          GET_REM_HACKING(ch) -= skill - soft->rating;

          if (success < 1)
            send_to_icon(PERSONA, "It fails to execute.\r\n");
          else {
            PERSONA->condition = MIN(10, success + PERSONA->condition);
            send_to_icon(PERSONA, "It repairs your icon.\r\n");
          }
          PERSONA->initiative -= 10;
          soft->rating--;
        }
        return;
    default:
      send_to_icon(PERSONA, "You don't need to manually run %s^n.\r\n", soft->name);
      break;
    }
  }
  else
    send_to_icon(PERSONA, "You don't seem to have that program loaded.\r\n");
}

void _decrypt_host_access(struct char_data *ch, rnum_t host_rnum) {
  int inhost = PERSONA->in_host;

  icon_from_host(PERSONA);
  icon_to_host(PERSONA, host_rnum);

  int success = system_test(PERSONA->in_host, ch, ACIFS_ACCESS, SOFT_DECRYPT, 0);

  if (success > 0) {
    if (matrix[host_rnum].stats[ACIFS_ACCESS][MTX_STAT_ENCRYPTED]) {
      send_to_icon(PERSONA, "You successfully decrypt that SAN.\r\n");
      matrix[host_rnum].stats[ACIFS_ACCESS][MTX_STAT_ENCRYPTED] = 0;
    } else {
      send_to_icon(PERSONA, "It doesn't appear to be encrypted.\r\n");
    }
  } else {
    send_to_icon(PERSONA, "You fail to decrypt that SAN.\r\n");
  }

  if (PERSONA) {
    icon_from_host(PERSONA);
    icon_to_host(PERSONA, inhost);
  }
}

ACMD(do_decrypt)
{
  if (!PERSONA) {
    send_to_char("You can't do that while hitching.\r\n", ch);
    return;
  }
  skip_spaces(&argument);
  if (!*argument) {
    send_to_char(ch, "%s what?\r\n", subcmd ? "Disarm" : "Decrypt");
    return;
  }
  WAIT_STATE(ch, (int) (DECKING_WAIT_STATE_TIME));
  {
    struct matrix_file *file = NULL;
    if ((file = get_matrix_file_in_list_vis(ch, argument, matrix[PERSONA->in_host].files)) && file->found_by == PERSONA->idnum) {
      if (!file->file_protection || (file->file_protection == FILE_PROTECTION_SCRAMBLED && subcmd) || (file->file_protection > 1 && !subcmd)) {
        send_to_icon(PERSONA, "There is no need to %s that file.\r\n", subcmd ? "disarm" : "decrypt");
        return;
      }
      int success = system_test(PERSONA->in_host, ch, ACIFS_FILES, subcmd ? SOFT_DEFUSE : SOFT_DECRYPT, 0);
      if (success > 0) {
        send_to_icon(PERSONA, "You successfully %s the file.\r\n", subcmd ? "disarm" : "decrypt");
        file->file_protection = 0;
      } else if (PERSONA) {
        send_to_icon(PERSONA, "You fail to %s the IC protecting that file.\r\n", subcmd ? "disarm" : "decrypt");
        if (file->file_protection == FILE_PROTECTION_SCRAMBLED)
          if (success_test(file->rating, GET_SKILL(ch, SKILL_COMPUTER)) > 0) {
            send_to_icon(PERSONA, "The Scramble IC destroys the file!\r\n");
            extract_matrix_file(file);
          }
      }
      return;
    }
  }
  if (is_abbrev(argument, "slave") || is_abbrev(argument, "files") || is_abbrev(argument, "access")) {
    int mode = 0;
    if (is_abbrev(argument, "access"))
      mode = ACIFS_ACCESS;
    else if (is_abbrev(argument, "files"))
      mode = ACIFS_FILES;
    else if (is_abbrev(argument, "slave"))
      mode = ACIFS_SLAVE;

    // If there's nothing to decrypt, there's nothing to succeed at.
    if (!matrix[PERSONA->in_host].stats[mode][MTX_STAT_ENCRYPTED]) {
      send_to_icon(PERSONA, "The %s subsystem doesn't seem to be encrypted.\r\n", mtx_subsystem_names[mode]);
      return;
    }

    int success = system_test(PERSONA->in_host, ch, mode, SOFT_DECRYPT, 0 );
    if (success > 0) {
      matrix[PERSONA->in_host].stats[mode][MTX_STAT_ENCRYPTED] = 0;
      send_to_icon(PERSONA, "You successfully decrypt the %s subsystem.\r\n", mtx_subsystem_names[mode]);
    } else {
      send_to_icon(PERSONA, "You fail to decrypt the %s subsystem.\r\n", mtx_subsystem_names[mode]);
      if (mode == ACIFS_FILES && PERSONA && PERSONA->decker && PERSONA->decker->ch) {
        // Paydata is destroyed on failed tests per SR3 p228.
        if (success_test(matrix[PERSONA->in_host].stats[mode][MTX_STAT_RATING], GET_SKILL(PERSONA->decker->ch, SKILL_COMPUTER)) > 0) {
          send_to_host(PERSONA->in_host, "A wash of static sweeps over the host, tearing away at potential paydata!\r\n", NULL, FALSE);

          // Remove the not-yet-found data.
          matrix[PERSONA->in_host].undiscovered_paydata = 0;
          matrix[PERSONA->in_host].ic_bound_paydata = 0;

          // Remove the already-found data.
          struct matrix_file *current, *next;
          for (current = matrix[PERSONA->in_host].files; current; current = next) {
            next = current->next_file;

            // Skip non-paydata.
            if (current->file_type != MATRIX_FILE_PAYDATA)
              continue;

            // The file is paydata-- delete it.
            extract_matrix_file(current);
          }
        }
      }
    }
    return;
  }

  if (is_abbrev(argument, "LTG")) {
    rnum_t parent_rnum = real_host(matrix[PERSONA->in_host].parent);
    if (parent_rnum < 0 || !(matrix[parent_rnum].type == HOST_LTG || matrix[parent_rnum].type == HOST_PLTG)) {
      send_to_char("This host isn't connected to an LTG.\r\n", ch);
    } else {
      _decrypt_host_access(ch, parent_rnum);
    }
    return;
  }

  for (struct exit_data *exit = matrix[PERSONA->in_host].exit; exit; exit = exit->next) {
    rnum_t host_rnum = real_host(exit->host);

    if (host_rnum < 0)
      continue;

    if (isname(argument, exit->addresses)) {
      _decrypt_host_access(ch, host_rnum);
      return;
    }
  }
  send_to_icon(PERSONA, "You can't seem to locate a file, connection, or subsystem named '%s'.\r\n", argument);
}

void send_active_program_list(struct char_data *ch) {
  if (DECKER->deck->obj_flags.extra_flags.IsSet(ITEM_EXTRA_OTAKU_RESONANCE)) {
    // We're an otaku using a living persona; we don't have active memory.
    for (struct obj_data *soft = DECKER->software; soft; soft = soft->next_content) {
      snprintf(buf, sizeof(buf), "%25s, Complex Form, %s-R^c%d^n\r\n", 
        GET_OBJ_NAME(soft),
        programs[GET_COMPLEX_FORM_PROGRAM(soft)].name,
        GET_OTAKU_PATH(ch) == OTAKU_PATH_CYBERADEPT ? GET_COMPLEX_FORM_RATING(soft) - 1 : GET_COMPLEX_FORM_RATING(soft)
      );

      // This cyberadept weirdness is because by the time we're in the matrix it's already received +1,
      // so we go down 1 to get real values
      // See above where it's -1'd.
      if (GET_OTAKU_PATH(ch) == OTAKU_PATH_CYBERADEPT) {
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " (^g%d^n)", GET_COMPLEX_FORM_RATING(soft));
      }

      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "\r\n");
      send_to_icon(PERSONA, buf);
    }
    return;
  }
  send_to_icon(PERSONA, "Active Memory Total:(^G%d^n) Free:(^R%d^n):\r\n", GET_OBJ_VAL(DECKER->deck, 2), DECKER->active);
  for (struct matrix_file *soft = DECKER->software; soft; soft = soft->next_file)
    send_to_icon(PERSONA, "%25s Rating: %2d\r\n", soft->name, soft->rating);
}

void send_storage_program_list(struct char_data *ch) {
  std::vector<struct obj_data*> devices = get_storage_devices(ch, TRUE);
  bool found_devices = FALSE;
  for (struct obj_data* device : devices) {
    if (device->obj_flags.extra_flags.IsSet(ITEM_EXTRA_OTAKU_RESONANCE)) {
      continue;
    }
    found_devices = TRUE;
    send_to_icon(PERSONA, "\r\n^c%s^n:\r\n", CAP(GET_OBJ_NAME(device)));
    for (struct matrix_file *soft = device->files; soft; soft = soft->next_file) {
      if (soft->program_type == MATRIX_FILE_PROGRAM)
        send_to_icon(PERSONA, "  %-30s^n Rating: %2d ^y(program)^n\r\n", soft->name, soft->rating);
      else if (soft->program_type == MATRIX_FILE_DESIGN)
        send_to_icon(PERSONA, "  %-30s^n Rating: %2d ^y(design)^n\r\n", soft->name, soft->rating);
      else if (soft->program_type == MATRIX_FILE_SOURCE_CODE)
        send_to_icon(PERSONA, "  %-30s^n Rating: %2d ^y(source)^n\r\n", soft->name, soft->rating);
      else
        send_to_icon(PERSONA, "%s^n\r\n", soft->name);
    }
    send_to_icon(PERSONA, "Storage Memory Total:(^G%d^n) Free:(^R%d^n):\r\n", get_device_total_memory(device),
               get_device_free_memory(device));
  }

  if (!found_devices) {
    send_to_icon(PERSONA, "\r\nNo Available Storage Memory\r\n");
    return;
  }
}

ACMD(quit_the_matrix_first) {
  send_to_char("You'll need to exit the Matrix to do that. You can either LOGOFF gracefully or DISCONNECT abruptly.\r\n", ch);
}

ACMD(do_software)
{
  int default_total = 0;

  if (PLR_FLAGGED(ch, PLR_MATRIX)) {
    if (!PERSONA) {
      send_to_char(ch, "You can't do that while hitching.\r\n");
      return;
    }
    if (!DECKER->deck) {
      send_to_char(ch, "You need a cyberdeck to store your software on!\r\n");
    } else {
      if (*argument) {
        skip_spaces(&argument);
        if (!strncmp(argument, "active", strlen(argument))) {
          send_active_program_list(ch);
        } else if (!strncmp(argument, "storage", strlen(argument))) {
          send_storage_program_list(ch);
        } else {
          send_to_char("Invalid filter. Please specify ACTIVE or STORAGE.\r\n", ch);
        }
        return;
      }
      send_active_program_list(ch);
      send_storage_program_list(ch);
    }
  } else {
    struct obj_data *cyberdeck = NULL, *cyber;

    // Error message sent in function.
    if (!get_datajack(ch, FALSE))
      return;
    
    for (cyber = ch->carrying; !cyberdeck && cyber; cyber = cyber->next_content)
      if (GET_OBJ_TYPE(cyber) == ITEM_CYBERDECK || GET_OBJ_TYPE(cyber) == ITEM_CUSTOM_DECK)
        cyberdeck = cyber;
    for (int i = 0; !cyberdeck && i < NUM_WEARS; i++)
      if (GET_EQ(ch, i) && (GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_CYBERDECK || GET_OBJ_TYPE(GET_EQ(ch, i)) == ITEM_CUSTOM_DECK))
        cyberdeck = GET_EQ(ch, i);

    if (!cyberdeck) {
      send_to_char(ch, "You have no cyberdeck to check the software on!\r\n");
      return;
    }

    if (GET_OBJ_TYPE(cyberdeck) == ITEM_CUSTOM_DECK && GET_CYBERDECK_IS_INCOMPLETE(cyberdeck)) {
      if (!display_cyberdeck_issues(ch, cyberdeck))
        return;
    }

    const char *format_string;
    if (PRF_FLAGGED(ch, PRF_SCREENREADER)) {
      format_string = "You jack into the deck and retrieve the following data:\r\n"
      "MPCP ^g%d^n\r\nActive Memory ^g%d^n\r\nStorage Memory ^R%d^n/^g%d^n\r\n"
      "Hardening ^g%d^n\r\nI/O ^g%d^n\r\nResponse Increase ^g%d^n\r\n";
    } else {
      format_string = "You jack into the deck and retrieve the following data:\r\n"
      "MPCP ^g%d^n - Active Memory ^g%d^n - Storage Memory ^R%d^n/^g%d^n\r\n"
      "Hardening ^g%d^n - I/O ^g%d^n - Response Increase ^g%d^n\r\n";
    }
    send_to_char(ch, format_string,
                 GET_CYBERDECK_MPCP(cyberdeck),
                 GET_CYBERDECK_ACTIVE_MEMORY(cyberdeck),
                 GET_CYBERDECK_TOTAL_STORAGE(cyberdeck) - GET_CYBERDECK_USED_STORAGE(cyberdeck),
                 GET_CYBERDECK_TOTAL_STORAGE(cyberdeck),
                 GET_CYBERDECK_HARDENING(cyberdeck),
                 GET_CYBERDECK_IO_RATING(cyberdeck),
                 GET_CYBERDECK_RESPONSE_INCREASE(cyberdeck));

    int bod = 0, sensor = 0, masking = 0, evasion = 0;
    for (struct obj_data *soft = cyberdeck->contains; soft; soft = soft->next_content)
      if (GET_OBJ_TYPE(soft) == ITEM_PROGRAM && GET_OBJ_VNUM(soft) != OBJ_BLANK_PROGRAM) {
        switch (GET_PROGRAM_TYPE(soft)) {
        case SOFT_BOD:
          bod = GET_PROGRAM_RATING(soft);
          break;
        case SOFT_SENSOR:
          sensor = GET_PROGRAM_RATING(soft);
          break;
        case SOFT_MASKING:
          masking = GET_PROGRAM_RATING(soft);
          break;
        case SOFT_EVASION:
          evasion = GET_PROGRAM_RATING(soft);
          break;
        }
      } else if (GET_OBJ_TYPE(soft) == ITEM_PART)
        switch (GET_PART_TYPE(soft)) {
        case PART_BOD:
          bod = GET_PART_RATING(soft);
          break;
        case PART_SENSOR:
          sensor = GET_PART_RATING(soft);
          break;
        case PART_MASKING:
          masking = GET_PART_RATING(soft);
          break;
        case PART_EVASION:
          evasion = GET_PART_RATING(soft);
          break;
        }
    if (PRF_FLAGGED(ch, PRF_SCREENREADER)) {
      send_to_char(ch, "Persona Programs:\r\n"
                   "Bod:     ^g%2d^n\r\nMasking: ^g%2d^n\r\n"
                   "Sensors: ^g%2d^n\r\nEvasion: ^g%2d^n\r\n", bod, masking, sensor, evasion);
    } else {
      send_to_char(ch, "Persona Programs:\r\n"
                   "Bod:     ^g%2d^n   Masking: ^g%2d^n\r\n"
                   "Sensors: ^g%2d^n   Evasion: ^g%2d^n\r\n", bod, masking, sensor, evasion);
    }

    strcpy(buf, "^cOther Software:^n\r\n");
    if (GET_OBJ_TYPE(cyberdeck) == ITEM_CUSTOM_DECK) {
      strcpy(buf2, "^cCustom Components:^n\r\n");
    } else {
      strcpy(buf2, "^cPersona Programs:^n\r\n");
    }

    bool has_other_software = FALSE;
    bool has_components = FALSE;

    const char *defaulted_string = PRF_FLAGGED(ch, PRF_SCREENREADER) ? "(defaulted)" : "*";
    for (struct obj_data *soft = cyberdeck->contains; soft; soft = soft->next_content) {
      if (GET_OBJ_TYPE(soft) == ITEM_PROGRAM && (GET_PROGRAM_TYPE(soft) > SOFT_SENSOR)) {
        has_other_software = TRUE;
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s^n Rating: %2d (%4d MP) %s\r\n",
                 get_obj_name_with_padding(soft, 40),
                 GET_PROGRAM_RATING(soft),
                 GET_PROGRAM_SIZE(soft),
                 GET_PROGRAM_IS_DEFAULTED(soft) ? defaulted_string : " ");
        if (GET_PROGRAM_IS_DEFAULTED(soft)) {
          default_total += GET_PROGRAM_SIZE(soft);
        }
      } else if (GET_OBJ_TYPE(soft) == ITEM_DECK_ACCESSORY && GET_DECK_ACCESSORY_TYPE(soft) == TYPE_FILE) {
        has_other_software = TRUE;
        snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%s^n (%4d MP)^n\r\n",
                 GET_OBJ_NAME(soft),
                 GET_PROGRAM_SIZE(soft));
      } else if (GET_OBJ_TYPE(soft) == ITEM_PART) {
        has_components = TRUE;
        snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "%s^n Type: ^c%-24s^n (rating ^c%d^n)\r\n",
                get_obj_name_with_padding(soft, 40),
                parts[GET_PART_TYPE(soft)].name,
                GET_PART_RATING(soft));
      } else if ((GET_OBJ_TYPE(cyberdeck) == ITEM_CYBERDECK) && (GET_OBJ_TYPE(soft) == ITEM_PROGRAM) && (GET_PROGRAM_TYPE(soft) <= SOFT_SENSOR)) {
        // Persona programs in store-bought decks are handled here as components
        has_components = TRUE;
        snprintf(ENDOF(buf2), sizeof(buf2) - strlen(buf2), "%s^n Rating: %2d (%4d MP)\r\n",
                 get_obj_name_with_padding(soft, 40),
                 GET_PROGRAM_RATING(soft),
                 GET_PROGRAM_SIZE(soft));
      }
    }

    if (has_other_software) {
      send_to_char(buf, ch);
      if (default_total) {
        send_to_char(ch, "%s%d^n/%d active memory used by defaulted programs.%s\r\n",
                     default_total > GET_CYBERDECK_ACTIVE_MEMORY(cyberdeck) ? "^R" : "",
                     default_total,
                     GET_CYBERDECK_ACTIVE_MEMORY(cyberdeck),
                     default_total > GET_CYBERDECK_ACTIVE_MEMORY(cyberdeck) ? " (not enough memory!)" : "");
      }
    }
    if (has_components)
      send_to_char(buf2, ch);
  }
}

void process_upload(struct matrix_icon *persona)
{
  if (!persona || !persona->decker) return;
  for (obj_data *device : get_storage_devices(persona->decker->ch, TRUE)) {
    for (struct matrix_file *file = device->files; file; file = file->next_file) {
      if (!file->transferring_to && !file->transferring_to_host) continue;

      if (file->transfer_remaining > 0) {
        // Require that we're on the same host as we started the upload on.
        if (file->transferring_to_host && persona->in_host != file->transferring_to_host->vnum) {
          send_to_icon(persona, "Your connection to the host was interrupted, so %s^n fails to upload.\r\n",
            file->name);
          file->transfer_remaining = 0;
          file->transferring_to_host = NULL;
          file->file_worker = 0;
          continue;
        }

        // Progress the download.
        file->transfer_remaining = MAX(0, file->transfer_remaining - persona->decker->io);
      }

      if(file->transfer_remaining <= 0) {
        if (file->transferring_to_host) {
          // We just completed an upload to a host.
          struct char_data *questor = NULL;

          if (file->quest_id) {
            // You must be grouped with the questor for the upload to complete.
            if (!(questor = ch_is_grouped_with_idnum(persona->decker->ch, file->quest_id)) || GET_QUEST(questor) < 0) {
              send_to_icon(persona, "%s^n failed to upload to the host: You're not grouped with the questor.\r\n", CAP(file->name));
              file->transfer_remaining = 0;
              file->transferring_to_host = NULL;
              file->file_worker = 0;
              continue;
            }
          }

          // Send the success message.
          send_to_icon(persona, "%s^n has finished uploading to the host.\r\n", CAP(file->name));

          // Move it onto the host.
          move_matrix_file_to(file, &matrix[persona->in_host]);

          // Make it seen by them.
          file->found_by = GET_IDNUM(persona->decker->ch);

          if (questor) {
            log_vfprintf("Questor found for upload.");
            bool potential_failure = FALSE;
            for (int i = 0; i < quest_table[GET_QUEST(questor)].num_objs; i++) {
              if (quest_table[GET_QUEST(questor)].obj[i].objective != QOO_UPLOAD)
                continue;

              if (file->idnum != quest_table[GET_QUEST(questor)].obj[i].vnum)
                continue;

              if (matrix[persona->in_host].vnum == quest_table[GET_QUEST(questor)].obj[i].o_data) {
                send_to_icon(persona, "You feel a small bit of satisfaction at having completed this part of %s%s^n job.\r\n",
                             questor == persona->decker->ch ? "your" : GET_CHAR_NAME(questor),
                             questor == persona->decker->ch ? "" : "'s");
                questor->player_specials->obj_complete[i] = 1;
                potential_failure = FALSE;
                break;
              } else {
                potential_failure = TRUE;
              }
            }

            if (potential_failure) {
              send_to_icon(persona, "Something doesn't seem quite right. You're suddenly unsure if this is the right host for the job.\r\n");
              snprintf(buf, sizeof(buf), "%s tried host %s (%ld) for job %ld, but it was incorrect. Quest recap rephrasing needed?",
                      GET_CHAR_NAME(persona->decker->ch),
                      matrix[persona->in_host].name,
                      matrix[persona->in_host].vnum,
                      quest_table[GET_QUEST(questor)].vnum
                    );
              mudlog(buf, questor, LOG_SYSLOG, TRUE);
            }
          }

          file->transfer_remaining = 0;
          file->transferring_to_host = NULL;
          file->file_worker = 0;
        } else if (file->transferring_to) {
          file->transfer_remaining = 0;
          file->transferring_to = NULL;
          file->file_worker = 0;
          if (file->size > persona->decker->active) {
            send_to_icon(persona, "%s crashes and fails to load due to insufficient active memory.\r\n", file->name);
            continue;
          }

          send_to_icon(persona, "%s has finished uploading to active memory.\r\n", file->name);

          struct matrix_file *active = clone_matrix_file(file);
          active->next_file = persona->decker->software;
          persona->decker->software = active;
          continue;
        }
      }
    }
  }
}

struct matrix_icon *find_icon_by_id(vnum_t idnum)
{
  struct matrix_icon *list = icon_list;
  for (;list; list = list->next)
    if (list->idnum == idnum)
      return list;
  return NULL;
}

#define HOST matrix[rnum]
void reset_host_paydata(rnum_t rnum) {
  int rand_result;
  switch (HOST.color) {
    case HOST_COLOR_BLUE:
      rand_result = number(1, 6) - 1; // Between 0-5, avg 2.5
      HOST.undiscovered_paydata = MIN(rand_result, MAX_PAYDATA_QTY_BLUE);
      HOST.ic_bound_paydata = 1;
      break;
    case HOST_COLOR_GREEN:
      rand_result = number(1, 6) + number(1, 6) - 2; // Between 0-10, avg 5
      HOST.undiscovered_paydata = MIN(rand_result, MAX_PAYDATA_QTY_GREEN);
      HOST.ic_bound_paydata = MIN(HOST.undiscovered_paydata, MAX(2, HOST.undiscovered_paydata * 1/5));
      HOST.undiscovered_paydata -= HOST.ic_bound_paydata;
      break;
    case HOST_COLOR_ORANGE:
      rand_result = number(1, 6) + number(1, 6); // Between 2-12, avg 7
      HOST.undiscovered_paydata = MIN(rand_result, MAX_PAYDATA_QTY_ORANGE);
      HOST.ic_bound_paydata = MIN(HOST.undiscovered_paydata, MAX(4, HOST.undiscovered_paydata * 1/3));
      HOST.undiscovered_paydata -= HOST.ic_bound_paydata;
      break;
    case HOST_COLOR_RED:
      rand_result = number(1, 6) + number(1, 6) + 2; // Between 4-14, avg 9
      HOST.undiscovered_paydata = MIN(rand_result, MAX_PAYDATA_QTY_RED_BLACK);
      HOST.ic_bound_paydata = MIN(HOST.undiscovered_paydata, MAX(6, HOST.undiscovered_paydata * 2/3));
      HOST.undiscovered_paydata -= HOST.ic_bound_paydata;
      break;
    case HOST_COLOR_BLACK:
      rand_result = number(1, 6) + number(1, 6) + 2;
      HOST.undiscovered_paydata = 1;
      HOST.ic_bound_paydata = rand_result - HOST.undiscovered_paydata;
      break;
    default:
      char oopsbuf[500];
      snprintf(oopsbuf, sizeof(oopsbuf), "SYSERR: Unknown host color %d for host %ld- can't generate paydata.", HOST.color, HOST.vnum);
      mudlog(oopsbuf, NULL, LOG_SYSLOG, TRUE);
      HOST.undiscovered_paydata = 0;
      HOST.ic_bound_paydata = 0;
  }
  // mudlog_vfprintf(NULL, LOG_SYSLOG, "PD:%ld-%s: %d UD, %d IC-B", host.vnum, host_color[host.color], host.undiscovered_paydata, host.ic_bound_paydata);
}

void matrix_update()
{
  PERF_PROF_SCOPE(pr_, __func__);
  for (rnum_t rnum = 1; rnum <= top_of_matrix; rnum++) {
    bool decker = FALSE;
    struct matrix_icon *nexticon;
    if (HOST.reset) {
      if (!--HOST.reset)
        HOST.alert = 0;
      else
        continue;
    }
    if (HOST.shutdown) {
      if (success_test(HOST.security, HOST.shutdown_mpcp) > 0) {
        send_to_host(HOST.rnum, HOST.shutdown_stop, NULL, FALSE);
        HOST.shutdown = 0;
        HOST.shutdown_mpcp = 0;
        HOST.shutdown_success = 0;
      } else if (!--HOST.shutdown) {
        struct matrix_file *nextfile = NULL;
        HOST.shutdown_mpcp = 0;
        HOST.shutdown_success = 0;
        HOST.alert = 3;
        while (HOST.icons)
          dumpshock(HOST.icons);
        if (HOST.files)
          for (struct matrix_file *obj = HOST.files; nextfile; obj = nextfile) {
            nextfile = obj->next_file;
            extract_matrix_file(obj);
          }
        HOST.reset = srdice() + srdice();
        continue;
      }
    }
    for (struct matrix_icon *icon = HOST.icons; icon; icon = nexticon) {
      nexticon = icon->next_in_host;
      if (!ICON_IS_IC(icon)) {
        process_upload(icon);
        decker = TRUE;
        if (!HOST.pass)
          GET_REM_HACKING(icon->decker->ch) = GET_HACKING(icon->decker->ch);
      } else {
        struct matrix_icon *icon2;
        for (icon2 = HOST.icons; icon2; icon2 = icon2->next_in_host)
          if (icon->ic.target == icon2->idnum)
            break;
        if (!icon2)
          extract_icon(icon);

      }
    }
    if (decker) {
      for (struct matrix_icon *icon = HOST.icons; icon; icon = icon->next_in_host)
        if (ICON_IS_IC(icon) && IS_PROACTIVE(icon) && !icon->fighting)
          for (struct matrix_icon *icon2 = HOST.icons; icon2; icon2 = icon2->next_in_host)
            if (icon->ic.target == icon2->idnum && icon2->decker) {
              if (icon->ic.type == IC_TRACE && icon->ic.subtype > 0) {
                if (!--icon->ic.subtype) {
                  icon2->decker->located = TRUE;
                  send_to_icon(icon2, "Alarms start to ring in your head as %s^n finds your location.\r\n", decapitalize_a_an(icon->name));
                  snprintf(buf, sizeof(buf), "%s located by Trace IC in host %ld (%s^g).", GET_CHAR_NAME(icon2->decker->ch), matrix[icon->in_host].vnum, matrix[icon->in_host].name);
                  mudlog(buf, icon2->decker->ch, LOG_GRIDLOG, TRUE);
                }
              } else if (icon->ic.type != IC_TRACE || (icon->ic.type == IC_TRACE && !icon2->decker->located)) {
                icon->fighting = icon2;
                icon->next_fighting = HOST.fighting;
                HOST.fighting = icon;
                roll_matrix_init(icon);
              }
              break;
            }
      matrix_file *next;
      for (matrix_file *file = HOST.files; file; file = next) {
        next = file->next_file;
        if (file->transfer_remaining) {
          struct matrix_icon *persona = find_icon_by_id(file->file_worker);
          if (!persona || persona->in_host != rnum) {
            file->file_worker = 0;
            file->transfer_remaining = 0;
            file->transferring_to = NULL;
            continue;
          }

          if (!file->transferring_to) {
            send_to_icon(persona, "%s^n has failed to download due to not having a download target. Please notify staff this is a bug.\r\n", CAP(file->name));
            file->file_worker = 0;
            file->transfer_remaining = 0;
            file->transferring_to = NULL;
            continue;
          }

          file->transfer_remaining -= persona->decker->io;
          if (file->transfer_remaining <= 0) {
            // Out of space? Inform them, reset the file, bail.
            if (!can_file_fit(file, file->transferring_to)) {
              send_to_icon(persona, "%s^n failed to download-- %s is out of space.\r\n", CAP(file->name), CAP(GET_OBJ_NAME(file->transferring_to)));
              file->file_worker = 0;
              file->transfer_remaining = 0;
              file->transferring_to = NULL;
              continue;
            }

            send_to_icon(persona, "%s^n has finished downloading to %s.\r\n", 
              file->name, GET_OBJ_NAME(file->transferring_to));
            move_matrix_file_to(file, file->transferring_to);
            file->found_by = 0;
            file->file_worker = 0;
            file->transfer_remaining = 0;
            file->transferring_to = NULL;
            
          }
        }
      }
    }
  }
}

// For things that should happen no more than once an hour.
void matrix_hour_update()
{
  PERF_PROF_SCOPE(pr_, __func__);
  extern struct time_info_data time_info;
  for (rnum_t rnum = 1; rnum <= top_of_matrix; rnum++) {
    bool decker = FALSE;

    // Paydara resets at midnight
    if (time_info.hours == 0) {
      if (HOST.type == HOST_DATASTORE && HOST.undiscovered_paydata <= 0) {
        reset_host_paydata(rnum);
      }
    }

    // Host alert state and subsystem encryption reviewed every 2 hours.
    // But only if no deckers in the area.
    if (time_info.hours % 2 == 0) {
      // Check the current host.
      for (struct matrix_icon *icon = HOST.icons; icon; icon = icon->next_in_host) {
        if (!ICON_IS_IC(icon)) {
          decker = TRUE;
          break;
        }
      }
      // Check surrounding hosts.
      struct host_data *adjacent_host = NULL;
      for (struct entrance_data *entrance = HOST.entrance; entrance && !decker; entrance = entrance->next) {
        if ((adjacent_host = entrance->host)) {
          for (struct matrix_icon *icon = adjacent_host->icons; icon; icon = icon->next_in_host) {
            if (!ICON_IS_IC(icon)) {
              decker = TRUE;
              break;
            }
          }
        }
      }

      // No deckers? Do the things.
      if (!decker) {
        // Alert active(2) to passive(1), passive to none(0).
        // Note: alert = 3 means host has been shut down, handled in matrix_update.
        if ((HOST.alert > 0) && (HOST.alert < 3)) {
          HOST.alert--;
        }

        // Re-encrypt subsystems.
        for (int x = ACIFS_ACCESS; x < NUM_ACIFS; x++) {
          if (HOST.stats[x][MTX_STAT_ENCRYPTED] != 0 && HOST.stats[x][MTX_STAT_ENCRYPTED] != 1) {
            char warnbuf[1000];
            snprintf(warnbuf, sizeof(warnbuf), "WARNING: %s mtx_stat_encrypted on %ld is %ld (must be 1 or 0)!",
                      acifs_strings[x],
                      HOST.vnum,
                      HOST.stats[x][MTX_STAT_ENCRYPTED]);
            mudlog(warnbuf, NULL, LOG_SYSLOG, TRUE);
            HOST.stats[x][MTX_STAT_ENCRYPTED] = 0;
          }

          if (HOST.stats[x][MTX_STAT_SCRAMBLE_IC_RATING] && HOST.stats[x][MTX_STAT_ENCRYPTED] == 0) {
            mudlog_vfprintf(NULL, LOG_GRIDLOG, "Host %ld's %s-subsystem has scramble-%ld and is not encrypted: re-encrypting.", HOST.vnum, mtx_subsystem_names[x], HOST.stats[x][MTX_STAT_SCRAMBLE_IC_RATING]);
            HOST.stats[x][MTX_STAT_ENCRYPTED] = 1;
          }
        }
      }
    }
  }
}

void matrix_violence()
{
  PERF_PROF_SCOPE(pr_, __func__);
  struct matrix_icon *temp, *icon;
  rnum_t rnum = 1;
  for (;rnum <= top_of_matrix; rnum++)
    if (HOST.fighting) {
      HOST.pass++;
      order_list(HOST.fighting);
      if (HOST.fighting->initiative <= 0) {
        HOST.pass = 0;
        for (icon = HOST.fighting; icon; icon = icon->next_fighting)
          roll_matrix_init(icon);
        order_list(HOST.fighting);
      }
      for (icon = HOST.fighting; icon; icon = icon->next_fighting) {
        if (icon->initiative > 0) {
          icon->initiative -= 10;
          if (icon->fighting) {
            if (icon->decker) {
              if (icon->evasion && !HOST.pass)
                icon->evasion--;
              if (!icon->fighting->evasion)
                matrix_fight(icon, icon->fighting);
            } else {
              switch(icon->evasion ? number(0, 5) : number(0, 10)) {
              case 1:
                parry_attack(icon);
                break;
              case 2:
                position_attack(icon);
                break;
              case 3:
              case 4:
                if (!icon->evasion)
                  evade_detection(icon);
                break;
              }
              if (icon->ic.targ_evasion) {
                if (!HOST.pass)
                  icon->ic.targ_evasion--;
              } else if (!icon->evasion)
                matrix_fight(icon, icon->fighting);
              else if (!HOST.pass)
                icon->evasion--;
            }
          } else
            REMOVE_FROM_LIST(icon, matrix[icon->in_host].fighting, next_fighting);
        }
      }
    } else {
      HOST.pass = 0;
    }
}

ACMD(do_crash)
{
  if (!PERSONA) {
    send_to_char(ch, "You can't do that while hitching.\r\n");
    return;
  }
  if (matrix[PERSONA->in_host].shutdown) {
    send_to_icon(PERSONA, "Someone has already initiated a shutdown.\r\n");
    return;
  }
  WAIT_STATE(ch, (int) (DECKING_WAIT_STATE_TIME));
  int success = system_test(PERSONA->in_host, ch, ACIFS_CONTROL, SOFT_CRASH, 0);
  if (success > 0) {
    matrix[PERSONA->in_host].shutdown_success = success;
    matrix[PERSONA->in_host].shutdown_mpcp = DECKER->mpcp;
    int x = matrix[PERSONA->in_host].security / 2;
    while (x > 0) {
      x--;
      matrix[PERSONA->in_host].shutdown += number(1, 6);
    }
    send_to_host(PERSONA->in_host, matrix[PERSONA->in_host].shutdown_start, NULL, FALSE);
    send_to_host(PERSONA->in_host, "\r\n", NULL, FALSE);
  } else
    send_to_icon(PERSONA, "Your program fails to run.\r\n");
}
ACMD(do_matrix_scan)
{
  skip_spaces(&argument);
  if (!PERSONA) {
    send_to_char(ch, "You can't do that while hitching.\r\n");
    return;
  }
  if (!*argument) {
    send_to_icon(PERSONA, "What do you wish to scan?\r\n");
    return;
  }
  WAIT_STATE(ch, (int) (DECKING_WAIT_STATE_TIME));
  for (struct matrix_icon *ic = matrix[PERSONA->in_host].icons; ic; ic = ic->next_in_host)
    if (ic->decker && has_spotted(PERSONA, ic) && keyword_appears_in_icon(argument, ic, TRUE, FALSE)) {
      struct matrix_file *soft;
      int target = ic->decker->masking;
      for (soft = ic->decker->software; soft; soft = soft->next_file)
        if (soft->program_type == SOFT_SLEAZE) {
          target += soft->rating;
          break;
        }
      for (soft = DECKER->software; soft; soft = soft->next_file)
        if (soft->program_type == SOFT_SCANNER) {
          target -= soft->rating;
          break;
        }
      int skill = get_skill(ch, SKILL_COMPUTER, target) + MIN(GET_MAX_HACKING(ch), GET_REM_HACKING(ch));
      int success = success_test(skill, target);
      GET_REM_HACKING(ch) = MAX(0, GET_REM_HACKING(ch) - GET_MAX_HACKING(ch));
      if (success > 7) {
        send_to_icon(PERSONA, "MPCP-%d %d/%d/%d/%d Response: %d Condition: %d "
                     "Privileges: None MXP: %ld\r\n", ic->decker->mpcp,
                     ic->decker->bod, ic->decker->evasion, ic->decker->masking,
                     ic->decker->sensor, ic->decker->response, ic->condition, ic->decker->mxp);
      } else if (success > 0) {
        int found[] = { 0, 0, 0, 0, 0, 0, 0, 0};
        for (int current;success; success--) {
          current = number(0, 7);
          int start = current;
          while (found[current]) {
            current++;
            if (current > 7)
              current = 0;
            if (current == start)
              break;
          }
          found[current] = 1;
        }
        snprintf(buf, sizeof(buf), "MPCP-");
        if (found[0])
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%d", ic->decker->mpcp);
        else
          snprintf(buf, sizeof(buf), "0");
        if (found[1])
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), " %d/", ic->decker->bod);
        else
          strcat(buf, " 0/");
        if (found[2])
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%d/", ic->decker->evasion);
        else
          strcat(buf, "0/");
        if (found[3])
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%d/", ic->decker->masking);
        else
          strcat(buf, "0/");
        if (found[4])
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%d", ic->decker->sensor);
        else
          strcat(buf, "0");
        strcat(buf, " Response: ");
        if (found[5])
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%d", ic->decker->response);
        else
          strcat(buf, "0");
        strcat(buf, " Condition: ");
        if (found[6])
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%d", ic->condition);
        else
          strcat(buf, "0");
        strcat(buf, " Privileges: None MXP: \r\n");
        if (found[7])
          snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%ld", ic->decker->mxp);
        else
          strcat(buf, "Not Found");
        strcat(buf, "\r\n");
        send_to_icon(PERSONA, buf);

      } else
        send_to_icon(PERSONA, "You don't find out anything new.\r\n");
      return;
    }
  send_to_icon(PERSONA, "You don't see anything named '%s' here.", argument);
}
ACMD(do_abort)
{
  if (!PERSONA) {
    send_to_char(ch, "You can't do that while hitching.\r\n");
    return;
  }
  if (!matrix[PERSONA->in_host].shutdown) {
    send_to_icon(PERSONA, "There is no shutdown to abort.\r\n");
    return;
  }
  WAIT_STATE(ch, (int) (DECKING_WAIT_STATE_TIME));
  int success = system_test(PERSONA->in_host, ch, ACIFS_CONTROL, SOFT_SWERVE, 0);
  success /= 2;
  if (success > matrix[PERSONA->in_host].shutdown_success) {
    send_to_host(PERSONA->in_host, matrix[PERSONA->in_host].shutdown_stop, NULL, FALSE);
    matrix[PERSONA->in_host].shutdown = 0;
    matrix[PERSONA->in_host].shutdown_success = 0;
    matrix[PERSONA->in_host].shutdown_mpcp = 0;
  } else if (success > 0) {
    send_to_icon(PERSONA, "You manage to prolong the shutdown.\r\n");
    matrix[PERSONA->in_host].shutdown += success;
  } else
    send_to_icon(PERSONA, "You fail to prolong the shutdown.\r\n");
}

ACMD(do_talk)
{
  // If they trigger automod with this, bail out.
  if (check_for_banned_content(argument, ch))
    return;

  skip_spaces(&argument);
  if (!PERSONA)
    send_to_char(ch, "You can't do that while hitching.\r\n");
  else if (!DECKER->phone->dest)
    send_to_icon(PERSONA, "You don't have a call connected.\r\n");
  else if (!DECKER->phone->dest->connected)
    send_to_icon(PERSONA, "They haven't answered yet.\r\n");
  else if (!*argument)
    send_to_icon(PERSONA, "Say what?\r\n");
  else {
    struct char_data *tch = NULL;
    bool tch_is_matrix = FALSE;

    snprintf(buf, sizeof(buf), "^Y$v on the other end of the line says, \"%s\"", argument);
    snprintf(buf2, sizeof(buf2), "^YYou say, \"%s\"\r\n", argument);
    send_to_char(buf2, ch);
    store_message_to_history(ch->desc, COMM_CHANNEL_PHONE, buf2);

    if (DECKER->phone->dest->persona && DECKER->phone->dest->persona->decker && DECKER->phone->dest->persona->decker->ch) {
      tch = DECKER->phone->dest->persona->decker->ch;
      tch_is_matrix = TRUE;
    } else {
      tch = get_obj_possessor(DECKER->phone->dest->phone);
    }

    if (tch) {
      // TODO: Add pseudolanguage code here. Also requires adding ability to switch language in matrix.
      if (tch_is_matrix) {
        // Re-send it to the Matrix folks.
        store_message_to_history(tch->desc, COMM_CHANNEL_PHONE, buf);
        send_to_char(buf, tch);
      } else {
        store_message_to_history(tch->desc, COMM_CHANNEL_PHONE, act(buf, FALSE, ch, 0, tch, TO_VICT));
      }
    }
  }
}

ACMD(do_comcall)
{
  struct char_data *tch;
  skip_spaces(&argument);
  if (!PERSONA) {
    send_to_char(ch, "You can't do that while hitching.\r\n");
    return;
  }

  if (!DECKER->phone) {
    send_to_char("You don't have a phone available.\r\n", ch);
    return;
  }

  if (!subcmd) {
    send_to_char(ch, "Your commlink phone number is %08d.\r\n", DECKER->phone->number);

    if (DECKER->phone->dest) {
      if (DECKER->phone->dest->connected && DECKER->phone->connected)
        send_to_char(ch, "Connected to: %d\r\n", DECKER->phone->dest->number);
      else if (!DECKER->phone->dest->connected)
        send_to_char(ch, "Calling: %d\r\n", DECKER->phone->dest->number);
      else
        send_to_char(ch, "Incoming call from: %08d\r\n", DECKER->phone->dest->number);
    }
    return;
  }

  if (subcmd == SCMD_ANSWER) {
    if (DECKER->phone->connected) {
      send_to_icon(PERSONA, "But you already have a call connected!\r\n");
      return;
    }
    if (!DECKER->phone->dest) {
      send_to_icon(PERSONA, "You have no incoming connections.\r\n");
      return;
    }
    if (DECKER->phone->dest->persona)
      send_to_icon(DECKER->phone->dest->persona, "The call is answered.\r\n");
    else {
      tch = DECKER->phone->dest->phone->carried_by;
      if (!tch)
        tch = DECKER->phone->dest->phone->worn_by;
      if (!tch && DECKER->phone->dest->phone->in_obj)
        tch = DECKER->phone->dest->phone->in_obj->carried_by;
      if (!tch && DECKER->phone->dest->phone->in_obj)
        tch = DECKER->phone->dest->phone->in_obj->worn_by;
      if (tch)
        send_to_char("The phone is answered.\r\n", tch);
    }
    send_to_icon(PERSONA, "You establish a connection.\r\n");
    DECKER->phone->connected = TRUE;
    DECKER->phone->dest->connected = TRUE;

  } else if (subcmd == SCMD_HANGUP) {
    if (!DECKER->phone->dest) {
      send_to_icon(PERSONA, "But you don't have a direct connection established.\r\n");
      return;
    }
    if (DECKER->phone->dest->persona)
      send_to_icon(DECKER->phone->dest->persona, "The flashing phone icon fades from view.\r\n");
    else {
      tch = DECKER->phone->dest->phone->carried_by;
      if (!tch)
        tch = DECKER->phone->dest->phone->worn_by;
      if (!tch && DECKER->phone->dest->phone->in_obj)
        tch = DECKER->phone->dest->phone->in_obj->carried_by;
      if (!tch && DECKER->phone->phone->in_obj)
        tch = DECKER->phone->dest->phone->in_obj->worn_by;
      if (tch) {
        if (DECKER->phone->dest->connected)
          send_to_char("The phone is hung up from the other side.\r\n", tch);
        else
          send_to_char("Your phone stops ringing.\r\n", tch);
      }
    }
    DECKER->phone->connected = FALSE;
    DECKER->phone->dest->dest = NULL;
    DECKER->phone->dest->connected = FALSE;
    DECKER->phone->dest = NULL;
    send_to_icon(PERSONA, "You cut the connection.\r\n");
  } else if (subcmd == SCMD_RING) {
    if (DECKER->phone->dest) {
      send_to_icon(PERSONA, "You already have a call connected.\r\n");
      return;
    }

    int ring;
    any_one_arg(argument, arg);
    if (!*arg)
      send_to_icon(PERSONA, "Ring what number?");
    else if (!(ring = atoi(arg)))
      send_to_icon(PERSONA, "That's not a valid number.\r\n");
    else if (matrix[PERSONA->in_host].type != HOST_RTG)
      send_to_icon(PERSONA, "You can only perform that action on an RTG.\r\n");
    else {
      struct phone_data *k;
      for (k = phone_list; k; k = k->next)
        if (k->number == ring && k != DECKER->phone && k->rtg == matrix[PERSONA->in_host].vnum)
          break;
      if (!k) {
        send_to_icon(PERSONA, "You can't seem to find that commcode.\r\n");
        return;
      }
      int success = system_test(PERSONA->in_host, ch, ACIFS_FILES, SOFT_COMMLINK, 0);
      if (success > 0) {
        if (k->dest) {
          send_to_icon(PERSONA, "That line is already busy.\r\n");
          return;
        }
        DECKER->phone->dest = k;
        DECKER->phone->connected = TRUE;
        k->dest = DECKER->phone;

        if (k->persona)
          send_to_icon(k->persona, "A small telephone symbol blinks in the top left of your view.\r\n");
        else {
          tch = k->phone->carried_by;
          if (!tch)
            tch = k->phone->worn_by;
          if (!tch && k->phone->in_obj)
            tch = k->phone->in_obj->carried_by;
          if (!tch && k->phone->in_obj)
            tch = k->phone->in_obj->worn_by;
          if (tch) {
            if (GET_POS(tch) == POS_SLEEPING) {
              if (success_test(GET_WIL(tch), 4)) {
                GET_POS(tch) = POS_RESTING;
                send_to_char("You are woken by your phone ringing.\r\n", tch);
              } else if (!GET_OBJ_VAL(k->phone, 3)) {
                act("$n's phone rings.", FALSE, tch, 0, 0, TO_ROOM);
                return;
              } else
                return;
            }
            if (!GET_OBJ_VAL(k->phone, 3)) {
              send_to_char("Your phone rings.\r\n", tch);
              act("$n's phone rings.", FALSE, tch, NULL, NULL, TO_ROOM);
            } else {
              if (GET_OBJ_TYPE(k->phone) == ITEM_CYBERWARE || success_test(GET_INT(tch), 4))
                send_to_char("You feel your phone ring.\r\n", tch);
            }
          } else {
              snprintf(buf, sizeof(buf), "%s^n rings.", GET_OBJ_NAME(k->phone));
            if (k->phone->in_veh) {
              send_to_veh(buf, k->phone->in_veh, ch, TRUE);
            } else {
              send_to_room(buf, k->phone->in_room);
            }
          }
        }
        send_to_icon(PERSONA, "It begins to ring.\r\n");
      } else
        send_to_icon(PERSONA, "You can't seem to connect.\r\n");
    }
  }
}
ACMD(do_tap)
{}


ACMD(do_restrict)
{
  struct matrix_icon *targ;
  if (!PERSONA) {
    send_to_char(ch, "You can't do that while hitching.\r\n");
    return;
  }
  two_arguments(argument, buf, arg);
  if (!*buf || !*arg) {
    send_to_icon(PERSONA, "You need to specify target and type of restriction.\r\n");
    return;
  }
  WAIT_STATE(ch, (int) (DECKING_WAIT_STATE_TIME));
  int success, detect = 0;
  for (targ = matrix[PERSONA->in_host].icons; targ; targ = targ->next_in_host)
    if (targ->decker && has_spotted(PERSONA, targ) && keyword_appears_in_icon(arg, targ, TRUE, FALSE))
      break;
  if (!targ) {
    send_to_icon(PERSONA, "You can't find that icon here.\r\n");
    return;
  }

#ifdef ENABLE_PK
  if (targ->decker && targ->decker->ch && (!PRF_FLAGGED(ch, PRF_PKER) || !PRF_FLAGGED(targ->decker->ch, PRF_PKER))) {
    send_to_icon(PERSONA, "Both you and your target need to be flagged PK for that.\r\n");
    return;
  }
#else
  if (targ->decker && targ->decker->ch) {
    send_to_icon(PERSONA, "You can't perform aggressive actions against player-controlled icons.\r\n");
    return;
  }
#endif

  detect = get_detection_factor(ch);

  if (is_abbrev(buf, "detection")) {
    success = system_test(PERSONA->in_host, ch, ACIFS_CONTROL, SOFT_VALIDATE, detect);
    if (success > 0) {
      targ->decker->res_det += success;
      send_to_icon(PERSONA, "You successfully restrict their detection factor.\r\n");
    } else {
      send_to_icon(PERSONA, "You fail to restrict their detection factor.\r\n");
    }
  } else if (is_abbrev(buf, "tests")) {
    success = system_test(PERSONA->in_host, ch, ACIFS_CONTROL, SOFT_VALIDATE, detect);
    if (success > 0) {
      targ->decker->res_test += success;
      send_to_icon(PERSONA, "You successfully restrict their system tests.\r\n");
    } else {
      send_to_icon(PERSONA, "You fail to restrict their system tests.\r\n");
    }
  } else
    send_to_icon(PERSONA, "You must specify wether you want to restrict their detection factor or systems test.");
}

ACMD(do_trace)
{
  long addr;
  if (!PERSONA)
    send_to_char(ch, "You can't do that while hitching.\r\n");
  else if (!*argument)
    send_to_icon(PERSONA, "What MXP address do you wish to trace?\r\n");
  else if (!(addr = atoi(argument)))
    send_to_icon(PERSONA, "Invalid MXP address.\r\n");
  else if (matrix[PERSONA->in_host].type != HOST_LTG)
    send_to_icon(PERSONA, "You can only perform this action on an LTG.\r\n");
  else {
    WAIT_STATE(ch, (int) (DECKING_WAIT_STATE_TIME));
    int success = system_test(PERSONA->in_host, ch, ACIFS_INDEX, SOFT_BROWSE, 0);
    if (success > 0) {
      for (struct matrix_icon *icon = icon_list; icon; icon = icon->next) {
        if (icon->decker && icon->decker->mxp == addr) {
          if (matrix[PERSONA->in_host].vnum == icon->decker->phone->rtg)
            snprintf(buf, sizeof(buf), "Your search returns:\r\nOriginating Grid: %s\r\nSerial number: %ld\r\n",
                    matrix[real_host(icon->decker->phone->rtg)].name, icon->decker->phone->rtg * icon->decker->phone->number);
          else
            snprintf(buf, sizeof(buf), "Your search returns:\r\nJackpoint Location: %s\r\n", icon->decker->ch->in_room->address);
          send_to_icon(PERSONA, buf);
          return;
        }
      }
    }
    send_to_icon(PERSONA, "You fail to get any information on that request.\r\n");
  }
}

ACMD(do_default)
{
  struct obj_data *cyberdeck = NULL, *soft, *cyber;
  if (!*argument) {
    send_to_char(ch, "Load what program by default?\r\n");
    return;
  }
  skip_spaces(&argument);

  // Error message sent in function.
  if (!get_datajack(ch, FALSE))
    return;
  
  for (cyber = ch->carrying; !cyberdeck && cyber; cyber = cyber->next_content)
    if (GET_OBJ_TYPE(cyber) == ITEM_CYBERDECK || GET_OBJ_TYPE(cyber) == ITEM_CUSTOM_DECK)
      cyberdeck = cyber;
  for (int i = 0; !cyberdeck && i < NUM_WEARS; i++)
    if (GET_EQ(ch, i) && (GET_OBJ_TYPE(GET_EQ(ch,i )) == ITEM_CYBERDECK || GET_OBJ_TYPE(GET_EQ(ch,i )) == ITEM_CUSTOM_DECK))
      cyberdeck = GET_EQ(ch, i);

  if (!cyberdeck) {
    send_to_char(ch, "You have no cyberdeck to check the software on!\r\n");
    return;
  }

  if (GET_CYBERDECK_MPCP(cyberdeck) == 0 || GET_CYBERDECK_IS_INCOMPLETE(cyberdeck)) {
    // returns TRUE if it fixed it, FALSE otherwise.
    if (!display_cyberdeck_issues(ch, cyberdeck))
      return;
  }

  for (soft = cyberdeck->contains; soft; soft = soft->next_content)
    if (GET_OBJ_TYPE(soft) == ITEM_PROGRAM && GET_OBJ_VAL(soft, 0) > SOFT_SENSOR && (keyword_appears_in_obj(argument, soft)))
      break;
  if (!soft) {
    send_to_char(ch, "You don't have that program installed.\r\n");
    return;
  }
  if (GET_OBJ_VAL(soft, 4)) {
    GET_OBJ_VAL(soft, 4)--;
    send_to_char(ch, "%s^n will no longer load upon connection.\r\n", CAP(GET_OBJ_NAME(soft)));
  } else {
    GET_OBJ_VAL(soft, 4)++;
    send_to_char(ch, "%s^n will now load upon connection.\r\n", CAP(GET_OBJ_NAME(soft)));
  }
}

ACMD(do_reveal)
{
  if (!PERSONA)
    send_to_char(ch, "You can't do that while hitching.\r\n");
  else {
    send_to_icon(PERSONA, "You lower your masking for a brief moment, broadcasting your location to the host.\r\n");
    for (struct matrix_icon *icon = matrix[PERSONA->in_host].icons; icon; icon = icon->next_in_host)
      if (icon != PERSONA && icon->decker && !has_spotted(icon, PERSONA)) {
        make_seen(icon, PERSONA->idnum);
        send_to_icon(icon, "%s^n fades into the host.\r\n", CAP(PERSONA->name));
      }
  }
}

ACMD(do_create)
{
  if (CH_IN_COMBAT(ch)) {
    send_to_char("You can't create things while fighting!\r\n", ch);
    return;
  }
  if (IS_NPC(ch)) {
    send_to_char("Sure...you do that.\r\n", ch);
    return;
  }
  if (IS_CARRYING_N(ch) >= CAN_CARRY_N(ch)) {
    send_to_char("Your arms are already full!\r\n", ch);
    return;
  }
  if (IS_WORKING(ch)) {
    send_to_char(TOOBUSY, ch);
    return;
  }
  argument = any_one_arg(argument, buf1);

  if (is_abbrev(buf1, "complex form"))
  {
    if (!IS_OTAKU(ch)) {
      send_to_char("Everyone knows that otaku aren't real, chummer.\r\n", ch);
      return;
    } else if (!GET_SKILL(ch, SKILL_COMPUTER)) {
      send_to_char("You must learn computer skills to create complex forms.\r\n", ch);
      return;
    }
    create_complex_form(ch);
  }

  else if (is_abbrev(buf1, "program")) {
    if (!GET_SKILL(ch, SKILL_COMPUTER)) {
      send_to_char("You must learn computer skills to create programs.\r\n", ch);
      return;
    }
    create_program(ch);
  }

  else if (is_abbrev(buf1, "part")) {
    if (!GET_SKILL(ch, SKILL_BR_COMPUTER)) {
      send_to_char("You must learn computer B/R skills to create parts.\r\n", ch);
      return;
    }
    create_part(ch);
  }

  else if (is_abbrev(buf1, "deck") || is_abbrev(buf1, "cyberdeck"))
    create_deck(ch);

  else if (is_abbrev(buf1, "ammo") || is_abbrev(buf1, "ammunition"))
    create_ammo(ch);

  else if (is_abbrev(buf1, "spell")) {
    if (!(GET_SKILL(ch, SKILL_SPELLDESIGN) || GET_SKILL(ch, SKILL_SORCERY))) {
      send_to_char("You must learn Spell Design or Sorcery to create a spell.\r\n", ch);
      return;
    }

    struct obj_data *library = ch->in_room ? ch->in_room->contents : ch->in_veh->contents;
    for (;library; library = library->next_content)
      if (GET_OBJ_TYPE(library) == ITEM_MAGIC_TOOL &&
          ((GET_TRADITION(ch) == TRAD_SHAMANIC
            && GET_OBJ_VAL(library, 0) == TYPE_LODGE && GET_OBJ_VAL(library, 3) == GET_IDNUM(ch)) ||
           (GET_TRADITION(ch) != TRAD_SHAMANIC && GET_OBJ_VAL(library, 0) == TYPE_LIBRARY_SPELL)))
        break;
    if (!library) {
      send_to_char(ch, "You don't have the right tools here to create a spell. You'll need to %s.\r\n",
                   GET_TRADITION(ch) == TRAD_SHAMANIC ? "build a lodge" : "get a library");
      return;
    }
    ch->desc->edit_number2 = GET_OBJ_VAL(library, 1);
    create_spell(ch);
  }

  else if (is_abbrev(buf1, "art") || is_abbrev(buf1, "book") || is_abbrev(buf1, "note") || is_abbrev(buf1, "business card") || is_abbrev(buf1, "card") || is_abbrev(buf1, "painting") || is_abbrev(buf1, "drawing")) {
    create_art(ch);
  }

  else if (is_abbrev(buf1, "pets")) {
    create_pet(ch);
  }

#ifdef TEMPORARY_COMPILATION_GUARD
  else if (is_abbrev(buf1, "secret_container")) {
    create_secret_container(ch);
  }

  else if (is_abbrev(buf1, "secret_contents")) {
    create_secret_contents(ch);
  }
#endif

  else {
    send_to_char("You can only create programs, parts, decks, ammunition, spells, complex forms, art, and pets.\r\n", ch);
    return;
  }
}

ACMD(do_matrix_max)
{
  if (!PERSONA)
    send_to_char(ch, "You can't do that while hitching.\r\n");
  else if (!*argument)
    send_to_char(ch, "Your maximum hacking pool used per action is %d.\r\n", GET_MAX_HACKING(ch));
  else if (!DECKER->asist[0])
    send_to_char("You can't use hacking pool while running a cold ASIST.\r\n", ch);
  else {
    int i = atoi(argument);
    int cap = MIN(GET_HACKING(ch), GET_SKILL(ch, SKILL_COMPUTER));
    if (i > cap || i < 0)
      send_to_char(ch, "You must set your max hacking pool to between 0 and %d.\r\n", cap);
    else {
      GET_MAX_HACKING(ch) = i;
      send_to_char(ch, "You will now use a maximum of %d hacking pool per action.\r\n", GET_MAX_HACKING(ch));
    }
  }

}

ACMD(do_asist)
{
  if (!PERSONA)
    send_to_char("You can't do that while hitching.\r\n", ch);
  else if (PERSONA->type == ICON_LIVING_PERSONA)
    send_to_char("You can't switch ASIST modes while using a living persona.\r\n", ch);
  else if (!DECKER->asist[1])
    send_to_char("You can't switch ASIST modes with a cold ASIST interface.\r\n", ch);
  else if (DECKER->asist[0]) {
    DECKER->asist[0] = 0;
    send_to_char("You switch to cold ASIST mode.\r\n", ch);
    GET_MAX_HACKING(ch) = 0;
    DECKER->response = 0;
  } else {
    send_to_char("You switch to hot ASIST mode.\r\n", ch);
    DECKER->asist[0] = 1;
    DECKER->response = GET_OBJ_VAL(DECKER->deck, 6);
    GET_MAX_HACKING(ch) = (int)(GET_HACKING(ch) / 3);
  }
}

// Formats and prints a message to the user about why their custom deck won't work.
bool display_cyberdeck_issues(struct char_data *ch, struct obj_data *cyberdeck) {
  if (GET_CYBERDECK_MPCP(cyberdeck) == 0) {
    send_to_char(ch, "The faint smell of burned MPCP tells you that %s^n is going to need some repairs first.\r\n", GET_OBJ_NAME(cyberdeck));
    return FALSE;
  }

  if (GET_OBJ_TYPE(cyberdeck) == ITEM_CUSTOM_DECK && GET_CYBERDECK_IS_INCOMPLETE(cyberdeck)) {
    bool has_mpcp = FALSE, has_active = FALSE, has_bod = FALSE, has_sensor = FALSE, has_io = FALSE, has_interface = FALSE;
    for (struct obj_data *part = cyberdeck->contains; part; part = part->next_content) {
      has_mpcp |= (GET_PART_TYPE(part) == PART_MPCP);
      has_active |= (GET_PART_TYPE(part) == PART_ACTIVE);
      has_bod |= (GET_PART_TYPE(part) == PART_BOD);
      has_sensor |= (GET_PART_TYPE(part) == PART_SENSOR);
      has_io |= (GET_PART_TYPE(part) == PART_IO);
      has_interface |= (GET_PART_TYPE(part) == PART_MATRIX_INTERFACE);
    }
    bool first = TRUE;
    snprintf(buf, sizeof(buf), "%s^n isn't in working condition, so it won't power on. It needs ", capitalize(GET_OBJ_NAME(cyberdeck)));
    if (!has_mpcp) {
      strcat(buf, "an MPCP chip");
      first = FALSE;
    }
    if (!has_active) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%san active memory module", first ? "" : (has_bod && has_sensor && has_io && has_interface) ? " and " : ", ");
      first = FALSE;
    }
    if (!has_bod) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%sa bod chip", first ? "" : (has_sensor && has_io && has_interface) ? " and " : ", ");
      first = FALSE;
    }
    if (!has_sensor) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%sa sensor chip", first ? "" : (has_io && has_interface) ? " and " : ", ");
      first = FALSE;
    }
    if (!has_io) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%san I/O module", first ? "" : (has_interface) ? " and " : ", ");
      first = FALSE;
    }
    if (!has_interface) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "%sa Matrix interface", first ? "" : " and ");
      first = FALSE;
    }

    // If we get here and haven't sent anything, something is wrong.
    if (first) {
      snprintf(buf2, sizeof(buf2), "SYSERR: Cyberdeck '%s^n' held by '%s' identifies itself as being incomplete, but has all necessary parts. Autofixing.",
              GET_OBJ_NAME(cyberdeck), GET_CHAR_NAME(ch));
      mudlog(buf2, ch, LOG_SYSLOG, TRUE);
      GET_CYBERDECK_IS_INCOMPLETE(cyberdeck) = 0;
      send_to_char(ch, "You smack the side of %s^n a few times. It sparks, then powers on.\r\n", GET_OBJ_NAME(cyberdeck));
      return TRUE;
    }

    strcat(buf, ".\r\n");
    send_to_char(buf, ch);
  }

  return FALSE;
}

int get_paydata_market_maximum(int host_color) {
  switch (host_color) {
    case HOST_COLOR_BLUE:   return HOST_COLOR_BLUE_MARKET_MAXIMUM;
    case HOST_COLOR_GREEN:  return HOST_COLOR_GREEN_MARKET_MAXIMUM;
    case HOST_COLOR_ORANGE: return HOST_COLOR_ORANGE_MARKET_MAXIMUM;
    case HOST_COLOR_RED:    return HOST_COLOR_RED_MARKET_MAXIMUM;
    case HOST_COLOR_BLACK:  return HOST_COLOR_BLACK_MARKET_MAXIMUM;
    default: return 0;
  }
}

int get_paydata_market_minimum(int host_color) {
  switch (host_color) {
    case HOST_COLOR_BLUE:   return HOST_COLOR_BLUE_MARKET_MINIMUM;
    case HOST_COLOR_GREEN:  return HOST_COLOR_GREEN_MARKET_MINIMUM;
    case HOST_COLOR_ORANGE: return HOST_COLOR_ORANGE_MARKET_MINIMUM;
    case HOST_COLOR_RED:    return HOST_COLOR_RED_MARKET_MINIMUM;
    case HOST_COLOR_BLACK:  return HOST_COLOR_BLACK_MARKET_MINIMUM;
    default: return 0;
  }
}

const char *acifs_strings[] =
  {
    "Access",
    "Control",
    "Index",
    "Files",
    "Slave"
  };
