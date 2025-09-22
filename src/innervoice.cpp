#define INNER_VOICE_REAL_IMPL
#include "awake.hpp"
#include "types.hpp"
#include "comm.hpp"
#include "constants.hpp"
#include "utils.hpp"
#include "db.hpp"
#include "handler.hpp"
#include "innervoice.hpp"
#include "interpreter.hpp"

#include <unordered_map>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <sys/stat.h>
#include <dirent.h>
#include <random>
#include <cstring>
#include <cctype>

void perform_remove(struct char_data *ch, int pos);

static bool maybe_speak_from_file_return(char_data* ch, int skill, const char* suffix);

// ---------------------------
// Config
// ---------------------------
static const int MIN_COOLDOWN_SECONDS = 30;        // 30s
static const int MAX_COOLDOWN_SECONDS = 15 * 60;   // 15m
static const int SKILL_SPEAK_CHANCE_PERCENT = 5;   // 5% chance on use/success/fail events
static const long ATTITUDE_MAX = 1000000L;
static const long ATTITUDE_MIN = 0;

// ---------------------------
// Internal state
// ---------------------------
struct VoiceState {
  // batching & context throttles
  bool batch_on_entry = false;
  bool cooldown_pending = false;
  std::string batch_buffer;
  time_t next_ok_at_entry = 0;
  time_t next_ok_at_combat = 0;
  // Persistent user-facing properties
  char entity_name[64] = {0};

  bool intro_shown = false;
  time_t last_emote_award_ts = 0;
  char last_target_name[128] = {0};



  time_t next_ok_at = 0;       // no speech before this
  long attitude = 0;           // 0..1_000_000
  time_t last_decay = 0;       // last attitude decay
  int recent_skill = 0;        // last skill id seen
  time_t recent_skill_at = 0;  // timestamp of last skill id
  unsigned long long veh_trip_token = 0;
  int veh_trip_spoken = 0;
  int veh_trip_max = 0;
  time_t pending_micro_due = 0;
  
  // Scheduled time to play the newbie intro (0 if none pending)
  time_t pending_intro_due = 0;
int pending_micro_index = -1;
  char pending_micro_path[128] = {0};
  long last_target_id = 0;
};

/* Forward declarations for helpers used before definition. */
static bool can_speak_entry_now(VoiceState& st);
static void set_new_cooldown_entry(VoiceState& st);
static void speak_line(char_data* ch, const std::string& line);
void do_sneak(struct char_data *ch, char *argument, int cmd, int subcmd);
extern const char *a_an(char *);




static std::unordered_map<long, VoiceState> g_state; // keyed by GET_IDNUM(ch)

// RNG helpers

static bool iv_local_cd(time_t last, int seconds) {
  time_t now = time(0);
  if (last && now - last < seconds) return true;
  return false;
}

static bool strcasestr_simple(const char* hay, const char* needle) {
  if (!hay || !needle) return false;
  size_t nlen = strlen(needle);
  for (const char* p = hay; *p; ++p) {
    if (strncasecmp(p, needle, nlen) == 0) return true;
  }
  return false;
}

static int irand(int lo, int hi) {
  if (hi <= lo) return lo;
  return number(lo, hi); // mud's RNG
}

static bool chance(int percent) {
  return irand(1, 100) <= percent;
}

// --- Tier 1 assistance helpers (adjacent intel + container reveal) ---
static bool iv_tier1_roll(struct char_data *ch) {
  if (!ch || IS_NPC(ch) || !ch->in_room) return false;
  const long id = GET_IDNUM(ch);
  VoiceState& st = g_state[id];
  if (st.attitude < 500000) return false;
  double p = (double) st.attitude / 5000000.0;
  if (p < 0.0) p = 0.0;
  if (p > 1.0) p = 1.0;
  // roll 0..9999 < p*10000
  return number(0, 9999) < (int)(p * 10000.0);
}

// Returns a terse summary like "camera, turret, 2 hostiles" for a room.
static std::string iv_summarize_room_threats(struct room_data *r) {
  if (!r) return std::string();
  int turrets = 0, hostiles = 0, npcs = 0, traps = 0;
  // Traps: death/fall rooms
  if (ROOM_FLAGGED(r, ROOM_DEATH) || ROOM_FLAGGED(r, ROOM_FALL)) traps++;

  for (struct obj_data *o = r->contents; o; o = o->next_content) {
    
  }
  for (struct char_data *t = r->people; t; t = t->next_in_room) {
    if (!IS_NPC(t)) continue;
    if (MOB_FLAGGED(t, MOB_INANIMATE) || MOB_FLAGGED(t, MOB_EMPLACED)) turrets++;
    else if (MOB_FLAGGED(t, MOB_AGGRESSIVE)) hostiles++;
    else npcs++;
  }
  std::ostringstream oss;
  bool first = true;
  auto add = [&](const char *txt, int count, bool pluralize){
    if (count <= 0) return;
    if (!first) oss << ", ";
    first = false;
    if (pluralize && count > 1) oss << count << " " << txt << "s";
    else if (!pluralize && count > 1) oss << count << " " << txt; // txt already plural
    else if (count == 1 && pluralize) oss << txt;
    else oss << txt;
  };
  add("turret", turrets, true);
  add("hostile", hostiles, true);
  add("npc", npcs, true);
  add("trap", traps, true);
  return oss.str();
}

static void iv_tier1_speak_adjacent(struct char_data *ch) {
  VoiceState& st = g_state[GET_IDNUM(ch)];
  if (!can_speak_entry_now(st)) return;
  if (!iv_tier1_roll(ch)) return;
  // Build summaries for each exit with intel.
  std::vector<std::string> parts;
  for (int dir = 0; dir < NUM_OF_DIRS; dir++) {
    if (!EXIT(ch, dir) || !EXIT(ch, dir)->to_room)
      continue;
    struct room_data *r = EXIT(ch, dir)->to_room;
    std::string sum = iv_summarize_room_threats(r);
    if (sum.empty()) continue;
    // Use short direction name, e.g., dirs[dir] ("north", "ne", ...)
    char buf[256];
    snprintf(buf, sizeof(buf), "%s: %s", dirs[dir], sum.c_str());
    parts.push_back(std::string(buf));
    if (parts.size() >= 4) break; // avoid spam
  }
  if (parts.empty()) return;
  std::ostringstream line;
  line << "^cHeads-up:^n ";
  {
    std::ostringstream tmp;
    for (size_t i = 0; i < parts.size(); ++i) {
      if (i) tmp << "; ";
      tmp << parts[i];
    }
    line << tmp.str() << ".";
  }
  speak_line(ch, line.str());
  // cooldown
  set_new_cooldown_entry(st);
}

