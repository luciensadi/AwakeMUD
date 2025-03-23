#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "structs.hpp"
#include "awake.hpp"
#include "interpreter.hpp"
#include "comm.hpp"
#include "constants.hpp"
#include "utils.hpp"
#include "db.hpp"
#include "screen.hpp"
#include "olc.hpp"
#include "newmatrix.hpp"
#include "memory.hpp"
#include "handler.hpp"
#include "matrix_storage.hpp"

#define HOST d->edit_host
#define NUM_OF_HOST_TYPES 6
extern char *prep_string_for_writing_to_savefile(char *dest, const char *src);

#define HT matrix[realcounter]
void write_host_to_disk(int vnum)
{
  long realcounter, counter;
  int zone = real_zone(vnum);
  FILE *fl;
  struct exit_data *exit;
  struct trigger_step *step;
  int i = 0;

  char final_file_name[1000];
  snprintf(final_file_name, sizeof(final_file_name), "%s/%d.host", MTX_PREFIX, vnum);

  char tmp_file_name[1000];
  snprintf(tmp_file_name, sizeof(tmp_file_name), "%s.tmp", final_file_name);
  
  fl = fopen(tmp_file_name, "w+");

  for (counter = zone_table[zone].number * 100;
       counter <= zone_table[zone].top;
       counter++) {
    realcounter = real_host(counter);
    if (realcounter >= 0) {
      if (!strcmp("An unfinished host", HT.name))
        continue;
      fprintf(fl, "#%ld\n", HT.vnum);
      fprintf(fl, "Name:\t%s\n", HT.name);
      fprintf(fl, "Keywords:\t%s\n", HT.keywords);
      fprintf(fl, "Description:$\n%s~\n", prep_string_for_writing_to_savefile(buf2, HT.desc));
      fprintf(fl, "ShutdownStart:\t%s\n", HT.shutdown_start);
      fprintf(fl, "ShutdownStop:\t%s\n", HT.shutdown_stop);
      fprintf(fl, "Parent:\t%ld\n", HT.parent);
      fprintf(fl, "Colour:\t%d\n", HT.color);
      fprintf(fl, "Security:\t%d\n", HT.security);
      fprintf(fl, "Difficulty:\t%d\n", HT.intrusion);
      fprintf(fl, "Access:\t%ld\n", HT.stats[ACIFS_ACCESS][0]);
      fprintf(fl, "AccessScramble:\t%ld\n", HT.stats[ACIFS_ACCESS][2]);
      fprintf(fl, "AccessTrapdoor:\t%ld\n", HT.stats[ACIFS_ACCESS][5]);
      fprintf(fl, "Control:\t%ld\n", HT.stats[ACIFS_CONTROL][0]);
      fprintf(fl, "ControlTrapdoor:\t%ld\n", HT.stats[ACIFS_CONTROL][5]);
      fprintf(fl, "Index:\t%ld\n", HT.stats[ACIFS_INDEX][0]);
      fprintf(fl, "IndexTrapdoor:\t%ld\n", HT.stats[ACIFS_INDEX][5]);
      fprintf(fl, "Files:\t%ld\n", HT.stats[ACIFS_FILES][0]);
      fprintf(fl, "FilesScramble:\t%ld\n", HT.stats[ACIFS_FILES][2]);
      fprintf(fl, "FilesTrapdoor:\t%ld\n", HT.stats[ACIFS_FILES][5]);
      fprintf(fl, "Slave:\t%ld\n", HT.stats[ACIFS_SLAVE][0]);
      fprintf(fl, "SlaveScramble:\t%ld\n", HT.stats[ACIFS_SLAVE][2]);
      fprintf(fl, "SlaveTrapdoor:\t%ld\n", HT.stats[ACIFS_SLAVE][5]);

      fprintf(fl, "Type:\t%s\n", host_type[HT.type]);
      if (HT.exit) {
        fprintf(fl, "[EXITS]\n");
        for (exit = HT.exit; exit; exit = exit->next, i++) {
          fprintf(fl, "\t[EXIT %d]\n", i);
          fprintf(fl, "\t\tExit:\t%ld\n", exit->host);
          fprintf(fl, "\t\tNumber:\t%s\n", exit->addresses);
          if (exit->roomstring && *(exit->roomstring))
            fprintf(fl, "\t\tRoomString:\t%s\n", exit->roomstring);
          fprintf(fl, "\t\tHidden:\t%s\n", exit->hidden ? "1" : "0");
        }
      }
      if (HT.trigger) {
        fprintf(fl, "[TRIGGERS]\n");
        for (step = HT.trigger, i = 0; step; step = step->next, i++) {
          fprintf(fl, "\t[TRIGGER %d]\n", i);
          fprintf(fl, "\t\tStep:\t%d\n", step->step);
          fprintf(fl, "\t\tAlert:\t%s\n", alerts[step->alert]);
          fprintf(fl, "\t\tIC:\t%ld\n", step->ic);

        }
      }
      fprintf(fl, "BREAK\n");
    }
  }

  fprintf(fl, "END\n");
  fclose(fl);
  write_index_file("host");

  // Remove old, move tmp to cover.
  remove(final_file_name);
  rename(tmp_file_name, final_file_name);
}

