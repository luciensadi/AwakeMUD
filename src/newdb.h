// file: newdb.h
// author: Andrew Hynek
// contents: class databii for players (todo: mobs, rooms, etc)

// for players: rnum = table index, vnum = id#

#ifndef __newdb_h__
#define __newdb_h__

#include <mysql/mysql.h>

#include "awake.h"
#include "structs.h"
#include "utils.h"

class File;

class DBIndex
{
public:
  typedef long vnum_t;
  typedef long rnum_t;

  virtual ~DBIndex()
  {}

  static bool IsValidR(rnum_t idx)
  {
    return (idx >= 0);
  }
  static bool IsValidV(vnum_t virt)
  {
    return (virt >= 0);
  }

};

class PCIndex : public DBIndex
{
private:
  struct entry
  {
    char name[MAX_NAME_LENGTH];
    vnum_t id;
    int level;
    dword flags;
    time_t last;
    char_data *active_data;
    int instance_cnt;

    entry() :
      id(0), level(0), flags(0), last(0), active_data(NULL), instance_cnt(0)
    {
      memset(name, 0, sizeof(name));
    }

    entry &operator=(const entry &two)
    {
      strcpy(name, two.name);
      id = two.id;
      level = two.level;
      flags = two.flags;
      last = two.last;
      active_data = two.active_data;
      instance_cnt = two.instance_cnt;

      return *this;
    }
  }
  *tab;

  // TODO: a tree structure

  int entry_cnt;
  int entry_size;
  bool needs_save;

  enum {
    NODELETE = (1 << 12)
  };

public:
  PCIndex();
  ~PCIndex();

  bool Load()
  {
    return load();
  }
  bool Save();

  // lookup/access functions
  //
  int NumPlayers()
  {
    return entry_cnt;
  }

  // mutation functions
  //
  char_data *CreateChar(char_data *ch);

  // loads the character and syncs IDs, returns the main struct
  // if logon is true, then the last logon time is set.
  char_data *LoadChar(const char *name, bool logon);

  // just saves the character
  bool SaveChar(char_data *ch, vnum_t loadroom = NOWHERE, bool fromCopyover = FALSE);

  // saves and puts character into storage
  // WARNING: this makes ch INVALID! (cause it returns the memory)

  // updates the index
  void Update(char_data *ch);
  vnum_t find_open_id();

private:
  void reset();
  bool load();
  int  count_entries(File *index);
  void sort_by_id();
  // void resize_table(int empty_slots = 100);
  void sorted_insert(const entry *info);

  void clear_by_time();
  friend int entry_compare(const void *one, const void *two);
};

extern PCIndex playerDB;
extern bool does_player_exist(vnum_t idnum);
extern bool does_player_exist(char *name);
extern char_data *CreateChar(struct char_data *ch);
extern void DeleteChar(vnum_t idnum);
extern bool load_char(const char *name, struct char_data *ch, bool logon);
extern vnum_t get_player_id(char *name);
extern int get_player_rank(long idnum);
extern char *get_player_name(vnum_t id);
extern char *prepare_quotes(char *dest, const char *str, size_t size_of_dest, bool include_newline_processing=FALSE, bool is_like=FALSE);
extern bool get_plr_flag_is_set_by_idnum(int flag, vnum_t id);
extern bool get_prf_flag_is_set_by_idnum(int flag, vnum_t id);
extern bool get_aff_flag_is_set_by_idnum(int flag, vnum_t id);

extern MYSQL *mysql;

extern int mysql_wrapper(MYSQL *mysql, const char *query);
#endif // ifndef __newdb_h__