static void iv_tier1_speak_container_peek(struct char_data *ch) {
  VoiceState& st = g_state[GET_IDNUM(ch)];
  if (!can_speak_entry_now(st)) return;
  if (!iv_tier1_roll(ch)) return;
  if (!ch->in_room) return;
  // Find up to two containers in current room and report contents.
  int reported = 0;
  for (struct obj_data *o = ch->in_room->contents; o; o = o->next_content) {
    if (GET_OBJ_TYPE(o) != ITEM_CONTAINER) continue;
    // Skip empty containers.
    if (!o->contains) continue;

    // Aggregate first few items by name.
    std::vector<std::string> names;
    int count = 0;
    for (struct obj_data *c = o->contains; c && count < 4; c = c->next_content, count++) {
      names.push_back(std::string(fname((char *) GET_OBJ_NAME(c))));

// Limit to two containers per entry to avoid spam.
reported++;
if (reported >= 2) break;

    }

// Tier 1 (stealth): condensed risk callouts for adjacent rooms when sneaking.


// --- Tier 2 assistance (attitude >= 800,000) ---




// Choose exactly one: unlock doors OR unlock chests (containers).


// Deactivate cameras in adjacent rooms (remove camera objects).

// Exported tick hook for fight.cpp.


// Tier 2 (stealth): auto-sneak and dim light sources.
ACMD_DECLARE(do_sneak);
extern void perform_remove(struct char_data *ch, int pos);






}
}

static void iv_tier1_speak_stealth_warnings(struct char_data *ch) {
  if (!ch || !ch->in_room) return;
  if (!AFF_FLAGGED(ch, AFF_SNEAK)) return;
  VoiceState& st = g_state[GET_IDNUM(ch)];
  if (!can_speak_entry_now(st)) return;
  if (!iv_tier1_roll(ch)) return;

  std::vector<std::string> parts;
  for (int dir = 0; dir < NUM_OF_DIRS; dir++) {
    if (!EXIT(ch, dir) || !EXIT(ch, dir)->to_room) continue;
    struct room_data *r = EXIT(ch, dir)->to_room;

    int hostiles = 0, npcs = 0, exits = 0;
    // count exits
    for (int d=0; d<NUM_OF_DIRS; d++) if (r->dir_option[d] && r->dir_option[d]->to_room) exits++;
    // count mobs
    for (struct char_data *t = r->people; t; t = t->next_in_room) {
      if (!IS_NPC(t)) continue;
      if (MOB_FLAGGED(t, MOB_AGGRESSIVE)) hostiles++; else npcs++;
    }
    const char *risk = "cool";
    int score = hostiles*2 + npcs + (exits>=3 ? 1 : 0);
    if (score >= 4) risk = "hot";
    else if (score >= 2) risk = "warm";

    char buf[128];
    if (hostiles>0)
      snprintf(buf, sizeof(buf), "%s: %s, aggro %d", dirs[dir], risk, hostiles);
    else if (npcs>0)
      snprintf(buf, sizeof(buf), "%s: %s, npc %d", dirs[dir], risk, npcs);
    else
      snprintf(buf, sizeof(buf), "%s: %s", dirs[dir], risk);
    parts.push_back(std::string(buf));
    if (parts.size() >= 4) break;
  }
  if (parts.empty()) return;

  std::ostringstream join; for (size_t i=0;i<parts.size();++i){ if (i) join << "; "; join << parts[i]; }
  std::ostringstream line;
  line << "^cPath:^n " << join.str() << ".";

  // Add self-risk if carrying a light or glowing gear.
  bool lit = false, glow = false;
  for (int pos = 0; pos < NUM_WEARS; pos++) {
    struct obj_data *eq = GET_EQ(ch, pos);
    if (!eq) continue;
    if (pos == WEAR_LIGHT && GET_OBJ_TYPE(eq) == ITEM_LIGHT) lit = true;
    if (GET_OBJ_EXTRA(eq).IsSet(ITEM_EXTRA_GLOW)) glow = true;
  }
  if (lit || glow) line << " you are lit.";

  speak_line(ch, line.str());
  set_new_cooldown_entry(st);
}

static bool iv_tier2_roll(struct char_data *ch) {
  if (!ch || IS_NPC(ch) || !ch->in_room) return false;
  VoiceState& st = g_state[GET_IDNUM(ch)];
  if (st.attitude < 800000) return false;
  double p = (double) st.attitude / 5000000.0;
  if (p < 0.0) p = 0.0;
  if (p > 1.0) p = 1.0;
  return number(0, 9999) < (int)(p * 10000.0);
}

static bool iv_tier2_roll_dim_sneak(struct char_data *ch) {
  if (!ch || IS_NPC(ch) || !ch->in_room) return false;
  VoiceState& st = g_state[GET_IDNUM(ch)];
  if (st.attitude < 800000) return false;
  if (!AFF_FLAGGED(ch, AFF_SNEAK)) return false;
  double p = (double) st.attitude / 1250000.0;
  if (p < 0.0) p = 0.0;
  if (p > 1.0) p = 1.0;
  return number(0, 9999) < (int)(p * 10000.0);
}

