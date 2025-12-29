#include "structs.hpp"
#include "db.hpp"
#include "olc.hpp"
#include "handler.hpp"
#include "creative_works.hpp"

#define CH d->character
#define ART d->edit_obj

void create_art_main_menu(struct descriptor_data *d) {
  CLS(CH);
  send_to_char(CH, "Welcome to art creation. ^WNo ASCII art, please.^n\r\n");
  send_to_char(CH, "1) ^cName: ^n%s^n\r\n", GET_OBJ_NAME(ART));
  send_to_char(CH, "2) ^cDescription: ^n%s^n\r\n", ART->photo ? ART->photo : ART->text.look_desc);
  send_to_char(CH, "3) ^cRoom Description: ^g%s^n\r\n", ART->graffiti ? ART->graffiti : ART->text.room_desc);
  send_to_char(CH, "q) ^cSave and Quit^n\r\n");
  send_to_char(CH, "Enter Option: ");

  STATE(d) = CON_ART_CREATE;
  d->edit_mode = ART_EDIT_MAIN;
}

void art_edit_parse(struct descriptor_data *d, const char *arg) {
  switch(d->edit_mode) {
    case ART_EDIT_MAIN:
      switch (*arg) {
        case '1':
          send_to_char(CH, "Enter artwork's name: ");
          d->edit_mode = ART_EDIT_NAME;
          break;
        case '2':
          send_to_char(CH, "Enter artwork's look description:\r\n");
          d->edit_mode = ART_EDIT_DESC;
          DELETE_D_STR_IF_EXTANT(d);
          INITIALIZE_NEW_D_STR(d);
          d->max_str = MAX_MESSAGE_LENGTH;
          d->mail_to = 0;
          break;
        case '3':
          send_to_char(CH, "Enter artwork's room description: ");
          d->edit_mode = ART_EDIT_ROOMDESC;
          break;
        case 'q':
        case 'Q':
          GET_OBJ_EXTRA(ART).SetBit(ITEM_EXTRA_KEPT);
          obj_to_char(ART, CH);
          mudlog_vfprintf(CH, LOG_GRIDLOG, "%s created an artwork named '%s'.", GET_CHAR_NAME(CH), GET_OBJ_NAME(ART));
          log_vfprintf("Art desc: '''%s'''\r\nArt room desc: '''%s'''", ART->photo ? ART->photo : "<default>", ART->graffiti ? ART->graffiti : "<default>");
          ART = NULL;
          STATE(d) = CON_PLAYING;
          send_to_char(CH, "Saving artwork.\r\n");
          break;
      }
      break;
    case ART_EDIT_NAME:
    case ART_EDIT_ROOMDESC:
      int length_with_no_color = get_string_length_after_color_code_removal(arg, CH);

      // Silent failure: We already sent the error message in get_string_length_after_color_code_removal().
      if (length_with_no_color == -1) {
        create_art_main_menu(d);
        return;
      }
      if (length_with_no_color >= LINE_LENGTH) {
        send_to_char(CH, "That string is too long, please shorten it. The maximum length after color code removal is %d characters.\r\n", LINE_LENGTH - 1);
        create_art_main_menu(d);
        return;
      }

      if (strlen(arg) >= MAX_RESTRING_LENGTH) {
        send_to_char(CH, "That string is too long, please shorten it. The maximum length with color codes included is %d characters.\r\n", MAX_RESTRING_LENGTH - 1);
        create_art_main_menu(d);
        return;
      }

      if (d->edit_mode == ART_EDIT_NAME) {
        DELETE_ARRAY_IF_EXTANT(ART->restring);
        ART->restring = str_dup(arg);
      } else {
        char replaced_colors[MAX_INPUT_LENGTH + 1];
        replace_substring(arg, buf2, "^n", "^g");
        replace_substring(buf2, replaced_colors, "^N", "^g");
        DELETE_ARRAY_IF_EXTANT(ART->graffiti);
        ART->graffiti = str_dup(replaced_colors);
      }
      
      create_art_main_menu(d);
      break;
  }
}

#undef ART

#define CREATE_ART_COST 500
void create_art(struct char_data *ch) {
  FAILURE_CASE(PLR_FLAGGED(ch, PLR_BLACKLIST), "You can't do that while blacklisted.");
  FAILURE_CASE_PRINTF(GET_NUYEN(ch) < CREATE_ART_COST, "It will cost you %d nuyen in supplies to create a work of art.", CREATE_ART_COST);
  FAILURE_CASE(!ch->desc || ch->desc->regenerating_art_quota <= 0, "You've created art and/or tagged too much recently. Please wait a while before making more art.");

  ch->desc->regenerating_art_quota--;
  lose_nuyen(ch, CREATE_ART_COST, NUYEN_OUTFLOW_DECORATING);

  struct obj_data *art = read_object(OBJ_CUSTOM_ART, VIRTUAL, OBJ_LOAD_REASON_CREATE_ART);
  GET_ART_AUTHOR_IDNUM(art) = GET_IDNUM_EVEN_IF_PROJECTING(ch);

  ch->desc->edit_obj = art;
  create_art_main_menu(ch->desc);
}