/* ************************************************************************
*   File: weather.c                                     Part of CircleMUD *
*  Usage: functions handling time and the weather                         *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "structs.h"
#include "awake.h"
#include "utils.h"
#include "comm.h"
#include "handler.h"
#include "interpreter.h"
#include "db.h"
#include "newmagic.h"
extern struct time_info_data time_info;
extern struct char_data *character_list;
extern struct index_data *mob_index;

const char *moon[] =
  {
    "new",
    "waxing",
    "full",
    "waning",
    "\n"
  };

void another_hour(void)
{
  bool new_month = FALSE;
  struct char_data *ch, *next;
  const char *temp;

  time_info.hours++;

  switch (time_info.hours) {
  case 7:
    weather_info.sunlight = SUN_RISE;
    if (weather_info.sky == SKY_CLOUDLESS)
      temp = "^y";
    else
      temp = "^L";
    snprintf(buf, sizeof(buf), "%sThe sun rises in the east.^n\r\n", temp);
    send_to_outdoor(buf);
    for (ch = character_list; ch; ch = next) {
      next = ch->next;
      if (IS_SPIRIT(ch)) {
        if (--GET_SPARE1(ch) <= 0) {
          act("$n abruptly fades from existance.", TRUE, ch, 0, 0, TO_ROOM);
          end_spirit_existance(ch, FALSE);
        } else {
          act("$n weakens as the metaphysical power of sunrise ripples through it.\r\n", TRUE, ch, 0, 0, TO_ROOM);
        }
      }
    }
    break;
  case 9:
    weather_info.sunlight = SUN_LIGHT;
    switch (weather_info.sky) {
    case SKY_CLOUDLESS:
      temp = "^Y";
      break;
    case SKY_CLOUDY:
    case SKY_RAINING:
      temp = "^n";
      break;
    case SKY_LIGHTNING:
      temp = "^L";
      break;
    default:
      temp = "^Y";
      break;
    }
    snprintf(buf, sizeof(buf), "%sThe day has begun.^n\r\n", temp);
    send_to_outdoor(buf);
    break;
  case 18:
    weather_info.sunlight = SUN_SET;
    switch (weather_info.sky) {
    case SKY_CLOUDLESS:
      temp = "^r";
      break;
    case SKY_CLOUDY:
      temp = "^B";
      break;
    case SKY_RAINING:
      temp = "^b";
      break;
    case SKY_LIGHTNING:
      temp = "^L";
      break;
    default:
      temp = "^r";
      break;
    }
    snprintf(buf, sizeof(buf), "%sThe sun slowly disappears in the west.^n\r\n", temp);
    send_to_outdoor(buf);
    for (ch = character_list; ch; ch = next) {
      next = ch->next;
      if (IS_SPIRIT(ch)) {
        act("$n abruptly fades from existance.", TRUE, ch, 0, 0, TO_ROOM);
        end_spirit_existance(ch, FALSE);
      }
    }
    break;
  case 21:
    weather_info.sunlight = SUN_DARK;
    if (weather_info.sky == SKY_CLOUDLESS)
      snprintf(buf, sizeof(buf), "^bThe night has begun. The moon is %s.^n\r\n",moon[weather_info.moonphase]);
    else
      snprintf(buf, sizeof(buf), "^LThe night has begun.^n\r\n");

    send_to_outdoor(buf);
    break;
  }

  if (time_info.hours > 23) {
    time_info.hours -= 24;
    time_info.day++;
    time_info.weekday++;
    if (time_info.weekday > 6)
      time_info.weekday = 0;

    if (((time_info.month == 3) || (time_info.month == 5) ||
         (time_info.month == 8) || (time_info.month == 10)) &&
        (time_info.day > 29))
      new_month = TRUE;
    else if ((time_info.month == 1) && (time_info.day > 27))
      new_month = TRUE;
    else if (time_info.day > 30)
      new_month = TRUE;
    if (new_month) {
      time_info.day = 0;
      time_info.month++;

      if (time_info.month > 11) {
        time_info.month = 0;
        time_info.year++;
      }
    }
  }
  if(time_info.day < 7)
    weather_info.moonphase = MOON_NEW;
  else if(time_info.day > 7 && time_info.day < 14)
    weather_info.moonphase = MOON_WAX;
  else if(time_info.day > 14 && time_info.day < 21)
    weather_info.moonphase = MOON_FULL;
  else
    weather_info.moonphase = MOON_WANE;

}

void another_minute(void)
{
  time_info.minute++;

  if (time_info.minute >= 60) {
    time_info.minute = 0;
    another_hour();
  }
}

void weather_change(void)
{
  PERF_PROF_SCOPE(pr_, __func__);
  int diff, change;

  if ((time_info.month >= 8) && (time_info.month <= 12))
    diff = (weather_info.pressure > 985 ? -2 : 2);
  else
    diff = (weather_info.pressure > 1015 ? -2 : 2);

  weather_info.change += (dice(1, 4) * diff + dice(2, 6) - dice(2, 6));

  weather_info.change = MIN(weather_info.change, 12);
  weather_info.change = MAX(weather_info.change, -12);

  weather_info.pressure += weather_info.change;

  weather_info.pressure = MIN(weather_info.pressure, 1040);
  weather_info.pressure = MAX(weather_info.pressure, 960);

  change = 0;
  switch (weather_info.sky) {
  case SKY_CLOUDLESS:
    if (weather_info.pressure < 990)
      change = 1;
    else if (weather_info.pressure < 1010)
      if (dice(1, 4) == 1)
        change = 1;
    break;
  case SKY_CLOUDY:
    if (weather_info.pressure < 970)
      change = 2;
    else if (weather_info.pressure < 990)
      if (dice(1, 4) == 1)
        change = 2;
      else
        change = 0;
    else if (weather_info.pressure > 1030)
      if (dice(1, 4) == 1)
        change = 3;
    break;
  case SKY_RAINING:
    if (weather_info.pressure < 970)
      if (dice(1, 4) == 1)
        change = 4;
      else
        change = 0;
    else if (weather_info.pressure > 1030)
      change = 5;
    else if (weather_info.pressure > 1010)
      if (dice(1, 4) == 1)
        change = 5;
    break;
  case SKY_LIGHTNING:
    if (weather_info.pressure > 1010)
      change = 6;
    else if (weather_info.pressure > 990)
      if (dice(1, 4) == 1)
        change = 6;
    break;
  default:
    change = 0;
    weather_info.sky = SKY_CLOUDLESS;
    break;
  }

  switch (change) {
  case 0:
    break;
  case 1:
    send_to_outdoor("^LThe sky starts to get cloudy.^n\r\n");
    weather_info.sky = SKY_CLOUDY;
    break;
  case 2:
    send_to_outdoor("^BIt starts to rain.^n\r\n");
    weather_info.sky = SKY_RAINING;
    break;
  case 3:
    send_to_outdoor("^CThe clouds disappear.^n\r\n");
    weather_info.sky = SKY_CLOUDLESS;
    break;
  case 4:
    send_to_outdoor("^WLightning^L starts to show in the sky.^n\r\n");
    weather_info.sky = SKY_LIGHTNING;
    break;
  case 5:
    send_to_outdoor("^cThe rain stops.^n\r\n");
    weather_info.sky = SKY_CLOUDY;
    weather_info.lastrain = 0;
    break;
  case 6:
    send_to_outdoor("^cThe ^Wlightning^c stops.^n\r\n");
    weather_info.sky = SKY_RAINING;
    break;
  default:
    break;
  }
  if (weather_info.sky < SKY_RAINING)
    weather_info.lastrain++;
}
