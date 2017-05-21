/* ************************************************************************
*   File: interpreter.h                                 Part of CircleMUD *
*  Usage: header file: public procs, macro defs, subcommand defines       *
*                                                                         *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  Copyright (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#ifndef _interpreter_h_
#define _interpreter_h_
/*
#define ACMD(name)  \
   void (name)(struct char_data *ch, __attribute__ ((unused)) char *argument, __attribute__ ((unused)) int cmd, __attribute__ ((unused)) int subcmd)
*/

#define ACMD(name)   \
     void (name)(struct char_data *ch, char *argument, int cmd, int subcmd)

const int RETURN_HELP = -37;

#define CMD_NAME (cmd_info[cmd].command)
#define CMD_IS(cmd_name) (!strcmp(cmd_name, cmd_info[cmd].command))
#define IS_MOVE(cmdnum) (cmdnum >= 1 && cmdnum <= 6)


void    command_interpreter(struct char_data *ch, char *argument, char *tchname);
int     search_block(char *arg, const char **list, bool exact);
char    lower( char c );
char    *one_argument(char *argument, char *first_arg);
char    *any_one_arg(char *argument, char *first_arg);
char    *two_arguments(char *argument, char *first_arg, char *second_arg);
int     fill_word(char *argument);
void    half_chop(char *string, char *arg1, char *arg2);
void    nanny(struct descriptor_data *d, char *arg);
int     is_abbrev(const char *arg1, const char *arg2);
int     is_number(char *str);
int     find_command(char *command);
int     find_mcommand(char *command);
void    skip_spaces(char **string);
char    *delete_doubledollar(char *string);

struct command_info
{
  const char *command;
  byte minimum_position;
  void (*command_pointer)
  (struct char_data *ch, char * argument, int cmd, int subcmd);
  sh_int minimum_level;
  int  subcmd;
  byte  action;
};

#ifndef __INTERPRETER_CC__
extern struct command_info cmd_info[];
extern struct command_info mtx_info[];
extern struct command_info rig_info[];
#endif

struct alias
{
  char *command;
  char *replacement;
  int type;
  struct alias *next;
  alias(): 
    command(NULL), replacement(NULL), next(NULL)
  {}
};
#endif
