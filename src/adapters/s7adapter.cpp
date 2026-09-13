#include <iostream>
#include <fstream>
#include "tkk-gw/adapters/s7adapter.hpp"
#include "snap7micro/s7_types.h"

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
        // DBG_printConfigElements();
    }
    else
    {
        exit;
    }
    const auto &test = readData();
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
        exit;
    }
    return file;
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
    ReadHeader tempReadHeader{};

    tempReadHeader.mode = ReadMode::AREA;
    tempReadHeader.target = cvrtTargetToSnap7Area(block_.at("target").get<std::string>());
    tempReadHeader.dbNumber = block_.at("dbno").get<u32>();
    tempReadHeader.offset = block_.at("offset").get<u32>();
    tempReadHeader.amount = block_.at("amount").get<u32>();

    tempConfigItem.first = tempReadHeader;

    const auto &tags = block_.at("tags");

    for (const auto &tag : tags)
    {
        tempConfigItem.second.push_back(createTagItem(tempConfigItem.first.mode, tag));
    }

    std::cout << "AreaRead config created" << std::endl;
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
    ReadHeader tempReadHeader{};
    int currentItemSize{0}, currentPduSize{14}, maxItemCount{(client->PDULength - 12) / 12};
    size_t currentItemCount{0};

    const auto &tags = block_.at("tags");

    for (const auto &tag : tags)
    {
        tempTagItem = createTagItem(tempConfigItem.first.mode, tag);
        currentItemSize = 4 + getTypeSize(tempTagItem.type);
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
    parseConfigFile();
    setupConnConfig();
}

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
        return sizeof(u8);
        break;
    }
    case S7Type::WORD:
    case S7Type::INT:
    case S7Type::UINT:
    case S7Type::WCHAR:
    case S7Type::S5TIME:
    {
        return sizeof(u16);
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
        return sizeof(u32);
        break;
    }
    case S7Type::LWORD:
    case S7Type::LINT:
    case S7Type::ULINT:
    case S7Type::LREAL:
    case S7Type::LTIME:
    {
        return sizeof(u64);
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
        item.target = cvrtTargetToSnap7Area(tag_.value("target", "unknownTarget"));
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
        std::cout << std::endl;
        std::cout << "Created tag item with a bit" << std::endl;
        std::cout << "Extrcted bit:" << int(item.bit) << std::endl;
    }
    return item;
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

std::vector<DataPoint> S7Adapter::readData()
{
    std::vector<DataPoint> dataVector;
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
    return dataVector;
}

void S7Adapter::startSnap7AreaRead(const ReadConfigItem &config_)
{
    const auto area = config_.first.target;
    const auto dbNumber = config_.first.dbNumber;
    const auto start = config_.first.offset;
    const auto amount = config_.first.amount;
    const auto wordLen = S7WLByte;
    std::vector<u8> buffer(amount);
    std::span<const u8> view(buffer);

    int result = client->ReadArea(area, dbNumber, start, amount, wordLen, buffer.data());

    createDataPoints(view);
}

std::vector<DataPoint> S7Adapter::createDataPoints(const std::span<const u8> &buffer_)
{
    std::vector<DataPoint> dataVector{};

    for (const auto &config : snap7Config)
    {
        for (const auto &item : config.second)
        {
            DataPoint tempDP{};
            tempDP.id = item.id;
            tempDP.Quality = 0; // FIX
            tempDP.name = item.name;
            tempDP.timestamp = std::chrono::system_clock::now();
            tempDP.data = cvrtBytesToType(extractBytes(buffer_, item.offset, getTypeSize(item.type)), item.type, item.bit);

            dataVector.push_back(tempDP);
            std::cout << "New DP created" << std::endl;
        }
    }
    std::cout << std::endl;
    std::cout << "Creation of DP finished" << std::endl;
    std::cout << "DP size:  " << dataVector.size() << std::endl;
    for (const auto &dp : dataVector)
    {
        std::cout << std::endl;
        std::cout << "<-DataPoint->" << std::endl;
        std::cout << "id:       " << dp.id << std::endl;
        std::cout << "Quality:  " << dp.Quality << std::endl;
        std::cout << "name:     " << dp.name << std::endl;
        std::cout << "time:     " << dp.timestamp << std::endl;
        std::cout << "data:     ";
        std::visit([](const auto &value)
                   { std::cout << value; }, dp.data);
        std::cout << std::endl;
    }
    return dataVector;
}

std::span<const u8> S7Adapter::extractBytes(std::span<const u8> buffer_, u32 offset_, u32 amount_)
{
    if (offset_ + amount_ > buffer_.size())
    {
        return buffer_;
    }
    return buffer_.subspan(offset_, amount_);
}

