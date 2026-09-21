/* Copyover Code, ported to Awake by Harlequin *
 * (c) 1996-97 Erwin S. Andreasen <erwin@andreasen.org> 
 * and of course, extended by Lucien 2026 -- ported from act.wizard.cpp to here */

#include <time.h>
#include <unistd.h>

#include "awake.hpp"
#include "file.hpp"
#include "utils.hpp"
#include "newdb.hpp"
#include "db.hpp"
#include "zoomies.hpp"
#include "handler.hpp"
#include "transport.hpp"
#include "constants.hpp"
#include "interpreter.hpp"
#include "vehicles.hpp"

idnum_t global_copyover_enqueued_by_idnum = 0;
time_t global_copyover_override_quests_at = 0;

extern int mother_desc, port;

SPECIAL(fixer);

extern void DBFinalize();
extern void save_shop_orders();

#define EXE_FILE "bin/awake" /* maybe use argv[0] but it's not reliable */

void execute_copyover() {
  FILE *fp = fopen (COPYOVER_FILE, "w");

  if (!fp) {
    mudlog_vfprintf(NULL, LOG_SYSLOG, "SYSERR: Failed to write to copyover file; copyover aborted! You're a little bit fucked.");
    return;
  }

  const char *messages[] =
    {
      "This copyover has been brought to you by NERPS.  It's more than a lubricant, it's a lifestyle!\r\n", // 0
      "Yes, the MUD is lagging.  Deal with it.\r\n",
      "It's a copyover.  Now would be a good time to take out the trash.\r\n",
      "My dog told me to copyover. Goood dog, good dog.\r\n",
      "It's called a changeover, the movie goes on, and nobody in the audience has any idea.\r\n",
      "Oh shit, I forgot to compile.  I'm gonna have to do this again!\r\n", // 5
      "An invisible staff member says \x1B[0;35mOOCly\x1B[0m, \"I'm going to get fired for this.\"\r\n",
      "Yum Yum Copyover Stew, out with the old code, in with the new!\r\n",
      "\x1B[0;35m[\x1B[0mSerge\x1B[0;35m] \x1B[0;31m(\x1B[0mOOC\x1B[0;31m)\x1B[0m, \"This porn's taking too long to download, needs more bandwidth. So the Mud'll be back up in a bit.\"\r\n",
      "\x1B[0;35m[\x1B[0mLucien\x1B[0;35m] \x1B[0;31m(\x1B[0mOOC\x1B[0;31m)\x1B[0m, \"Honestly, I give this new code a 30% chance of crashing outright.\"\r\n",
      "There's a sound like a record scratching, and everything around you stutters to a standstill.", // 10
      "One moment while we drive up the server cost with heavy CPU usage...\r\n",
      "You wake up. You're still a lizard sunning on a red rock. It was all a dream. The concept of selling 'feet pics' to pay back 'ripperdocs' is already losing its meaning as you open and lick your own eyeballs to moisten them. Time to eat a bug.\r\n",
      "For the briefest of moments, you peer beyond the veil, catching a glimpse of the whirling, gleaming machinery that lies at the heart of the world. Your mind begins to break down at the sight...\r\n",
      "Your vision goes black, then starts to fade in again. You're sitting in the back of a cart, your hands bound before you. A disheveled blonde man sitting across from you meets your eyes. \"Hey, you. You're finally awake.\"\r\n",
      "A sharp tone sounds, and suddenly all you can see is a glitchy field of yellow-and-black. A red info box hovers in front of your paralyzed form, reading \"FIXING ANOMALY, PLEASE STAND BY\"...\r\n", // 15
      "An unexplainable mist pours into the area, shrouding your surroundings into undefinable half-shapes that twitch oddly in the dimness. As you look around in bewilderment, a civil defense siren begins to sound...\r\n",
      "Your pocket secretary beeps, and you glance at it distractedly. But what's this? A notification of winning the lottery? A wire transfer of a hundred million nuyen?! You're finally free! You can retire from the shadows and live a normal life! You can--\r\n",
      "A bone-shaking rumble startles you, and your gaze flicks towards its source-- the rising sun on the horizon. But wait, the sun rose hours ago, and it's not usually shaped like a mushroom, is it...?\r\n",
      "You're one of the lucky ones-- you were looking at the night sky when it happened. Some barely-discernable speck flickered by, crossing the sky in the span of a second, and shattered the Moon into seven pieces...\r\n",
      "Some indescribable force urges you to visit https://youtu.be/x31tDT-4fQw.\r\n", // 20
      "A fuzz of static washes over your vision, and for a brief moment you realize that you're nothing more than a naked brain suspended in a jar, with electrodes plunging into you to carry a simulation of reality. You would scream, but you don't even have a mouth...\r\n",
      "A vibrant cobalt flash cracks across the sky, and as one, all the electronics around you shut down.\r\n",
      "You suddenly realize that you can't remember where you left your towel...!\r\n",
      "The leathery flap of dragon wings fills the air, and as you spin to look in that direction, all you see is a ball of fire racing towards you...!\r\n",
      "You're thrown from your feet as the Cascadia Fault ruptures! The scream of tortured steel fills the air as skyscrapers begin to collapse.\r\n", // 25
      "Remember when these used to take upwards of five minutes? Pepperidge Farm remembers.\r\n",
      "\"Right, see, the genre's called 'cyberpunk'. Sometimes you get cybered, and sometimes you get punked.\"\r\n",
      "\x1B[0;35m[\x1B[0mVile\x1B[0;35m] \x1B[0;31m(\x1B[0mOOC\x1B[0;31m)\x1B[0m, \"This one's probably my fault, too.\"\r\n",
      "\x1B[0;35m[\x1B[0mJank\x1B[0;35m] \x1B[0;31m(\x1B[0mOOC\x1B[0;31m)\x1B[0m, \"This is the perfect time to buy more NERPS!\"\r\n",
      "This is the way the world ends: Not with a bang, but with a copyover.\r\n", // 30
      "Your vision is briefly encompassed by a ring of ten candles, which extinguish one by one. As the final one darkens, a voice intones, 'These things are true: The world is dark.'\r\n",
      "The throaty rumble of your Super Destroyer's engines is a comforting feel beneath your feet. You clench the grip of your Liberator in anticipation as the PA calls out, \"Helldivers to Hellpods. Repeat, Helldivers, to Hellpods.\"\r\n",
      "Off in the distance, you can faintly make out the unearthly form of Vile standing atop a cliff. He's got his arms towards the sky in some sort of anime-esque power pose, and as he contorts his face with effort, an ominous blue glow starts to shine through the clouds above...\r\n",
      "Suddenly, your arm flops bonelessly to your side-- rather literally, I'm afraid. Damn you, Lockhart!\r\n",
      "Without warning, a raging torrent of red light crashes down on the city! This is it! The end times! The--\r\n" // 35
    };
    int mesnum = number(0, 35);
    /* Old messages, preserved for posterity.
      // "I like copyovers, yes I do!  Eating player corpses in a copyover stew!\r\n",
      // "A Haiku while you wait: Copyover time.  Your quests and corpses are fucked.  Ha ha ha ha ha.\r\n",
      // "Yes. We did this copyover solely to fuck YOUR character over.\r\n",
      // "Ahh drek, Maestra's broke the mud again!  Go bug Che and he might fix it.\r\n",
      // "Deleting player corpses, please wait...\r\n",
      // "Please wait while your character is deleted.\r\n",
      // "You are mortally wounded and will die soon if not aided.\r\n",
      // "Connection closed by foreign host.\r\n",
      // "Jerry Garcia told me to type copyover.  He is wise, isn't he?\r\n",
    */
    
    rnum_t airborne_rnum = real_room(RM_AIRBORNE);
  rnum_t boneyard_rnum = real_room(65505);

  if (boneyard_rnum < 0 || airborne_rnum < 0) {
    log("COPYOVERLOG: Skipping flying folks, you have no airborne room and/or boneyard.");
  } else {
    log("COPYOVERLOG: Shifting flying folks to a runway.");
    struct room_data *boneyard = &world[boneyard_rnum];
    struct room_data *airborne = &world[airborne_rnum];

    for (struct veh_data *aircraft = airborne->vehicles, *next_veh; aircraft; aircraft = next_veh) {
      next_veh = aircraft->next_veh;
      mudlog_vfprintf(aircraft->people, LOG_SYSLOG, "Transferring '%s' to Boneyard for copyover.", GET_VEH_NAME(aircraft));
      veh_from_room(aircraft);
      veh_to_room(aircraft, boneyard);
      send_to_veh("Your vehicle has been safely transferred to the Boneyard.\r\n", aircraft, NULL, TRUE);
    }
  }

  log("COPYOVERLOG: Cleaning up repairman.");
  struct room_data *staff_workroom = &world[real_room(10000)];
  for (struct char_data *i = character_list; i; i = i->next_in_character_list) {
    if (IS_NPC(i) && MOB_HAS_SPEC(i, fixer)) {
      while (i->carrying) {
        struct obj_data *obj = i->carrying;

        char *representation = generate_new_loggable_representation(obj);

        // No PC, no problem.
        if (!does_player_exist(GET_OBJ_TIMER(obj))) {
          mudlog_vfprintf(NULL, LOG_SYSLOG, "Discarding unowned repairman object: %s", representation);
          obj_from_char(obj);
          extract_obj(obj);
          continue;
        } else {
          mudlog_vfprintf(NULL, LOG_SYSLOG, "Preserving %ld's repairman object: %s", GET_OBJ_TIMER(obj), representation);
        }

        delete [] representation;

        // Fully repair item.
        GET_OBJ_CONDITION(obj) = GET_OBJ_BARRIER(obj);

        // Fully repair MPCP of storebought deck.
        rnum_t obj_rnum = real_object(GET_OBJ_VNUM(obj));
        if (obj_rnum >= 0 && GET_OBJ_TYPE(obj) == ITEM_CYBERDECK) {
          GET_CYBERDECK_MPCP(obj) = GET_CYBERDECK_MPCP(&obj_proto[obj_rnum]);
        }

        // Box it up for handing off.
        struct obj_data *container = read_object(OBJ_LARGE_PLASTIBOARD_BOX, VIRTUAL, OBJ_LOAD_REASON_SPECPROC);

        char *player_name = get_player_name(GET_OBJ_TIMER(obj));
        snprintf(buf, sizeof(buf), "%s's boxed-up repairman item: %s^n", player_name, get_string_after_color_code_removal(GET_OBJ_NAME(obj), NULL));
        delete [] player_name;

        container->restring = str_dup(buf);
        container->graffiti = str_dup(buf);

        obj_from_char(obj);
        obj_to_obj(obj, container);

        // Put it in the staff workroom.
        obj_to_room(container, staff_workroom);
        if (staff_workroom->people)
          act("The repairman's assistant drops off $p for post-copyover distribution.", FALSE, staff_workroom->people, obj, 0, TO_ROOM);
      }
    }
  }


  log("COPYOVERLOG: Disconnecting players.");
  /* For each playing descriptor, save its state */
  for (struct descriptor_data *d = descriptor_list, *d_next; d ; d = d_next) {
    struct char_data *och = d->character;
    d_next = d->next; // delete from list, save stuff

    // drops those logging on
    if (!och || d->connected > CON_PLAYING) {
      write_to_descriptor (d->descriptor, "\r\nSorry, we are rebooting. Come back in a few minutes.\r\n");
      close_socket (d); // yer outta here!
      continue;
    }

    // Refund people in cabs for the extra cash. Fixes edge case of 'I only had enough to cover my original cab fare'.
    if (!PLR_FLAGGED(och, PLR_NEWBIE) && och->in_room && room_is_a_taxicab(GET_ROOM_VNUM(och->in_room))) {
      snprintf(buf, sizeof(buf), "You have been refunded %d nuyen to compensate for the extra cab fare.\r\n", MAX_CAB_FARE);
      write_to_descriptor(d->descriptor, buf);
      GET_NUYEN_RAW(och) += MAX_CAB_FARE;
    }

    // Refund services for sustained spells: iterate through characters on the descriptor
    for (struct char_data *ch_ptr = och; ch_ptr; ch_ptr = (ch_ptr == d->character ? d->original : nullptr)) {
      // iterate through their sustains
      if (GET_TRADITION(ch_ptr) == TRAD_HERMETIC) {
        for (struct sustain_data *sust = ch_ptr->sustained; sust; sust = sust->next) {
          // Skip spells cast on them, and spells that aren't sustained by a spirit.
          if (!sust->is_caster_record || !sust->spirit)
            continue;

          // Spell is sustained by a spirit; iterate through your spiritdata to find it
          for (struct spirit_data *spiritdata = GET_SPIRIT(ch_ptr); spiritdata; spiritdata = spiritdata->next) {
            if (spiritdata->id == GET_GRADE(sust->spirit)) {
              // Found it. Add one to the services and bail.
              mudlog_vfprintf(ch_ptr, LOG_SYSLOG, "Adding 1 service to %s's spirit record #%d (f%d-%d) from sustained %s.",
                              GET_CHAR_NAME(ch_ptr),
                              spiritdata->id,
                              spiritdata->force,
                              spiritdata->type,
                              spells[sust->spell].name);
              spiritdata->services++;
              // Set the dirty bit here so the service data is saved to DB.
              GET_ELEMENTALS_DIRTY_BIT(ch_ptr) = true;
              break;
            }
          }
        }
      }
    }

    fprintf (fp, "%d\t%s\t%s\t%s\t%s\n", d->descriptor, GET_CHAR_NAME(och), d->host, CopyoverGet(d), CopyoverGetJSON(d));
    GET_LAST_IN(och) = get_ch_in_room(och)->number;
    if (!GET_LAST_IN(och) || GET_LAST_IN(och) == NOWHERE) {
      // Fuck it, send them to Grog's.
      snprintf(buf, sizeof(buf), "%s's location could not be determined by the current copyover logic. %s will load at Grog's (35500).",
              GET_CHAR_NAME(och), HSSH(och));
      mudlog(buf, och, LOG_SYSLOG, TRUE);
      GET_LAST_IN(och) = RM_ENTRANCE_TO_DANTES;
    }
    SaveChar(och, GET_LOADROOM(och), TRUE);
    write_to_descriptor(d->descriptor, messages[mesnum]);
  }

  fprintf (fp, "-1\n");
  fclose (fp);

  log("Saving houses.");
  save_all_apartments_and_storage_rooms();
  /* Close reserve and other always-open files and release other resources */

  // Save vehicles.
  log("COPYOVERLOG: Saving vehicles.");
  save_vehicles(TRUE);

  // Save shop orders.
  log("COPYOVERLOG: Saving shop orders.");
  save_shop_orders();

  log("COPYOVERLOG: Closing database connection.");
  DBFinalize();

  log("COPYOVERLOG: Clearing alarm handler.");
  signal(SIGALRM, SIG_IGN);

  snprintf(buf, sizeof(buf), "%d", port);
  snprintf(buf2, sizeof(buf2), "-o%d", mother_desc);
  /* Ugh, seems it is expected we are 1 step above lib - this may be dangerous! */

  chdir ("..");

  execl (EXE_FILE, "awake", buf2, buf, (char *) NULL); // Flawfinder: ignore

  /* Failed - sucessful exec will not return */

  perror ("do_copyover: execl");
  mudlog_vfprintf(NULL, LOG_SYSLOG, "Ah shit son, the copyover failed. Terminating the game so it can cleanly come back.");

  exit (1); /* too much trouble to try to recover! */
}

