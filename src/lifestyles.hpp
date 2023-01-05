#ifndef __LIFESTYLES_HPP__
#define __LIFESTYLES_HPP__

#define LIFESTYLE_STREETS         0
#define LIFESTYLE_SQUATTER        1
#define LIFESTYLE_LOW             2
#define LIFESTYLE_MIDDLE          3
#define LIFESTYLE_HIGH            4
#define LIFESTYLE_LUXURY          5
#define NUM_LIFESTYLES            6

#define NUM_STRINGS_PER_LIFESTYLE 6
#define NUM_GARAGE_STRINGS        3

// The actual lifestyle number (ex LIFESTYLE_SQUATTER)
#define GET_LIFESTYLE_STRING(ch) ((ch)->player_specials ? ((ch)->player_specials->saved.lifestyle_string ? (ch)->player_specials->saved.lifestyle_string : "<no lifestyle string>") : "<no player specials>")
// NOTE: Any time you use the below settable, you MUST confirm that the target has player_specials available!
#define GET_SETTABLE_LIFESTYLE_STRING(ch) ((ch)->player_specials->saved.lifestyle_string)

// The lifestyle data structure itself.
struct lifestyle_data {
  const char *name;
  std::vector<const char *> default_strings;
  std::vector<const char *> garage_strings;
  long monthly_cost_min;
};

// Variables:
extern struct lifestyle_data lifestyles[NUM_LIFESTYLES];

// Function prototypes:
extern const char *get_lifestyle_string(struct char_data *ch);
extern void determine_lifestyle(struct char_data *ch);
extern void cedit_lifestyle_parse(struct descriptor_data *d, char *arg);

#endif // __LIFESTYLES_HPP__