void hedit_disp_data_menu(struct descriptor_data *d)
{
  CLS(CH);
  send_to_char(CH, "^WHost: ^c%ld^n\r\n", d->edit_number);
  send_to_char(CH, "^G1^Y) ^WHost: ^c%s^n\r\n", HOST->name);
  if (real_host(HOST->parent) > 0)
    send_to_char(CH, "^G2^Y) ^WParent Host: ^c%s(%ld)^n\r\n", matrix[real_host(HOST->parent)].name, HOST->parent);
  else
    send_to_char(CH, "^G2^Y) ^WParent Host: ^cNowhere(0)^n\r\n");
  send_to_char(CH, "^G3^Y) ^WKeywords: ^c%s^n\r\n", HOST->keywords);
  send_to_char(CH, "^G4^Y) ^WDescription: ^c%s^n\r\n", HOST->desc);
  send_to_char(CH, "^G5^Y) ^WSecurity: ^c%s-%d^c(%s)^n\r\n", host_color[HOST->color], HOST->security,
               intrusion[HOST->intrusion]);
  send_to_char(CH, "^G6^Y) ^WType: ^c%s^n\r\n", host_type[HOST->type]);
  send_to_char(CH, "^G7^Y) ^WRatings: A(^c%d^N) C(^c%d^N) I(^c%d^N) F(^c%d^N) S(^c%d^N)\r\n",
               HOST->stats[ACIFS_ACCESS][0], HOST->stats[ACIFS_CONTROL][0], HOST->stats[ACIFS_INDEX][0],
               HOST->stats[ACIFS_FILES][0], HOST->stats[ACIFS_SLAVE][0]);
  send_to_char(CH, "^G8^Y) ^WSubsystem Extras\r\n");
  send_to_char(CH, "^G9^Y) ^WTrigger Steps^n\r\n");
  send_to_char(CH, "^G0^Y) ^WExits^n\r\n");
  send_to_char(CH, "^GA^Y) ^WShutdown Start Text: ^c%s^n\r\n", HOST->shutdown_start);
  send_to_char(CH, "^GB^Y) ^WShutdown Stop Text: ^c%s^n\r\n", HOST->shutdown_stop);
  send_to_char("^Gq^Y) ^WQuit\r\n^Gx^Y) ^WQuit without saving\r\n^wEnter selection: ", CH);
  d->edit_mode = HEDIT_MAIN_MENU;
}

void hedit_disp_subsystem_extras(struct descriptor_data *d)
{
  send_to_char(CH, "^G1^Y) ^WAccess Scramble Rating: %2ld Access Trapdoor: %ld\r\n", HOST->stats[ACIFS_ACCESS][2], HOST->stats[ACIFS_ACCESS][5]);
  send_to_char(CH, "^G2^Y) ^WControl Trapdoor: %ld\r\n", HOST->stats[ACIFS_CONTROL][5]);
  send_to_char(CH, "^G3^Y) ^WIndex Trapdoor: %ld\r\n", HOST->stats[ACIFS_INDEX][5]);
  send_to_char(CH, "^G4^Y) ^WFiles Scramble Rating: %2ld Files Trapdoor: %ld\r\n", HOST->stats[ACIFS_FILES][2], HOST->stats[ACIFS_FILES][5]);
  send_to_char(CH, "^G5^Y) ^WSlave Scramble Rating: %2ld Slave Trapdoor: %ld\r\n", HOST->stats[ACIFS_SLAVE][2], HOST->stats[ACIFS_SLAVE][5]);
  send_to_char(CH, "^Gq^Y) ^WQuit\r\n^wEnter selection: ");
  d->edit_mode = HEDIT_EXTRA_MENU;
}

void hedit_disp_trigger_menu(struct descriptor_data *d)
{
  struct trigger_step *trigger;
  int i = 1;
  CLS(CH);
  for (trigger = HOST->trigger; trigger; trigger = trigger->next)
  {
    send_to_char(CH, "^G%d^Y) ^WStep ^c%d^n Go to alert ^c%s^n Load IC ^c%s(%ld)^n\r\n",
                 i, trigger->step, alerts[trigger->alert], trigger->ic ? ic_proto[real_ic(trigger->ic)].name : "None", trigger->ic);
    i++;
  }
  send_to_char("\r\n^Ga^Y) ^WAdd new trigger\r\n^Gd^Y) ^WDelete a trigger\r\n^Gq^Y) ^WQuit\r\n^wEnter selection: ", CH);
  d->edit_mode = HEDIT_TRIGGER;
}

