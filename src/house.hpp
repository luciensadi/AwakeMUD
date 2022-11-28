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

#endif
