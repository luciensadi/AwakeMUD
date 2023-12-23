#ifndef __moderation_h__
#define __moderation_h__

#include <regex.h>

#define AUTOMOD_FREEZE_THRESHOLD  2

#define GET_AUTOMOD_COUNTER(ch)  (((ch) && (ch)->player_specials) ? (ch)->player_specials.automod_counter : 0)

class automod_entry {
public:
    const char *stringified_regex;
    const char *plain_form;
    const char *explanation;
    regex_t *compiled_regex;

    void compile();
};

// This is only run once in comm.cpp at boot.
void prepare_compiled_regexes();

bool check_for_banned_content(const char *msg, struct char_data *speaker);

#ifdef osx
#define REGEX_SEP_START "[[:<:]]"
#define REGEX_SEP_END   "[[:>:]]"
#else
#define REGEX_SEP_START "\\b"
#define REGEX_SEP_END   "\\b"
#endif

#endif