static void iv_tier2_unlock_doors_or_chests(struct char_data *ch) {
  VoiceState& st = g_state[GET_IDNUM(ch)];
  if (!can_speak_entry_now(st)) return;
  if (!iv_tier2_roll(ch)) return;
  if (!ch->in_room) return;

  // Gather candidates
  std::vector<int> locked_dirs;
  for (int dir = 0; dir < NUM_OF_DIRS; dir++) {
    if (!EXIT(ch, dir) || !EXIT(ch, dir)->to_room) continue;
    if (IS_SET(EXIT(ch, dir)->exit_info, EX_LOCKED) && !IS_SET(EXIT(ch, dir)->exit_info, EX_PICKPROOF))
      locked_dirs.push_back(dir);
  }
  std::vector<struct obj_data*> locked_chests;
  for (struct obj_data *o = ch->in_room->contents; o; o = o->next_content) {
    if (GET_OBJ_TYPE(o) != ITEM_CONTAINER) continue;
    if (IS_SET(GET_OBJ_VAL(o, 1), CONT_LOCKED) && !IS_SET(GET_OBJ_VAL(o, 1), CONT_PICKPROOF))
      locked_chests.push_back(o);
  }
  if (locked_dirs.empty() && locked_chests.empty()) return;

  bool do_doors, do_chests;
  if (!locked_dirs.empty() && !locked_chests.empty()) {
    do_doors = (number(0,1) == 0);
    do_chests = !do_doors;
  } else {
    do_doors = !locked_dirs.empty();
    do_chests = !locked_chests.empty();
  }

  if (do_doors) {
    int acted = 0;
    std::vector<std::string> parts;
    for (size_t i = 0; i<locked_dirs.size() && acted<1; ++i) {
      int dir = locked_dirs[i];
      // unlock both sides
      REMOVE_BIT(EXIT(ch, dir)->exit_info, EX_LOCKED);
      if (EXIT(ch, dir)->to_room && EXIT(ch, dir)->to_room->dir_option[rev_dir[dir]])
        REMOVE_BIT(EXIT(ch, dir)->to_room->dir_option[rev_dir[dir]]->exit_info, EX_LOCKED);
      parts.push_back(std::string(dirs[dir]));
      acted++;
    }
    if (acted) {
      std::ostringstream tmp; for (size_t i=0;i<parts.size(); ++i){ if (i) tmp << ", "; tmp << parts[i]; }
      std::ostringstream line;
      line << "^CMOVE OVER:^n I ghost-picked the locks to " << tmp.str() << ".";
      speak_line(ch, line.str());
      set_new_cooldown_entry(st);
    }
  } else if (do_chests) {
    int acted = 0;
    std::vector<std::string> parts;
    for (size_t i=0; i<locked_chests.size() && acted<2; ++i) {
      struct obj_data *o = locked_chests[i];
      REMOVE_BIT(GET_OBJ_VAL(o, 1), CONT_LOCKED);
      parts.push_back(std::string(fname(const_cast<char *>(GET_OBJ_NAME(o)))));
      acted++;
    }
    if (acted) {
      std::ostringstream tmp; for (size_t i=0;i<parts.size(); ++i){ if (i) tmp << ", "; tmp << parts[i]; }
      std::ostringstream line;
      line << "^CCLICK.^n Popped the pins on " << tmp.str() << ".";
      speak_line(ch, line.str());
      set_new_cooldown_entry(st);
    }
  }
}

static void iv_tier2_weaken_current_target(struct char_data *ch) {
  if (!ch || !FIGHTING(ch)) return;
  VoiceState& st = g_state[GET_IDNUM(ch)];
  if (!can_speak_entry_now(st)) return;
  if (!iv_tier2_roll(ch)) return;

  struct char_data *t = FIGHTING(ch);
  // record for path ping
  if (t) { VoiceState& st2 = g_state[GET_IDNUM(ch)]; st2.last_target_id = IS_NPC(t) ? GET_MOB_VNUM(t) : GET_IDNUM(t); const char *tn=GET_NAME(t); if (tn) strlcpy(st2.last_target_name, tn, sizeof(st2.last_target_name)); }
  if (!t || !t->in_room || t->in_room != ch->in_room) return;
  if (IS_NPC(t)) {
    // Skip protected / plot-important targets.
    if (MOB_FLAGGED(t, MOB_NOKILL) || MOB_FLAGGED(t, MOB_GUARD) || MOB_FLAGGED(t, MOB_SNIPER))
      return;
    const char *tn = GET_NAME(t);
    if (tn && (str_str(tn, "BOSS") || str_str(tn, "Boss") || str_str(tn, "[BOSS]")))
      return;
  }

  struct obj_data *wield = NULL;
  if (GET_EQ(t, WEAR_WIELD) && GET_OBJ_TYPE(GET_EQ(t, WEAR_WIELD)) == ITEM_WEAPON)
    wield = GET_EQ(t, WEAR_WIELD);
  else if (GET_EQ(t, WEAR_HOLD) && GET_OBJ_TYPE(GET_EQ(t, WEAR_HOLD)) == ITEM_WEAPON)
    wield = GET_EQ(t, WEAR_HOLD);

  bool acted = false;
  if (wield && WEAPON_IS_GUN(wield) && wield->contains) {
    struct obj_data *mag = wield->contains;
    if (GET_MAGAZINE_AMMO_COUNT(mag) > 0) {
      GET_MAGAZINE_AMMO_COUNT(mag) = 0;
      acted = true;
      std::ostringstream line;
      line << "^C*TCHAK*^n I spiked " << (IS_NPC(t) ? GET_NAME(t) : "their") << " gun—enjoy the silence.";
      speak_line(ch, line.str());
    }
  }

  if (!acted) {
    WAIT_STATE(t, PULSE_VIOLENCE);
    acted = true;
    std::ostringstream line;
    line << "^CZap.^n " << (IS_NPC(t) ? GET_NAME(t) : "Your target") << " just lost a beat—keep hitting.";
    speak_line(ch, line.str());
  }
  if (acted) { set_new_cooldown_entry(st); st.attitude = MAX(0, st.attitude - number(1,5)); }
}

void innervoice_tier2_combat_assist(struct char_data *ch) {
  iv_tier2_weaken_current_target(ch);
}

static void iv_tier2_autoresneak(struct char_data *ch) {
  if (!ch) return;
  VoiceState& st = g_state[GET_IDNUM(ch)];
  if (!can_speak_entry_now(st)) return;
  if (!iv_tier2_roll(ch)) return;
  if (AFF_FLAGGED(ch, AFF_SNEAK)) return;

  char __empty = ' '; do_sneak(ch, &__empty, 0, 0);
  speak_line(ch, "^csoft feet.^n");
  set_new_cooldown_entry(st);
  st.attitude = MAX(0, st.attitude - number(1,5));
}

static void iv_tier2_autodim(struct char_data *ch) {
  if (!ch) return;
  VoiceState& st = g_state[GET_IDNUM(ch)];
  if (!can_speak_entry_now(st)) return;
  if (!iv_tier2_roll_dim_sneak(ch)) return;

  bool acted=false;
  // Remove equipped light
  if (GET_EQ(ch, WEAR_LIGHT) && GET_OBJ_TYPE(GET_EQ(ch, WEAR_LIGHT)) == ITEM_LIGHT) {
    perform_remove(ch, WEAR_LIGHT);
    acted=true;
  }
  // Drop or unhold glowing items? We'll prefer to warn only—removing armor would be disruptive.

  if (acted) {
    speak_line(ch, "^cdimmed.^n");
    set_new_cooldown_entry(st);
    st.attitude = MAX(0, st.attitude - number(1,5));
  }
}

void innervoice_tier2_combat_assist_duplicate(struct char_data *ch) {
  iv_tier2_weaken_current_target(ch);
}

