#pragma once
#include <memory>
#include <span>
#include "tkk-gw/types.hpp"
#include "IDataSourceAdapater.hpp"
#include "snap7micro/s7_micro_client.h"
#include "nlohmann/json.hpp"

enum class S7Type
{
    BOOL,
    BYTE,
    WORD,
    DWORD,
    LWORD,
    SINT,
    INT,
    DINT,
    USINT,
    UINT,
    UDINT,
    LINT,
    ULINT,
    REAL,
    LREAL,
    CHAR,
    WCHAR,
    STRING,
    S5TIME,
    TIME,
    LTIME,
    TIMER,
    COUNTER,
    INVALID,
};

enum class ReadMode
{
    SINGLE,
    AREA
};

struct TagItem
{
    u32 id{0};
    std::string name{""};
    u8 target{0};
    u32 dbNumber{0};
    u32 offset{0};
    S7Type type{0};
    u8 bit{0};
};

struct ReadHeader
{
    ReadMode mode{ReadMode::SINGLE};
    u8 target{0};
    u32 dbNumber{0};
    u32 offset{0};
    u32 amount{0};

    int requestPduSize{0};
    int responsePduSize{0};
    size_t itemCount{0};
};

struct S7Connection
{
    std::string ip{};
    u32 rack{0};
    u32 slot{2};
};

/**
 * @brief Adapter for S7comm
 *
 */
class S7Adapter : public IDataSourceAdapter
{
    using ReadConfigItem = std::pair<ReadHeader, std::vector<TagItem>>;
    using ReadConfig = std::vector<ReadConfigItem>;

private:
    S7Connection connectionConfig{};
    std::unique_ptr<TSnap7MicroClient> client;
    bool status{false};
    const std::string configPath;
    nlohmann::json configDataJson;
    ReadConfig snap7Config{};

    void connect() const override;
    void disconnect() const override;
    bool getConnectedState() const override;
    std::vector<DataPoint> readData() override;
    void writeData() const override;

    std::ifstream openConfigFile();
    void parseConfigFile();
    void configureAdapter();
    TagItem createTagItem(ReadMode mode_, const nlohmann::json_abi_v3_12_0::json &tag_);
    S7Type parseS7Type(const std::string &type_);
    int getTypeSize(S7Type type_);
    void setupConnConfig();
    ReadConfigItem createAreaReadConfigItem(const nlohmann::json_abi_v3_12_0::json &block_);
    std::vector<ReadConfigItem> createSingleReadConfigItem(const nlohmann::json_abi_v3_12_0::json &block_);
    ReadConfig createReadConfig();
    int cvrtTargetToSnap7Area(const std::string &target_);
    void startSnap7AreaRead(const ReadConfigItem &config_);
    void startSnap7SingleRead(const ReadConfigItem &config_);
    std::vector<DataPoint> createDataPoints(const std::span<const u8> &buffer_);

    std::span<const u8> extractBytes(std::span<const u8> buffer_, u32 offset_, u32 length);
    GenericType cvrtBytesToType(std::span<const u8> bytes_, S7Type type_);

    void DBG_printConfigElements();

public:
    void init();

    S7Adapter(const std::string &configPath_);
    ~S7Adapter();
};
