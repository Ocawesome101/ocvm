#include "modem.h"
#include "drivers/modem_drv.h"
#include "model/client.h"
#include "model/log.h"

#include <sstream>
#include <iterator>
using std::stringstream;

Modem::Modem()
{
    add("setWakeMessage", &Modem::setWakeMessage);
    add("isWireless", &Modem::isWireless);
    add("close", &Modem::close);
    add("getWakeMessage", &Modem::getWakeMessage);
    add("maxPacketSize", &Modem::maxPacketSize);
    add("isOpen", &Modem::isOpen);
    add("broadcast", &Modem::broadcast);
    add("send", &Modem::send);
    add("open", &Modem::open);
}

Modem::~Modem()
{
    _modem->stop();
}

bool Modem::onInitialize()
{
    int system_port = config().get(ConfigIndex::SystemPort).Or(56000).toNumber();
    _modem.reset(new ModemDriver(this, system_port));
    if (!_modem->start())
    {
        lout() << "modem driver failed to start\n";
        return false;
    }

    _maxPacketSize = config().get(ConfigIndex::MaxPacketSize).Or(8192).toNumber();
    _maxArguments = config().get(ConfigIndex::MaxArguments).Or(8).toNumber();
    
    return true;
}

template<typename T>
void write(const T& num, vector<char>* pOut)
{
    const char* p = reinterpret_cast<const char*>(&num);
    for (size_t i = 0; i < sizeof(T); i++)
    {
        pOut->push_back(*p++);
    }
}

void write(const string& text, vector<char>* pOut)
{
    write<int32_t>(text.size(), pOut);
    std::copy(text.begin(), text.end(), std::back_inserter(*pOut));
}

void write(const char* data, int len, vector<char>* pOut)
{
    write<int32_t>(len, pOut);
    std::copy(data, data + len, std::back_inserter(*pOut));
}

int Modem::tryPack(lua_State* lua, const vector<char>* pAddr, int port, vector<char>* pOut) const
{
    if (port < 1 || port > 0xffff)
        return luaL_error(lua, "invalid port number");

    pOut->clear();

    int offset = 2; // first is at least the port
    int last_index = lua_gettop(lua);

    // {sender, has_target(1 or 0), target, port, num_args, arg_type_id_1, arg_value_1, ..., arg_type_id_n, arg_value_n)
    string addr = address();
    write(addr, pOut);
    
    if (pAddr)
    {
        offset++;
        write<bool>(true, pOut);
        write(pAddr->data(), pAddr->size(), pOut);
    }
    else
    {
        write<bool>(false, pOut);
    }

    write<int32_t>(port, pOut);

    int num_arguments = last_index - offset + 1;
    if (num_arguments > _maxArguments)
    {
        return luaL_error(lua, "packet has too many parts");
    }

    write<int32_t>(num_arguments, pOut);

    // the accumulating packet size does not include the serialization
    // the ACTUAL packet size may be larger than what OC packs
    size_t total_packet_size = 0;
    const char* pValue;
    int valueLen;

    // loop through the remaining values
    for (int index = offset; index <= last_index; ++index)
    {
        int type_id = lua_type(lua, index);
        write<int32_t>(type_id, pOut);
        switch (type_id)
        {
            case LUA_TNIL:
                // size: 6
                total_packet_size += 6;
            break;
            case LUA_TBOOLEAN:
                // size: 6
                total_packet_size += 6;
                write<bool>(lua_toboolean(lua, index), pOut);
            break;
            case LUA_TNUMBER:
                // size: 10
                total_packet_size += 10;
                write<LUA_NUMBER>(lua_tonumber(lua, index), pOut);
            break;
            case LUA_TSTRING:
                // size of string in bytes, +2
                pValue = lua_tostring(lua, index);
                valueLen = lua_rawlen(lua, index);
                total_packet_size += valueLen;
                total_packet_size += 2;
                write(pValue, valueLen, pOut);
            break;
            default: return luaL_error(lua, "unsupported data type");
        }

        if (total_packet_size > _maxPacketSize)
        {
            std::stringstream ss;
            ss << "packet too big (max " << _maxPacketSize << ")";
            return luaL_error(lua, ss.str().c_str());
        }
    }

    return 0;
}

int Modem::setWakeMessage(lua_State* lua)
{
    return ValuePack::ret(lua, Value::nil, "not supported");
}

