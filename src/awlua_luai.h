#ifndef awlua_luai_h
#define awlua_luai_h

struct descriptor_data;

namespace awlua
{

void luai_handle(descriptor_data *d, const char *comm);
const char *luai_get_prompt(descriptor_data *d);

} // namespace awlua

#endif // awlua_luai_h