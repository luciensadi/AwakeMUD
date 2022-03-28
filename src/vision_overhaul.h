/* General design: All characters have a vision array that takes NONE, IS_NATURAL, IS_ARTIFICIAL.
   Vision-using code calls functions from here to see what level of vision they have.
   We also rip out the vision mods from the current penalty function and put them here for centralization. */

#ifndef _VISION_H_
#define _VISION_H_

// Vision is done with bitfields. No bits set mean that vision type is not available.
#define VISION_BIT_IS_NATURAL        0
#define VISION_BIT_FROM_IMPLANTS     1
#define VISION_BIT_FROM_EQUIPMENT    2
#define VISION_BIT_IS_OPTICAL        3  /* We gained this vision through purely optical means- it does not block magic. */
#define VISION_BIT_IS_ELECTRONIC     4  /* We gained this vision bit through electronic means- blocks magic. Overrides optical bit. */
#define VISION_BIT_IS_ADEPT_POWER    5  /* We gained this vision from an adept power. */
#define VISION_BIT_IS_SPELL          6  /* We gained this vision from a spell. */
#define VISION_BIT_OVERRIDE          7  /* Has been temporarily overridden by code. */
#define NUM_VISION_BITS              8

#define VISION_STRING_MODE_STAFF     0
#define VISION_STRING_MODE_SCORE     1
#define VISION_STRING_MODE_STATUS    2

/* Getters. */
// Do we have a given type of vision at all?
bool has_vision(struct char_data *ch, int type, bool staff_override=FALSE);

// Do we have a given type of vision in a natural form? (Used for vision penalties)
bool has_natural_vision(struct char_data *ch, int type);

// Do we have a given type of vision in a form we can cast with?
bool has_castable_vision(struct char_data *ch, int type);


/* Setters. */
void set_vision_bit(struct char_data *ch, int type, int bit);
void remove_vision_bit(struct char_data *ch, int type, int bit);
void clear_ch_vision_bits(struct char_data *ch);

/* Utilities. */
void set_natural_vision_for_race(struct char_data *ch);
void remove_racial_vision_due_to_eye_replacement(struct char_data *ch);

void apply_vision_bits_from_implant(struct char_data *ch, struct obj_data *cyber);

int get_vision_penalty(struct char_data *ch, struct room_data *temp_room, char *rbuf, int rbuf_len);

void copy_vision_from_original_to_clone(struct char_data *original, struct char_data *clone);

const char *write_vision_string_for_display(struct char_data *ch, int mode);

#endif
