local M = {}


local function make_lib_proxy(t)
    local mt = {
        __index = t,
        __newindex = function(t, k, v)
            error("Cannot alter library functions.")
        end,
        __metatable = 0 -- any value here protects it
    }
    local proxy={}
    setmetatable(proxy, mt)
    return proxy
end

local function protect_lib(t)
    for k, v in pairs(t) do
        if type(v) == "table" then
            protect_lib(v)
            t[k] = make_lib_proxy(v)
        end
    end
    return make_lib_proxy(t)
end

local main_lib = protect_lib({
    assert=assert,
    -- collectgarbage=collectgarbage,
    -- dofile=dofile,
    error=error,
    -- _G=_G,
    -- getmetatable=getmetatable,
    ipairs=ipairs,
    -- load=load,
    -- loadfile=loadfile,
    next=next,
    pairs=pairs,
    -- pcall=pcall,
    -- print=print,
    -- rawequal=rawequal,
    -- rawget=rawget,
    -- rawlen=rawlen,
    -- rawset=rawset,
    select=select,
    -- setmetatable=setmetatable,
    tonumber=tonumber,
    tostring=tostring,
    type=type,
    -- _VERSION=_VERSION,
    -- xpcall=xpcall,
    -- coroutine={
    --     create=coroutine.create,
    --     isyieldable=coroutine.isyieldable,
    --     resume=coroutine.resume,
    --     running=coroutine.running,
    --     status=coroutine.status,
    --     wrap=coroutine.wrap,
    --     yield=coroutine.yield
    -- },
    -- require=require,
    -- package={
    --     config=package.config,
    --     cpath=package.cpath,
    --     loaded=package.loaded,
    --     loadlib=package.loadlib,
    --     path=package.path,
    --     preload=package.preload,
    --     searchers=package.searchers,
    --     searchpath=package.searchpath
    -- },
    -- string={
    --     byte=string.byte,
    --     char=string.char,
    --     dump=string.dump,
    --     find=string.find,
    --     format=string.format,
    --     gmatch=string.gmatch,
    --     gsub=string.gsub,
    --     len=string.len,
    --     lower=string.lower,
    --     match=string.match,
    --     pack=string.pack,
    --     packsize=string.packsize,
    --     rep=string.rep,
    --     reverse=string.reverse,
    --     sub=string.sub,
    --     unpack=string.unpack,
    --     upper=string.upper,
    -- },
    -- utf8={
    --     char=utf8.char,
    --     charpattern=utf8.charpattern,
    --     codes=utf8.codes,
    --     codepoints=utf8.codepoint,
    --     len=utf8.len,
    --     offset=utf8.offset,
    -- },
    table={
        concat=table.concat,
        insert=table.insert,
        move=table.move,
        pack=table.pack,
        remove=table.remove,
        sort=table.sort,
        unpack=table.unpack,
    },
    math={
        abs=math.abs,
        acos=math.acos,
        asin=math.asin,
        atan=math.atan,
        ceil=math.ceil,
        cos=math.cos,
        deg=math.deg,
        exp=math.exp,
        floor=math.floor,
        fmod=math.fmod,
        huge=math.huge,
        log=math.log,
        max=math.max,
        maxinteger=math.maxinteger,
        min=math.min,
        mininteger=math.mininteger,
        modf=math.modf,
        pi=math.pi,
        rad=math.rad,
        random=math.random,
        randomseed=math.randomseed,
        sin=math.sin,
        sqrt=math.sqrt,
        tan=math.tan,
        tointeger=math.tointeger,
        type=math.type,
        ult=math.ult,
    },
    -- io={
    --     close=io.close,
    --     flush=io.flush,
    --     input=io.input,
    --     lines=io.lines,
    --     open=io.open,
    --     output=io.output,
    --     popen=io.popen,
    --     read=io.read,
    --     tmpfile=io.tmpfile,
    --     type=io.type,
    --     write=io.write,
    -- },
    -- os={
    --     clock=os.clock,
    --     date=os.date,
    --     difftime=os.difftime,
    --     execute=os.execute,
    --     exit=os.exit,
    --     getenv=os.getenv,
    --     remove=os.remove,
    --     rename=os.rename,
    --     selocale=os.setlocale,
    --     time=os.time,
    --     tmpname=os.tmpname,
    -- },
    -- debug={
    --     debug=debug.debug,
    --     gethook=debug.gethook,
    --     getinfo=debug.getinfo,
    --     getlocal=debug.getlocal,
    --     getmetatable=debug.getmetatable,
    --     getregistry=debug.getregistry,
    --     getupvalue=debug.getupvalue,
    --     getuservalue=debug.getuservalue,
    --     sethook=debug.sethook,
    --     setlocal=debug.setlocal,
    --     setmetatable=debug.setmetatable,
    --     setupvalue=debug.setupvalue,
    --     setuservalue=debug.setuservalue,
    --     traceback=debug.traceback,
    --     upvalueid=debug.upvalueid,
    --     upvaluejoin=debug.upvaluejoin
    -- }
})


function M.new_script_env(self_obj)
    local env = {}
    local mt = {
        __index = function(t, k)
            if k == "self" then
                return self_obj
            else
                return main_lib[k]
            end
        end
    }

    setmetatable(env, mt)
    return env
end

function M.luai_result_tostring(...)
    local n = select("#", ...)
    if n < 1 then return nil end

    vals = {}

    for i = 1, n do
        vals[i] = tostring(select(i, ...))
    end

    return table.concat(vals, ", ").."\r\n"
end

function M.luai_handle(luai, env, comm)
    local first_line
    local all_input
    
    if not(luai.input) then
        first_line = true
        all_input = comm
    else
        all_input = luai.input.."\n"..comm
        luai.input = nil
    end

    local f, err

    if first_line then
        local ret_line = "return "..all_input..";"
        f, err = load(ret_line, "=(luai)", "t", env)
    end

    if not(f) then
        f, err = load(all_input, "=(luai)", "t", env)
    end

    if f then
        -- Runnable, let's run it
        luai.input = nil
        return f
    end

    if err:len() >= 5 and err:sub(-5) == "<eof>" then
        -- incomplete
        luai.input = all_input
        return nil
    else
        return err
    end
end


return M