void hedit_disp_exit_menu(struct descriptor_data *d)
{
  struct exit_data *exit;
  int i = 0;
  CLS(CH);
  for (exit = HOST->exit; exit; exit = exit->next)
  {
    send_to_char(CH, "^G%d^Y) ^c%s^N(^c%ld^N) ^c%s^n%s: %s\r\n",
                 i,
                 (real_host(exit->host) >= 0 && exit->addresses) ? matrix[real_host(exit->host)].name : "Nowhere",
                 exit->host,
                 exit->addresses,
                 exit->hidden ? " (hidden)" : "",
                 (exit->roomstring && *(exit->roomstring)) ? exit->roomstring : "(none)");
    i++;
  }
  send_to_char("\r\n^Ga^Y) ^WAdd new exit\r\n^Gd^Y) ^WDelete an exit\r\n^Gq^Y) ^WQuit\r\n^wEnter selection: ", CH);
  d->edit_mode = HEDIT_EXIT;

}

void hedit_disp_rating_menu(struct descriptor_data *d)
{
  CLS(CH);
  send_to_char(CH, "^G1^Y) ^WAccess: ^c%d^n\r\n"
               "^G2^Y) ^WControl: ^c%d^n\r\n"
               "^G3^Y) ^WIndex: ^c%d^n\r\n"
               "^G4^Y) ^WFiles: ^c%d^n\r\n"
               "^G5^Y) ^WSlave: ^c%d^n\r\n"
               "^Gq^Y) ^WReturn to main^n\r\n"
               "^wEnter selection: ",
               HOST->stats[ACIFS_ACCESS][0],
               HOST->stats[ACIFS_CONTROL][0],
               HOST->stats[ACIFS_INDEX][0],
               HOST->stats[ACIFS_FILES][0],
               HOST->stats[ACIFS_SLAVE][0]);

  d->edit_mode = HEDIT_RATINGS;
}


