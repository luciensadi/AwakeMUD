#ifndef __moderation_h__
#define __moderation_h__

#include <regex.h>

#define AUTOMOD_FREEZE_THRESHOLD  2

#define MODERATION_MODE_NORMAL                 0
#define MODERATION_MODE_THING_DESCRIPTIONS     1
#define MODERATION_MODE_CHARACTER_DESCRIPTIONS 2

#define GET_AUTOMOD_COUNTER(ch)  (((ch) && (ch)->player_specials) ? (ch)->player_specials->saved.automod_counter : 0)
#define GET_SETTABLE_AUTOMOD_COUNTER(ch) ((ch)->player_specials->saved.automod_counter)

class automod_entry {
public:
    const char *stringified_regex;
    const char *explanation;
    regex_t *compiled_regex;

    void compile();

    // 1. Add a constructor to support the list-initialization used in moderation.cpp
    automod_entry(const char *s, const char *e, regex_t *c) 
        : stringified_regex(s), explanation(e), compiled_regex(c) {}

    // 2. Fix the copy constructor signature to use 'const'
    // Also, ensure we don't shallow-copy the pointer to avoid a double-free
    automod_entry(const automod_entry& other) 
        : stringified_regex(other.stringified_regex), 
          explanation(other.explanation), 
          compiled_regex(nullptr) {} 

    // 3. Update the destructor to 'delete' the pointer since compile() uses 'new'
    ~automod_entry() {
        if (compiled_regex) {
            regfree(compiled_regex);
            delete compiled_regex; // Must delete if allocated with 'new' in moderation.cpp
            compiled_regex = nullptr;
        }
    }
};

// This is only run once in comm.cpp at boot.
void prepare_compiled_regexes();

bool check_for_banned_content(const char *msg, struct char_data *speaker, int moderation_mode=MODERATION_MODE_NORMAL);

#ifdef osx
#define REGEX_SEP_START "[[:<:]]"
#define REGEX_SEP_END   "[[:>:]]"
#else
#define REGEX_SEP_START "\\b"
#define REGEX_SEP_END   "\\b"
#endif

#endif