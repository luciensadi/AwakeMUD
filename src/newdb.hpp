// file: newdb.h
// author: Andrew Hynek
// contents: class databii for players (todo: mobs, rooms, etc)

// for players: rnum = table index, vnum = id#

#ifndef __newdb_h__
#define __newdb_h__

#include <mysql/mysql.h>

#include "awake.hpp"
#include "structs.hpp"
#include "utils.hpp"

#define CODEBASE_REQUIRED_MYSQL_MAX_PACKET_MINIMUM_SIZE 10000000

class File;

extern bool SaveChar(char_data *ch, vnum_t loadroom = NOWHERE, bool fromCopyover = FALSE);
extern char_data *LoadChar(const char *name, bool logon, int load_origin);
extern char_data *CreateChar(struct char_data *ch);

extern bool does_player_exist(vnum_t idnum);
extern bool does_player_exist(const char *name);
extern void DeleteChar(vnum_t idnum);
extern idnum_t get_player_id(const char *name);
extern int get_player_rank(long idnum);
extern char *get_player_name(vnum_t id);
extern char *prepare_quotes(char *dest, const char *str, size_t size_of_dest, bool include_newline_processing=FALSE, bool is_like=FALSE);
extern bool get_plr_flag_is_set_by_idnum(int flag, vnum_t id);
extern bool get_prf_flag_is_set_by_idnum(int flag, vnum_t id);
extern bool get_aff_flag_is_set_by_idnum(int flag, vnum_t id);
extern bool pc_active_in_last_30_days(idnum_t owner_id);
extern int get_attr_max(struct char_data *ch, int attr);
extern bool player_is_dead_hardcore(long id);

// DB tag functions, for when you need to persist data but don't want to write a new table for it.
bool player_has_db_tag(idnum_t idnum, const char *tag_name);
void set_db_tag(idnum_t idnum, const char *tag_name);
void remove_db_tag(idnum_t idnum, const char *tag_name);

extern MYSQL *mysql;
extern std::unordered_map<std::string, bool> global_existing_player_cache;

extern int mysql_wrapper(MYSQL *mysql, const char *query);
#endif // ifndef __newdb_h__
