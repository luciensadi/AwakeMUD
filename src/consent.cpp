// Consent system. Very bare-bones right now, to be expanded later.

#include "awake.hpp"
#include "structs.hpp"
#include "interpreter.hpp"
#include "comm.hpp"

ACMD(do_consent) {
  const char *consent_syntax = "\r\nPlease specify the consent level you'd like to anonymously show: "
                             "\r\n  ^gGREEN^n: full / enthusiastic consent"
                             "\r\n  ^yYELLOW^n: slow down / be careful with this subject matter"
                             "\r\n  ^rRED^n: fade to black, discuss and resolve OOCly"
                             "\r\n  ^LBLACK^n: immediately halt scene, staff will handle resolution / retconning as needed"
                             "\r\n"
                             "\r\nExample: CONSENT YELLOW\r\n";

  // Valid options are flag colors: green, yellow, red.
  skip_spaces(&argument);

  if (!*argument) {
    send_to_char(consent_syntax, ch);
    return;
  }

  // Compose the list of who all is present.
  char participants[10000] = { '\0' };

  // Get people standing in the room / veh.
  for (struct char_data *tmp = ch->in_room ? ch->in_room->people : ch->in_veh->people; 
       tmp; 
       tmp = ch->in_room ? tmp->next_in_room : tmp->next_in_veh) 
  {
    if (!tmp->desc)
      continue;

    snprintf(ENDOF(participants), sizeof(participants) + strlen(participants), "%s%s", *participants ? ", " : "", GET_CHAR_NAME(tmp));
  }

  // Get people in surrounding vehs.
  for (struct veh_data *veh = ch->in_room ? ch->in_room->vehicles : ch->in_veh->carriedvehs; 
       veh; 
       veh = veh->next_veh) 
  {
    // People in the veh.
    for (struct char_data *tmp = veh->people; tmp; tmp = tmp->next_in_veh) {
      if (!tmp->desc)
        continue;

      snprintf(ENDOF(participants), sizeof(participants) + strlen(participants), "%s%s (veh)", *participants ? ", " : "", GET_CHAR_NAME(tmp));
    }

    // Rigger of the veh.
    if (veh->rigger)
      snprintf(ENDOF(participants), sizeof(participants) + strlen(participants), "%s%s (rigging)", *participants ? ", " : "", GET_CHAR_NAME(veh->rigger));
  }

  if (IS_SENATOR(ch)) {
    send_to_char(ch, "^Lconsent debug: '%s'; %s\r\n", argument, participants);
  }

  // GREEN: Full consent, enthusiastic.
  if (is_abbrev(argument, "green flag") || is_abbrev(argument, "green card") || is_abbrev(argument, "enthusiastic") || is_abbrev(argument, "granted")) {
    const char *msg = "[^gOOC NOTICE:^n Someone present has indicated full consent.]\r\n";
    if (ch->in_room) {
      send_to_room(msg, ch->in_room, NULL);
    } else {
      send_to_veh(msg, ch->in_veh, NULL, TRUE);
    }
    log_vfprintf("Consent Note: Someone in a scene involving (%s) has raised a green consent flag.", participants);
    return;
  }

  // YELLOW: Uncertain, potentially touchy, slow down and be careful.
  if (is_abbrev(argument, "yellow flag") || is_abbrev(argument, "yellow card") || is_abbrev(argument, "slow") || is_abbrev(argument, "careful") || is_abbrev(argument, "tentative") || is_abbrev(argument, "sensitive") || is_abbrev(argument, "maybe")) {
    const char *msg = "[^yOOC NOTICE:^n Someone present has indicated a desire for restraint. Please be sensitive and ramp back potentially-objectionable content.]\r\n";
    if (ch->in_room) {
      send_to_room(msg, ch->in_room, NULL);
    } else {
      send_to_veh(msg, ch->in_veh, NULL, TRUE);
    }
    log_vfprintf("Consent Note: Someone in a scene involving (%s) has raised a yellow consent flag.", participants);
    return;
  }

  // RED: Fade to black, we'll work it out.
  if (is_abbrev(argument, "red flag") || is_abbrev(argument, "red card") || is_abbrev(argument, "fade") || is_abbrev(argument, "ftb") || is_abbrev(argument, "retract") || is_abbrev(argument, "withdraw") || is_abbrev(argument, "rescinded") || is_abbrev(argument, "revoke")) {
    const char *msg = "[^rOOC NOTICE:^n Someone present has indicated a desire to fade to black. Please terminate this scene and agree on an outcome. Involve staff if consensus can't be reached.]\r\n";
    if (ch->in_room) {
      send_to_room(msg, ch->in_room, NULL);
    } else {
      send_to_veh(msg, ch->in_veh, NULL, TRUE);
    }
    mudlog_vfprintf(NULL, LOG_SYSLOG, "Consent Note: FYI, someone in a scene involving (%s) has raised a RED consent flag. Please DO NOT engage unless requested.", participants);

    send_to_char(ch, "\r\n[FYI, we have a robust IGNORE system that lets you block content from other players. You can IGNORE X ALL to block all contact with X, or IGNORE X to view more granular controls. Please feel free to use this system, and also to contact staff if help is needed!]\r\n");    return;
  }

  // BLACK: wtf, stop, staff will work it out.
  if (is_abbrev(argument, "black flag") || is_abbrev(argument, "black card") || is_abbrev(argument, "nope") || is_abbrev(argument, "stop") || is_abbrev(argument, "panic button") || is_abbrev(argument, "emergency")) {
    const char *msg = "[^ROOC NOTICE:^n Consent for this scene is not present. ^WIt must be stopped immediately with no further emoting.^n Staff have been notified and will assist in mediating any outcome or retconning needed.]\r\n";
    if (ch->in_room) {
      send_to_room(msg, ch->in_room, NULL);
    } else {
      send_to_veh(msg, ch->in_veh, NULL, TRUE);
    }
    mudlog_vfprintf(NULL, LOG_SYSLOG, "^RURGENT HELP NEEDED FOR CONSENT ISSUE^n: Someone in a scene involving (%s) has raised a BLACK consent flag. Please ensure RP is halted and tag Lucien or Jank for followup retconning / resolution.", participants);
    
    send_to_char(ch, "\r\n[Staff will respond as soon as possible. In the meantime, please be aware that we have a robust IGNORE system that lets you block content from other players. You can IGNORE X ALL to block all contact with X, or IGNORE X to view more granular controls.]\r\n");
    return;
  }

  // No valid keyword found.
  send_to_char(consent_syntax, ch);
}