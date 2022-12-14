//  file: dblist.h
//  authors: Chris Dickey, Andrew Hynek
//  purpose: contains the objList class, a "child" of List
//  Copyright (c) 1996 by Chris Dickey,
//  some parts Copyright (c) 1998 by Andrew Hynek

#include "list.hpp"

class objList : public List<struct obj_data *>
{
  int trideo_ticks;
public:
  objList() { trideo_ticks = 0; }

  int PrintList(struct char_data *ch, const char *arg, bool override_vis_check=FALSE);
  int CountObj(int num);
  int CountPlayerCorpses();
  struct obj_data *FindObj(int num);
  struct obj_data *FindObj(struct char_data *ch, char *name, int num);
  void UpdateObjs(const struct obj_data *proto, int rnum);
  void UpdateObjsIDelete(const struct obj_data *proto, int rnum, int new_rnum);
  void UpdateCounters(void);
  void UpdateNums(int num);
  void Traverse(void (*func)(struct obj_data *));
  void CallSpec();
  void RemoveObjNum(int num);
  void RemoveQuestObjs(int id);
  void DisassociateCyberdeckPartsFromDeck(struct obj_data *deck);

  #ifdef ENABLE_THIS_IF_YOU_WANT_TO_HATE_YOUR_LIFE
  void CheckPointers();
  #endif
};
