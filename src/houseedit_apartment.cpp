#include "awake.hpp"
#include "structs.hpp"
#include "interpreter.hpp"
#include "utils.hpp"
#include "db.hpp"
#include "newdb.hpp"
#include "olc.hpp"
#include "newhouse.hpp"

///////////////////////////// OLC functions below //////////////////////////////
void houseedit_display_apartment_edit_menu(struct descriptor_data *d) {
  // TODO
  
  // Set editing state.
  STATE(d) = CON_HOUSEEDIT_APARTMENT;
  d->edit_mode = HOUSEEDIT_APARTMENT_MAIN_MENU;
}

void houseedit_apartment_parse(struct descriptor_data *d, const char *arg) {
  switch (d->edit_mode) {
    case HOUSEEDIT_APARTMENT_MAIN_MENU:
      break;
    default:
      mudlog_vfprintf(CH, LOG_SYSLOG, "SYSERR: Unknown edit state %d in houseedit_apartment_parse()! Restoring to main menu.", d->edit_mode);
      houseedit_display_apartment_edit_menu(d);
      break;
  }
}
