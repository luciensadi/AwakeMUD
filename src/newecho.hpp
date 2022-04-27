#ifndef _newecho_h_
#define _newecho_h_

const char *get_random_word_from_lexicon(int language_skill);

bool has_required_language_ability_for_sentence(struct char_data *ch, const char *message, int language_skill);

const char *replace_too_long_words(struct char_data *ch, struct char_data *speaker, const char *message, int language_skill, const char *terminal_code, bool override_skill_to_zero=FALSE);

// Deprecated: Use replace_too_long_words so you preserve the proper nouns from the sentence.
//const char *generate_random_lexicon_sentence(int language_skill, int length);

#endif
