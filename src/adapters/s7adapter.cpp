#include <iostream>
#include <fstream>
#include <arpa/inet.h>
#include "tkk-gw/adapters/s7adapter.hpp"
#include "snap7micro/s7_types.h"
#include "utfcpp/utf8.h"

S7Adapter::S7Adapter(const std::string &configPath_) : client{std::make_unique<TSnap7MicroClient>()},
                                                       status{false},
                                                       configPath{configPath_}
{
}

S7Adapter::~S7Adapter()
{
    client->Disconnect();
}

void S7Adapter::init()
{
    configureAdapter();
    connect();

    if (client->PDULength > 0)
    {
        snap7Config = createReadConfig();
        postConfigChecks();

        totalReadSize = 0;

        for (const auto &item : snap7Config)
        {
            for (const auto &subItem : item.second)
            {
                totalReadSize += getItemReadSize(subItem);
            }
            totalDpItems += item.second.size();
        }
        commonReadBuffer.reserve(totalReadSize);
        currentDatapPoints.reserve(totalDpItems);
        previousDatapPoints.reserve(totalDpItems);
        std::cout << "Total read size calculated (bytes):  " << totalReadSize << std::endl;
        // DBG_printConfigElements();
    }
    else
    {
        exit(1);
    }
    auto test = readData();
    DBG_printConfigElements();
    createDataPoints();
    std::cout << "Created DPs, size:  " << currentDatapPoints.size() << std::endl;
    DBG_printCurrentDPElements();
}

void S7Adapter::postConfigChecks()
{
    auto result = checkItemsFitPdu();
    if (!result)
    {
        std::cout << "Error occured during post config check:      " << ConfigErrorToString(result.error()) << std::endl;
        exit(1);
    }
}

std::expected<void, ConfigError> S7Adapter::checkItemsFitPdu()
{
    for (const auto &item : snap7Config)
    {
        if (item.first.mode == ReadMode::AREA)
        {
            continue;
        }
        // Only check item sizes against pdu size when using snap7 multivar read (Single)

        for (const auto &subItem : item.second)
        {
            if (18 + getItemReadSize(subItem) > client->PDULength) // 18 byte overhead for 1 item
            {
                return std::unexpected(ConfigError::ItemNotFitPdu);
            }
        }
    }
    return {};
}

/**
 * @brief *pens the adapter config file
 *
 * @param path_
 */
std::expected<void, ConfigError> S7Adapter::openAndParseConfigFile()
{
    std::ifstream file(configPath); // std::ifstream is RAII -> closes itself when leaving scope
    if (!file.is_open())
    {
        return std::unexpected(ConfigError::FileNotFound);
    }
    configDataJson = nlohmann::json::parse(file, nullptr, false);
    if (configDataJson.is_discarded())
    {
        return std::unexpected(ConfigError::ParseError);
    }
    return {};
}
/**
 * @brief Validate the Json config file scheme
 *
 * @return std::expected<void, ConfigError>
 */
std::expected<void, ConfigError> S7Adapter::validateJsonScheme()
{
    if (!configDataJson.contains("connection"))
    {
        return std::unexpected(ConfigError::MissingNode);
    }
    auto result = validateJsonConnection(configDataJson.at("connection"));
    if (!result)
    {
        return result;
    }

    if (configDataJson.contains("read"))
    {
        if (!configDataJson.at("read").is_array())
        {
            return std::unexpected(ConfigError::TypeMismatch);
        }
        result = validateJsonReadBlock(configDataJson.at("read"));
        if (!result)
        {
            return result;
        }
    }
    return {};
}

/**
 * @brief Validate the Json file connection node
 *
 * @param config_
 * @return std::expected<void, ConfigError>
 */
std::expected<void, ConfigError> S7Adapter::validateJsonConnection(const Json &config_)
{
    if (!config_.contains("ip"))
    {
        return std::unexpected(ConfigError::MissingKey);
    }
    if (!config_.at("ip").is_string())
    {
        return std::unexpected(ConfigError::TypeMismatch);
    }
    std::string ip = config_.at("ip");
    struct in_addr addr;
    auto res = inet_pton(AF_INET, ip.c_str(), &addr);
    if (res != 1)
    {
        return std::unexpected(ConfigError::IpMismatch);
    }
    if (!config_.contains("rack"))
    {
        return std::unexpected(ConfigError::MissingKey);
    }
    if (!config_.at("rack").is_number_unsigned())
    {
        return std::unexpected(ConfigError::TypeMismatch);
    }
    if (!config_.contains("slot"))
    {
        return std::unexpected(ConfigError::MissingKey);
    }
    if (!config_.at("slot").is_number_unsigned())
    {
        return std::unexpected(ConfigError::TypeMismatch);
    }
    return {};
}
/**
 * @brief Validate the Json file Read Block
 *
 * @param config_
 * @return std::expected<void, ConfigError>
 */
