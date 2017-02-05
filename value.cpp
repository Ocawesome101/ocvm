#include "value.h"
#include "log.h"

#include <lua.hpp>

#include <sstream>
using std::string;

Value::Value(const std::string& v)
{
    _type = "string";
    _string = v;
}

Value::Value()
{
    _type = "nil";
}

Value::Value(bool b)
{
    _type = "boolean";
    _bool = b;
}

Value::Value(double d)
{
    _type = "number";
    _number = d;
}

string Value::toString() const
{
    return _string;
}

bool Value::toBool() const
{
    return _bool;
}

double Value::toNumber() const
{
    return _number;
}

string Value::type() const
{
    return _type;
}

string Value::serialize() const
{
    if (_type == "string")
    {
        return "\"" + _string + "\"";
    }
    else if (_type == "boolean")
    {
        return _bool ? "true" : "false";
    }
    else if (_type == "number")
    {
        std::stringstream ss;
        ss << _number;
        return ss.str();
    }
    else if (_type == "nil")
    {
        return "nil";
    }

    log << "failed to serialize Value[" << _type << "]\n";
    return "";
}

bool operator< (const Value& a, const Value& b)
{
    return a.serialize() < b.serialize();
}

Value::operator bool() const
{
    return _type != "nil" && (_type != "boolean" || _bool);
}

ValuePack Value::unpack() const
{
    return ValuePack();
}

Value Value::make(lua_State* lua, int index)
{
    int top = lua_gettop(lua);
    if (index <= top)
    {
        int type = lua_type(lua, index);
        string name = lua_typename(lua, type);
        switch (type)
        {
            case LUA_TSTRING:
                return Value(lua_tostring(lua, index));
            break;
            case LUA_TBOOLEAN:
                return Value(lua_toboolean(lua, index));
            break;
            case LUA_TNUMBER:
                return Value(lua_tonumber(lua, index));
            break;
        }
    }
    return Value();
}
