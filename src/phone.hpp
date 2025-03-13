#ifndef __phone_h__
#define __phone_h__
#include <unordered_map>

using namespace std;

extern unordered_map<string, phone_data*> phone_db;

bool             try_resolve_phone_number(const char* phone_number, struct phone_data *&phone);
bool             is_phone_number_valid_format(const char* input);
bool             phone_exists(const char* phone);
phone_data*      create_phone_number(struct obj_data *for_obj, int load_origin);
bool             try_resolve_obj_phone(struct obj_data *for_obj, struct phone_data *&phone);
void             extract_phone_number(const char* phone_number);
void             extract_phone_number(struct obj_data *obj);
void             extract_phone_number(struct matrix_icon *icon);
void             send_to_other_phone(struct phone_data *sender, const char* message);
void             send_to_phone(struct phone_data *receiver, const char* message);
bool             try_get_phone_character(struct phone_data *receiver, struct char_data *&to_character);
void             terminate_call(struct phone_data *for_phone, bool silent);
void             ring_phone(struct phone_data *receiver);
void             do_phone_talk(struct char_data *ch, struct phone_data *phone, char* message);

void             handle_phone_command(struct char_data *ch, int subcmd, char *argument);
void             phones_update();

#endif
