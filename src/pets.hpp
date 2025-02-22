#ifndef __PETS_H
#define __PETS_H

#include <vector>

#define CUSTOM_PET_SYSPOINT_COST 50

#define GET_PET_OWNER_IDNUM(pet)    (GET_OBJ_VAL((pet), 0))
#define GET_PET_ECHO_SET_IDNUM(pet) (GET_OBJ_VAL((pet), 1))

#define PET_EDIT_MAIN            0
#define PET_EDIT_NAME            1
#define PET_EDIT_DESC            2
#define PET_EDIT_ROOMDESC        3
#define PET_EDIT_FLAVOR_MESSAGES 4

class PetEchoSet {
  idnum_t author_idnum;
  bool public_use_ok;
  const char *name;
  const char *arrive;
  const char *leave;
  std::vector<const char *> environmental_messages = {};
public:
  PetEchoSet(
    idnum_t author_idnum,
    bool public_use_ok,
    const char *set_name,
    const char *set_arrive,
    const char *set_leave,
    std::vector<const char *> messages
  ) : 
    author_idnum(author_idnum), public_use_ok(public_use_ok), environmental_messages(messages)
  {
    name = str_dup(set_name);
    arrive = str_dup(set_arrive);
    leave = str_dup(set_leave);
  };

  const char *get_name() { return name; }
  void set_name(const char *replacement);

  const char *get_arrive() { return arrive; }
  void set_arrive(const char *replacement);

  const char *get_leave() { return leave; }
  void set_leave(const char *replacement);

  bool is_usable_by(idnum_t idnum) { return public_use_ok || idnum == author_idnum; }

  std::vector<const char *> get_environmental_messages() { return environmental_messages; }

  const char *get_random_echo_message() {
    std::vector<const char *>::iterator randIt = environmental_messages.begin();
    std::advance(randIt, std::rand() % environmental_messages.size());
    return *randIt;
  }
};

void create_pet(struct char_data *ch);
void create_pet_main_menu(struct descriptor_data *d);
void create_pet_parse(struct descriptor_data *d, const char *arg);
void pet_acts(struct obj_data *pet);

#endif // __PETS_H