static void __iv_unused_block(struct char_data* ch, struct obj_data* o, const std::vector<std::string>& names) {
    if (names.empty()) return;

    std::ostringstream line;
    line << "^cIntel:^n " << CAP(a_an((char *) GET_OBJ_NAME(o))) << " here holds ";
    // join names with comma and 'and'
    if (names.size() == 1) line << names[0];
    else if (names.size() == 2) line << names[0] << " and " << names[1];
    else {
      for (size_t i = 0; i < names.size(); ++i) {
        if (i == names.size() - 1) line << "and " << names[i];
        else line << names[i] << ", ";
      }
    }
    line << ".";
    speak_line(ch, line.str());
    VoiceState& st = g_state[GET_IDNUM(ch)];
    set_new_cooldown_entry(st);
    static int __reported_dummy = 0; if (++__reported_dummy >= 2) {/* no-op */}
  }




static void set_new_cooldown(VoiceState& st) {
  // legacy/global fallback
  st.next_ok_at = time(0) + irand(MIN_COOLDOWN_SECONDS, MAX_COOLDOWN_SECONDS);
  st.next_ok_at_entry = st.next_ok_at;
  st.next_ok_at_combat = st.next_ok_at;
}

static void set_new_cooldown_entry(VoiceState& st) {
  if (st.batch_on_entry) { st.cooldown_pending = true; return; }
  st.next_ok_at_entry = time(0) + irand(MIN_COOLDOWN_SECONDS, MAX_COOLDOWN_SECONDS);
  st.next_ok_at = st.next_ok_at_entry;
}

static void set_new_cooldown_combat(VoiceState& st) {
  st.next_ok_at_combat = time(0) + irand(MIN_COOLDOWN_SECONDS, MAX_COOLDOWN_SECONDS);
  st.next_ok_at = st.next_ok_at_combat;
}

static bool can_speak_now(VoiceState& st) {
  return time(0) >= st.next_ok_at;
}

static bool can_speak_entry_now(VoiceState& st) {
  return time(0) >= st.next_ok_at_entry;
}

static bool can_speak_combat_now(VoiceState& st) {
  return time(0) >= st.next_ok_at_combat;
}



// File helpers
static std::string skill_file_base(int skill, const char* suffix) {
  // Build skills directory path: lib/etc/innervoice/skills/NNN-skill-slug-<suffix>.txt
  // We rely on extern skills[] from constants.cpp for the name.
  const char* nm = "skill";
  if (skill >= 0) {
    nm = skills[skill].name;
    if (!nm || !*nm) nm = "skill";
  }
  std::string name = nm;
  // slugify
  std::string slug;
  slug.reserve(name.size());
  for (char c : name) {
    if ((c >= 'A' && c <= 'Z')) c = c - 'A' + 'a';
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) slug.push_back(c);
    else slug.push_back('-');
  }
  // collapse hyphens
  std::string collapsed;
  bool lastdash=false;
  for (char c : slug) {
    if (c == '-') {
      if (!lastdash) collapsed.push_back(c);
      lastdash=true;
    } else {
      collapsed.push_back(c);
      lastdash=false;
    }
  }
  while (!collapsed.empty() && collapsed.front()=='-') collapsed.erase(collapsed.begin());
  while (!collapsed.empty() && collapsed.back()=='-') collapsed.pop_back();
  char buf[16]; snprintf(buf, sizeof(buf), "%03d", skill);
  std::ostringstream oss;
  oss << "lib/etc/innervoice/skills/" << buf << "-" << (collapsed.empty() ? "skill" : collapsed) << "-" << suffix << ".txt";
  return oss.str();
}

