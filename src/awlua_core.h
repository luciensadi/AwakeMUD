#ifndef awlua_core_h_
#define awlua_core_h_

#include <string>


struct lua_State;

namespace awlua
{

void Init();

lua_State *GetLS();
#if defined(UNITTEST)
void SetLS(lua_State *LS);
#endif

std::string StackDump (lua_State *LS);

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

int CallWithTraceback(lua_State *LS, int nargs, int nresults);
int ScriptCall(lua_State *LS, int nargs, int nresults);

namespace ref
{
    namespace debug
    {
        extern LuaRef traceback;
    } // namespace debug

    namespace awlua
    {
        extern LuaRef new_script_env;
        extern LuaRef luai_result_tostring;
        extern LuaRef luai_handle;
    } // namespace awlua
} // namespace ref
} // namespace awlua


void handle_awlua_assert(const char *cond, const char *func, const char *file, int line);
#define AWLUA_ASSERT(cond) \
    do { \
        if ( !(cond) ) \
        { \
            handle_awlua_assert( #cond, __func__, __FILE__, __LINE__); \
        } \
    } while(0)
    

#endif // awlua_core_h_