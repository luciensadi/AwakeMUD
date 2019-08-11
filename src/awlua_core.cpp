#include "awlua_core.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <sstream>
#include <chrono>


static lua_State *awLS = nullptr;

void awlua::Init()
{
    lua_State *LS = luaL_newstate();
    AWLUA_ASSERT(LS);
    luaL_openlibs(LS);    
    awLS = LS;

    // TODO: check return val
    luaL_dostring(LS, 
        "local traceback = debug.traceback \n"
        "debug.traceback = function(message, level) \n"
          "level = level or 1 \n"
          "local rtn = traceback(message, level + 1) \n"
          "if not rtn then return rtn end \n"
          "rtn = rtn:gsub(\"\\t\", \"    \") \n"
          "rtn = rtn:gsub(\"\\n\", \"\\n\\r\") \n"
          "return rtn \n"
        "end \n");

    luaL_loadstring(LS, "package.path = '../src/lua/?.lua'");
    lua_call(LS, 0, 0);

    lua_getglobal(LS, "require");
    lua_pushliteral(LS, "awlua");
    lua_call(LS, 1, 1);

    // TODO: save awlua ref somewhere
    lua_pop(LS, 1);


    AWLUA_ASSERT(0 == lua_gettop(LS));
}

lua_State *awlua::GetLS()
{
    return awLS;
}

#ifdef UNITTEST
void awlua::SetLS(lua_State *LS)
{
    awLS = LS;
}
#endif

std::string awlua::StackDump(lua_State *LS)
{
    std::ostringstream oss;

    int top = lua_gettop(LS);
    for (int i = 1; i <= top; ++i)
    {  
        int t = lua_type(LS, i);
        switch (t)
        {
            case LUA_TSTRING:  /* strings */
                oss << '`' << lua_tostring(LS, i) << '\'';
                break;

            case LUA_TBOOLEAN:  /* booleans */
                oss << (lua_toboolean(LS, i) ? "true" : "false");
                break;

            case LUA_TNUMBER:  /* numbers */
                oss << lua_tonumber(LS, i);
                break;

            default:  /* other values */
                oss << lua_typename(LS, t);
                break;

        }
        oss << "\r\n";
    }

    return oss.str();
}

awlua::LuaRef::LuaRef()
    : val_(LUA_NOREF)
{ }

awlua::LuaRef::~LuaRef()
{
    Release();
}

void awlua::LuaRef::Save(int index)
{
    Release();
    lua_pushvalue(GetLS(), index);
    val_ = luaL_ref(GetLS(), LUA_REGISTRYINDEX);
}

void awlua::LuaRef::Release()
{
    luaL_unref(GetLS(), LUA_REGISTRYINDEX, val_); 
    val_ = LUA_NOREF;
}

void awlua::LuaRef::Push()
{
    lua_rawgeti(GetLS(), LUA_REGISTRYINDEX, val_);
}

bool awlua::LuaRef::IsSet()
{
    return (val_ != LUA_NOREF);
}


static bool is_script_running = false;
static std::chrono::system_clock::time_point script_start_time;
static const int kScriptMaxMs = 25;
#define LUA_LOOP_CHECK_INCREMENT 1000

static void infinite_loop_check_hook(lua_State *LS, lua_Debug *ar)
{
    auto now = std::chrono::high_resolution_clock::now();
    int duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - script_start_time).count();

    if (duration > kScriptMaxMs)
    {
        luaL_error(LS, "Script timeout after %d ms.", duration);
    }
}

int awlua::CallWithTraceback(lua_State *LS, int nargs, int nresults)
{
    int ind_base = lua_gettop(LS) - nargs; // ind_base is the index of the chunk right now
    
    lua_getglobal(LS, "debug");
    lua_getfield(LS, -1, "traceback");
    lua_remove(LS, -2); // remove debug table
    lua_insert(LS, ind_base); // traceback func below chunk which is shifted up

    int rtn = lua_pcall(LS, nargs, nresults, ind_base);
    lua_remove(LS, ind_base); // remove traceback func

    return rtn;
}

int awlua::ScriptCall(lua_State *LS, int nargs, int nresults)
{
    if (is_script_running)
    {
        // If script running already, just call and let errors propagate up
        lua_call(LS, nargs, nresults);
        return LUA_OK;
    }
    
    lua_sethook(LS, infinite_loop_check_hook, LUA_MASKCOUNT, LUA_LOOP_CHECK_INCREMENT);
    is_script_running = true;
    script_start_time = std::chrono::high_resolution_clock::now();

    int rtn = CallWithTraceback(LS, nargs, nresults);

    lua_sethook(LS, 0, 0, 0);
    is_script_running = false;

    return rtn;
}
