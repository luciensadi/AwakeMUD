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
#define GET_LIFESTYLE_SELECTION(ch) ((ch)->player_specials ? (ch)->player_specials->saved.selected_lifestyle : LIFESTYLE_SQUATTER)
#define GET_SETTABLE_LIFESTYLE(ch) ((ch)->player_specials->saved.selected_lifestyle)
// The original version of this.
#define GET_ORIGINAL_LIFESTYLE(ch) ((ch)->player_specials ? (ch)->player_specials->original_lifestyle : LIFESTYLE_SQUATTER)
#define GET_SETTABLE_ORIGINAL_LIFESTYLE(ch) ((ch)->player_specials->original_lifestyle)

// Whether or not they live in a garage.
#define GET_LIFESTYLE_IS_GARAGE_SELECTION(ch) ((ch)->player_specials ? (ch)->player_specials->saved.selected_lifestyle_is_garage : FALSE)
#define GET_SETTABLE_LIFESTYLE_IS_GARAGE(ch) ((ch)->player_specials->saved.selected_lifestyle_is_garage)
// The original version of this.
#define GET_ORIGINAL_LIFESTYLE_IS_GARAGE(ch) ((ch)->player_specials ? (ch)->player_specials->original_lifestyle_is_garage : FALSE)
#define GET_SETTABLE_ORIGINAL_LIFESTYLE_IS_GARAGE(ch) ((ch)->player_specials->original_lifestyle_is_garage)

// The amount of time left on their impersonation command, if any.
#define GET_LIFESTYLE_DRESS_EXPIRATION_TICKS(ch) ((ch)->player_specials ? (ch)->player_specials->saved.lifestyle_dress_expiration_ticks : FALSE)
#define GET_SETTABLE_LIFESTYLE_DRESS_EXPIRATION_TICKS(ch) ((ch)->player_specials->saved.lifestyle_dress_expiration_ticks)

// The index of the lifestyle string they've selected
#define GET_LIFESTYLE_STRING_SELECTION(ch) ((ch)->player_specials ? (ch)->player_specials->saved.lifestyle_string_selection : 0)
#define GET_SETTABLE_LIFESTYLE_STRING_SELECTION(ch) ((ch)->player_specials->saved.lifestyle_string_selection)

// The index of the lifestyle string they've selected
#define GET_LIFESTYLE_GARAGE_STRING_SELECTION(ch) ((ch)->player_specials ? (ch)->player_specials->saved.lifestyle_garage_string_selection : 0)
#define GET_SETTABLE_LIFESTYLE_GARAGE_STRING_SELECTION(ch) ((ch)->player_specials->saved.lifestyle_garage_string_selection)

// The lifestyle data structure itself.
struct lifestyle_data {
  const char *name;
  int monthly_cost;
  const char *strings[NUM_STRINGS_PER_LIFESTYLE][2];
};

// Variables:
extern struct lifestyle_data lifestyles[NUM_LIFESTYLES];

// Function prototypes:
extern const char *get_lifestyle_string(struct char_data *ch);
extern void determine_lifestyle(struct char_data *ch);
