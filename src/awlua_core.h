#ifndef awlua_core_h_
#define awlua_core_h_

struct lua_State;

namespace awlua
{
lua_State *GetLS();
#if defined(UNITTEST)
void SetLS(lua_State *LS);
#endif

class LuaRef
{
public:
    LuaRef();
    ~LuaRef();

    void Save(int index);
    void Release();
    void Push();
    bool IsSet();

private:
    int val_;
};

} // namespace awlua
#endif // awlua_core_h_