GenericType S7Adapter::cvrtBytesToType(std::span<const u8> bytes_, S7Type type_, u8 bit_)
{
    size_t size = getTypeSize(type_);

    if (bytes_.size() != size)
    {
        return u32(99); // ERH
    }

    switch (type_)
    {
        // Signle bit
    case S7Type::BOOL:
    {
        u8 raw{};
        std::memcpy(&raw, bytes_.data(), sizeof(u8));
        return bool((raw & (1 << bit_)) != 0);
        break;
    }
        // Unsigned 1 byte
    case S7Type::BYTE:
    case S7Type::CHAR:
    case S7Type::USINT:
    {
        u8 value{};
        std::memcpy(&value, bytes_.data(), sizeof(u8));
        return value;
        break;
    }
        // Signed 1 byte
    case S7Type::SINT:
    {
        i8 value{};
        std::memcpy(&value, bytes_.data(), sizeof(i8));
        return value;
        break;
    }
        // Signed 2 byte
    case S7Type::WORD:
    case S7Type::INT:
    {
        i16 raw{};
        i16 value{};
        std::memcpy(&raw, bytes_.data(), sizeof(i16));
        raw = std::byteswap(raw);
        std::memcpy(&value, &raw, sizeof(i16));
        return value;
        break;
    }
        // Signed 4 byte
    case S7Type::DWORD:
    case S7Type::DINT:
    {
        i32 raw{};
        i32 value{};
        std::memcpy(&raw, bytes_.data(), sizeof(i32));
        raw = std::byteswap(raw);
        std::memcpy(&value, &raw, sizeof(i32));
        return value;
        break;
    }
        // Signed 8 byte
    case S7Type::LWORD:
    case S7Type::LINT:
    {
        i64 raw{};
        i64 value{};
        std::memcpy(&raw, bytes_.data(), sizeof(i64));
        raw = std::byteswap(raw);
        std::memcpy(&value, &raw, sizeof(i64));
        return value;
        break;
    }
        // Unsigned 2 byte
    case S7Type::UINT:
    case S7Type::WCHAR:
    {
        u16 raw{};
        u16 value{};
        std::memcpy(&raw, bytes_.data(), sizeof(u16));
        raw = std::byteswap(raw);
        std::memcpy(&value, &raw, sizeof(u16));
        return value;
        break;
    }
        // Unsigned 4 byte
    case S7Type::UDINT:
    {
        u32 raw{};
        u32 value{};
        std::memcpy(&raw, bytes_.data(), sizeof(u32));
        raw = std::byteswap(raw);
        std::memcpy(&value, &raw, sizeof(u32));
        return value;
        break;
    }
        // Unsigned 8 byte
    case S7Type::ULINT:
    {
        u64 raw{};
        u64 value{};
        std::memcpy(&raw, bytes_.data(), sizeof(u64));
        raw = std::byteswap(raw);
        std::memcpy(&value, &raw, sizeof(u64));
        return value;
        break;
    }
    // IEEE Standard 4 byte float
    case S7Type::REAL:
    {
        u32 raw{};
        f32 value{};
        std::memcpy(&raw, bytes_.data(), sizeof(u32));
        raw = std::byteswap(raw);
        std::memcpy(&value, &raw, sizeof(f32));
        return value;
        break;
    }
    // IEEE Standard 8 byte double
    case S7Type::LREAL:
    {
        u64 raw{};
        f64 value{};
        std::memcpy(&raw, bytes_.data(), sizeof(u64));
        raw = std::byteswap(raw);
        std::memcpy(&value, &raw, sizeof(f64));
        return value;
        break;
    }
        // String
    case S7Type::STRING:
    {
        break;
    }
    // BCD
    case S7Type::S5TIME:
    {
        break;
    }
    // IEC Time
    case S7Type::TIME:
    {
        break;
    }
    // IEC LTime
    case S7Type::LTIME:
    {
        break;
    }
    // Timer
    case S7Type::TIMER:
    {
        break;
    }
    // Counter
    case S7Type::COUNTER:
    {
        break;
    }

    default:
        return u32(99);
        break;
    }
}

void S7Adapter::startSnap7SingleRead(const ReadConfigItem &config_)
{
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
        (i.first.mode == ReadMode::AREA) ? cout << "AREA" : cout << "SIGNLE";
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
            cout << "dbNumber: " << t.dbNumber << endl;
            cout << "offset:   " << t.offset << endl;
            cout << "type:     " << int(t.type) << endl;
            cout << "bit:      " << int(t.bit) << endl;
        }
    }
}