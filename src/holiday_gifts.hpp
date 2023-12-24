#ifndef __holiday_gifts__
#define __holiday_gifts__

#include "types.hpp"

#define NUM_DAYS_AFTER_HOLIDAY_FOR_GIFTS  3

class holiday_entry {
public:
   const char *holiday_name;
   int month;
   int day;
   int year;

   const char *present_message;
   const char *present_restring;
   const char *present_photo;

   vnum_t gift_vnum;

   bool is_active();
   bool already_got_award(struct char_data *ch);
   bool is_eligible(struct char_data *ch);
   void award_gift(struct char_data *ch);
};

#endif // __holiday_gifts__