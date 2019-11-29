#include <stdio.h>
#include <string.h>

#include "structs.h"
#include "awake.h"
#include "interpreter.h"
#include "comm.h"
#include "constants.h"
#include "utils.h"
#include "db.h"
#include <stdlib.h>
#include "screen.h"
#include "olc.h"
#include "newmatrix.h"
#include "memory.h"

#define HOST d->edit_host
#define NUM_OF_HOST_TYPES 6
extern int from_ip_zone(int vnum);
extern char *cleanup(char *dest, const char *src);

#define HT matrix[realcounter]
void write_host_to_disk(int vnum)
{
  long realcounter, counter;
  int zone = real_zone(vnum);
  FILE *fl;
  struct exit_data *exit;
  struct trigger_step *step;
  int i = 0;

  sprintf(buf, "%s/%d.host", MTX_PREFIX, vnum);
  fl = fopen(buf, "w+");
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
      fprintf(fl, "Description:$\n%s~\n", cleanup(buf2, HT.desc));
      fprintf(fl, "ShutdownStart:\t%s\n", HT.shutdown_start);
      fprintf(fl, "ShutdownStop:\t%s\n", HT.shutdown_stop);
      fprintf(fl, "Parent:\t%ld\n", HT.parent);
      fprintf(fl, "Colour:\t%d\n", HT.colour);
      fprintf(fl, "Security:\t%d\n", HT.security);
      fprintf(fl, "Difficulty:\t%d\n", HT.intrusion);
      fprintf(fl, "Access:\t%ld\n", HT.stats[ACCESS][0]);
      fprintf(fl, "AccessScramble:\t%ld\n", HT.stats[ACCESS][2]);
      fprintf(fl, "AccessTrapdoor:\t%ld\n", HT.stats[ACCESS][5]);
      fprintf(fl, "Control:\t%ld\n", HT.stats[CONTROL][0]);
      fprintf(fl, "ControlTrapdoor:\t%ld\n", HT.stats[CONTROL][5]);
      fprintf(fl, "Index:\t%ld\n", HT.stats[INDEX][0]);
      fprintf(fl, "IndexTrapdoor:\t%ld\n", HT.stats[INDEX][5]);
      fprintf(fl, "Files:\t%ld\n", HT.stats[FILES][0]);
      fprintf(fl, "FilesScramble:\t%ld\n", HT.stats[FILES][2]);
      fprintf(fl, "FilesTrapdoor:\t%ld\n", HT.stats[FILES][5]);
      fprintf(fl, "Slave:\t%ld\n", HT.stats[SLAVE][0]);
      fprintf(fl, "SlaveScramble:\t%ld\n", HT.stats[SLAVE][2]);
      fprintf(fl, "SlaveTrapdoor:\t%ld\n", HT.stats[SLAVE][5]);

      fprintf(fl, "Type:\t%s\n", host_type[HT.type]);
      if (HT.exit) {
        fprintf(fl, "[EXITS]\n");
        for (exit = HT.exit; exit; exit = exit->next, i++) {
          fprintf(fl, "\t[EXIT %d]\n", i);
          fprintf(fl, "\t\tExit:\t%ld\n", exit->host);
          fprintf(fl, "\t\tNumber:\t%s\n", exit->number);
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
}

void hedit_disp_data_menu(struct descriptor_data *d)
{
  CLS(CH);
  send_to_char(CH, "^WHost: ^c%d^n\r\n", d->edit_number);
  send_to_char(CH, "^G1^Y) ^WHost: ^c%s^n\r\n", HOST->name);
  if (real_host(HOST->parent) > 0)
    send_to_char(CH, "^G2^Y) ^WParent Host: ^c%s(%ld)^n\r\n", matrix[real_host(HOST->parent)].name, HOST->parent);
  else
    send_to_char(CH, "^G2^Y) ^WParent Host: ^cNowhere(0)^n\r\n");
  send_to_char(CH, "^G3^Y) ^WKeywords: ^c%s^n\r\n", HOST->keywords);
  send_to_char(CH, "^G4^Y) ^WDescription: ^c%s^n\r\n", HOST->desc);
  send_to_char(CH, "^G5^Y) ^WSecurity: ^c%s-%d^c(%s)^n\r\n", host_sec[HOST->colour], HOST->security,
               intrusion[HOST->intrusion]);
  send_to_char(CH, "^G6^Y) ^WType: ^c%s^n\r\n", host_type[HOST->type]);
  send_to_char(CH, "^G7^Y) ^WRatings: A(^c%d^N) C(^c%d^N) I(^c%d^N) F(^c%d^N) S(^c%d^N)\r\n",
               HOST->stats[ACCESS][0], HOST->stats[CONTROL][0], HOST->stats[INDEX][0],
               HOST->stats[FILES][0], HOST->stats[SLAVE][0]);
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
  send_to_char(CH, "^G1^Y) ^WAccess Scramble Rating: %2ld Access Trapdoor: %ld\r\n", HOST->stats[ACCESS][2], HOST->stats[ACCESS][5]);
  send_to_char(CH, "^G2^Y) ^WControl Trapdoor: %ld\r\n", HOST->stats[CONTROL][5]);
  send_to_char(CH, "^G3^Y) ^WIndex Trapdoor: %ld\r\n", HOST->stats[INDEX][5]);
  send_to_char(CH, "^G4^Y) ^WFiles Scramble Rating: %2ld Files Trapdoor: %ld\r\n", HOST->stats[FILES][2], HOST->stats[FILES][5]);
  send_to_char(CH, "^G5^Y) ^WSlave Scramble Rating: %2ld Slave Trapdoor: %ld\r\n", HOST->stats[SLAVE][2], HOST->stats[SLAVE][5]);
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
    send_to_char(CH, "^G%d^Y) ^c%s^N(^c%ld^N) ^c%s^N\r\n", i, exit->number ? matrix[real_host(exit->host)].name : "Nowhere", exit->host,
                 exit->number);
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
               "^wEnter selection: ", HOST->stats[ACCESS][0], HOST->stats[CONTROL][0], HOST->stats[INDEX][0], HOST->stats[FILES][0], HOST->stats[SLAVE][0]);

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
      if (d->edit_host)
        Mem->DeleteHost(d->edit_host);
      d->edit_host = NULL;
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
        if (!from_ip_zone(d->edit_number)) {
          sprintf(buf,"%s wrote new host #%ld",
                  GET_CHAR_NAME(d->character), d->edit_number);
          mudlog(buf, d->character, LOG_WIZLOG, TRUE);
        }
        host_num = real_host(d->edit_number);
        if (host_num > 0) {
          d->edit_host->icons = matrix[host_num].icons;
          d->edit_host->alert = matrix[host_num].alert;
          d->edit_host->file = matrix[host_num].file;
          free_host(matrix + host_num);
          matrix[host_num] = *d->edit_host;
        } else {
          int             counter;
          int             counter2;
          int             found = 0;
          for (counter = 0; counter <= top_of_matrix; counter++) {
            if (!found) {
              /* check if current virtual is bigger than our virtual */
              if (matrix[counter].vnum > d->edit_number) {
                // now, zoom backwards through the list copying over
                // TODO: Okay, but what if top_of_matrix == top_of_matrix_array - 1? SIGSEGV right? -LS
                for (counter2 = top_of_matrix + 1; counter2 > counter; counter2--) {
                  matrix[counter2] = matrix[counter2 - 1];
                }
                matrix[counter] = *(d->edit_host);
                matrix[counter].vnum = d->edit_number;
                found = TRUE;
              }
            } else {
              struct matrix_icon *temp_icon;
              for (temp_icon = matrix[counter].icons; temp_icon; temp_icon = temp_icon->next)
                if (temp_icon->in_host != NOWHERE)
                  temp_icon->in_host++;
            }
          }

          /* if place not found, insert at end */
          if (!found) {
            matrix[top_of_matrix + 1] = *d->edit_host;
            matrix[top_of_matrix + 1].vnum = d->edit_number;
          }
          top_of_matrix++;
          host_num = real_host(d->edit_number);
        }
        send_to_char("Writing host to disk.\r\n", d->character);
        write_host_to_disk(d->character->player_specials->saved.zonenum);
        send_to_char("Saved.\r\n", CH);
        Mem->ClearHost(d->edit_host);
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
      PLR_FLAGS(d->character).RemoveBit(PLR_EDITING);
      if (d->edit_host)
        Mem->DeleteHost(d->edit_host);
      d->edit_host = NULL;
      d->edit_number = 0;
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
      d->edit_mode = HEDIT_SECURITY_COLOUR;
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
    switch (LOWER(*arg)) {
    case 'q':
    case 'Q':
      hedit_disp_data_menu(d);
      break;
    case '1':
      send_to_char(CH, "Enter hosts Access rating: \r\n");
      d->edit_mode = HEDIT_RATINGS_ACCESS;
      break;
    case '2':
      send_to_char(CH, "Enter hosts Control rating: \r\n");
      d->edit_mode = HEDIT_RATINGS_CONTROL;
      break;
    case '3':
      send_to_char(CH, "Enter hosts Index rating: \r\n");
      d->edit_mode = HEDIT_RATINGS_INDEX;
      break;
    case '4':
      send_to_char(CH, "Enter hosts Files rating: \r\n");
      d->edit_mode = HEDIT_RATINGS_FILES;
      break;
    case '5':
      send_to_char(CH, "Enter hosts Slave rating: \r\n");
      d->edit_mode = HEDIT_RATINGS_SLAVE;
      break;
    default:
      send_to_char("Invalid choice!\r\n", d->character);
      hedit_disp_rating_menu(d);
      break;
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
    HOST->stats[ACCESS][2] = atoi(arg);
    send_to_char(CH, "Trapdoor to which host (0 for none): ");
    d->edit_mode = HEDIT_EXTRA_ACCESS2;
    break;
  case HEDIT_EXTRA_ACCESS2:
    HOST->stats[ACCESS][5] = atoi(arg);
    hedit_disp_subsystem_extras(d);
    break;
  case HEDIT_EXTRA_CONTROL:
    HOST->stats[CONTROL][5] = atoi(arg);
    hedit_disp_subsystem_extras(d);
    break;
  case HEDIT_EXTRA_INDEX:
    HOST->stats[INDEX][5] = atoi(arg);
    hedit_disp_subsystem_extras(d);
    break;
  case HEDIT_EXTRA_FILES:
    HOST->stats[FILES][2] = atoi(arg);
    send_to_char(CH, "Trapdoor to which host (0 for none): ");
    d->edit_mode = HEDIT_EXTRA_FILES2;
    break;
  case HEDIT_EXTRA_FILES2:
    HOST->stats[FILES][5] = atoi(arg);
    hedit_disp_subsystem_extras(d);
    break;
  case HEDIT_EXTRA_SLAVE:
    HOST->stats[SLAVE][2] = atoi(arg);
    send_to_char(CH, "Trapdoor to which host (0 for none): ");
    d->edit_mode = HEDIT_EXTRA_SLAVE2;
    break;
  case HEDIT_EXTRA_SLAVE2:
    HOST->stats[SLAVE][5] = atoi(arg);
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
  case HEDIT_SECURITY_COLOUR:
    number = atoi(arg);
    if (number <= 0 || number > 5) {
      send_to_char("Invalid choice!\r\n", d->character);
      send_to_char("Enter security level: ", CH);
    } else {
      d->edit_host->colour = --number;
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
    if (number <= 0) {
      send_to_char("Invalid choice!\r\n", d->character);
      send_to_char(CH, "Enter Access rating: ");
    } else {
      d->edit_host->stats[ACCESS][0] = number;
      hedit_disp_rating_menu(d);
    }
    break;
  case HEDIT_RATINGS_CONTROL:
    number = atoi(arg);
    if (number <= 0) {
      send_to_char("Invalid choice!\r\n", d->character);
      send_to_char(CH, "Enter Control rating: ");
    } else {
      d->edit_host->stats[CONTROL][0] = number;
      hedit_disp_rating_menu(d);
    }
    break;
  case HEDIT_RATINGS_INDEX:
    number = atoi(arg);
    if (number <= 0) {
      send_to_char("Invalid choice!\r\n", d->character);
      send_to_char(CH, "Enter Index rating: ");
    } else {
      d->edit_host->stats[INDEX][0] = number;
      hedit_disp_rating_menu(d);
    }
    break;
  case HEDIT_RATINGS_FILES:
    number = atoi(arg);
    if (number <= 0) {
      send_to_char("Invalid choice!\r\n", d->character);
      send_to_char(CH, "Enter Files rating: ");
    } else {
      d->edit_host->stats[FILES][0] = number;
      hedit_disp_rating_menu(d);
    }
    break;
  case HEDIT_RATINGS_SLAVE:
    number = atoi(arg);
    if (number <= 0) {
      send_to_char("Invalid choice!\r\n", d->character);
      send_to_char(CH, "Enter slave rating: ");
    } else {
      d->edit_host->stats[SLAVE][0] = number;
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
    if (number < 1) {
      send_to_char(CH, "Invalid choice!\r\n");
      hedit_disp_exit_menu(d);
    } else {
      struct exit_data *exit, *temp;
      for (exit = HOST->exit; exit && number; exit = exit->next, number--)
        break;
      if (!exit) {
        send_to_char(CH, "Invalid choice!\r\n");
        hedit_disp_exit_menu(d);
      } else {
        REMOVE_FROM_LIST(exit, HOST->exit, next);
        DELETE_ARRAY_IF_EXTANT(exit->number);
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
      send_to_char(CH, "Enter address to access host: ");
      d->edit_mode = HEDIT_EXIT_ADD2;
    }
    break;
  case HEDIT_EXIT_ADD2:
    DELETE_ARRAY_IF_EXTANT(d->edit_host->exit->number);
    d->edit_host->exit->number = str_dup(arg);
    hedit_disp_exit_menu(d);
    break;
  }
}