std::expected<void, ConfigError> S7Adapter::validateJsonReadBlock(const Json &config_)
{
    std::unordered_set<int> seenIDs;

    for (const auto &block : config_)
    {
        if (!block.contains("mode"))
        {
            return std::unexpected(ConfigError::MissingKey);
        }
        if (!block.at("mode").is_string())
        {
            return std::unexpected(ConfigError::TypeMismatch);
        }
        std::string mode = block.at("mode");
        if (mode != "single" && mode != "area")
        {
            return std::unexpected(ConfigError::OutOfBound);
        }
        if (mode == "area")
        {
            auto result = validateJsonAreaBlock(block, seenIDs);
            if (!result)
            {
                return result;
            }
        }
        if (mode == "single")
        {
            auto result = validateJsonSingleBlock(block, seenIDs);
            if (!result)
            {
                return result;
            }
        }
    }
    return {};
}
/**
 * @brief Validate the Json file Area Read block
 *
 * @param config_
 * @return std::expected<void, ConfigError>
 */
std::expected<void, ConfigError> S7Adapter::validateJsonAreaBlock(const Json &config_, uset_i &seenIDs)
{
    if (!config_.contains("target"))
    {
        return std::unexpected(ConfigError::MissingKey);
    }
    if (!config_.at("target").is_string())
    {
        return std::unexpected(ConfigError::TypeMismatch);
    }
    if (cvrtTargetToSnap7Area(config_.at("target")) == -1)
    {
        return std::unexpected(ConfigError::OutOfBound);
    }
    if (!config_.contains("number"))
    {
        return std::unexpected(ConfigError::MissingKey);
    }
    if (!config_.at("number").is_number_unsigned())
    {
        return std::unexpected(ConfigError::TypeMismatch);
    }
    if (!config_.contains("offset"))
    {
        return std::unexpected(ConfigError::MissingKey);
    }
    if (!config_.at("offset").is_number_unsigned())
    {
        return std::unexpected(ConfigError::TypeMismatch);
    }
    if (!config_.contains("amount"))
    {
        return std::unexpected(ConfigError::MissingKey);
    }
    if (!config_.at("amount").is_number_unsigned())
    {
        return std::unexpected(ConfigError::TypeMismatch);
    }
    if (!config_.contains("tags"))
    {
        return std::unexpected(ConfigError::MissingNode);
    }
    if (!config_.at("tags").is_array())
    {
        return std::unexpected(ConfigError::TypeMismatch);
    }
    const auto &tags = config_.at("tags");
    for (const auto &tag : tags)
    {
        auto result = validateJsonTagItemArea(tag, seenIDs);
        if (!result)
        {
            return result;
        }
    }
    return {};
}
/**
 * @brief Validate the Json file Single(Multivar) Read block
 *
 * @param config_
 * @return std::expected<void, ConfigError>
 */
std::expected<void, ConfigError> S7Adapter::validateJsonSingleBlock(const Json &config_, uset_i &seenIDs)
{
    if (!config_.contains("tags"))
    {
        return std::unexpected(ConfigError::MissingNode);
    }
    if (!config_.at("tags").is_array())
    {
        return std::unexpected(ConfigError::TypeMismatch);
    }
    const auto &tags = config_.at("tags");
    for (const auto &tag : tags)
    {
        auto result = validateJsonTagItemSingle(tag, seenIDs);
        if (!result)
        {
            return result;
        }
    }
    return {};
}
/**
 * @brief Validate a tag item for Area Read Block
 *
 * @param tag_
 * @return std::expected<void, ConfigError>
 */
