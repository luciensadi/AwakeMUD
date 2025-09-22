#pragma once

#define MAX_ATTITUDE 1000000


#include <ctime>
#include <string>

struct char_data;

enum class VoiceTrigger {
  AfterMove,
  AfterCommand,
  VehicleEvent,
  QuestStart,
  QuestStep,
  QuestComplete,
  CombatWin,
  AboutToLose,
  AmbientSeen,
  EnterHouse,
  SpokeToNPC,
  SkillUse,
  SkillSuccess,
  SkillFail
};

struct VoiceContext {
  const char* cmd = nullptr;
  int skill = 0;
  int dice = 0;
};

namespace InnerVoice {
  // Randomized global cooldown per player: 30s..20m
  void heartbeat_all_players();

  // Skill hooks
  void notify_skill_used(char_data* ch, int skill);
  void notify_skill_result_from_success_test(char_data* ch, int total_successes, int dice, bool botched);

  // Attitude API (0..1,000,000); positive means they "like" the player more.
  long get_attitude(char_data* ch);
  void set_attitude(char_data* ch, long value);
  void adjust_attitude(char_data* ch, long delta);
}


struct obj_data;

namespace InnerVoice {
  void notify_item_interaction(char_data* ch, obj_data* obj, const char* action);
  void notify_door_interaction(char_data* ch, int subcmd, int dir);
}


namespace InnerVoice {
  // Called after a successful room change; pass the destination room rnum.
  void notify_room_move(char_data* ch, int room_rnum);
}


namespace InnerVoice {
  void notify_low_health(char_data* ch);
  void notify_enemy_death(char_data* killer, long mob_vnum);
}


namespace InnerVoice {
  void notify_vehicle_travel_tick(struct char_data* ch, struct veh_data* veh);
}


namespace InnerVoice {
  void notify_quest_accept(struct char_data* ch, long quest_vnum);
  void notify_quest_complete(struct char_data* ch, long quest_vnum);
}


namespace InnerVoice {
  void queue_micro_comment(struct char_data* ch, const char* theme, const char* variant_suffix, int line_index);
  void tick_inner_voice(void);
}


namespace InnerVoice {
  void maybe_grant_emote_attitude(struct char_data* ch);
  void maybe_parse_rename_from_speech(struct char_data* ch, const char* said);
  void introduce_in_chargen(struct char_data* ch);
}


namespace InnerVoice {
  void persist_state(struct char_data* ch);
  void load_state(struct char_data* ch);
}


namespace InnerVoice {
  void notify_eat(struct char_data* ch, struct obj_data* food);
  void notify_drink(struct char_data* ch, struct obj_data* drink);
  void notify_wear(struct char_data* ch, struct obj_data* obj, int where);
  void notify_remove(struct char_data* ch, struct obj_data* obj, int where);
  void notify_social(struct char_data* ch, struct char_data* vict);
  void notify_room_ambient(struct char_data* ch, int room_rnum);
  void notify_ghost_tag(struct char_data* ch);
  // Schedule the first-time intro to run after a delay (in seconds).
  void schedule_intro_for_newbie(struct char_data* ch, int delay_seconds = 5);
  void append_score_readout(struct char_data* ch, char* outbuf, size_t outbuf_len);
}

void innervoice_tier2_combat_assist(struct char_data *ch);
