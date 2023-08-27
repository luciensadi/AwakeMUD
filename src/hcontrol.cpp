#include <algorithm>

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

bool name_compare_func(const char *a, const char *b) {
  return strcmp(a, b) < 0;
}

const char *get_crap_count_string(int crap_count, const char *default_color = "^n") {
  static char crap_count_string[50];

  const char *crap_count_color = NULL;
  if (crap_count > 500) {
    crap_count_color = "^R";
  } else if (crap_count > 400) {
    crap_count_color = "^r";
  } else if (crap_count > 300) {
    crap_count_color = "^Y";
  } else if (crap_count > 200) {
    crap_count_color = "^y";
  } else {
    crap_count_color = "^n";
  }

  snprintf(crap_count_string, sizeof(crap_count_string), "%s%d%s item%s",
           crap_count_color,
           crap_count,
           default_color,
           crap_count == 1 ? "" : "s");

  return crap_count_string;
}

/* The hcontrol command itself, used by imms to create/destroy houses */
const char *HCONTROL_FORMAT =
  "Usage:  hcontrol destroy <full apartment name with complex>\r\n"
  "        hcontrol show\r\n"
  "        hcontrol show <full apt name | character name>\r\n";

void hcontrol_list_houses(struct char_data *ch) {
  char compose_buf[1000];
  std::vector<const char *> entries = {};

  for (auto &complex : global_apartment_complexes) {
    for (auto &apartment : complex->get_apartments()) {
      if (apartment->get_paid_until() > 0) {
        const char *owner_name = apartment->get_owner_name__returns_new();

        if (!owner_name) {
#ifdef IS_BUILDPORT
          owner_name = str_dup("<owner not loaded>");
#else
          owner_name = str_dup("<BAD OWNER>");
#endif
        }

        snprintf(compose_buf, sizeof(compose_buf), "%s (%ld): %s^n (%s, %ld guest%s)\r\n", 
                 CAP(owner_name), 
                 apartment->get_owner_id(), 
                 apartment->get_full_name(),
                 get_crap_count_string(apartment->get_crap_count()),
                 apartment->get_guests().size(),
                 apartment->get_guests().size() == 1 ? "" : "s");
        entries.push_back(str_dup(compose_buf));

        DELETE_ARRAY_IF_EXTANT(owner_name);
      }
    }
  }

  sort(entries.begin(), entries.end(), name_compare_func);

  for (auto &entry : entries)
    send_to_char(entry, ch);
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

        // TODO: Find the owner and notify them, if online. Otherwise, send a mail.

        apartment->break_lease();
        return;
      }
    }
  }
  send_to_char("No apartment matched your search. Please use the exact and full name of the apartment, like 'Triple Tree Inn - Garages's Unit B2'.\r\n", ch);
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

        send_to_char(ch, "It consists of the following room%s:\r\n", apartment->get_rooms().size() == 1 ? "" : "s");
        for (auto &room : apartment->get_rooms()) {
          rnum_t rnum = real_room(room->get_vnum());
          if (rnum < 0) {
            send_to_char(ch, "    n/a^n: ERRONEOUS (invalid vnum %ld)\r\n", room->get_vnum());
          } else {
            struct room_data *world_room = &world[rnum];

            // TODO: Fill out crap count.
            int crap_count_obj = -1, crap_count_veh = -1;

            send_to_char(ch, "%7ld^n: %s^n (%ld) [%d items, %d vehicles]\r\n",
                         room->get_vnum(),
                         GET_ROOM_NAME(world_room),
                         GET_ROOM_VNUM(world_room),
                         crap_count_obj,
                         crap_count_veh);
          }
        }

        send_to_char(ch, "Its crap count is %s.", get_crap_count_string(apartment->get_crap_count()));

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
        send_to_char(ch, "- Owner privileges at ^c%s^n.\r\n", apartment->get_full_name());
        printed_something = TRUE;
      } else {
        // Traditional guests.
        if (find(apartment->get_guests().begin(), apartment->get_guests().end(), idnum) != apartment->get_guests().end()) {
          const char *owner_name = apartment->get_owner_name__returns_new();
          send_to_char(ch, "- Guest at ^c%s^n, owned by ^c%s^n (^c%ld^n).\r\n", apartment->get_full_name(), owner_name, apartment->get_owner_id());
          delete [] owner_name;
          printed_something = TRUE;
        }
        // Pgroup guests.
        if (apartment->get_owner_pgroup()) {
          Bitfield required_privileges;
          required_privileges.SetBits(PRIV_LEADER, PRIV_LANDLORD, PRIV_TENANT, ENDBIT);
          if (pgroup_char_has_any_priv(idnum, apartment->get_owner_pgroup()->get_idnum(), required_privileges))
          send_to_char(ch, "- Can enter ^c%s^n due to pgroup privileges.\r\n", apartment->get_full_name());
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

    // Otherwise, it's assumed to be a character name. Look up their houses and also houses where they're a guest.
    if ((idnum = get_player_id(arg2)) > 0) {
      hcontrol_display_house_with_owner_or_guest(ch, capitalize(arg2), idnum);
    } else {
      send_to_char(ch, "There is no player named '%s'.\r\n", arg2);
    }

    return;
  }

  // No valid command found.
  send_to_char("Usage: hcontrol destroy <apartment number>; or hcontrol show; or hcontrol show (<apartment number> | <character name>).\r\n", ch);
}
