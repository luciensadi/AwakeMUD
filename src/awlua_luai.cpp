#include "awlua_luai.h"
#include "awlua_core.h"

#include "structs.h"
#include "interpreter.h"
#include "comm.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <sstream>


ACMD(do_luai)
{
    if (!ch->desc)
        return;

    if (ch->desc->ref_luai.IsSet())
    {
        send_to_char(ch, "Lua interpreter already active!\r\n");
        return;
    }

    lua_State *LS = awlua::GetLS();
    int top = lua_gettop(LS);

    lua_newtable(LS);
    ch->desc->ref_luai.Save(-1);

    send_to_char(ch, "Entered lua interpreter mode.\r\n"
                     "Use @ on a blank line to exit.\r\n"
                     "Use do and end to create multiline chunks.\r\n");

    lua_settop(LS, top);
}

static int tbcall(lua_State *LS, int nargs, int nresults)
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

void awlua::luai_handle(descriptor_data *d, const char *comm)
{
    if (!strcmp(comm, "@"))
    {
        SEND_TO_Q("Exited lua interpreter.\r\n", d);
        d->ref_luai.Release();
        return;
    }

    lua_State *LS = GetLS();
    int top = lua_gettop(LS);
    AWLUA_ASSERT(d->ref_luai.IsSet());

    lua_getglobal(LS, "require");
    lua_pushliteral(LS, "awlua");
    lua_call(LS, 1, 1);
    lua_getfield(LS, -1, "luai_handle");
    lua_remove(LS, -2); // remove awlua table

    d->ref_luai.Push();

    // TODO: clean up env creation
    if (d->character->ref_env.IsSet())
    {
        d->character->ref_env.Push();
    }
    else
    {
        lua_getglobal(LS, "require");
        lua_pushliteral(LS, "awlua");
        lua_call(LS, 1, 1);
        lua_getfield(LS, -1, "new_script_env");
        lua_remove(LS, -2);
        lua_newtable(LS); // TODO: push CH
        lua_call(LS, 1, 1);
        d->character->ref_env.Save(-1);
    }

    lua_pushstring(LS, comm);

    if (tbcall(LS, 3, 1))
    {
        SEND_TO_Q("Error in luai_handle: ", d);
        SEND_TO_Q(lua_tostring(LS, -1), d);
        lua_pop(LS, 1);
    }
    else if (lua_isstring(LS, -1))
    {
        SEND_TO_Q(lua_tostring(LS, -1), d);
    }

    lua_settop(LS, top);
    return;
}

const char *awlua::luai_get_prompt(descriptor_data *d)
{
    AWLUA_ASSERT(d->ref_luai.IsSet());

    lua_State *LS = GetLS();

    int top = lua_gettop(LS);

    const char *rtn;
    d->ref_luai.Push();
    lua_getfield(LS, -1, "input");

    if (lua_isnil(LS, -1))
    {
        rtn = "lua> ";
    }
    else
    {
        rtn = "lua>> ";
    }
    lua_pop(LS, 2);

    AWLUA_ASSERT(top == lua_gettop(LS));

    return rtn;
}