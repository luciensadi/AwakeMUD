#ifndef _drugs_h_
#define _drugs_h_

void    do_drug_take(struct char_data *ch, struct obj_data *obj);
void    reset_drug_for_char(struct char_data *ch, int drugval);
void    reset_all_drugs_for_char(struct char_data *ch);
int     get_drug_pain_resist_level(struct char_data *ch);
void    process_drug_limit_tick(struct char_data *ch);
int     get_drug_pain_resist_level(struct char_data *ch);
void    apply_drug_modifiers_to_ch(struct char_data *ch);

#define GET_DRUG_EDGE(ch, i)                       (ch->player_specials->drugs[i][0])
#define GET_DRUG_ADDICT(ch, i)                     (ch->player_specials->drugs[i][1])
#define GET_DRUG_LIFETIME_DOSES(ch, i)             (ch->player_specials->drugs[i][2])
#define GET_DRUG_LAST_FIX(ch, i)                   (ch->player_specials->drugs[i][3])
#define GET_DRUG_ADDICTION_TICK_COUNTER(ch, i)     (ch->player_specials->drugs[i][4])
#define GET_DRUG_TOLERANCE_LEVEL(ch, i)            (ch->player_specials->drugs[i][5])
#define GET_DRUG_LAST_WITHDRAWAL(ch, i)            (ch->player_specials->drugs[i][6])
#define GET_DRUG_DURATION(ch, i)                   (ch->player_specials->drugs[i][7])
#define GET_DRUG_DOSE(ch, i)                       (ch->player_specials->drugs[i][8])
#define GET_DRUG_STAGE(ch, i)                      (ch->player_specials->drugs[i][9])

#define MIN_DRUG 1
#define DRUG_ACTH  1
#define DRUG_HYPER  2
#define DRUG_JAZZ  3
#define DRUG_KAMIKAZE  4
#define DRUG_PSYCHE  5
#define DRUG_BLISS  6
#define DRUG_BURN  7
#define DRUG_CRAM  8
#define DRUG_NITRO  9
#define DRUG_NOVACOKE  10
#define DRUG_ZEN  11
#define NUM_DRUGS       12

#define DRUG_STAGE_UNAFFECTED        0
#define DRUG_STAGE_ONSET             1
#define DRUG_STAGE_COMEDOWN          2
#define DRUG_STAGE_GUIDED_WITHDRAWAL 3
#define DRUG_STAGE_FORCED_WITHDRAWAL 4

struct drug_data {
  char name[9];
  unsigned char power;
  unsigned char damage_level;
  unsigned char mental_addiction;
  unsigned char physical_addiction;
  unsigned char tolerance;
  unsigned char edge_preadd;
  unsigned char edge_posadd;
  unsigned char fix;

  drug_data() :
    power(0), level(0), mental_addiction(0), physical_addiction(0), tolerance(0),
    edge_preadd(0), edge_posadd(0), fix(0)
  {
    memset(name, 0, sizeof(char) * MAX_DRUG_NAME_LENGTH);
  }
};

extern struct drug_data drug_types[];

#endif