std::expected<void, ConfigError> S7Adapter::validateJsonTagItemArea(const Json &tag_, uset_i &seenIDs)
{
    if (!tag_.contains("id"))
    {
        return std::unexpected(ConfigError::MissingKey);
    }
    if (!tag_.at("id").is_number_unsigned())
    {
        return std::unexpected(ConfigError::TypeMismatch);
    }
    u32 id = tag_.at("id");
    auto [it, inserted] = seenIDs.insert(id);
    if (inserted == false)
    {
        return std::unexpected(ConfigError::DuplicateID);
    }
    if (!tag_.contains("name"))
    {
        return std::unexpected(ConfigError::MissingKey);
    }
    if (!tag_.at("name").is_string())
    {
        return std::unexpected(ConfigError::TypeMismatch);
    }
    if (!tag_.contains("offset"))
    {
        return std::unexpected(ConfigError::MissingKey);
    }
    if (!tag_.at("offset").is_number_unsigned())
    {
        return std::unexpected(ConfigError::TypeMismatch);
    }
    if (!tag_.contains("type"))
    {
        return std::unexpected(ConfigError::MissingKey);
    }
    if (!tag_.at("type").is_string())
    {
        return std::unexpected(ConfigError::TypeMismatch);
    }
    std::string type = tag_.at("type");
    auto s7type = stringToS7Type(type);
    if (s7type == S7Type::INVALID)
    {
        return std::unexpected(ConfigError::OutOfBound);
    }
    if (s7type == S7Type::BOOL)
    {
        if (!tag_.contains("bit"))
        {
            return std::unexpected(ConfigError::MissingKey);
        }
        if (!tag_.at("bit").is_number_unsigned())
        {
            return std::unexpected(ConfigError::TypeMismatch);
        }
        u8 bit = tag_.at("bit");
        if (bit < 0 || bit > 7)
        {
            return std::unexpected(ConfigError::OutOfBound);
        }
    }
    if (s7type == S7Type::STRING || s7type == S7Type::WSTRING)
    {
        if (!tag_.contains("length"))
        {
            return std::unexpected(ConfigError::MissingKey);
        }
        if (!tag_.at("length").is_number_unsigned())
        {
            return std::unexpected(ConfigError::TypeMismatch);
        }
    }
    return {};
}
/**
 * @brief Validate a tag item for Single(Multivar) Read Block
 *
 * @param tag_
 * @return std::expected<void, ConfigError>
 */
std::expected<void, ConfigError> S7Adapter::validateJsonTagItemSingle(const Json &tag_, uset_i &seenIDs)
{
    if (!tag_.contains("id"))
    {
        return std::unexpected(ConfigError::MissingKey);
    }
    if (!tag_.at("id").is_number_unsigned())
    {
        return std::unexpected(ConfigError::TypeMismatch);
    }
    u32 id = tag_.at("id");
    auto [it, inserted] = seenIDs.insert(id);
    if (inserted == false)
    {
        return std::unexpected(ConfigError::DuplicateID);
    }
    if (!tag_.contains("name"))
    {
        return std::unexpected(ConfigError::MissingKey);
    }
    if (!tag_.at("name").is_string())
    {
        return std::unexpected(ConfigError::TypeMismatch);
    }
    if (!tag_.contains("target"))
    {
        return std::unexpected(ConfigError::MissingKey);
    }
    if (!tag_.at("target").is_string())
    {
        return std::unexpected(ConfigError::TypeMismatch);
    }
    auto target = cvrtTargetToSnap7Area(tag_.at("target"));
    if (target == -1)
    {
        return std::unexpected(ConfigError::OutOfBound);
    }
    if (target == S7AreaDB || target == S7AreaTM || target == S7AreaCT)
    {
        if (!tag_.contains("number"))
        {
            return std::unexpected(ConfigError::MissingKey);
        }
        if (!tag_.at("number").is_number_unsigned())
        {
            return std::unexpected(ConfigError::TypeMismatch);
        }
        if (target == S7AreaDB)
        {
            if (!tag_.contains("offset"))
            {
                return std::unexpected(ConfigError::MissingKey);
            }
            if (!tag_.at("offset").is_number_unsigned())
            {
                return std::unexpected(ConfigError::TypeMismatch);
            }
        }
    }
    else
    {
        if (!tag_.contains("offset"))
        {
            return std::unexpected(ConfigError::MissingKey);
        }
        if (!tag_.at("offset").is_number_unsigned())
        {
            return std::unexpected(ConfigError::TypeMismatch);
        }
    }
    if (!tag_.contains("type"))
    {
        return std::unexpected(ConfigError::MissingKey);
    }
    if (!tag_.at("type").is_string())
    {
        return std::unexpected(ConfigError::TypeMismatch);
    }
    std::string type = tag_.at("type");
    auto s7type = stringToS7Type(type);
    if (s7type == S7Type::INVALID)
    {
        return std::unexpected(ConfigError::OutOfBound);
    }
    if (s7type == S7Type::BOOL)
    {
        if (!tag_.contains("bit"))
        {
            return std::unexpected(ConfigError::MissingKey);
        }
        if (!tag_.at("bit").is_number_unsigned())
        {
            return std::unexpected(ConfigError::TypeMismatch);
        }
        u8 bit = tag_.at("bit");
        if (bit < 0 || bit > 7)
        {
            return std::unexpected(ConfigError::OutOfBound);
        }
    }
    if (s7type == S7Type::STRING || s7type == S7Type::WSTRING)
    {
        if (!tag_.contains("length"))
        {
            return std::unexpected(ConfigError::MissingKey);
        }
        if (!tag_.at("length").is_number_unsigned())
        {
            return std::unexpected(ConfigError::TypeMismatch);
        }
    }
    return {};
}

