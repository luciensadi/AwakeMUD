//  file: dblist.h
//  authors: Chris Dickey, Andrew Hynek
//  purpose: contains the objList class, a "child" of List
//  Copyright (c) 1996 by Chris Dickey,
//  some parts Copyright (c) 1998 by Andrew Hynek

#include "list.h"

class objList : public List<struct obj_data *>
{
public:
  int PrintList(struct char_data *ch, const char *arg);
  int CountObj(int num);
  struct obj_data *FindObj(int num);
  struct obj_data *FindObj(struct char_data *ch, char *name, int num);
  void UpdateObjs(const struct obj_data *proto, int rnum);
  void UpdateCounters(void);
  void UpdateNums(int num);
  void Traverse(void (*func)(struct obj_data *));
  void CallSpec();
  void RemoveObjNum(int num);
  void RemoveQuestObjs(int id);
};

