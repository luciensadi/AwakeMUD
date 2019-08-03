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

static int L_luai_print(lua_State *LS)
{
    std::ostringstream oss;
    int n = lua_gettop(LS);
    lua_getglobal(LS, "tostring");
    for (int i = 1; i <= n; ++i)
    {
        const char *s;
        size_t l;
        lua_pushvalue(LS, -1); // function to be called
        lua_pushvalue(LS, i);
        lua_call(LS, 1, 1);
        s = lua_tolstring(LS, -1, &l); // get result
        if (s == NULL)
            return luaL_error(LS, "'tostring' must return a string to 'luaiprint'");
        if (i > 1) oss << ", ";
        oss << s;
        lua_pop(LS, 1);
    }
    oss << "\r\n";
    std::string str = oss.str();
    lua_pushlstring(LS, str.c_str(), str.size());
    return 1;
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
    bool first_line = false;

    lua_getfield(LS, -1, "input");
    if (lua_isnil(LS, -1))
    {
        first_line = true;
        lua_pop(LS, 1);
        lua_pushstring(LS, comm);
    }
    else
    {
        lua_pushliteral(LS, "\n");
        lua_pushstring(LS, comm);
        lua_concat(LS, 3);
    }
    
    int ind_all_input = lua_gettop(LS);

    // Try to load it
    size_t len;
    const char *all_input = lua_tolstring(LS, ind_all_input, &len);
    int status = -1;

    if (first_line)
    {
        const char *ret_line = lua_pushfstring(LS, "return %s;", all_input);
        status = luaL_loadbuffer(LS, ret_line, strlen(ret_line), "=luai");
        if (status == LUA_OK)
        {
            lua_remove(LS, -2); // remove modified line
        }
        else
        {
            lua_pop(LS, 2); // pop result from 'luaL_loadbuffer' and modified line
        }
    }
    if (status != LUA_OK)
    {
        status = luaL_loadbuffer(LS, all_input, len, "=luai");
    }
    if (status == LUA_OK)
    {
        const int ref_top = lua_gettop(LS) - 1;
        status = lua_pcall(LS, 0, LUA_MULTRET, 0);
        if (status == LUA_OK)
        {
            const int nrtn = lua_gettop(LS) - ref_top;
            lua_pushcfunction(LS, L_luai_print);
            lua_insert(LS, -1 - nrtn);
            int rc = lua_pcall(LS, nrtn, 1, 0);
            if (rc != LUA_OK)
            {
                SEND_TO_Q("Error in luai_print: ", d);
                SEND_TO_Q(lua_tostring(LS, -1), d);
                lua_pop(LS, 1);
            }
            else
            {
                SEND_TO_Q(lua_tostring(LS, -1), d);
                lua_pop(LS, 1);
            }

            lua_pushnil(LS);
            lua_setfield(LS, ind_luai, "input");
            lua_settop(LS, top);
            return;
        }
        else
        {
            // print error
            SEND_TO_Q(luaL_checkstring(LS, -1), d);
            lua_pop(LS, 1); // pop error
            lua_pushnil(LS);
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
        lua_pushnil(LS);
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