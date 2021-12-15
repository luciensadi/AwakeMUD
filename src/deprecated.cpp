#include <math.h>

#include "awake.h"
#include "structs.h"
#include "utils.h"

/* Given a cybereye package, calculate what its essence cost would have been under the old system. */
int get_deprecated_cybereye_essence_cost(struct obj_data *obj) {
  if (!obj || GET_OBJ_TYPE(obj) != ITEM_CYBERWARE || GET_CYBERWARE_TYPE(obj) != CYB_EYES) {
    mudlog("SYSERR: Non-cybereyes supplied to get_deprecated_cybereye_essence_cost().", NULL, LOG_SYSLOG, TRUE);
    return 0;
  }

  int essence_cost = 0;

  // DEBUG
  essence_cost += 20;

  if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_CAMERA)) {
    essence_cost = 40;
  }
  if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_DISPLAYLINK)) {
    essence_cost += 10;
  }
  if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_FLARECOMP)) {
    essence_cost += 10;
  }
  if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_IMAGELINK)) {
    essence_cost += 20;
  }
  if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_LOWLIGHT)) {
    essence_cost += 20;
  }
  if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_CLOCK)) {
    essence_cost += 10;
  }
  if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_RETINALDUPLICATION)) {
    essence_cost += 10;
  }
  if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_THERMO)) {
    essence_cost += 20;
  }
  if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_OPTMAG1)) {
    essence_cost += 20;
  }
  if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_OPTMAG2)) {
    essence_cost += 20;
  }
  if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_OPTMAG3)) {
    essence_cost += 20;
  }
  if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_ELECMAG1)) {
    essence_cost += 10;
  }
  if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_ELECMAG2)) {
    essence_cost += 10;
  }
  if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_ELECMAG3)) {
    essence_cost += 10;
  }
  if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_GUN)) {
    essence_cost += 40;
  }
  if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_DATAJACK)) {
    essence_cost += 25;
  }
  if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_DART)) {
    essence_cost += 25;
  }
  if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_LASER)) {
    switch (GET_OBJ_VAL(obj, 1)) {
      case 1:
        essence_cost += 20;
        break;
      case 2:
        essence_cost += 30;
        break;
      case 3:
        essence_cost += 50;
        break;
    }
  }
  if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_LIGHT)) {
    essence_cost += 20;
  }
  if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_ULTRASOUND)) {
    essence_cost += 50;
  }
  if (IS_SET(GET_OBJ_VAL(obj, 3), EYE_CYBEREYES)) {
    essence_cost = 20 + MAX(0, essence_cost - 50);
  }

  switch (GET_OBJ_VAL(obj, 2)) {
    case GRADE_ALPHA:
      essence_cost = (int) round(essence_cost * .8);
      break;
    case GRADE_BETA:
      essence_cost = (int) round(essence_cost * .6);
      break;
    case GRADE_DELTA:
      essence_cost = (int) round(essence_cost * .5);
      break;
  }

  return essence_cost;
}
