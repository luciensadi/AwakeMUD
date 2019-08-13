#include "awlua_core.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <sstream>
#include <chrono>


static lua_State *awLS = nullptr;

awlua::LuaRef awlua::ref::debug::traceback;

awlua::LuaRef awlua::ref::awlua::new_script_env;
awlua::LuaRef awlua::ref::awlua::luai_handle;


static int L_ScriptCall(lua_State *LS);


void awlua::Init()
{
    lua_State *LS = luaL_newstate();
    AWLUA_ASSERT(LS);
    luaL_openlibs(LS);    
    awLS = LS;

    // For tracebacks, replace tabs with 4 spaces and newlines with \r\n
    AWLUA_ASSERT(LUA_OK == luaL_loadstring(LS, 
        "local traceback = debug.traceback \n"
        "debug.traceback = function(message, level) \n"
          "level = level or 1 \n"
          "local rtn = traceback(message, level + 1) \n"
          "if not rtn then return rtn end \n"
          "rtn = rtn:gsub(\"\\t\", \"    \") \n"
          "rtn = rtn:gsub(\"\\n\", \"\\r\\n\") \n"
          "return rtn \n"
        "end\n"
        "return traceback\n")
    );
    lua_call(LS, 0, 1);
    ref::debug::traceback.Save(-1);
    lua_pop(LS, 1);
    AWLUA_ASSERT(0 == lua_gettop(LS));

    AWLUA_ASSERT(LUA_OK == luaL_dostring(LS, "package.path = '../src/lua/?.lua'"));

    // load awlua.lua
    lua_getglobal(LS, "require");
    AWLUA_ASSERT(!lua_isnil(LS, -1));
    lua_pushliteral(LS, "awlua");
    lua_call(LS, 1, 1);
    
    lua_getfield(LS, -1, "new_script_env");
    AWLUA_ASSERT(!lua_isnil(LS, -1));
    ref::awlua::new_script_env.Save(-1);
    lua_pop(LS, 1);

    lua_getfield(LS, -1, "luai_handle");
    AWLUA_ASSERT(!lua_isnil(LS, -1));
    ref::awlua::luai_handle.Save(-1);
    lua_pop(LS, 1);

    lua_getfield(LS, -1, "init");
    AWLUA_ASSERT(lua_isfunction(LS, -1));
    lua_pushcfunction(LS, L_ScriptCall);
    lua_call(LS, 1, 0);

    lua_pop(LS, 1); // pop awlua module

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
    ref::debug::traceback.Push();
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

static int L_ScriptCall(lua_State *LS)
{
    // func at 1, any additional args above that
    int nargs = lua_gettop(LS) - 1;
    int rc = awlua::ScriptCall(LS, nargs, LUA_MULTRET);

    if (LUA_OK == rc)
    {
        lua_pushnil(LS);
        lua_insert(LS, 1);
    }

    return lua_gettop(LS);
}