/**
 * @brief Setup connection params
 *
 */
void S7Adapter::setupConnConfig()
{
    const auto &con = configDataJson.at("connection");
    connectionConfig.ip = con.at("ip").get<std::string>();
    connectionConfig.rack = con.value("rack", 0);
    connectionConfig.slot = con.value("slot", 2);
}

/**
 * @brief Create readconfig for snap7 area reads
 *
 * @param block_
 * @return S7Adapter::ReadConfigItem
 */
S7Adapter::ReadConfigItem S7Adapter::createAreaReadConfigItem(const Json &block_)
{
    ReadConfigItem tempConfigItem{};
    ReadHeader tempReadHeader{};

    tempReadHeader.mode = ReadMode::AREA;
    tempReadHeader.target = cvrtTargetToSnap7Area(block_.at("target").get<std::string>());
    tempReadHeader.number = block_.at("number").get<u32>();
    tempReadHeader.offset = block_.at("offset").get<u32>();
    tempReadHeader.amount = block_.at("amount").get<u32>();

    tempConfigItem.first = tempReadHeader;

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
std::vector<S7Adapter::ReadConfigItem> S7Adapter::createSingleReadConfigItem(const Json &block_)
{
    std::vector<ReadConfigItem> readConfigItemMemory{};
    ReadConfigItem tempConfigItem{}, tempConfigItemRef{};
    TagItem tempTagItem{};
    ReadHeader tempReadHeader{};
    int currentItemSize{0}, currentPduSize{14}, maxItemCount{(client->PDULength - 12) / 12};
    size_t currentItemCount{0};

    const auto &tags = block_.at("tags");

    for (const auto &tag : tags)
    {
        tempTagItem = createTagItem(tempConfigItem.first.mode, tag);
        currentItemSize = 4 + getItemReadSize(tempTagItem);
        if (currentItemSize % 2 != 0)
        {
            currentItemSize += 1; // Must be 2 byte aligned
        }

        if (currentPduSize + currentItemSize > client->PDULength || currentItemCount >= maxItemCount)
        {
            tempConfigItem.first.requestPduSize = 12 + (currentItemCount * 12);
            tempConfigItem.first.responsePduSize = currentPduSize;
            tempConfigItem.first.itemCount = currentItemCount;
            readConfigItemMemory.push_back(tempConfigItem);
            tempConfigItem = tempConfigItemRef; // Empty tempConfigItem

            currentPduSize = 14;
            currentItemCount = 0;
        }
        currentPduSize += currentItemSize;
        currentItemCount++;
        tempConfigItem.second.push_back(tempTagItem);
    }

    if (tempConfigItem.second.size() > 0)
    {
        tempConfigItem.first.requestPduSize = 12 + (currentItemCount * 12);
        tempConfigItem.first.responsePduSize = currentPduSize;
        tempConfigItem.first.itemCount = currentItemCount;
        readConfigItemMemory.push_back(tempConfigItem);
    }

    return readConfigItemMemory;
}

/**
 * @brief Config for snap7
 *
 */
void S7Adapter::configureAdapter()
{
    auto resParse = openAndParseConfigFile();
    if (!resParse)
    {
        std::cout << "Error occured while parsing:      " << ConfigErrorToString(resParse.error()) << std::endl;
        exit(1);
    }
    auto resValidate = validateJsonScheme();
    if (!resValidate)
    {
        std::cout << "Error occured while validating:   " << ConfigErrorToString(resValidate.error()) << std::endl;
        exit(1);
    }
    setupConnConfig();
}

/**
 * @brief Create config for reading data
 *
 * @return S7Adapter::ReadConfig
 */
S7Adapter::ReadConfig S7Adapter::createReadConfig()
{
    S7Adapter::ReadConfig configVector{};
    ReadHeader ReadHeader;
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
 * @brief Convert target to Snap7 specific area code
 *
 * @param target_
 * @return int
 * @details This function returns the Snap7 specific 8bit code for area type
 */
int S7Adapter::cvrtTargetToSnap7Area(const std::string &target_)
{
    if (target_ == "e")
    {
        return S7AreaPE;
    }
    if (target_ == "a")
    {
        return S7AreaPA;
    }
    if (target_ == "m")
    {
        return S7AreaMK;
    }
    if (target_ == "db")
    {
        return S7AreaDB;
    }
    if (target_ == "c")
    {
        return S7AreaCT;
    }
    if (target_ == "t")
    {
        return S7AreaTM;
    }
    return -1;
}

int S7Adapter::getItemReadSize(const TagItem &item_)
{
    switch (item_.type)
    {
    case S7Type::BOOL:
    case S7Type::BYTE:
    case S7Type::SINT:
    case S7Type::USINT:
    case S7Type::CHAR:
    {
        return sizeof(u8);
    }
    case S7Type::WORD:
    case S7Type::INT:
    case S7Type::UINT:
    case S7Type::WCHAR:
    case S7Type::S5TIME:
    case S7Type::DATE:
    case S7Type::TIMER:
    case S7Type::COUNTER:
    {
        return sizeof(u16);
    }
    case S7Type::DWORD:
    case S7Type::DINT:
    case S7Type::UDINT:
    case S7Type::REAL:
    case S7Type::TIME:
    case S7Type::TOD:
    {
        return sizeof(u32);
    }
    case S7Type::LREAL:
    {
        return sizeof(u64);
    }
    case S7Type::STRING:
    {
        return 2 + item_.length; // First byte "max length", second byte "actual length"
    }
    case S7Type::WSTRING:
    {
        return 4 + 2 * item_.length; // First 2 byte "max length", second 2 byte "actual length"
    }
    default:
        return -1;
    }
}

/**
 * @brief Create a tag item for the read config
 *
 * @param mode_
 * @param tag_
 * @return TagItem
 */
TagItem S7Adapter::createTagItem(ReadMode mode_, const Json &tag_)
{
    TagItem item{};
    item.id = tag_.value("id", -1);
    item.name = tag_.value("name", "N/A");

    if (mode_ == ReadMode::SINGLE)
    {
        item.target = cvrtTargetToSnap7Area(tag_.at("target"));
    }
    if (tag_.contains("number"))
    {
        item.number = tag_.value("number", 0);
    }
    if (tag_.contains("offset"))
    {
        item.offset = tag_.value("offset", 0);
    }
    item.type = stringToS7Type(tag_.at("type"));
    if (item.type == S7Type::BOOL)
    {
        item.bit = tag_.at("bit");
    }
    if (item.type == S7Type::STRING || item.type == S7Type::WSTRING)
    {
        item.length = tag_.at("length");
    }
    return item;
}

/**
 * @brief Convert a string to S7Type enum
 *
 * @param type_
 * @return S7Type
 */
S7Type S7Adapter::stringToS7Type(const std::string &type_)
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
    if (type_ == "WSTRING")
    {
        return S7Type::WSTRING;
    }
    if (type_ == "S5TIME")
    {
        return S7Type::S5TIME;
    }
    if (type_ == "TIME")
    {
        return S7Type::TIME;
    }
    if (type_ == "DATE")
    {
        return S7Type::DATE;
    }
    if (type_ == "TOD")
    {
        return S7Type::TOD;
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

/**
 * @brief Convert a S7Type enum to string
 *
 * @param type_
 * @return S7Type
 */
std::string S7Adapter::S7TypeToString(const S7Type type_)
{
    switch (type_)
    {
    case S7Type::BOOL:
    {
        return "BOOL";
        break;
    }
    case S7Type::BYTE:
    {
        return "BYTE";
        break;
    }
    case S7Type::WORD:
    {
        return "WORD";
        break;
    }
    case S7Type::DWORD:
    {
        return "DWORD";
        break;
    }
    case S7Type::SINT:
    {
        return "SINT";
        break;
    }
    case S7Type::INT:
    {
        return "INT";
        break;
    }
    case S7Type::DINT:
    {
        return "DINT";
        break;
    }
    case S7Type::USINT:
    {
        return "USINT";
        break;
    }
    case S7Type::UINT:
    {
        return "UINT";
        break;
    }
    case S7Type::UDINT:
    {
        return "UDINT";
        break;
    }
    case S7Type::REAL:
    {
        return "REAL";
        break;
    }
    case S7Type::LREAL:
    {
        return "LREAL";
        break;
    }
    case S7Type::CHAR:
    {
        return "CHAR";
        break;
    }
    case S7Type::WCHAR:
    {
        return "WCHAR";
        break;
    }
    case S7Type::STRING:
    {
        return "STRING";
        break;
    }
    case S7Type::WSTRING:
    {
        return "WSTRING";
        break;
    }
    case S7Type::S5TIME:
    {
        return "S5TIME";
        break;
    }
    case S7Type::TIME:
    {
        return "TIME";
        break;
    }
    case S7Type::DATE:
    {
        return "DATE";
        break;
    }
    case S7Type::TOD:
    {
        return "TOD";
        break;
    }
    case S7Type::TIMER:
    {
        return "TIMER";
        break;
    }
    case S7Type::COUNTER:
    {
        return "COUNTER";
        break;
    }
    default:
        break;
    }
    return "INVALID";
}

/**
 * @brief Read data from given endpoint
 *
 * @return std::vector<DataPoint>
 */
std::vector<DataPoint> S7Adapter::readData()
{
    commonReadBuffer.clear(); // Clear read buffer, still has reserved size but end() iterator is at the start again

    for (const auto &config : snap7Config)
    {
        if (config.first.mode == ReadMode::AREA)
        {
            startSnap7AreaRead(config);
        }
        if (config.first.mode == ReadMode::SINGLE)
        {
            startSnap7SingleRead(config);
        }
    }
    return currentDatapPoints;
}

/**
 * @brief Start a snap7 Area read
 *
 * @param config_ Config item
 */
void S7Adapter::startSnap7AreaRead(const ReadConfigItem &config_)
{
    const auto area = config_.first.target;
    const auto number = config_.first.number;
    const auto start = config_.first.offset;
    const auto amount = config_.first.amount;
    const auto wordLen = S7WLByte;
    std::vector<u8> buffer(amount);

    int result = client->ReadArea(area, number, start, amount, wordLen, buffer.data());

    std::cout << "Area read result:     " << result << std::endl;

    commonReadBuffer.insert(commonReadBuffer.end(), buffer.begin(), buffer.end());
}

/**
 * @brief Start a snap7 MulitVar read
 *
 * @param config_ Config item
 */
void S7Adapter::startSnap7SingleRead(const ReadConfigItem &config_)
{
    std::vector<TS7DataItem> snap7DataItems{};
    snap7DataItems.reserve(config_.second.size());

    size_t bufferSize{0};
    for (const auto &item : config_.second)
    {
        bufferSize += getItemReadSize(item);
    }

    std::vector<u8> buffer(bufferSize);

    size_t offset{0};
    // Setup items for snap7
    for (const auto &item : config_.second)
    {
        TS7DataItem tempTS7item{};
        tempTS7item.Area = item.target;
        tempTS7item.WordLen = S7WLByte;
        tempTS7item.DBNumber = item.number;
        // Amount for timers/counter set to 1 and not byte size (2)
        if (tempTS7item.Area == S7AreaTM || tempTS7item.Area == S7AreaCT)
        {
            tempTS7item.Start = item.number;
            tempTS7item.Amount = 1;
        }
        else
        {
            tempTS7item.Start = item.offset;
            tempTS7item.Amount = getItemReadSize(item);
        }
        tempTS7item.pdata = buffer.data() + offset;
        snap7DataItems.push_back(tempTS7item);

        offset += getItemReadSize(item);
    }
    int result = client->ReadMultiVars(snap7DataItems.data(), snap7DataItems.size());
    std::cout << "Single read result:     " << result << std::endl;

    commonReadBuffer.insert(commonReadBuffer.end(), buffer.begin(), buffer.end());
}

/**
 * @brief Create generic datapoints
 *
 * @param buffer_
 * @return std::vector<DataPoint>
 */
void S7Adapter::createDataPoints()
{
    // Swap DP buffers
    std::swap(currentDatapPoints, previousDatapPoints);
    currentDatapPoints.clear();

    size_t offset{0};
    std::span<const u8> bufferView(commonReadBuffer);

    // Start filling DataPoints
    // This creates Datapoints out the commonReadBuffer
    for (const auto &config : snap7Config)
    {
        for (const auto &item : config.second)
        {
            DataPoint tempDP{};
            tempDP.id = item.id;
            tempDP.Quality = 0; // FIX
            tempDP.name = item.name;
            tempDP.timestamp = std::chrono::system_clock::now();
            tempDP.type = S7TypeToString(item.type);
            tempDP.data = cvrtBytesToType(extractBytes(bufferView, offset, getItemReadSize(item)), item);

            currentDatapPoints.push_back(tempDP);
            offset += getItemReadSize(item);
        }
    }
}

/**
 * @brief Extract bytes from a byte buffer
 *
 * @param buffer_ std::span on const u8
 * @param offset_
 * @param amount_
 * @return std::span<const u8>
 */
std::span<const u8> S7Adapter::extractBytes(std::span<const u8> buffer_, u32 offset_, u32 amount_)
{
    if (offset_ + amount_ > buffer_.size())
    {
        return buffer_;
    }
    return buffer_.subspan(offset_, amount_);
}

/**
 * @brief Convert bytes to S7 type
 *
 * @param bytes_
 * @param type_
 * @param bit_
 * @return GenericType
 */
GenericType S7Adapter::cvrtBytesToType(std::span<const u8> bytes_, const TagItem &item_)
{
    size_t size = getItemReadSize(item_);

    if (bytes_.size() != size)
    {
        return i32(-1); // ERH
    }

    switch (item_.type)
    {
        // Signle bit
    case S7Type::BOOL:
    {
        u8 raw{};
        raw = bytes_[0];
        return static_cast<bool>((raw & (1 << item_.bit)) != 0);
    }
        // Unsigned 1 byte
    case S7Type::BYTE:
    case S7Type::USINT:
    {
        u8 value{};
        value = bytes_[0];
        return value;
    }
        // Signed 1 byte
    case S7Type::SINT:
    {
        i8 value{};
        value = bytes_[0];
        return value;
    }
        // Signed 2 byte
    case S7Type::WORD:
    case S7Type::INT:
    {
        i16 raw{};
        i16 value{};
        std::memcpy(&raw, bytes_.data(), sizeof(raw));
        raw = std::byteswap(raw);
        std::memcpy(&value, &raw, sizeof(value));
        return value;
    }
        // Signed 4 byte
    case S7Type::DWORD:
    case S7Type::DINT:
    case S7Type::TIME:
    {
        i32 raw{};
        i32 value{};
        std::memcpy(&raw, bytes_.data(), sizeof(raw));
        raw = std::byteswap(raw);
        std::memcpy(&value, &raw, sizeof(value));
        return value;
    }
        // Unsigned 2 byte
    case S7Type::UINT:
    case S7Type::DATE:
    {
        u16 raw{};
        u16 value{};
        std::memcpy(&raw, bytes_.data(), sizeof(raw));
        raw = std::byteswap(raw);
        std::memcpy(&value, &raw, sizeof(value));
        return value;
    }
        // Unsigned 4 byte
    case S7Type::UDINT:
    case S7Type::TOD:
    {
        u32 raw{};
        u32 value{};
        std::memcpy(&raw, bytes_.data(), sizeof(raw));
        raw = std::byteswap(raw);
        std::memcpy(&value, &raw, sizeof(value));
        return value;
    }
    // IEEE Standard 4 byte float
    case S7Type::REAL:
    {
        u32 raw{};
        f32 value{};
        std::memcpy(&raw, bytes_.data(), sizeof(raw));
        raw = std::byteswap(raw);
        std::memcpy(&value, &raw, sizeof(value));
        return value;
    }
    // IEEE Standard 8 byte double
    // Date and Time
    case S7Type::LREAL:
    {
        u64 raw{};
        f64 value{};
        std::memcpy(&raw, bytes_.data(), sizeof(raw));
        raw = std::byteswap(raw);
        std::memcpy(&value, &raw, sizeof(value));
        return value;
    }
        // String
    case S7Type::CHAR:
    {
        std::string value{};
        value = latin1ToUtf8(bytes_);
        return value;
    }
    case S7Type::WCHAR:
    {
        std::string value{};
        value = utf16ToUtf8(bytes_);
        return value;
    }
    case S7Type::STRING:
    {
        std::string value{};

        int maxLength = std::min(item_.length, static_cast<u32>(bytes_[0])); // First byte: max length of string
        value = latin1ToUtf8(bytes_.subspan(2, maxLength));                  // Offst 2 byte (max/actual len)
        return value;
    }
    case S7Type::WSTRING:
    {
        std::string value{};
        u16 len{};

        std::memcpy(&len, bytes_.data(), sizeof(len)); // First 2 byte: max length of string
        len = std::byteswap(len);
        int maxLength = std::min(item_.length, static_cast<u32>(len));
        value = latin1ToUtf8(bytes_.subspan(4, 2 * maxLength)); // Offset 4 byte (max/actual len)
        return value;
    }
    // Timer
    case S7Type::S5TIME:
    case S7Type::TIMER:
    {
        u16 raw{};
        u16 value{};
        std::memcpy(&raw, bytes_.data(), sizeof(raw));
        raw = std::byteswap(raw);
        std::memcpy(&value, &raw, sizeof(value));
        return S5TimeToMilis(value);
    }
    // Counter
    case S7Type::COUNTER:
    {
        u16 raw{};
        u16 value{};
        std::memcpy(&raw, bytes_.data(), sizeof(raw));
        raw = std::byteswap(raw);
        std::memcpy(&value, &raw, sizeof(value));
        return value;
    }
    default:
        return i32(-2);
    }
}

void S7Adapter::writeData() const
{
    return;
}

void S7Adapter::connect() const
{
    std::cout << std::endl;
    std::cout << "Connecting to PLC at IP " << connectionConfig.ip << " ..." << std::endl;
    int result;
    result = client->ConnectTo(connectionConfig.ip.c_str(), connectionConfig.rack, connectionConfig.slot);
    if (client->Connected)
    {
        std::cout << "Connected to PLC at IP: " << connectionConfig.ip << std::endl;
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

/**
 * @brief Convert s5time format to miliseconds
 *
 * @param time_
 * @return u32
 */
u32 S7Adapter::S5TimeToMilis(const u16 time_)
{
    // Bit 12/13 give the time base
    u8 base = (time_ >> 12);
    base &= 0x03;
    // Goddamn this s5time format
    // bit 0-3: dezimal but only valid for 0-9 * 1
    // bit 4-7: dezimal but only valid for 0-9 * 10
    // bit 8-11: dezimal but only valid for 0-9 * 100

    // bit 0-3
    u8 bcd1 = (time_);
    bcd1 &= 0x0F; // Lower 4 bit
    // bit 4-7
    u8 bcd2 = (time_ >> 4);
    bcd2 &= 0x0F; // Lower 4 bit
    // bit 8-11
    u8 bcd3 = (time_ >> 8);
    bcd3 &= 0x0F; // Lower 4 bit
    u16 bcdValue = bcd3 * 100 + bcd2 * 10 + bcd1 * 1;

    u32 bcdMiliseconds{};
    switch (base)
    {
    case 0:
    {
        bcdMiliseconds = bcdValue * 10;
        break;
    }
    case 1:
    {
        bcdMiliseconds = bcdValue * 100;
        break;
    }
    case 2:
    {
        bcdMiliseconds = bcdValue * 1000;
        break;
    }
    case 3:
    {
        bcdMiliseconds = bcdValue * 10000;
        break;
    }

    default:
    {
        bcdMiliseconds = bcdValue * 1000;
        break;
    }
    }
    return bcdMiliseconds;
}

std::string S7Adapter::latin1ToUtf8(std::span<const u8> bytes_)
{
    std::string result;
    for (auto c : bytes_)
        utf8::append(static_cast<char32_t>(c), result);
    return result;
}

std::string S7Adapter::utf16ToUtf8(std::span<const u8> bytes_)
{
    std::u16string u16;
    for (size_t i = 0; i + 1 < bytes_.size(); i += 2)
        u16.push_back(static_cast<char16_t>(bytes_[i] << 8 | bytes_[i + 1]));

    std::string result;
    utf8::utf16to8(u16.begin(), u16.end(), std::back_inserter(result));
    return result;
}

void S7Adapter::DBG_printConfigElements()

{
    using namespace std;
    cout << "Amount of Read configs:     " << snap7Config.size() << endl;
    for (const auto &i : snap7Config)
    {

        cout << endl;
        cout << endl;
        cout << "<---New config--->" << endl;
        cout << "mode:      ";
        (i.first.mode == ReadMode::AREA) ? cout << "AREA" : cout << "SINGLE";
        cout << endl;
        cout << "target:    " << int(i.first.target) << endl;
        cout << "offset:    " << i.first.offset << endl;
        cout << "amount:    " << i.first.amount << endl;
        cout << "req pdu:   " << i.first.requestPduSize << endl;
        cout << "res pdu:   " << i.first.responsePduSize << endl;
        cout << "items:     " << i.first.itemCount << endl;
        cout << endl;
        cout << "<--Tags-->" << endl;
        cout << endl;
        for (const auto &t : i.second)
        {
            cout << endl;
            cout << "<-Tag->" << endl;
            cout << "id:       " << t.id << endl;
            cout << "name:     " << t.name << endl;
            cout << "target:   " << int(t.target) << endl;
            cout << "number:   " << t.number << endl;
            cout << "offset:   " << t.offset << endl;
            cout << "length:   " << t.length << endl;
            cout << "type:     " << S7TypeToString(t.type) << endl;
            cout << "bit:      " << int(t.bit) << endl;
        }
    }
}

void S7Adapter::DBG_printCurrentDPElements()
{
    using namespace std;

    for (const auto &dp : currentDatapPoints)
    {
        cout << endl;
        cout << "<-DataPoint->" << endl;
        cout << "id:       " << dp.id << endl;
        cout << "Quality:  " << dp.Quality << endl;
        cout << "name:     " << dp.name << endl;
        cout << "time:     " << dp.timestamp << endl;
        cout << "type:     " << dp.type << endl;
        cout << "data:     ";
        visit([](const auto &value)
              { cout << value; }, dp.data);
        cout << endl;
    }
}