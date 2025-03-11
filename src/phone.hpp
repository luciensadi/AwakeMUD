#ifndef __phone_h__
#define __phone_h__
#include <unordered_map>

using namespace std;

unordered_map<string, phone_data*> phone_db;

bool             resolve_phone_number(const char* phone_number, struct phone_data *&phone);
bool             is_phone_number_valid_format(const char* input);
bool             phone_exists(const char* phone);
phone_data*      create_phone_number(struct obj_data *for_obj, int load_origin);
void             extract_phone_number(const char* phone_number);
void             extract_phone_number(struct obj_data *obj);
void             extract_phone_number(struct matrix_icon *icon);
void             send_to_other_phone(struct phone_data *sender, const char* message);
void             send_to_phone(struct phone_data *receiver, const char* message);
bool             try_get_phone_character(struct phone_data *receiver, struct char_data *&to_character);

#endif
