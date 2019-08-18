#ifndef awlua_char_h_
#define awlua_char_h_

#include "awlua_ud.h"

struct char_data;

namespace awlua
{
typedef UdType<char_data *> UdChar;
} // namespace awlua

template<> const std::string awlua::UdChar::mt_name;
template<> const awlua::ud_field awlua::UdChar::get_fields[];
template<> const awlua::ud_field awlua::UdChar::set_fields[];
template<> const awlua::ud_field awlua::UdChar::meth_fields[];

#endif // awlua_char_h_