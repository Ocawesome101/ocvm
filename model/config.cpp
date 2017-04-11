#include "config.h"
#include "log.h"

#include <lua.hpp>

#include <iostream>
#include <sstream>
#include "drivers/fs_drv.h"

extern "C"
{
    static int l_cpp_store(lua_State* lua)
    {
        const void* raw = lua_topointer(lua, 1);
        Value* pdata = const_cast<Value*>(static_cast<const Value*>(raw));
        string key = Value::checkArg<string>(lua, 2);
        Value value(lua, 3);
        pdata->set(key, value);

        lout << "config loading [" << key << "]: " << value.serialize() << "\n";
        return 0;
    }
}

Config::Config() : _data(Value::table())
{}

bool Config::load(const string& path, const string& name)
{
    // clear out previous values (if any)
    _data = Value::nil;
    _path = path;
    _name = name;

    // first check _path, else local for name
    string table;
    if (!utils::read(savePath(), &table) && !utils::read(name + ".cfg", &table))
    {
        lout << "config could not load: " << name << endl;
        return false;
    }

    lout << "config [" << _name << "]: table: " << table;
    lout << endl;

    if (table.empty())
    {
        return true; // ok, just nothing to load
    }

    string loader =
    "for k,v in pairs(" + table + ") do\n"
    "   cpp_store(_data, k, v)\n"
    "end";

    lua_State* lua = luaL_newstate();
    if (luaL_loadstring(lua, loader.c_str()) == 0)
    {
        Value tmpData = Value::table();
        luaL_openlibs(lua);
        lua_pushcfunction(lua, l_cpp_store);
        lua_setglobal(lua, "cpp_store");
        lua_pushlightuserdata(lua, &tmpData);
        lua_setglobal(lua, "_data");
        int result_status = lua_pcall(lua, 0, LUA_MULTRET, 0);
        if (result_status != LUA_OK)
        {
            lout << "Failed to digest the configuration\n";
            lout << lua_tostring(lua, -1) << endl;
            return false;
        }
        _data = tmpData;
        _cache = _data.serialize(true);
    }
    else
    {
        lerr << "\nConfiguration could not load\n";
        lerr << lua_tostring(lua, -1) << endl;
        return false;
    }
    lua_close(lua);
    return true;
}

string Config::name() const
{
    return _name;
}

string Config::savePath() const
{
    return _path + "/" + _name + ".cfg";
}

const Value& Config::get(const string& key) const
{
    return _data.get(key);
}
Value& Config::get(const string& key)
{
    return _data.get(key);
}

bool Config::set(const string& key, const Value& value, bool bCreateOnly)
{
    if (!bCreateOnly || !_data.contains(key))
    {
        _data.set(key, value);
        return true;
    }
    return false;
}

bool Config::save() const
{
    if (_data)
    {
        string updated_config = _data.serialize(true);
        if (updated_config != _cache)
        {
            lout << "saving " << _name << ": config\n";
            return utils::write(updated_config, savePath());
        }
    }
    return true;
}

vector<string> Config::keys() const
{
    return _data.keys();
}