#include "awlua_luai.h"
#include "awlua_core.h"

#include "structs.h"
#include "interpreter.h"
#include "comm.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>


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
    lua_pushliteral(LS, "");
    lua_setfield(LS, -2, "input");
    ch->desc->ref_luai.Save(-1);

    send_to_char(ch, "Entered lua interpreter mode.\r\n"
                     "Use @ on a blank line to exit.\r\n"
                     "Use do and end to create multiline chunks.\r\n");

    lua_settop(LS, top);
}

/* mark in error messages for incomplete statements */
#define EOFMARK     "<eof>"
#define marklen     (sizeof(EOFMARK)/sizeof(char) - 1)

/*
** Check whether 'status' signals a syntax error and the error
** message at the top of the stack ends with the above mark for
** incomplete statements.
*/
static int incomplete (lua_State *L, int status) {
    if (status == LUA_ERRSYNTAX) {
        size_t lmsg;
        const char *msg = lua_tolstring(L, -1, &lmsg);
        if (lmsg >= marklen && strcmp(msg + lmsg - marklen, EOFMARK) == 0) {
            lua_pop(L, 1);
            return 1;
        }
    }
    return 0;  /* else... */
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

    // Add input to any previous lines
    d->ref_luai.Push();
    int ind_luai = lua_gettop(LS);

    lua_getfield(LS, -1, "input");
    lua_pushliteral(LS, "\n");
    lua_pushstring(LS, comm);
    lua_concat(LS, 3);

    int ind_all_input = lua_gettop(LS);

    // Try to load it
    size_t len;
    const char *all_input = lua_tolstring(LS, ind_all_input, &len);
    int status = luaL_loadbuffer(LS, all_input, len, "=luai");
    if (status == LUA_OK)
    {
        status = lua_pcall(LS, 0, LUA_MULTRET, 0);
        if (status == LUA_OK)
        {
            // TODO: print anything on stack
            lua_pushliteral(LS, "");
            lua_setfield(LS, ind_luai, "input");
            lua_settop(LS, top);
            return;
        }
        else
        {
            // print error
            SEND_TO_Q(luaL_checkstring(LS, -1), d);
            lua_pop(LS, 1); // pop error
            lua_pushliteral(LS, "");
            lua_setfield(LS, ind_luai, "input");
            lua_settop(LS, top);
            return;
        }
    }
    else if (incomplete(LS, status))
    {
        lua_pushvalue(LS, ind_all_input);
        lua_setfield(LS, ind_luai, "input"); 
        lua_settop(LS, top);
        return;
    }
    else
    {
        /* cannot or should not try to add continuation line */
        SEND_TO_Q(luaL_checkstring(LS, -1), d);
        lua_pop(LS, 1); // pop error
        lua_pushliteral(LS, "");
        lua_setfield(LS, ind_luai, "input");
        lua_settop(LS, top);
        return;
    }
}

const char *awlua::luai_get_prompt(descriptor_data *d)
{
    AWLUA_ASSERT(d->ref_luai.IsSet());

    lua_State *LS = GetLS();

    int top = lua_gettop(LS);

    const char *rtn;
    d->ref_luai.Push();
    lua_getfield(LS, -1, "input");

    if (!strcmp(luaL_checkstring(LS, -1), ""))
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