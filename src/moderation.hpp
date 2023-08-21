#ifndef __moderation_h__
#define __moderation_h__

// This is only run once in comm.cpp at boot.
void prepare_compiled_regexes();

bool check_for_banned_content(const char *msg, struct char_data *speaker);

#endif