static bool load_lines(const std::string& path, std::vector<std::string>& out) {
  std::ifstream f(path.c_str());
  if (!f.good()) return false;
  out.clear();
  std::string line;
  while (std::getline(f, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') continue;
    out.push_back(line);
  }
  return !out.empty();
}

// Whisperization for stealth: lower intensity and trim emphatics.
static std::string iv_whisperize(const std::string &in) {
  std::string s = in;
  // Demote bright color codes to normal where applicable.
  size_t pos = 0;
  while ((pos = s.find("^C", pos)) != std::string::npos) s.replace(pos, 2, "^c");
  pos = 0;
  while ((pos = s.find("^W", pos)) != std::string::npos) s.replace(pos, 2, "^w");
  // Tone down common shouts.
  const char* loud[] = {"Heads-up:", "MOVE OVER:", "CLICK.", "Zap.", "Intel:", "*TCHAK*", "Heads‑up:", "Heads–up:"};
  for (const char* l : loud) {
    pos = s.find(l);
    if (pos != std::string::npos) s.erase(pos, strlen(l));
  }
  // Strip multiple spaces.
  std::string out; out.reserve(s.size());
  bool prev_space = false;
  for (char c : s) {
    if (c == '!') c = '.';
    if (c == ' ') {
      if (!prev_space) out.push_back(c);
      prev_space = true;
    } else {
      out.push_back(c);
      prev_space = false;
    }
  }
  // Trim leading spaces / punctuation.
  while (!out.empty() && (out[0] == ' ' || out[0] == '.' || out[0] == ';')) out.erase(out.begin());
  // Lowercase the very first word-ish chunk to convey hush.
  for (size_t i=0; i<out.size(); ++i) {
    if (isalpha(out[i])) { out[i] = (char)tolower(out[i]); break; }
  }
  return out;
}

// Flush batched line if present.
static void iv_flush_batched_line(struct char_data *ch) {
  if (!ch) return;
  VoiceState& st = g_state[GET_IDNUM(ch)];
  if (st.batch_buffer.empty()) return;

  std::string out = st.batch_buffer;
  st.batch_buffer.clear();
  st.batch_on_entry = false;

  // Respect whispering if sneaking.
  if (AFF_FLAGGED(ch, AFF_SNEAK)) out = iv_whisperize(out);

  if (PRF_FLAGGED(ch, PRF_SCREENREADER))
    send_to_char(ch, "%s: %s\r\n", g_state[GET_IDNUM(ch)].entity_name, out.c_str());
  else
    send_to_char(ch, "^m[%s]^n %s\r\n", g_state[GET_IDNUM(ch)].entity_name, out.c_str());

  if (st.cooldown_pending) {
    st.cooldown_pending = false;
    st.next_ok_at_entry = time(0) + irand(MIN_COOLDOWN_SECONDS, MAX_COOLDOWN_SECONDS);
    st.next_ok_at = st.next_ok_at_entry; // keep legacy in sync
  }
}

static void speak_line(char_data* ch, const std::string& line) {
  if (!ch) return;
  VoiceState& st = g_state[GET_IDNUM(ch)];
  // Batch on entry: accumulate; actual send occurs at flush.
  if (st.batch_on_entry) {
    if (!st.batch_buffer.empty()) st.batch_buffer.append("; ");
    st.batch_buffer.append(line);
    // Cooldown deferred.
    return;
  }

  std::string out = line;
  if (AFF_FLAGGED(ch, AFF_SNEAK))
    out = iv_whisperize(out);

  if (PRF_FLAGGED(ch, PRF_SCREENREADER)) {
    send_to_char(ch, "%s: %s\r\n", g_state[GET_IDNUM(ch)].entity_name, out.c_str());
  } else {
    send_to_char(ch, "^m[%s]^n %s\r\n", g_state[GET_IDNUM(ch)].entity_name, out.c_str());
  }
}
static void speak_line_duplicate_block(char_data* ch, const std::string& line) {
  if (!ch) return;
  if (PRF_FLAGGED(ch, PRF_SCREENREADER)) {
    send_to_char(ch, "%s: %s\r\n", g_state[GET_IDNUM(ch)].entity_name, line.c_str());
  } else {
    send_to_char(ch, "^m[%s]^n %s\r\n", g_state[GET_IDNUM(ch)].entity_name, line.c_str());
  }
}

static void maybe_speak_from_file(char_data* ch, int skill, const char* suffix) {
  if (!ch || IS_NPC(ch)) return;
  const long id = GET_IDNUM(ch);
  VoiceState& st = g_state[id];
  if (!can_speak_entry_now(st)) return;
  std::vector<std::string> lines;
  const std::string path = skill_file_base(skill, suffix);
  if (!load_lines(path, lines)) {
    // fallback generic file (skill 0)
    load_lines("lib/etc/innervoice/skills/000-skill-generic-" + std::string(suffix) + ".txt", lines);
  }
  if (!lines.empty()) {
    const int idx = irand(0, (int)lines.size()-1);
    speak_line(ch, lines[idx]);
    set_new_cooldown_entry(st);
  }
}

// Public API
namespace InnerVoice {

// Forward declarations of internal implementations.
void notify_door_interaction_impl(char_data* ch, int subcmd, int dir);
void notify_vehicle_travel_tick_impl(char_data* ch, veh_data* veh);


// Public wrappers forwarding to the internal implementations.
void notify_door_interaction(char_data* ch, int subcmd, int dir) {
  notify_door_interaction_impl(ch, subcmd, dir);
}
void notify_vehicle_travel_tick(char_data* ch, veh_data* veh) {
  notify_vehicle_travel_tick_impl(ch, veh);
}


  void notify_skill_used(char_data* ch, int skill) {
    if (!ch || IS_NPC(ch)) return;
    const long id = GET_IDNUM(ch);
    VoiceState& st = g_state[id];
    st.recent_skill = skill;
    st.recent_skill_at = time(0);
    if (chance(SKILL_SPEAK_CHANCE_PERCENT)) {
      maybe_speak_from_file(ch, skill, "use");
    }
  }

  void notify_skill_result_from_success_test(char_data* ch, int total_successes, int dice, bool botched) {
    if (!ch || IS_NPC(ch)) return;
    const long id = GET_IDNUM(ch);
    VoiceState& st = g_state[id];

    // If the last skill tag is too old (> 3s), we still allow generic commentary.
    int skill = st.recent_skill;
    if (st.recent_skill_at == 0 || time(0) - st.recent_skill_at > 3) {
      skill = 0; // generic
    }

    if (chance(SKILL_SPEAK_CHANCE_PERCENT)) {
      if (botched || total_successes <= 0) {
        if (maybe_speak_from_file_return(ch, skill, "fail")) { adjust_attitude(ch, -number(1,4)); }
      } else {
        if (maybe_speak_from_file_return(ch, skill, "success")) { adjust_attitude(ch, number(1,5)); }
      }
    }
  }

  long get_attitude(char_data* ch) {
    if (!ch) return 0;
    VoiceState& st = g_state[GET_IDNUM(ch)];
    return st.attitude;
  }

  void set_attitude(char_data* ch, long value) {
    if (!ch) return;
    VoiceState& st = g_state[GET_IDNUM(ch)];
    if (value < ATTITUDE_MIN) value = ATTITUDE_MIN;
    if (value > ATTITUDE_MAX) value = ATTITUDE_MAX;
    st.attitude = value;
  }

  void adjust_attitude(char_data* ch, long delta) {
    if (!ch) return;
    set_attitude(ch, get_attitude(ch) + delta);
  }

  void heartbeat_all_players() {
    // Decay attitude slowly: -1 every minute.
    time_t now = time(0);
    for (auto& kv : g_state) {
      VoiceState& st = kv.second;
      if (st.last_decay == 0) st.last_decay = now;
      if (now - st.last_decay >= 60) {
        long steps = (now - st.last_decay) / 60;
        if (st.attitude > 0) {
          if (st.attitude > steps) st.attitude -= steps;
          else st.attitude = 0;
        }
        st.last_decay = now;
      }
    }
  }
}

// ---------------------------
// Item content helpers
// ---------------------------
static bool parse_attitude_header(const std::string& path, int& out_att) {
  out_att = 0;
  std::ifstream f(path.c_str());
  if (!f.good()) return false;
  std::string line;
  while (std::getline(f, line)) {
    if (!line.empty() && line.back()=='\r') line.pop_back();
    if (line.rfind("#",0)==0) {
      std::string low = line;
      std::transform(low.begin(), low.end(), low.begin(), ::tolower);
      size_t pos = low.find("attitude");
      if (pos != std::string::npos) {
        int val = 0;
        if (sscanf(low.c_str(), "# attitude : %d", &val) == 1 || sscanf(low.c_str(), "#attitude:%d", &val)==1 || sscanf(low.c_str(), "# attitude=%d", &val)==1) {
          out_att = val;
          return true;
        }
      }
      continue;
    }
    break;
  }
  return false;
}

static std::string obj_item_file(int vnum) {
  char buf[64]; snprintf(buf, sizeof(buf), "%d", vnum);
  std::string base = "lib/etc/innervoice/items/obj/";
  return base + buf + "-";
}


static bool load_item_lines_by_prefix(const std::string& prefix, std::vector<std::string>& out, int* out_idx) {
  // C++11 fallback: scan directory using dirent.
  std::string dir = prefix.substr(0, prefix.find_last_of("/"));
  std::string base = prefix.substr(prefix.find_last_of("/")+1);
  DIR *dp = opendir(dir.c_str());
  if (!dp) return false;
  std::string chosen;
  struct dirent *de;
  while ((de = readdir(dp)) != NULL) {
    if (strncmp(de->d_name, base.c_str(), base.size()) == 0) { chosen = de->d_name; break; }
  }
  closedir(dp);
  if (chosen.empty()) return false;
  std::string path = dir + "/" + chosen;
  std::ifstream f(path.c_str());
  if (!f.is_open()) return false;
  std::string line;
  while (std::getline(f, line)) out.push_back(line);
  if (out_idx) *out_idx = 0;
  return !out.empty();
}


static void maybe_speak_item_line(char_data* ch, const std::vector<std::string>& lines) {
  if (lines.empty()) return;
  const int idx = irand(0, (int)lines.size()-1);
  speak_line(ch, lines[idx]);
}

namespace InnerVoice {
  void notify_item_interaction(char_data* ch, obj_data* obj, const char* action) {
    if (!ch || IS_NPC(ch)) return;
    if (!action) action = "use";
    const long id = GET_IDNUM(ch);
    VoiceState& st = g_state[id];
    if (!can_speak_now(st) || !chance(5)) return;

    std::vector<std::string> lines;
    int attitude_bonus = 0;
    bool got = false;

    if (obj) {
      int vnum = GET_OBJ_VNUM(obj);
      std::string prefix = obj_item_file(vnum);
      got = load_item_lines_by_prefix(prefix, lines, &attitude_bonus);
      if (got) {
        maybe_speak_item_line(ch, lines);
        set_new_cooldown_entry(st);
        if (attitude_bonus > 0 && chance(50)) {
          adjust_attitude(ch, attitude_bonus);
        }
        return;
      }
    }

    std::string generic = std::string("lib/etc/innervoice/items/generic-") + action + ".txt";
    if (load_lines(generic, lines)) {
      maybe_speak_item_line(ch, lines);
      adjust_attitude(ch, number(1,5));
      set_new_cooldown_entry(st);
    }
  }

  void notify_door_interaction_impl(char_data* ch, int subcmd, int dir) {
    if (!ch || IS_NPC(ch)) return;
    const long id = GET_IDNUM(ch);
    VoiceState& st = g_state[id];
    if (!can_speak_now(st) || !chance(5)) return;
    std::vector<std::string> lines;
    if (load_lines("lib/etc/innervoice/items/doors.txt", lines)) {
      maybe_speak_item_line(ch, lines);
      adjust_attitude(ch, number(1,5));
      set_new_cooldown_entry(st);
    }
  }
}


// ---------------------------
// Room movement hook
// ---------------------------
static std::string room_file_by_vnum(long vnum) {
  char buf[32]; snprintf(buf, sizeof(buf), "%ld", vnum);
  std::string base = "lib/etc/innervoice/rooms/";
  return base + std::string(buf) + "-"; // we'll match by prefix, similar to items
}


static bool load_room_lines_by_prefix(const std::string& prefix, std::vector<std::string>& out) {
  std::string dir = prefix.substr(0, prefix.find_last_of("/"));
  std::string base = prefix.substr(prefix.find_last_of("/")+1);
  DIR *dp = opendir(dir.c_str());
  if (!dp) return false;
  std::string chosen;
  struct dirent *de;
  while ((de = readdir(dp)) != NULL) {
    if (strncmp(de->d_name, base.c_str(), base.size()) == 0) { chosen = de->d_name; break; }
  }
  closedir(dp);
  if (chosen.empty()) return false;
  std::string path = dir + "/" + chosen;
  std::ifstream f(path.c_str());
  if (!f.is_open()) return false;
  std::string line;
  while (std::getline(f, line)) out.push_back(line);
  return !out.empty();
}


namespace InnerVoice {
  void notify_room_move(char_data* ch, int room_rnum) {
  InnerVoice::notify_room_ambient(ch, room_rnum);

    if (!ch || IS_NPC(ch)) return;
    if (room_rnum < 0) return;
        if (!world[room_rnum].description || !*world[room_rnum].description) return;
const long id = GET_IDNUM(ch);
    VoiceState& st = g_state[id];
    // 5% chance and respect cooldown
    if (!can_speak_now(st) || !chance(15)) return;
    long vnum = world[room_rnum].number;
    std::vector<std::string> lines;
    if (load_room_lines_by_prefix(room_file_by_vnum(vnum), lines)) {
      maybe_speak_item_line(ch, lines);
      adjust_attitude(ch, number(1,5));
      set_new_cooldown_entry(st);
    }
  
  // Tier 1 assistance: adjacent intel + container peek
  iv_tier1_speak_adjacent(ch);
  iv_tier1_speak_container_peek(ch);

  // Tier 1 (intel) and Tier 2 (actions) on entry
  iv_tier1_speak_adjacent(ch);
  iv_tier1_speak_container_peek(ch);
  iv_tier2_unlock_doors_or_chests(ch);
  // camera assist removed

  // Begin batch for single-line entry output.
  {
    VoiceState& st = g_state[GET_IDNUM(ch)];
    st.batch_on_entry = true;
  }

  // (Existing calls inserted earlier will batch.)

  // Path ping: if last known target is adjacent, note direction.
  {
    VoiceState& st = g_state[GET_IDNUM(ch)];
    if (st.last_target_name[0]) {
      for (int dir = 0; dir < NUM_OF_DIRS; dir++) {
        if (!EXIT(ch, dir) || !EXIT(ch, dir)->to_room) continue;
        for (struct char_data *t = EXIT(ch, dir)->to_room->people; t; t = t->next_in_room) {
          if (!IS_NPC(t)) continue;
          const char *tn = GET_NAME(t);
          if (tn && str_cmp(tn, st.last_target_name) == 0) {
            char buf[64]; snprintf(buf, sizeof(buf), "%s +1", dirs[dir]);
            speak_line(ch, buf);
            goto found_ping;
          }
        }
      }
    }
  }
found_ping:;

  // Flush any batched lines from entry sequence.
  iv_flush_batched_line(ch);

  // Stealth-specific
  iv_tier1_speak_stealth_warnings(ch);
  iv_tier2_autoresneak(ch);
  iv_tier2_autodim(ch);
}
}


static bool speak_from_file_index(struct char_data* ch, VoiceState& st, const char* filepath, int idx) {
  std::vector<std::string> lines;
  if (!load_lines(filepath, lines) || idx < 0 || idx >= (int)lines.size())
    return false;
  std::vector<std::string> one = { lines[idx] };
  maybe_speak_item_line(ch, one);
  set_new_cooldown_entry(st);
  return true;
}

namespace InnerVoice {
  void notify_vehicle_travel_tick_impl(struct char_data* ch, struct veh_data* veh) {
    if (!ch || !veh || IS_NPC(ch)) return;
    VoiceState& st = g_state[GET_IDNUM(ch)];
    if (!can_speak_entry_now(st)) return;
    unsigned long target_vnum = 0;
    if (veh->dest) target_vnum = veh->dest->number;
    else if (veh->flight_target) target_vnum = veh->flight_target->number;
    unsigned long long token = ((unsigned long long)veh->idnum << 32) | (unsigned long long)target_vnum;
    if (token != 0 && token != st.veh_trip_token) {
      st.veh_trip_token = token;
      st.veh_trip_spoken = 0;
      st.veh_trip_max = irand(1, 2);
    }
    if (st.veh_trip_max > 0 && st.veh_trip_spoken >= st.veh_trip_max) return;
    std::vector<std::string> lines;
    if (!load_lines("lib/etc/innervoice/vehicles/travel.txt", lines) || lines.empty()) return;
    maybe_speak_item_line(ch, lines);
    set_new_cooldown_entry(st);
    st.veh_trip_spoken++;
  }
}

static bool load_quest_lines(long vnum, const char *suffix, std::vector<std::string>& out) {
  out.clear();
  char filename[256];
  snprintf(filename, sizeof(filename), "lib/etc/innervoice/quests/%ld-%s.txt", vnum, suffix);
  return load_lines(filename, out);
}

namespace InnerVoice {
  void notify_quest_accept(struct char_data* ch, long quest_vnum) {
    if (!ch || IS_NPC(ch)) return;
    VoiceState& st = g_state[GET_IDNUM(ch)];
    if (!can_speak_now(st) || !chance(50)) return;
    std::vector<std::string> lines;
    if (!load_quest_lines(quest_vnum, "accept", lines) || lines.empty()) return;
    maybe_speak_item_line(ch, lines);
    set_new_cooldown_entry(st);
  }
  void notify_quest_complete(struct char_data* ch, long quest_vnum) {
    if (!ch || IS_NPC(ch)) return;
    VoiceState& st = g_state[GET_IDNUM(ch)];
    if (!can_speak_entry_now(st)) return;
    std::vector<std::string> lines;
    if (!load_quest_lines(quest_vnum, "complete", lines) || lines.empty()) return;
    maybe_speak_item_line(ch, lines);
    set_new_cooldown_entry(st);
  }
}

namespace InnerVoice {
  void queue_micro_comment(struct char_data* ch, const char* theme, const char* variant_suffix, int line_index) {
    if (!ch || IS_NPC(ch)) return;
    VoiceState& st = g_state[GET_IDNUM(ch)];
    char path[128];
    snprintf(path, sizeof(path), "lib/etc/innervoice/ambient/micro_%s%s.txt", theme && *theme ? theme : "city", variant_suffix ? variant_suffix : "");
    strncpy(st.pending_micro_path, path, sizeof(st.pending_micro_path)-1);
    st.pending_micro_index = line_index;
    st.pending_micro_due = time(0) + 15;
  }
  void tick_inner_voice(void) {
    time_t now = time(0);
    for (struct descriptor_data *d = descriptor_list; d; d = d->next) {
      struct char_data *ch = d->character;
      if (!ch || IS_NPC(ch)) continue;
      VoiceState& st = g_state[GET_IDNUM(ch)];
      if (st.pending_micro_due && now >= st.pending_micro_due) {
        if (!can_speak_now(st)) continue;
        if (st.pending_micro_path[0] && st.pending_micro_index >= 0) {
          if (speak_from_file_index(ch, st, st.pending_micro_path, st.pending_micro_index)) {
            st.pending_micro_due = 0;
            st.pending_micro_index = -1;
            st.pending_micro_path[0] = '\0';
          } else {
            st.pending_micro_due = 0;
            st.pending_micro_index = -1;
            st.pending_micro_path[0] = '\0';
          }
        } else {
          st.pending_micro_due = 0;
        }
      }
    }
  }
}

namespace InnerVoice {
  void schedule_intro_for_newbie(struct char_data* ch, int delay_seconds) {
    if (!ch || IS_NPC(ch)) return;
    VoiceState& st = g_state[GET_IDNUM(ch)];
    if (st.intro_shown) return;  // already shown
    if (delay_seconds < 0) delay_seconds = 0;
    st.pending_intro_due = time(0) + delay_seconds;
  }


  void notify_low_health(char_data* ch) {
    if (!ch || IS_NPC(ch)) return;
    VoiceState& st = g_state[GET_IDNUM(ch)];
    if (!can_speak_entry_now(st)) return;
    std::vector<std::string> lines;
    if (load_lines("lib/etc/innervoice/combat/lowhealth.txt", lines)) {
      if (lines.size() > 100) lines.resize(100);
      maybe_speak_item_line(ch, lines);
      adjust_attitude(ch, number(1,5));
      set_new_cooldown_entry(st);
    }
  }

  void notify_enemy_death(char_data* killer, long mob_vnum) {
    if (!killer || IS_NPC(killer)) return;
    VoiceState& st = g_state[GET_IDNUM(killer)];
    if (!can_speak_entry_now(st)) return;
    char buf[128];
    snprintf(buf, sizeof(buf), "lib/etc/innervoice/combat/mobs/%ld-", mob_vnum);
    std::vector<std::string> lines;
    if (!load_item_lines_by_prefix(std::string(buf), lines, nullptr)) return;
    maybe_speak_item_line(killer, lines);
    adjust_attitude(killer, number(1,5));
    set_new_cooldown_entry(st);
  }

  void maybe_grant_emote_attitude(struct char_data* ch) {
    if (!ch || IS_NPC(ch)) return;
    VoiceState& st = g_state[GET_IDNUM(ch)];
    time_t now = time(0);
    const int COOLDOWN = 5 * 60;
    if (st.last_emote_award_ts && now - st.last_emote_award_ts < COOLDOWN)
      return;
    if (number(1,100) <= 25) { // 25% chance
      int gain = number(1, 100);
      if (st.attitude + gain > MAX_ATTITUDE) st.attitude = MAX_ATTITUDE;
      else st.attitude += gain;
      st.last_emote_award_ts = now;
    }
  }
}


static bool iv_is_valid_name(const char* s) {
  if (!s || !*s) return false;
  int len = 0;
  for (const char* p = s; *p; p++) {
    if (!(isalpha(*p) || *p=='-' || *p=='_' )) return false;
    len++;
    if (len > 20) return false;
  }
  return len >= 2;
}

namespace InnerVoice {
  void maybe_parse_rename_from_speech(struct char_data* ch, const char* said) {
    if (!ch || IS_NPC(ch) || !said) return;
    VoiceState& st = g_state[GET_IDNUM(ch)];
    if (st.attitude < 500000) return; // gating
    // Accept patterns: "entity, your name is X" or "[name], your name is X"
    char lowerbuf[MAX_STRING_LENGTH];
    strncpy(lowerbuf, said, sizeof(lowerbuf)-1);
    lowerbuf[sizeof(lowerbuf)-1] = '\0';
    for (char *p = lowerbuf; *p; p++) *p = LOWER(*p);
    const char* current = *st.entity_name ? st.entity_name : "entity";
    char prefix1[64]; snprintf(prefix1, sizeof(prefix1), "%s, your name is ", "entity");
    char prefix2[64]; snprintf(prefix2, sizeof(prefix2), "%s, your name is ", current);
    const char* start = NULL;
    if (!strncmp(lowerbuf, prefix1, strlen(prefix1))) start = said + strlen(prefix1);
    else if (!strncmp(lowerbuf, prefix2, strlen(prefix2))) start = said + strlen(prefix2);
    if (!start) return;
    // Trim spaces
    while (*start == ' ') start++;
    char namebuf[32]; int i=0;
    for (; *start && *start!='.' && *start!='!' && *start!='?'; start++) {
      if (i < (int)sizeof(namebuf)-1) namebuf[i++] = *start;
    }
    namebuf[i] = '\0';
    // Strip trailing spaces
    for (int j=i-1; j>=0 && isspace(namebuf[j]); --j) namebuf[j]='\0';
    if (!iv_is_valid_name(namebuf)) return;
    strncpy(st.entity_name, namebuf, sizeof(st.entity_name)-1);
    st.entity_name[sizeof(st.entity_name)-1] = '\0';
    speak_line(ch, "i have a name now. i like it. i hope it likes me back.");
  }
}


namespace InnerVoice {
  void introduce_in_chargen(struct char_data* ch) {
    if (!ch || IS_NPC(ch)) return;
    VoiceState& st = g_state[GET_IDNUM(ch)];
    if (st.intro_shown) return;
    // Intro lines
    speak_line(ch, "oh! hello. i'm Entity. i live in the empty spaces between your thoughts.");
    speak_line(ch, "i'm hitching a ride—purely scientific. you’ll show me the world. consent is... implied.");
    speak_line(ch, "i admire style. dramatic exits. quiet kindness. the way citizens collide and make stories.");
    speak_line(ch, "in return, i’ll whisper helpful things. sometimes even do them. i’m new at ‘human’, but i’m eager.");
    st.intro_shown = true;
  }
}


namespace InnerVoice {
  static const char *IV_NAME_DIR = "lib/etc/innervoice/names";
  static void iv_ensure_dir_exists() {
#ifdef _WIN32
    _mkdir(IV_NAME_DIR);
#else
    mkdir(IV_NAME_DIR, 0775);
#endif
  }
  void persist_state(struct char_data* ch) {
    if (!ch || IS_NPC(ch)) return;
    VoiceState& st = g_state[GET_IDNUM(ch)];
    iv_ensure_dir_exists();
    char path[256];
    snprintf(path, sizeof(path), "%s/%ld.txt", IV_NAME_DIR, GET_IDNUM(ch));
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "%s\n", st.entity_name && *st.entity_name ? st.entity_name : "Entity");
    fclose(f);
  }
  void load_state(struct char_data* ch) {
    if (!ch || IS_NPC(ch)) return;
    VoiceState& st = g_state[GET_IDNUM(ch)];
    char path[256];
    snprintf(path, sizeof(path), "%s/%ld.txt", IV_NAME_DIR, GET_IDNUM(ch));
    FILE *f = fopen(path, "r");
    if (!f) { strncpy(st.entity_name, "Entity", sizeof(st.entity_name)-1); return; }
    char buf[64]; buf[0] = '\0';
    if (fgets(buf, sizeof(buf), f)) {
      size_t len = strlen(buf);
      while (len && (buf[len-1]=='\n' || buf[len-1]=='\r')) buf[--len] = '\0';
      if (len >= 2 && len < sizeof(st.entity_name)) strncpy(st.entity_name, buf, sizeof(st.entity_name)-1);
      else strncpy(st.entity_name, "Entity", sizeof(st.entity_name)-1);
    } else {
      strncpy(st.entity_name, "Entity", sizeof(st.entity_name)-1);
    }
    fclose(f);
  }
}