int Modem::isWireless(lua_State* lua)
{
    return ValuePack::ret(lua, false);
}

int Modem::close(lua_State* lua)
{
    int port = Value::checkArg<int>(lua, 1);
    if (port < 1 || port > 0xffff)
        return luaL_error(lua, "invalid port number");
    bool changed = false;
    if (_ports.find(port) != _ports.end())
    {
        changed = true;
        _ports.erase(port);
    }
    return ValuePack::ret(lua, changed);
}

int Modem::getWakeMessage(lua_State* lua)
{
    return ValuePack::ret(lua, Value::nil, "not supported");
}

int Modem::maxPacketSize(lua_State* lua)
{
    return ValuePack::ret(lua, _maxPacketSize);
}

int Modem::isOpen(lua_State* lua)
{
    int port = Value::checkArg<int>(lua, 1);
    if (port < 1 || port > 0xffff)
        return luaL_error(lua, "invalid port number");
    return ValuePack::ret(lua, _ports.find(port) != _ports.end());
}

int Modem::broadcast(lua_State* lua)
{
    int port = Value::checkArg<int>(lua, 1);
    vector<char> payload;
    int ret = tryPack(lua, nullptr, port, &payload);
    if (ret)
        return ret;
    return ValuePack::ret(lua, _modem->send(payload));
}

int Modem::send(lua_State* lua)
{
    vector<char> address = Value::checkArg<vector<char>>(lua, 1);
    int port = Value::checkArg<int>(lua, 2);
    vector<char> payload;
    int ret = tryPack(lua, &address, port, &payload);
    if (ret)
        return ret;
    return ValuePack::ret(lua, _modem->send(payload));
}

int Modem::open(lua_State* lua)
{
    int port = Value::checkArg<int>(lua, 1);
    if (port < 1 || port > 0xffff)
        return luaL_error(lua, "invalid port number");
    bool changed = false;
    if (_ports.find(port) == _ports.end())
    {
        changed = true;
        _ports.insert(port);
    }
    return ValuePack::ret(lua, changed);
}

template<typename T>
T read(const char** pInput)
{
    const char*& input = *pInput;
    T result {};
    char* p = reinterpret_cast<char*>(&result);
    for (size_t i = 0; i < sizeof(T); i++)
    {
        *p++ = *input++;
    }
    return result;
}

vector<char> read_vector(const char** pInput)
{
    int size = read<int32_t>(pInput);
    vector<char> result;
    std::copy(*pInput, *pInput + size, std::back_inserter(result));
    *pInput += size;
    return result;
}

RunState Modem::update()
{
    ModemEvent me;
    while (EventSource<ModemEvent>::pop(me))
    {
        // broadcast packets have no target
        // {sender, has_target(1 or 0)[, target], port, num_args, arg_type_id_1, arg_value_1, ..., arg_type_id_n, arg_value_n)
        const char* input = me.payload.data();
        vector<char> send_address = read_vector(&input);
        bool has_target = read<bool>(&input);
        vector<char> recv_address;
        if (has_target)
        {
            recv_address = read_vector(&input);
        }
        int port = read<int32_t>(&input);

        if (!isApplicable(port, has_target ? &recv_address : nullptr))
        {
            continue;
        }

        int distance = 0; // always zero in simulation
        ValuePack pack {"modem_message", address(), send_address, port, distance};

        int num_args = read<int32_t>(&input);
        for (int n = 0; n < num_args; n++)
        {
            int type_id = read<int32_t>(&input);
            switch (type_id)
            {
                case LUA_TSTRING:
                    pack.push_back(read_vector(&input));
                break;
                case LUA_TBOOLEAN:
                    pack.push_back(read<bool>(&input));
                break;
                case LUA_TNUMBER:
                    pack.push_back(read<LUA_NUMBER>(&input));
                break;
                case LUA_TNIL:
                    pack.push_back(Value::nil);
                break;
            }
        }

        client()->pushSignal(pack);
    }

    return RunState::Continue;
}

bool Modem::isApplicable(int port, vector<char>* target)
{
    if (_ports.find(port) == _ports.end())
    {
        return false;
    }

    if (target)
    {
        string addr = address();
        if (addr.size() != target->size())
            return false;

        for (size_t i = 0; i < addr.size(); i++)
        {
            if (addr.at(i) != target->at(i))
                return false;
        }
    }

    return true;
}