void hedit_parse(struct descriptor_data *d, const char *arg)
{
  long number, host_num;
  int i = 0;
  switch (d->edit_mode)
  {
  case HEDIT_CONFIRM_EDIT:
    switch (*arg) {
    case 'y':
    case 'Y':
      hedit_disp_data_menu(d);
      break;
    case 'n':
    case 'N':
      STATE(d) = CON_PLAYING;
      if (d->edit_host) {
        // Drop our exits, they have no pair to remove so no further cleanup is needed.
        for (struct entrance_data *entrance = d->edit_host->entrance, *next; entrance; entrance = next) {
          next = entrance->next;
          delete entrance;
        }
        d->edit_host->entrance = NULL;
        Mem->DeleteHost(d->edit_host);
      }
      d->edit_host = NULL;
      d->edit_number = 0;
      PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
      break;
    default:
      send_to_char("That's not a valid choice!\r\n", d->character);
      send_to_char("Do you wish to edit it?\r\n", d->character);
      break;
    }
    break;
  case HEDIT_CONFIRM_SAVESTRING:
    switch (*arg) {
    case 'y':
    case 'Y': {
#ifdef ONLY_LOG_BUILD_ACTIONS_ON_CONNECTED_ZONES
        if (!vnum_from_non_approved_zone(d->edit_number)) {
#else
        {
#endif
          snprintf(buf, sizeof(buf),"%s wrote new host #%ld",
                  GET_CHAR_NAME(d->character), d->edit_number);
          mudlog(buf, d->character, LOG_WIZLOG, TRUE);
        }
        host_num = real_host(d->edit_number);
        if (host_num > 0) {
          d->edit_host->icons = matrix[host_num].icons;
          d->edit_host->alert = matrix[host_num].alert;
          d->edit_host->files = matrix[host_num].files;
          free_host(matrix + host_num);
          matrix[host_num] = *d->edit_host;
        } else {
          int found = 0;

          for (int counter = 0; counter <= top_of_matrix; counter++) {
            /* check if current virtual is bigger than our virtual */
            if (matrix[counter].vnum > d->edit_number) {
              // now, zoom backwards through the list copying over
              // TODO: Okay, but what if top_of_matrix == top_of_matrix_array - 1? SIGSEGV right? -LS
              for (int counter2 = top_of_matrix + 1; counter2 > counter; counter2--) {
                matrix[counter2] = matrix[counter2 - 1];

                // Update icon backlinks (they work on rnum indexes)
                for (struct matrix_icon *temp_icon = matrix[counter].icons; temp_icon; temp_icon = temp_icon->next) {
                  if (temp_icon->in_host != NOWHERE)
                    temp_icon->in_host++;
                }

                // Update object backlinks (they work on direct pointers)
                for (struct matrix_file *soft = matrix[counter2].files; soft; soft = soft->next_file) {
                  soft->in_host = &(matrix[counter2]);
                }
              }
              matrix[counter] = *(d->edit_host);
              matrix[counter].vnum = d->edit_number;
              found = TRUE;
              break;
            }
          }

          /* if place not found, insert at end */
          if (!found) {
            matrix[top_of_matrix + 1] = *d->edit_host;
            matrix[top_of_matrix + 1].vnum = d->edit_number;
          }
          top_of_matrix++;
          host_num = real_host(d->edit_number);

          // Update zcmds that point to the table.
          for (int zone = 0; zone <= top_of_zone_table; zone++) {
            for (int cmd_no = 0; cmd_no < zone_table[zone].num_cmds; cmd_no++) {
              switch (ZCMD.command) {
                case 'H':
                  if (ZCMD.arg3 >= host_num)
                    ZCMD.arg3++;
                  break;
              }
            }
          }
        }

        // Since host memory locations or exits may have changed, redo entrance lists
        collate_host_entrances();
        
        send_to_char("Writing host to disk.\r\n", d->character);
        write_host_to_disk(d->character->player_specials->saved.zonenum);
        send_to_char("Saved.\r\n", CH);
        clear_host(d->edit_host);
        delete d->edit_host;
        d->edit_host = NULL;
        PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
        STATE(d) = CON_PLAYING;
        send_to_char("Done.\r\n", d->character);
        break;
      }
    case 'n':
    case 'N':
      send_to_char("Host not saved, aborting.\r\n", d->character);
      STATE(d) = CON_PLAYING;
      if (d->edit_host) {
        // Drop our exits, they have no pair to remove so no further cleanup is needed.
        for (struct entrance_data *entrance = d->edit_host->entrance, *next; entrance; entrance = next) {
          next = entrance->next;
          delete entrance;
        }
        d->edit_host->entrance = NULL;
        Mem->DeleteHost(d->edit_host);
      }
      d->edit_host = NULL;
      d->edit_number = 0;
      PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
      break;
    default:
      send_to_char("Invalid choice!\r\n", d->character);
      send_to_char("Do you wish to save this host internally?\r\n", d->character);
      break;
    }
    break;
  case HEDIT_MAIN_MENU:
    switch (LOWER(*arg)) {
    case 'q':
      d->edit_mode = HEDIT_CONFIRM_SAVESTRING;
      hedit_parse(d, "y");
      break;
    case 'x':
      d->edit_mode = HEDIT_CONFIRM_SAVESTRING;
      hedit_parse(d, "n");
      break;
    case '1':
      send_to_char("Enter host name:", d->character);
      d->edit_mode = HEDIT_NAME;
      break;
    case '2':
      send_to_char("Enter Parent PLTG/LTG/RTG:", d->character);
      d->edit_mode = HEDIT_PARENT;
      break;
    case '3':
      send_to_char("Enter Keywords:", d->character);
      d->edit_mode = HEDIT_KEYWORDS;
      break;
    case '4':
      send_to_char("Enter Host description:\r\n", d->character);
      d->edit_mode = HEDIT_DESC;
      DELETE_D_STR_IF_EXTANT(d);
      INITIALIZE_NEW_D_STR(d);
      d->max_str = MAX_MESSAGE_LENGTH;
      d->mail_to = 0;
      break;
    case '5':
      CLS(CH);
      send_to_char(CH, "1) Blue\r\n"
                   "2) Green\r\n"
                   "3) Orange\r\n"
                   "4) Red\r\n"
                   "5) Black\r\n"
                   "Enter security level: ");
      d->edit_mode = HEDIT_SECURITY_COLOR;
      break;
    case '6':
      CLS(CH);
      for (i = 0; i < NUM_OF_HOST_TYPES; i++)
        send_to_char(CH, "%d) %s\r\n", i + 1, host_type[i]);
      send_to_char(CH, "Enter host type: ");
      d->edit_mode = HEDIT_TYPE;
      break;
    case '7':
      hedit_disp_rating_menu(d);
      break;
    case '8':
      hedit_disp_subsystem_extras(d);
      break;
    case '9':
      hedit_disp_trigger_menu(d);
      break;
    case '0':
      hedit_disp_exit_menu(d);
      break;
    case 'a':
      send_to_char(CH, "Enter Shutdown Start Message: ");
      d->edit_mode = HEDIT_SSTART;
      break;
    case 'b':
      send_to_char(CH, "Enter Shutdown Stop Message: ");
      d->edit_mode = HEDIT_SSTOP;
      break;
    default:
      send_to_char("Invalid choice!\r\n", d->character);
      hedit_disp_data_menu(d);
      break;
    }
    break;
  case HEDIT_RATINGS:
    {
      char hedit_rating_buf[200];
      snprintf(hedit_rating_buf, sizeof(hedit_rating_buf), "(for %s hosts: min %d, max %d)",
               intrusion[d->edit_host->intrusion],
               host_subsystem_acceptable_ratings[d->edit_host->intrusion][IDX_MTX_ACCEPTABLE_RATING_MINIMUM],
               host_subsystem_acceptable_ratings[d->edit_host->intrusion][IDX_MTX_ACCEPTABLE_RATING_MAXIMUM]);

      switch (LOWER(*arg)) {
        case 'q':
        case 'Q':
          hedit_disp_data_menu(d);
          break;
        case '1':
          send_to_char(CH, "Enter host's Access rating %s: \r\n", hedit_rating_buf);
          d->edit_mode = HEDIT_RATINGS_ACCESS;
          break;
        case '2':
          send_to_char(CH, "Enter host's Control rating %s: \r\n", hedit_rating_buf);
          d->edit_mode = HEDIT_RATINGS_CONTROL;
          break;
        case '3':
          send_to_char(CH, "Enter host's Index rating %s: \r\n", hedit_rating_buf);
          d->edit_mode = HEDIT_RATINGS_INDEX;
          break;
        case '4':
          send_to_char(CH, "Enter host's Files rating %s: \r\n", hedit_rating_buf);
          d->edit_mode = HEDIT_RATINGS_FILES;
          break;
        case '5':
          send_to_char(CH, "Enter host's Slave rating %s: \r\n", hedit_rating_buf);
          d->edit_mode = HEDIT_RATINGS_SLAVE;
          break;
        default:
          send_to_char("Invalid choice!\r\n", d->character);
          hedit_disp_rating_menu(d);
          break;
      }
    }
    break;
  case HEDIT_EXTRA_MENU:
    switch (LOWER(*arg)) {
    case '1':
      send_to_char(CH, "Enter Access Scramble Rating (0 For None): ");
      d->edit_mode = HEDIT_EXTRA_ACCESS;
      break;
    case '2':
      send_to_char(CH, "Trapdoor to which Host (0 For None): ");
      d->edit_mode = HEDIT_EXTRA_CONTROL;
      break;
    case '3':
      send_to_char(CH, "Trapdoor to which Host (0 For None): ");
      d->edit_mode = HEDIT_EXTRA_INDEX;
      break;
    case '4':
      send_to_char(CH, "Enter Files Scramble Rating (0 For None): ");
      d->edit_mode = HEDIT_EXTRA_FILES;
      break;
    case '5':
      send_to_char(CH, "Enter Slave Scramble Rating (0 For None): ");
      d->edit_mode = HEDIT_EXTRA_SLAVE;
      break;
    case 'q':
    case 'Q':
      hedit_disp_data_menu(d);
      break;
    default:
      send_to_char("Invalid choice!\r\n", d->character);
      hedit_disp_subsystem_extras(d);
      break;
    }
    break;
  case HEDIT_TRIGGER:
    switch (LOWER(*arg)) {
    case 'q':
    case 'Q':
      hedit_disp_data_menu(d);
      break;
    case 'a':
    case 'A':
      send_to_char(CH, "Enter trigger step: ");
      d->edit_mode = HEDIT_TRIGGER_ADD;
      break;
    case 'd':
    case 'D':
      send_to_char(CH, "Enter trigger to delete: ");
      d->edit_mode = HEDIT_TRIGGER_DEL;
      break;
    }
    break;
  case HEDIT_EXIT:
    switch (LOWER(*arg)) {
    case 'q':
    case 'Q':
      hedit_disp_data_menu(d);
      break;
    case 'a':
    case 'A':
      send_to_char(CH, "Enter target host: ");
      d->edit_mode = HEDIT_EXIT_ADD;
      break;
    case 'd':
    case 'D':
      send_to_char(CH, "Enter exit to delete: ");
      d->edit_mode = HEDIT_EXIT_DEL;
      break;
    }
    break;
  case HEDIT_EXTRA_ACCESS:
    HOST->stats[ACIFS_ACCESS][2] = atoi(arg);
    send_to_char(CH, "Trapdoor to which host (0 for none): ");
    d->edit_mode = HEDIT_EXTRA_ACCESS2;
    break;
  case HEDIT_EXTRA_ACCESS2:
    HOST->stats[ACIFS_ACCESS][5] = atoi(arg);
    hedit_disp_subsystem_extras(d);
    break;
  case HEDIT_EXTRA_CONTROL:
    HOST->stats[ACIFS_CONTROL][5] = atoi(arg);
    hedit_disp_subsystem_extras(d);
    break;
  case HEDIT_EXTRA_INDEX:
    HOST->stats[ACIFS_INDEX][5] = atoi(arg);
    hedit_disp_subsystem_extras(d);
    break;
  case HEDIT_EXTRA_FILES:
    HOST->stats[ACIFS_FILES][2] = atoi(arg);
    send_to_char(CH, "Trapdoor to which host (0 for none): ");
    d->edit_mode = HEDIT_EXTRA_FILES2;
    break;
  case HEDIT_EXTRA_FILES2:
    HOST->stats[ACIFS_FILES][5] = atoi(arg);
    hedit_disp_subsystem_extras(d);
    break;
  case HEDIT_EXTRA_SLAVE:
    HOST->stats[ACIFS_SLAVE][2] = atoi(arg);
    send_to_char(CH, "Trapdoor to which host (0 for none): ");
    d->edit_mode = HEDIT_EXTRA_SLAVE2;
    break;
  case HEDIT_EXTRA_SLAVE2:
    HOST->stats[ACIFS_SLAVE][5] = atoi(arg);
    hedit_disp_subsystem_extras(d);
    break;

  case HEDIT_PARENT:
    number = atoi(arg);
    if (real_host(number) < 0) {
      send_to_char(CH, "Invalid Choice!\r\n");
    } else {
      d->edit_host->parent = number;
      hedit_disp_data_menu(d);
    }
    break;
  case HEDIT_NAME:
    DELETE_ARRAY_IF_EXTANT(d->edit_host->name);
    d->edit_host->name = str_dup(arg);
    hedit_disp_data_menu(d);
    break;
  case HEDIT_KEYWORDS:
    DELETE_ARRAY_IF_EXTANT(d->edit_host->keywords);
    d->edit_host->keywords = str_dup(arg);
    hedit_disp_data_menu(d);
    break;
  case HEDIT_SSTOP:
    DELETE_ARRAY_IF_EXTANT(d->edit_host->shutdown_stop);
    d->edit_host->shutdown_stop = str_dup(arg);
    hedit_disp_data_menu(d);
    break;
  case HEDIT_SSTART:
    DELETE_ARRAY_IF_EXTANT(d->edit_host->shutdown_start);
    d->edit_host->shutdown_start = str_dup(arg);
    hedit_disp_data_menu(d);
    break;
  case HEDIT_TYPE:
    number = atoi(arg);
    if (number <= 0 || number > NUM_OF_HOST_TYPES) {
      send_to_char("Invalid choice!\r\n", d->character);
    } else {
      d->edit_host->type = --number;
      hedit_disp_data_menu(d);
    }
    break;
  case HEDIT_SECURITY_COLOR:
    number = atoi(arg);
    if (number <= 0 || number > 5) {
      send_to_char("Invalid choice!\r\n", d->character);
      send_to_char("Enter security level: ", CH);
    } else {
      d->edit_host->color = --number;
      send_to_char(CH, "Enter security rating: ");
      d->edit_mode = HEDIT_SECURITY_RATING;
    }
    break;

  case HEDIT_SECURITY_RATING:
    number = atoi(arg);
    if (number <= 0) {
      send_to_char("Invalid choice!\r\n", d->character);
      send_to_char(CH, "Enter security rating: ");
    } else {
      d->edit_host->security = number;
      CLS(CH);
      send_to_char(CH, "^G1^Y) ^WEasy^n\r\n"
                   "^G2^Y) ^WModerate^n\r\n"
                   "^G3^Y) ^WHard^n\r\n"
                   "Enter intrusion difficulty: ");
      d->edit_mode = HEDIT_SECURITY_DIFF;
    }
    break;
  case HEDIT_SECURITY_DIFF:
    number = atoi(arg);
    if (number <= 0 || number > 3) {
      send_to_char("Invalid choice!\r\n", d->character);
      send_to_char(CH, "Enter intrusion difficulty: ");
    } else {
      d->edit_host->intrusion = --number;
      hedit_disp_data_menu(d);
    }
    break;
  case HEDIT_RATINGS_ACCESS:
    number = atoi(arg);
    if (number < host_subsystem_acceptable_ratings[d->edit_host->intrusion][IDX_MTX_ACCEPTABLE_RATING_MINIMUM]) {
      send_to_char(CH, "The minimum rating for this system in %s hosts is %d. Enter Access rating: ", intrusion[d->edit_host->intrusion], host_subsystem_acceptable_ratings[d->edit_host->intrusion][IDX_MTX_ACCEPTABLE_RATING_MINIMUM]);
    } else if (number > host_subsystem_acceptable_ratings[d->edit_host->intrusion][IDX_MTX_ACCEPTABLE_RATING_MAXIMUM]) {
      send_to_char(CH, "The maximum rating for this system in %s hosts is %d. Enter Access rating: ", intrusion[d->edit_host->intrusion], host_subsystem_acceptable_ratings[d->edit_host->intrusion][IDX_MTX_ACCEPTABLE_RATING_MAXIMUM]);
    } else {
      d->edit_host->stats[ACIFS_ACCESS][0] = number;
      hedit_disp_rating_menu(d);
    }
    break;
  case HEDIT_RATINGS_CONTROL:
    number = atoi(arg);
    if (number < host_subsystem_acceptable_ratings[d->edit_host->intrusion][IDX_MTX_ACCEPTABLE_RATING_MINIMUM]) {
      send_to_char(CH, "The minimum rating for this system in %s hosts is %d. Enter Control rating: ", intrusion[d->edit_host->intrusion], host_subsystem_acceptable_ratings[d->edit_host->intrusion][IDX_MTX_ACCEPTABLE_RATING_MINIMUM]);
    } else if (number > host_subsystem_acceptable_ratings[d->edit_host->intrusion][IDX_MTX_ACCEPTABLE_RATING_MAXIMUM]) {
      send_to_char(CH, "The maximum rating for this system in %s hosts is %d. Enter Control rating: ", intrusion[d->edit_host->intrusion], host_subsystem_acceptable_ratings[d->edit_host->intrusion][IDX_MTX_ACCEPTABLE_RATING_MAXIMUM]);
    } else {
      d->edit_host->stats[ACIFS_CONTROL][0] = number;
      hedit_disp_rating_menu(d);
    }
    break;
  case HEDIT_RATINGS_INDEX:
    number = atoi(arg);
    if (number < host_subsystem_acceptable_ratings[d->edit_host->intrusion][IDX_MTX_ACCEPTABLE_RATING_MINIMUM]) {
      send_to_char(CH, "The minimum rating for this system in %s hosts is %d. Enter Index rating: ", intrusion[d->edit_host->intrusion], host_subsystem_acceptable_ratings[d->edit_host->intrusion][IDX_MTX_ACCEPTABLE_RATING_MINIMUM]);
    } else if (number > host_subsystem_acceptable_ratings[d->edit_host->intrusion][IDX_MTX_ACCEPTABLE_RATING_MAXIMUM]) {
      send_to_char(CH, "The maximum rating for this system in %s hosts is %d. Enter Index rating: ", intrusion[d->edit_host->intrusion], host_subsystem_acceptable_ratings[d->edit_host->intrusion][IDX_MTX_ACCEPTABLE_RATING_MAXIMUM]);
    } else {
      d->edit_host->stats[ACIFS_INDEX][0] = number;
      hedit_disp_rating_menu(d);
    }
    break;
  case HEDIT_RATINGS_FILES:
    number = atoi(arg);
    if (number < host_subsystem_acceptable_ratings[d->edit_host->intrusion][IDX_MTX_ACCEPTABLE_RATING_MINIMUM]) {
      send_to_char(CH, "The minimum rating for this system in %s hosts is %d. Enter Files rating: ", intrusion[d->edit_host->intrusion], host_subsystem_acceptable_ratings[d->edit_host->intrusion][IDX_MTX_ACCEPTABLE_RATING_MINIMUM]);
    } else if (number > host_subsystem_acceptable_ratings[d->edit_host->intrusion][IDX_MTX_ACCEPTABLE_RATING_MAXIMUM]) {
      send_to_char(CH, "The maximum rating for this system in %s hosts is %d. Enter Files rating: ", intrusion[d->edit_host->intrusion], host_subsystem_acceptable_ratings[d->edit_host->intrusion][IDX_MTX_ACCEPTABLE_RATING_MAXIMUM]);
    } else {
      d->edit_host->stats[ACIFS_FILES][0] = number;
      hedit_disp_rating_menu(d);
    }
    break;
  case HEDIT_RATINGS_SLAVE:
    number = atoi(arg);
    if (number < host_subsystem_acceptable_ratings[d->edit_host->intrusion][IDX_MTX_ACCEPTABLE_RATING_MINIMUM]) {
      send_to_char(CH, "The minimum rating for this system in %s hosts is %d. Enter Slave rating: ", intrusion[d->edit_host->intrusion], host_subsystem_acceptable_ratings[d->edit_host->intrusion][IDX_MTX_ACCEPTABLE_RATING_MINIMUM]);
    } else if (number > host_subsystem_acceptable_ratings[d->edit_host->intrusion][IDX_MTX_ACCEPTABLE_RATING_MAXIMUM]) {
      send_to_char(CH, "The maximum rating for this system in %s hosts is %d. Enter Slave rating: ", intrusion[d->edit_host->intrusion], host_subsystem_acceptable_ratings[d->edit_host->intrusion][IDX_MTX_ACCEPTABLE_RATING_MAXIMUM]);
    } else {
      d->edit_host->stats[ACIFS_SLAVE][0] = number;
      hedit_disp_rating_menu(d);
    }
    break;
  case HEDIT_TRIGGER_DEL:
    number = atoi(arg);
    if (number < 0) {
      send_to_char(CH, "Invalid choice!\r\n");
      hedit_disp_trigger_menu(d);
    } else {
      struct trigger_step *trigger, *temp;
      for (trigger = HOST->trigger; trigger && number; trigger = trigger->next, number--)
        break;
      if (!trigger) {
        send_to_char(CH, "Invalid choice!\r\n");
        hedit_disp_trigger_menu(d);
      } else {
        REMOVE_FROM_LIST(trigger, HOST->trigger, next);
        delete trigger;
        hedit_disp_trigger_menu(d);
      }
    }
    break;
  case HEDIT_TRIGGER_ADD:
    number = atoi(arg);
    if (number <= 0) {
      send_to_char(CH, "Invalid choice!\r\n");
      hedit_disp_trigger_menu(d);
    } else {
      struct trigger_step *trigger;
      trigger = new trigger_step;
      trigger->step = number;
      if (HOST->trigger)
        trigger->next = HOST->trigger;
      HOST->trigger = trigger;
      send_to_char(CH, "Go to Alert (0 - No Change, 1 - Passive, 2 - Active): ");
      d->edit_mode = HEDIT_TRIGGER_ADD2;
    }
    break;
  case HEDIT_TRIGGER_ADD2:
    number = atoi(arg);
    if (number < 0 || number > 2)
      send_to_char(CH, "Invalid choice!\r\nGo to Alert (0 - No Change, 1 - Passive, 2 - Active): ");
    else {
      HOST->trigger->alert = number;
      send_to_char(CH, "Load IC (0 for none): ");
      d->edit_mode = HEDIT_TRIGGER_ADD3;
    }
    break;
  case HEDIT_TRIGGER_ADD3:
    number = atoi(arg);
    if (number != 0 && real_ic(number) < 0)
      send_to_char(CH, "Invalid choice!\r\nLoad IC (0 for none): ");
    else {
      HOST->trigger->ic = number;
      hedit_disp_trigger_menu(d);
    }
    break;
  case HEDIT_EXIT_DEL:
    number = atoi(arg);
    if (number < 0) {
      send_to_char(CH, "Invalid choice!\r\n");
      hedit_disp_exit_menu(d);
    } else {
      struct exit_data *exit, *temp;
      for (exit = HOST->exit; exit && number; exit = exit->next, number--)
        ;
      if (!exit) {
        send_to_char(CH, "Invalid choice!\r\n");
        hedit_disp_exit_menu(d);
      } else {
        REMOVE_FROM_LIST(exit, HOST->exit, next);
        DELETE_ARRAY_IF_EXTANT(exit->addresses);
        DELETE_ARRAY_IF_EXTANT(exit->roomstring);
        delete [] exit;
        hedit_disp_exit_menu(d);
      }
    }
    break;
  case HEDIT_EXIT_ADD:
    number = atoi(arg);
    if (real_host(number) < 0) {
      send_to_char(CH, "Invalid choice!\r\n");
      hedit_disp_exit_menu(d);
    } else {
      struct exit_data *exit;
      exit = new exit_data;
      exit->host = number;
      if (HOST->exit)
        exit->next = HOST->exit;
      HOST->exit = exit;
      send_to_char(CH, "Enter the addresses and keywords for this host, separated by spaces, with the first one being is its official address: ");
      d->edit_mode = HEDIT_EXIT_ADD2;
    }
    break;
  case HEDIT_EXIT_ADD2:
    DELETE_ARRAY_IF_EXTANT(d->edit_host->exit->addresses);
    d->edit_host->exit->addresses = str_dup(arg);
    send_to_char(CH, "Is the host hidden? (1 = yes, 0 = no): ");
    d->edit_mode = HEDIT_EXIT_ADD3;
    break;
  case HEDIT_EXIT_ADD3:
    number = atoi(arg);
    if (number < 0 || number > 1) {
      send_to_char(CH, "Invalid choice. Is the host hidden from the player when they LOOK in the host? (1 = yes, 0 = no): ");
    } else {
      d->edit_host->exit->hidden = (bool) number;
      send_to_char(CH, "What's the host's icon look like? (ex: 'A glowing eye with a red border around it burns menacingly in the sky.'): \r\n");
      d->edit_mode = HEDIT_EXIT_ADD4;
    }
    break;
  case HEDIT_EXIT_ADD4:
    DELETE_ARRAY_IF_EXTANT(d->edit_host->exit->roomstring);
    d->edit_host->exit->roomstring = str_dup(arg);
    hedit_disp_exit_menu(d);
    break;
  }
}
