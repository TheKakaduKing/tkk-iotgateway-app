#include <fstream>
#include "tkk-gw/adapters/s7adapter.hpp"
#include "snap7micro/s7_types.h"

S7Adapter::S7Adapter() : client{std::make_unique<TSnap7MicroClient>()},
                         status{false},
                         snap7Config{configureAdapter()}
{
}

S7Adapter::~S7Adapter()
{
    client->Disconnect();
}

/**
 * @brief *pens the adapter config file
 *
 * @param path_
 */
void S7Adapter::openConfigFile(const std::string &path_)
{
    std::ifstream file(path_); // std::ifstream is RAII -> closes itself when leaving scope
    if (!file.is_open())
    {
        return;
    }
    parseConfigFile(file);
}

/**
 * @brief Parses config file into nlohmann::json object
 *
 * @param file_
 */
void S7Adapter::parseConfigFile(std::ifstream &file_)
{
    configDataJson = nlohmann::json::parse(file_, NULL, true); // Exceptions alloewd for now for testing
}

/**
 * @brief Config for snap7
 *
 */
S7Adapter::ReadConfig S7Adapter::configureAdapter()
{
    S7Adapter::ReadConfig configVector;
    ReadType readType;

    const auto &con = configDataJson.at("connection");
    if (!con.contains("ip") || !con["ip"].is_string())
    {
        return;
    }
    connectionConfig.ip = con.at("ip").get<std::string>();
    connectionConfig.rack = con.value("rack", 0);
    connectionConfig.slot = con.value("slot", 2);

    if (configDataJson.contains("area"))
    {
        const auto &area = configDataJson.at("area");
        readType.readArea = true;
        readType.dbNumber = area.value("dbno", 0);
        readType.offset = area.value("offset", 0);
    }

    if (!configDataJson.contains("tags"))
    {
        return;
    }

    const auto &tags = configDataJson.at("tags");

    configVector[0].first = readType;

    size_t v{0}, maxItemCount{client->PDULength / 12}, currentItemCount{0};
    int currentPduSize{0}, currentItemSize{0};
    TagItem tempTagItem{};
    for (const auto &tag : tags)
    {
        tempTagItem = createTagItem(configVector[v].first.readArea, tag);
        if (!configVector[v].first.readArea)
        {
            currentItemSize = 4 + getTypeSize(tempTagItem.type);
            if (currentItemSize % 2 != 0)
            {
                currentItemSize++; // Must be 2 byte aligned
            }

            if (currentPduSize + currentItemSize > client->PDULength || currentItemCount >= maxItemCount)
            {
                v++; // Setup new ReadMultiVar request
                configVector[v].first = readType;
                currentPduSize = 0 + currentItemSize;
            }
            currentItemCount++;
        }
        configVector[v].second.push_back(tempTagItem);
    }
    return configVector;
}

/**
 * @brief Return S7 type size in byte
 *
 * @param type_
 * @return int
 */
int S7Adapter::getTypeSize(S7Type type_)
{
    switch (type_)
    {
    case S7Type::BOOL:
    case S7Type::BYTE:
    case S7Type::SINT:
    case S7Type::USINT:
    case S7Type::CHAR:
    {
        return 1;
        break;
    }
    case S7Type::WORD:
    case S7Type::INT:
    case S7Type::UINT:
    case S7Type::WCHAR:
    case S7Type::S5TIME:
    {
        return 2;
        break;
    }
    case S7Type::DWORD:
    case S7Type::DINT:
    case S7Type::UDINT:
    case S7Type::REAL:
    case S7Type::TIME:
    case S7Type::TIMER:
    case S7Type::COUNTER:
    {
        return 4;
        break;
    }
    case S7Type::LWORD:
    case S7Type::LINT:
    case S7Type::ULINT:
    case S7Type::LREAL:
    case S7Type::LTIME:
    {
        return 8;
        break;
    }
    default:
        return 99;
        break;
    }
}

