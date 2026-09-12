#include <iostream>
#include <fstream>
#include "tkk-gw/adapters/s7adapter.hpp"
#include "snap7micro/s7_types.h"

S7Adapter::S7Adapter(const std::string &configPath_) : client{std::make_unique<TSnap7MicroClient>()},
                                                       status{false},
                                                       configPath{configPath_}
{
    snap7Config = configureAdapter();
    connect();
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
std::ifstream S7Adapter::openConfigFile()
{
    std::ifstream file(configPath); // std::ifstream is RAII -> closes itself when leaving scope
    if (!file.is_open())
    {
        std::cout << "Error opening config file" << std::endl;
        return;
    }
}

/**
 * @brief Parses config file into nlohmann::json object
 *
 * @param file_
 */
void S7Adapter::parseConfigFile()
{
    std::ifstream file{openConfigFile()};
    std::cout << "Parsing config file" << std::endl;
    configDataJson = nlohmann::json::parse(file, nullptr, true); // Exceptions alloewd for now for testing
}

/**
 * @brief Setup connection params
 *
 */
void S7Adapter::setupConnConfig()
{
    const auto &con = configDataJson.at("connection");
    if (!con.contains("ip") || !con["ip"].is_string())
    {
        return;
    }
    connectionConfig.ip = con.at("ip").get<std::string>();
    connectionConfig.rack = con.value("rack", 0);
    connectionConfig.slot = con.value("slot", 2);

    std::cout << "Connection config finished" << std::endl;
    std::cout << "IP: " << connectionConfig.ip << std::endl;
    std::cout << "Rack: " << connectionConfig.rack << std::endl;
    std::cout << "Slot: " << connectionConfig.slot << std::endl;
}

/**
 * @brief Create readconfig for snap7 area reads
 *
 * @param block_
 * @return S7Adapter::ReadConfigItem
 */
S7Adapter::ReadConfigItem S7Adapter::createAreaReadConfigItem(const nlohmann::json_abi_v3_12_0::json &block_)
{
    ReadConfigItem tempConfigItem{};
    ReadType tempReadType{};

    tempReadType.mode = ReadMode::AREA;
    tempReadType.target = parseAreaTarget(block_.at("target").get<std::string>());
    tempReadType.offset = block_.at("offset").get<u32>();
    tempReadType.amount = block_.at("amount").get<u32>();

    tempConfigItem.first = tempReadType;

    const auto &tags = block_.at("tags");

    for (const auto &tag : tags)
    {
        tempConfigItem.second.push_back(createTagItem(tempConfigItem.first.mode, tag));
    }

    return tempConfigItem;
}

/**
 * @brief Create readconfig for snap7 multivar reads
 *
 * @param block_
 * @return S7Adapter::ReadConfigItem
 */
std::vector<S7Adapter::ReadConfigItem> S7Adapter::createSingleReadConfigItem(const nlohmann::json_abi_v3_12_0::json &block_)
{
    std::vector<ReadConfigItem> readConfigItemMemory{};
    ReadConfigItem tempConfigItem{}, tempConfigItemRef{};
    TagItem tempTagItem{};
    ReadType tempReadType{};
    int currentItemSize{0}, currentPduSize{12};
    size_t currentItemCount{0}, maxItemCount{(client->PDULength - 12) / 12};

    const auto &tags = block_.at("tags");

    for (const auto &tag : tags)
    {
        tempTagItem = createTagItem(tempConfigItem.first.mode, tag);
        currentItemSize = 4 + getTypeSize(tempTagItem.type);
        if (currentItemSize % 2 != 0)
        {
            currentItemSize += 1; // Must be 2 byte aligned
        }

        currentPduSize += currentItemSize;

        if (currentPduSize > client->PDULength || currentItemCount >= maxItemCount)
        {
            readConfigItemMemory.push_back(tempConfigItem);
            tempConfigItem = tempConfigItemRef; // Empty tempConfigItem

            currentPduSize = 12 + currentItemSize;
            currentItemCount = 0;
        }
        currentItemCount++;
        tempConfigItem.second.push_back(tempTagItem);
    }
    return readConfigItemMemory;
}

/**
 * @brief Config for snap7
 *
 */
S7Adapter::ReadConfig S7Adapter::configureAdapter()
{
    parseConfigFile();
    setupConnConfig();

    S7Adapter::ReadConfig configVector{};
    ReadType readType;
    const auto &blocks = configDataJson.at("read");

    for (const auto &b : blocks)
    {
        if (b.at("mode").get<std::string>() == "area")
        {
            configVector.push_back(createAreaReadConfigItem(b));
            continue;
        }
        if (b.at("mode").get<std::string>() == "single")
        {
            const auto &readConfigItemMemory = createSingleReadConfigItem(b);
            for (const auto &config : readConfigItemMemory)
            {
                configVector.push_back(config);
            }
            continue;
        }
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

TagItem S7Adapter::createTagItem(ReadMode mode_, const nlohmann::json_abi_v3_12_0::json &tag_)
{
    TagItem item{};
    item.id = tag_.value("id", 0);
    item.name = tag_.value("name", "unknown");

    if (mode_ == ReadMode::SINGLE)
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

AreaTarget S7Adapter::parseAreaTarget(const std::string &areaTarget_)
{
    if (areaTarget_ == "db")
    {
        return AreaTarget::DB;
    }
    if (areaTarget_ == "e")
    {
        return AreaTarget::INPUT;
    }
    if (areaTarget_ == "a")
    {
        return AreaTarget::OUTPUT;
    }
    if (areaTarget_ == "m")
    {
        return AreaTarget::MERKER;
    }
    return AreaTarget::INVALID;
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
    std::vector<DataPoint> dataVector;
    for (const auto &[r, i] : snap7Config)
    {
        if (r.mode == ReadMode::AREA)
        {
            return dataVector;
        }
        if (r.mode == ReadMode::SINGLE)
        {
        }
    }
}

void S7Adapter::writeData() const
{
    return;
}

void S7Adapter::connect() const
{
    std::cout << std::endl;
    std::cout << "Connecting to PLC..." << std::endl;
    int result;
    result = client->ConnectTo(connectionConfig.ip.c_str(), connectionConfig.rack, connectionConfig.slot);
    if (client->Connected)
    {
        std::cout << "Connected to PLC on IP: " << connectionConfig.ip << std::endl;
        std::cout << "Negotiated PDU size   : " << client->PDULength << std::endl;
        return;
    }

    std::cout << "Connecting failed... result: " << result << std::endl;
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
