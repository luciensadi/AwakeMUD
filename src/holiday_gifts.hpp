#ifndef __holiday_gifts__
#define __holiday_gifts__

#include "types.hpp"

class holiday_entry {
public:
   const char *holiday_name;
   time_t start_epoch;
   time_t end_epoch;

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