TagItem S7Adapter::createTagItem(bool readArea_, const nlohmann::json_abi_v3_12_0::json &tag_)
{
    TagItem item{};
    item.id = tag_.value("id", 0);
    item.name = tag_.value("id", "unknown");

    if (!readArea_)
    {
        item.target = parseTarget(tag_.value("target", "unknownTarget"));
    }
    if (tag_.contains("dbno"))
    {
        item.dbNumber = tag_.value("dbno", 0);
    }
    if (tag_.contains("offset"))
    {
        item.offset = tag_.value("offset", 0);
    }
    item.type = parseS7Type(tag_.value("type", "unkownType"));
    if (tag_.contains("bit") && item.type == S7Type::BOOL)
    {
        item.bit = tag_.value("bit", 0);
    }
    return item;
}

Target S7Adapter::parseTarget(const std::string &target_)
{
    if (target_ == "db")
    {
        return Target::DB;
    }
    if (target_ == "e")
    {
        return Target::INPUT;
    }
    if (target_ == "a")
    {
        return Target::OUTPUT;
    }
    if (target_ == "m")
    {
        return Target::MERKER;
    }
    if (target_ == "t")
    {
        return Target::TIMER;
    }
    if (target_ == "c")
    {
        return Target::COUNTER;
    }
    if (target_ == "arr")
    {
        return Target::ARRAY;
    }
    return Target::INVALID;
}

S7Type S7Adapter::parseS7Type(const std::string &type_)
{
    if (type_ == "BOOL")
    {
        return S7Type::BOOL;
    }
    if (type_ == "BYTE")
    {
        return S7Type::BYTE;
    }
    if (type_ == "WORD")
    {
        return S7Type::WORD;
    }
    if (type_ == "DWORD")
    {
        return S7Type::DWORD;
    }
    if (type_ == "LWORD")
    {
        return S7Type::LWORD;
    }
    if (type_ == "SINT")
    {
        return S7Type::SINT;
    }
    if (type_ == "INT")
    {
        return S7Type::INT;
    }
    if (type_ == "DINT")
    {
        return S7Type::DINT;
    }
    if (type_ == "USINT")
    {
        return S7Type::USINT;
    }
    if (type_ == "UINT")
    {
        return S7Type::UINT;
    }
    if (type_ == "UDINT")
    {
        return S7Type::UDINT;
    }
    if (type_ == "LINT")
    {
        return S7Type::LINT;
    }
    if (type_ == "ULINT")
    {
        return S7Type::ULINT;
    }
    if (type_ == "REAL")
    {
        return S7Type::REAL;
    }
    if (type_ == "LREAL")
    {
        return S7Type::LREAL;
    }
    if (type_ == "CHAR")
    {
        return S7Type::CHAR;
    }
    if (type_ == "WCHAR")
    {
        return S7Type::WCHAR;
    }
    if (type_ == "STRING")
    {
        return S7Type::STRING;
    }
    if (type_ == "S5TIME")
    {
        return S7Type::S5TIME;
    }
    if (type_ == "TIME")
    {
        return S7Type::TIME;
    }
    if (type_ == "LITME")
    {
        return S7Type::LTIME;
    }
    if (type_ == "TIMER")
    {
        return S7Type::TIMER;
    }
    if (type_ == "COUNTER")
    {
        return S7Type::COUNTER;
    }
    return S7Type::INVALID;
}

std::vector<DataPoint> S7Adapter::readData() const
{
    for (const auto &[r, i] : snap7Config)
    {
        if (r.readArea == true)
        {
            return;
        }
        if (r.readArea == false)
        {
        }
    }
}

void S7Adapter::splitMultiVarReq()
{
}

void S7Adapter::connect() const
{
    int result;
    result = client->ConnectTo(connectionConfig.ip.c_str(), connectionConfig.rack, connectionConfig.slot);
}

void S7Adapter::disconnect() const
{
    int result;
    result = client->Disconnect();
}

bool S7Adapter::getConnectedState() const
{
    return client->Connected;
}
