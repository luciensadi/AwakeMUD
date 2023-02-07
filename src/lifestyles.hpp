#ifndef __LIFESTYLES_HPP__
#define __LIFESTYLES_HPP__

#define LIFESTYLE_STREETS         0
#define LIFESTYLE_SQUATTER        1
#define LIFESTYLE_LOW             2
#define LIFESTYLE_MIDDLE          3
#define LIFESTYLE_HIGH            4
#define LIFESTYLE_LUXURY          5
#define NUM_LIFESTYLES            6

// The lifestyle data structure itself.
struct lifestyle_data {
  const char *name;
  std::vector<const char *> default_strings_neutral;
  std::vector<const char *> default_strings_gendered;
  std::vector<const char *> garage_strings_neutral;
  std::vector<const char *> garage_strings_gendered;
  long monthly_cost_min;
};

// Variables:
extern struct lifestyle_data lifestyles[NUM_LIFESTYLES];

// Function prototypes:
extern const char *get_lifestyle_string(struct char_data *ch);
extern void set_lifestyle_string(struct char_data *ch, const char *str);
extern void determine_lifestyle(struct char_data *ch);
extern void cedit_lifestyle_parse(struct descriptor_data *d, char *arg);
extern void cedit_lifestyle_menu(struct descriptor_data *d);
extern void load_lifestyles();

#endif // __LIFESTYLES_HPP__
