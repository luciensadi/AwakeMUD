#ifndef _lexicons_h_
#define _lexicons_h_

#define LEXICON_ENGLISH    0
#define LEXICON_SPERETHIEL 1
#define LEXICON_SPANISH    2
#define LEXICON_JAPANESE   3
#define LEXICON_CHINESE    4
#define LEXICON_KOREAN     5
#define LEXICON_ITALIAN    6
#define LEXICON_RUSSIAN    7
#define LEXICON_SIOUX      8
#define LEXICON_MAKAW      9
#define LEXICON_CROW       10
#define LEXICON_SALISH     11
#define LEXICON_UTE        12
#define LEXICON_NAVAJO     13
#define LEXICON_GERMAN     14
#define LEXICON_ORZET      15
#define LEXICON_ARABIC     16
#define LEXICON_LATIN      17
#define LEXICON_GAELIC     18
#define LEXICON_FRENCH     19
#define NUM_LEXICONS       20

extern const char **lexicons[];
extern const char *lexicon_english[];
extern const char *lexicon_sperethiel[];
extern const char *lexicon_spanish[];
extern const char *lexicon_japanese[];
extern const char *lexicon_chinese[];
extern const char *lexicon_korean[];
extern const char *lexicon_italian[];
extern const char *lexicon_russian[];
extern const char *lexicon_sioux[];
extern const char *lexicon_makaw[];
extern const char *lexicon_crow[];
extern const char *lexicon_salish[];
extern const char *lexicon_ute[];
extern const char *lexicon_navajo[];
extern const char *lexicon_german[];
extern const char *lexicon_orzet[];
extern const char *lexicon_arabic[];
extern const char *lexicon_latin[];
extern const char *lexicon_gaelic[];
extern const char *lexicon_french[];

extern void populate_lexicon_size_table();
extern const char *get_random_word_from_lexicon(int language_skill);

#endif
