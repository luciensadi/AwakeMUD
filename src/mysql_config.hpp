#ifndef _mysql_config_h_
#define _mysql_config_h_

extern const char *mysql_host;
extern const char *mysql_password;
extern const char *mysql_user;
extern const char *mysql_db;

/* MudVault API key (format: "mv_<mud_id>_<secret>"). Only ever defined in the
 * gitignored mysql_config.cpp -- never copy this value anywhere else. */
extern const char *mudvault_api_key;

#define GAME_MYSQL_PORT 0

#endif
