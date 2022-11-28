#include "awake.hpp"
#include "structs.hpp"
#include "interpreter.hpp"
#include "utils.hpp"
#include "db.hpp"
#include "newdb.hpp"
#include "newhouse.hpp"

int count_objects(struct obj_data *obj) {
  if (!obj)
    return 0;

  int total = 1; // self

  for (struct obj_data *contents = obj->contains; contents; contents = contents->next_content)
    total += count_objects(contents);

  return total;
}

/* The hcontrol command itself, used by imms to create/destroy houses */

const char *HCONTROL_FORMAT =
  "Usage:  hcontrol destroy <full apartment name with complex>\r\n"
  "        hcontrol show\r\n"
  "        hcontrol show <full apt name | character name>\r\n";

void hcontrol_list_houses(struct char_data *ch) {
  for (auto &complex : global_apartment_complexes) {
    for (auto &apartment : complex->get_apartments()) {
      // TODO: Add back in info like formatting, guest count, crap count etc
      const char *owner_name = apartment->get_owner_name__returns_new();

      if (!owner_name)
        owner_name = str_dup("<UNDEF>");

      send_to_char(ch, "%s: owner %s (%ld)\r\n", apartment->get_full_name(), CAP(owner_name), apartment->get_owner_id());

      DELETE_ARRAY_IF_EXTANT(owner_name);
    }
  }
}

void hcontrol_destroy_house(struct char_data * ch, char *arg) {
  if (!*arg) {
    send_to_char(HCONTROL_FORMAT, ch);
    return;
  }

  for (auto &complex : global_apartment_complexes) {
    for (auto &apartment : complex->get_apartments()) {
      if (!str_cmp(arg, apartment->get_full_name())) {
        {
          const char *name = apartment->get_owner_name__returns_new();
          send_to_char(ch, "OK, breaking lease for apartment %s (owned by %s).\r\n", apartment->get_full_name(), name);
          mudlog_vfprintf(ch, LOG_SYSLOG, "Destroying lease for %s (owned by %s) via HCONTROL DESTROY.", apartment->get_full_name(), name);
          delete [] name;
        }

        apartment->break_lease();
        return;
      }
    }
  }
}

void hcontrol_display_house_by_name(struct char_data * ch, vnum_t house_number) {
  for (auto &complex : global_apartment_complexes) {
    for (auto &apartment : complex->get_apartments()) {
      if (!str_cmp(arg, apartment->get_full_name())) {
        {
          const char *owner_name = apartment->get_owner_name__returns_new();
          send_to_char(ch, "%s is owned by ^c%s^n (^c%ld^n).\r\n",
                       apartment->get_full_name(),
                       owner_name,
                       apartment->get_owner_id());
          delete [] owner_name;
        }

        send_to_char(ch, "It is paid up until epoch ^c%ld^n.\r\n", apartment->get_paid_until());

        apartment->list_guests_to_char(ch);

        // TODO: List of rooms it encompasses and their crap count.

        // TODO: Total crap count.
        return;
      }
    }
  }


}

void hcontrol_display_house_with_owner_or_guest(struct char_data * ch, const char *name, vnum_t idnum) {
  bool printed_something = FALSE;

  send_to_char(ch, "Fetching data for owner/guest ^c%s^n (^c%ld^n)...\r\n", name, idnum);

  for (auto &complex : global_apartment_complexes) {
    for (auto &apartment : complex->get_apartments()) {
      if (apartment->has_owner_privs_by_idnum(idnum)) {
        send_to_char(ch, "- Owner privileges at %s.\r\n", apartment->get_full_name());
        printed_something = TRUE;
      } else {
        // Traditional guests.
        if (find(apartment->get_guests().begin(), apartment->get_guests().end(), idnum) != apartment->get_guests().end()) {
          const char *owner_name = apartment->get_owner_name__returns_new();
          send_to_char(ch, "- Guest at %s, owned by ^c%s^n (^c%ld^n).\r\n", apartment->get_full_name(), owner_name, apartment->get_owner_id());
          delete [] owner_name;
          printed_something = TRUE;
        }
        // Pgroup guests.
        if (apartment->get_owner_pgroup()) {
          Bitfield required_privileges;
          required_privileges.SetBits(PRIV_LEADER, PRIV_LANDLORD, PRIV_TENANT, ENDBIT);
          if (pgroup_char_has_any_priv(idnum, apartment->get_owner_pgroup()->get_idnum(), required_privileges))
          send_to_char(ch, "- Can enter %s due to pgroup privileges.\r\n", apartment->get_full_name());
        }
      }
    }
  }

  if (!printed_something) {
    send_to_char("- No records found.\r\n", ch);
  }
}


ACMD(do_hcontrol)
{
  char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
  int house_number;
  vnum_t idnum;

  skip_spaces(&argument);
  half_chop(argument, arg1, arg2);

  if (!str_cmp(arg1, "destroy")) {
    if (GET_LEVEL(ch) >= LVL_EXECUTIVE) {
      hcontrol_destroy_house(ch, arg2);
    } else {
      send_to_char("Sorry, you can't do that at your level.\r\n", ch);
    }
    return;
  }

  if (is_abbrev(arg1, "show")) {
    // With no argument, we default to the standard behavior.
    if (!*arg2) {
      hcontrol_list_houses(ch);
      return;
    }

    // If the argument is an int, we assume you want to know about a specific house.
    if ((house_number = atoi(arg2)) > 0) {
      // hcontrol_display_house_by_number(ch, house_number);
      send_to_char("Not implemented\r\n", ch);
      return;
    }

    // Otherwise, it's assumed to be a character name. Look up their houses and also houses where they're a guest.
    if ((idnum = get_player_id(arg2)) > 0) {
      // hcontrol_display_house_with_owner_or_guest(ch, capitalize(arg2), idnum);
      send_to_char("Not implemented\r\n", ch);
    } else {
      send_to_char(ch, "There is no player named '%s'.\r\n", arg2);
    }

    return;
  }

  // No valid command found.
  send_to_char("Usage: hcontrol destroy <apartment number>; or hcontrol show; or hcontrol show (<apartment number> | <character name>).\r\n", ch);
}
