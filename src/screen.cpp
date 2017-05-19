/* ************************************************************************
*   File: screen.h                                      Part of CircleMUD *
*  Usage: header file with ANSI color codes for online color              *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include "screen.h"


/* Attribute codes:                                                       */
/* 00=none 01=bold 04=underscore 05=blink 07=reverse 08=concealed         */
/* Text color codes:                                                      */
/* 30=black 31=red 32=green 33=yellow 34=blue 35=magenta 36=cyan 37=white */
/* Background color codes:                                                */
/* 40=black 41=red 42=green 43=yellow 44=blue 45=magenta 46=cyan 47=white */

const char *KNRM  = "\x1B[0m";    /* Clears Color */
const char *KBLK  = "\x1B[0;30m";
const char *KRED  = "\x1B[0;31m";  /* Standard Colors */
const char *KGRN  = "\x1B[0;32m";
const char *KYEL  = "\x1B[0;33m";
const char *KBLU  = "\x1B[0;34m";
const char *KMAG  = "\x1B[0;35m";
const char *KCYN  = "\x1B[0;36m";
const char *KWHT  = "\x1B[0;37m";

const char *CGLOB = KNRM;

const char *B_BLK      =     "\x1B[1;30m";
const char *B_RED      =     "\x1B[1;31m";  /* Bold Colors */
const char *B_GREEN    =     "\x1B[1;32m";
const char *B_YELLOW   =     "\x1B[1;33m";
const char *B_BLUE     =     "\x1B[1;34m";
const char *B_MAGENTA  =     "\x1B[1;35m";
const char *B_CYAN     =     "\x1B[1;36m";
const char *B_WHITE    =     "\x1B[1;37m";
const char *BL_RED     =     "\x1B[5;31m";  /* Blinking Colors */
const char *BL_GREEN   =     "\x1B[5;32m";
const char *BL_YELLOW  =     "\x1B[5;33m";
const char *BL_BLUE    =     "\x1B[5;34m";
const char *BL_MAGENTA =     "\x1B[5;35m";
const char *BL_CYAN    =     "\x1B[5;36m";
const char *BL_WHITE   =     "\x1B[5;37m";
const char *BB_RED     =     "\x1B[1;5;31m"; /* Bold Blinking Colors */
const char *BB_GREEN   =     "\x1B[1;5;32m";
const char *BB_YELLOW  =     "\x1B[1;5;33m";
const char *BB_BLUE    =     "\x1B[1;5;34m";
const char *BB_MAGENTA =     "\x1B[1;5;35m";
const char *BB_CYAN    =     "\x1B[1;5;36m";
const char *BB_WHITE   =     "\x1B[1;5;37m";
const char *H_RED      =     "\x1B[7;31m";  /* Highlighted (Inverse) Colors */
const char *H_GREEN    =     "\x1B[7;32m";
const char *H_YELLOW   =     "\x1B[7;33m";
const char *H_BLUE     =     "\x1B[7;34m";
const char *H_MAGENTA  =     "\x1B[7;35m";
const char *H_CYAN     =     "\x1B[7;36m";
const char *H_WHITE    =     "\x1B[7;37m";
const char *KNUL  = "";
