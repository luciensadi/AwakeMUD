#ifndef __moderation_h__
#define __moderation_h__

#define AUTOMOD_FREEZE_THRESHOLD  2

#define GET_AUTOMOD_COUNTER(ch)  (((ch) && (ch)->player_specials) ? (ch)->player_specials.automod_counter : 0)

// This is only run once in comm.cpp at boot.
void prepare_compiled_regexes();

bool check_for_banned_content(const char *msg, struct char_data *speaker);

#endif