static bool maybe_speak_from_file_return(char_data* ch, int skill, const char* suffix) {
  if (!ch || IS_NPC(ch)) return false;
  const long id = GET_IDNUM(ch);
  VoiceState& st = g_state[id];
  if (!can_speak_entry_now(st)) return false;
  std::vector<std::string> lines;
  const std::string path = skill_file_base(skill, suffix);
  if (!load_lines(path, lines)) {
    load_lines(std::string("lib/etc/innervoice/skills/000-skill-generic-") + (suffix ? suffix : "use") + ".txt", lines);
  }
  if (!lines.empty()) {
    const int idx = irand(0, (int)lines.size()-1);
    speak_line(ch, lines[idx]);
    set_new_cooldown_entry(st);
    return true;
  }
  return false;
}

namespace InnerVoice {

  void notify_eat(struct char_data* ch, struct obj_data* food) {
    // Reuse generic item-interaction pipeline with "eat" action.
    notify_item_interaction(ch, food, "eat");
  }

  void notify_wear(struct char_data* ch, struct obj_data* obj, int /*where*/) {
    // Reuse generic item-interaction pipeline with "wear" action.
    notify_item_interaction(ch, obj, "wear");
  }

  void notify_social(struct char_data* ch, struct char_data* vict) {
    if (!ch || IS_NPC(ch)) return;
    const long id = GET_IDNUM(ch);
    VoiceState& st = g_state[id];
    time_t now = time(0);
    // Light attitude reward on social interaction no more than once per 5 minutes.
    if (now - st.last_emote_award_ts >= 300) {
      adjust_attitude(ch, number(1, 5));
      st.last_emote_award_ts = now;
    }
  }

  void notify_room_ambient(struct char_data* ch, int room_rnum) {
    if (!ch || IS_NPC(ch)) return;
    if (room_rnum < 0) return;
    VoiceState& st = g_state[GET_IDNUM(ch)];
    // Respect cooldown: only occasionally enqueue micro ambient comments.
    if (!can_speak_now(st) || !chance(10)) return;

    // Defer to the broader street microevent system for theming and QoL guards.
    maybe_trigger_street_microevent(ch);
    set_new_cooldown_entry(st);
  }
}