ACMD(do_copyover)
{
  struct descriptor_data *d;
  struct char_data *och;

  // Check for PCs on quests and people not in PLAYING state.
  strncpy(buf, "Characters currently on quests: ", sizeof(buf));
  int num_questors = 0;
  int fucky_states = 0;
  int cab_inhabitants = 0;
  for (d = descriptor_list; d; d = d->next) {
    // Count PCs in weird states.
    if (STATE(d) != CON_PLAYING) {
      fucky_states++;
      continue;
    }

    if (!(och = d->character))
      continue;

    if (och == ch)
      continue;

    if (GET_QUEST(och)) {
      snprintf(ENDOF(buf), sizeof(buf) - strlen(buf), "^c%s%s^n (idle: %d)",
               num_questors > 0 ? ", " : "",
               GET_CHAR_NAME(och),
               och->char_specials.timer);
      num_questors += 1;
    }
    // Count PCs in cabs.
    if (och->in_room && room_is_a_taxicab(GET_ROOM_VNUM(och->in_room)))
      cab_inhabitants++;
  }

  if (!ch->desc) {
    mudlog("SYSERR: Somehow, we ended up in COPYOVER with no ch->desc!", ch, LOG_SYSLOG, TRUE);
    return;
  }

  skip_spaces(&argument);
  // COPYOVER FORCE.
  if (!str_cmp(argument, "force")){
    snprintf(buf, sizeof(buf), "Forcibly copying over. This will disconnect %d player%s, refund %d cab fare%s, drop %d quest%s, and lose any repairman items.\r\n",
             fucky_states,    fucky_states    != 1 ? "s" : "",
             cab_inhabitants, cab_inhabitants != 1 ? "s" : "",
             num_questors,    num_questors    != 1 ? "s" : "");
    if (write_to_descriptor(ch->desc->descriptor, buf) < 0) {
      // Rofl, the copyover initiatior disconnected? Um.
      close_socket(ch->desc);
    }
  }
#ifdef IS_BUILDPORT
  else if (!str_cmp(argument, "start") || !str_cmp(argument, "begin") || !str_cmp(argument, "enqueue")) {
    FAILURE_CASE(global_copyover_enqueued_by_idnum, "A copyover has already been enqueued. You can cancel it with COPYOVER STOP.");
    mudlog_vfprintf(ch, LOG_WIZLOG, "%s has enqueued a copyover.", GET_CHAR_NAME(ch));

    for (d = descriptor_list; d; d = d->next) {
      if (d->character && d->character != ch) {
        send_to_char(d->character, "^CSystem Notice:^n ^WThe game will copyover to install new code sometime within the next 20 minutes.^n"
                                   " Please finish work in menus and wrap up autoruns in progress."
                                   " Any runs in progress at copyover will be canceled with no penalty.\r\n");
      }
    }
    send_to_char(ch, "OK, enqueued a copyover. Jobs will be canceled in 10 minutes, copyover forced in 20 minutes.\r\n");

    global_copyover_enqueued_by_idnum = GET_IDNUM_EVEN_IF_PROJECTING(ch);
    global_copyover_override_quests_at = time(0) + (10 * 60);

    // Disable OLC.
    olc_state = 0;
    mudlog_vfprintf(ch, LOG_WIZLOG, "OLC turned OFF by %s in preparation for copyover.", GET_CHAR_NAME(ch));
    return;
  }
  else if (!str_cmp(argument, "stop") || !str_cmp(argument, "cancel") || !str_cmp(argument, "dequeue") || !str_cmp(argument, "abort")) {
    FAILURE_CASE(!global_copyover_enqueued_by_idnum, "No copyover is currently queued.");
    mudlog_vfprintf(ch, LOG_WIZLOG, "%s has canceled the enqueued copyover.", GET_CHAR_NAME(ch));

    for (d = descriptor_list; d; d = d->next) {
      if (d->character && d->character != ch) {
        send_to_char(d->character, "^CSystem Notice:^n ^WThe copyover has been canceled.^n You may now return to your regularly-scheduled shenanigans.\r\n");
      }
    }

    const char *char_name = get_player_name(global_copyover_enqueued_by_idnum);
    send_to_char(ch, "OK, canceled the forced copyover enqueued by %s (%ld).", char_name, global_copyover_enqueued_by_idnum);
    delete [] char_name;

    global_copyover_enqueued_by_idnum = 0;

    // Enable OLC.
    olc_state = 1;
    mudlog_vfprintf(ch, LOG_WIZLOG, "OLC turned ON by %s due to copyover cancelation.", GET_CHAR_NAME(ch));
    return;
  }
#endif
  // Non-forcible copyover command (CHECK or CONFIRM).
  else {
    bool will_not_copyover = FALSE;

    // Check for questors.
    if (num_questors > 0) {
      send_to_char(ch, "There %s %d character%s doing autoruns right now.\r\n%s^n.\r\n",
                   num_questors != 1 ? "are" : "is",
                   num_questors,
                   num_questors != 1 ? "s" : "",
                   buf);
      will_not_copyover = TRUE;
    }

    // Check for PCs in non-playing states.
    if (fucky_states > 0) {
      send_to_char(ch, "%d player%s not in the playing state. Check USERS for details.\r\n",
                   fucky_states, fucky_states != 1 ? "s are" : " is");
      will_not_copyover = TRUE;
    }

    // Check for cab-riders.
    if (cab_inhabitants) {
      send_to_char(ch, "There %s %d %s.\r\n",
                   cab_inhabitants != 1 ? "are" : "is",
                   cab_inhabitants,
                   cab_inhabitants != 1 ? "people taking taxis" : "person taking a cab");
      will_not_copyover = TRUE;
    }

    // Check for repairman items.
    /*
    for (struct char_data *i = character_list; i; i = i->next_in_character_list) {
      if (IS_NPC(i) && MOB_HAS_SPEC(i, fixer) && i->carrying) {
        send_to_char("The repairman has unclaimed items.\r\n", ch);
        will_not_copyover = TRUE;
        break;
      }
    }
    */

    // There is no command in this if-statement that allows us to bypass a failed check state.
    if (will_not_copyover) {
      send_to_char("Copyover aborted. Use 'copyover force' to override this.\r\n", ch);
      return;
    } 
    // If there were no errors, note this.
    else {
      send_to_char("Copyover is possible, no error conditions noted.\r\n", ch);
    }

    // COPYOVER CHECK bails out at this point.
    if (!str_cmp(argument, "check")) {
      return;
    }

#ifdef USE_PRIVATE_CE_WORLD
#ifndef IS_BUILDPORT
    // Guard against accidental copyovers on main port.
    if (str_cmp(argument, "confirm") != 0) {
      send_to_char("This is the PLAYER PORT, so you need to type ^WCOPYOVER CONFIRM^n to execute.\r\n", ch);
      return;
    }
#endif // IS_BUILDPORT
#endif // USE_PRIVATE_CE_WORLD
  }

  mudlog_vfprintf(ch, LOG_WIZLOG, "Copyover initiated by %s", GET_CHAR_NAME(ch));
  execute_copyover();
}

/* End This Part of the Copyover System */