#ifndef _house_h_
#define _house_h_

#define HOUSE_PRIVATE 0
#define MAX_GUESTS 10

#include "types.hpp"
#include "bitfield.hpp"

#define NOELF	1
#define NOORK	2
#define NOTROLL	3
#define NOHUMAN	4
#define NODWARF	5

#define TOROOM(room_rnum, dir) ((world[(room_rnum)].dir_option[(dir)] && world[(room_rnum)].dir_option[(dir)]->to_room) ? \
                            real_room(world[(room_rnum)].dir_option[(dir)]->to_room->number) : NOWHERE)

bool House_can_enter(struct char_data *ch, vnum_t house);
void House_listrent(struct char_data *ch, vnum_t vnum);
void House_boot(void);
void House_save_all(void);
void House_crashsave(vnum_t vnum);
void House_list_guests(struct char_data *ch, struct house_control_rec *i, int quiet);

extern struct house_control_rec *find_house(vnum_t vnum);

#endif
