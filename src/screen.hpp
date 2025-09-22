/* ************************************************************************
*   File: screen.h                                      Part of CircleMUD *
*  Usage: header file with ANSI color codes for online color              *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#ifndef _screen_h_
#define _screen_h_

extern const char *KNRM;
extern const char *KBLK;
extern const char *KRED;
extern const char *KGRN;
extern const char *KYEL;
extern const char *KBLU;
extern const char *KMAG;
extern const char *KCYN;
extern const char *KWHT;
extern const char *CGLOB;

extern const char *B_BLK;
extern const char *B_RED;
extern const char *B_GREEN;
extern const char *B_YELLOW;
extern const char *B_BLUE;
extern const char *B_MAGENTA;
extern const char *B_CYAN;
extern const char *B_WHITE;
extern const char *BL_RED;
extern const char *BL_GREEN;
extern const char *BL_YELLOW;
extern const char *BL_BLUE;
extern const char *BL_MAGENTA;
extern const char *BL_CYAN;
extern const char *BL_WHITE;
extern const char *BB_RED;
extern const char *BB_GREEN;
extern const char *BB_YELLOW;
extern const char *BB_BLUE;
extern const char *BB_MAGENTA;
extern const char *BB_CYAN;
extern const char *BB_WHITE;
extern const char *H_RED;
extern const char *H_GREEN;
extern const char *H_YELLOW;
extern const char *H_BLUE;
extern const char *H_MAGENTA;
extern const char *H_CYAN;
extern const char *H_WHITE;
extern const char *KNUL;


/* conditional color.  pass it a pointer to a char_data and a color level. */
#define C_OFF   0
#define C_SPR   1
#define C_NRM   2
#define C_CMP   3

#define CCNRM(ch,lvl)  (KNRM)
#define CCRED(ch,lvl)  (KRED)
#define CCGRN(ch,lvl)  (KGRN)
#define CCYEL(ch,lvl)  (KYEL)
#define CCBLU(ch,lvl)  (KBLU)
#define CCMAG(ch,lvl)  (KMAG)
#define CCCYN(ch,lvl)  (KCYN)
#define CCWHT(ch,lvl)  (KWHT)
#define CBCYN(ch,lvl)  (B_CYAN)
#define CBYEL(ch,lvl)  (B_YELLOW)
#define CBWHT(ch,lvl)  (B_WHITE)
#define CBRED(ch,lvl)  (B_RED)
#define CBBLU(ch,lvl)  (B_BLUE)
#define CBGRN(ch,lvl)  (B_GREEN)
#define CBBLK(ch,lvl)  (B_BLK)

#define QNRM CCNRM(ch,C_SPR)
#define QRED CCRED(ch,C_SPR)
#define QGRN CCGRN(ch,C_SPR)
#define QYEL CCYEL(ch,C_SPR)
#define QBLU CCBLU(ch,C_SPR)
#define QMAG CCMAG(ch,C_SPR)
#define QCYN CCCYN(ch,C_SPR)
#define QWHT CCWHT(ch,C_SPR)

#endif
