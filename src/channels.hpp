#ifndef __channels_hpp__
#define __channels_hpp__

#include <vector>

#include "structs.hpp"

class CommsChannel {
  const char *name;
  const char *alias;
  const char *slug;
  const char *color_code;
  const char *pre_name;
  const char *post_name;
  bool quoted_message;

  std::vector<const char *> history = {};

public:
  bool ch_is_member(struct char_data *ch);
  bool ch_is_moderator(struct char_data *ch);

  const char *render_writer_for_ch(struct char_data *writer, struct char_data *viewer);

  bool attempt_send_message(struct char_data *ch, char *msg);
};

bool send_command_as_custom_channel_message(struct char_data *ch, char *arg);

#endif // __channels_hpp__