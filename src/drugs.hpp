#ifndef _drugs_h_
#define _drugs_h_

bool    do_drug_take(struct char_data *ch, struct obj_data *obj);
void    reset_drug_for_char(struct char_data *ch, int drugval);
void    reset_all_drugs_for_char(struct char_data *ch);
int     get_drug_pain_resist_level(struct char_data *ch);
bool    process_drug_point_update_tick(struct char_data *ch);
void    apply_drug_modifiers_to_ch(struct char_data *ch);
void    render_drug_info_for_targ(struct char_data *ch, struct char_data *targ);
void    process_withdrawal(struct char_data *ch);
void    attempt_safe_withdrawal(struct char_data *ch, const char *target_arg);
float   get_drug_heal_multiplier(struct char_data *ch);
const char* get_time_until_withdrawal_ends(struct char_data *ch, int drug_id);

void    reset_all_drugs_for_char(struct char_data *ch);
void    clear_all_drug_data_for_char(struct char_data *ch);

#define GET_DRUG_ADDICTION_EDGE(ch, i)             (ch->player_specials->drugs[i][0])
#define GET_DRUG_ADDICT(ch, i)                     (ch->player_specials->drugs[i][1])
#define GET_DRUG_LIFETIME_DOSES(ch, i)             (ch->player_specials->drugs[i][2])
#define GET_DRUG_ADDICTION_TICK_COUNTER(ch, i)     (ch->player_specials->drugs[i][3])
#define GET_DRUG_TOLERANCE_LEVEL(ch, i)            (ch->player_specials->drugs[i][4])
#define GET_DRUG_LAST_WITHDRAWAL_TICK(ch, i)       (ch->player_specials->drugs[i][5])
#define GET_DRUG_DURATION(ch, i)                   (ch->player_specials->drugs[i][6])
#define GET_DRUG_DOSE(ch, i)                       (ch->player_specials->drugs[i][7])
#define GET_DRUG_STAGE(ch, i)                      (ch->player_specials->drugs[i][8])
#define NUM_DRUG_PLAYER_SPECIAL_FIELDS                                            9

#define GET_DRUG_LAST_FIX(ch, i)                   (ch->player_specials->drug_last_fix[i])

#define MIN_DRUG                     1
#define DRUG_ACTH                    1
#define DRUG_HYPER                   2
#define DRUG_JAZZ                    3
#define DRUG_KAMIKAZE                4
#define DRUG_PSYCHE                  5
#define DRUG_BLISS                   6
#define DRUG_BURN                    7
#define DRUG_CRAM                    8
#define DRUG_NITRO                   9
#define DRUG_NOVACOKE                10
#define DRUG_ZEN                     11
#define NUM_DRUGS                    12

#define MAX_DRUG_DOSE_COUNT         100

#define DRUG_STAGE_UNAFFECTED        0
#define DRUG_STAGE_ONSET             1
#define DRUG_STAGE_COMEDOWN          2
#define DRUG_STAGE_GUIDED_WITHDRAWAL 3
#define DRUG_STAGE_FORCED_WITHDRAWAL 4

#define NOT_ADDICTED                 0
#define IS_ADDICTED                  1

#define MAX_DRUG_NAME_LENGTH         12
#define MAX_DELIVERY_METHOD_LENGTH   10

struct drug_data {
  char name[MAX_DRUG_NAME_LENGTH];
  unsigned char power;
  unsigned char damage_level;
  unsigned char mental_addiction_rating;
  unsigned char physical_addiction_rating;
  unsigned char tolerance;
  unsigned char edge_preadd;
  unsigned char edge_posadd;
  unsigned char fix_factor;
  int cost;
  float street_idx;
  char delivery_method[MAX_DELIVERY_METHOD_LENGTH];
};

extern struct drug_data drug_types[];

#endif
