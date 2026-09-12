#pragma once
#include <memory>
#include "tkk-gw/types.hpp"
#include "IDataSourceAdapater.hpp"
#include "snap7micro/s7_micro_client.h"
#include "nlohmann/json.hpp"

enum class Target
{
    DB,
    INPUT,
    OUTPUT,
    MERKER,
    TIMER,
    COUNTER,
    ARRAY,
    INVALID,
};

enum class AreaTarget
{
    DB,
    INPUT,
    OUTPUT,
    MERKER,
    INVALID,
};

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
    Target target{Target::DB}; // Set standard type to DB for area reads
    u32 dbNumber{0};
    u32 offset{0};
    S7Type type{0};
    u8 bit{0};
};

struct ReadType
{
    ReadMode mode{ReadMode::SINGLE};
    AreaTarget target{AreaTarget::DB};
    u32 offset{0};
    u32 amount{0};
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
    using ReadConfigItem = std::pair<ReadType, std::vector<TagItem>>;
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
    std::vector<DataPoint> readData() const override;
    void writeData() const override;

    std::ifstream openConfigFile();
    void parseConfigFile();
    ReadConfig configureAdapter();
    TagItem createTagItem(ReadMode mode_, const nlohmann::json_abi_v3_12_0::json &tag_);
    Target parseTarget(const std::string &target_);
    AreaTarget parseAreaTarget(const std::string &areaTarget_);
    S7Type parseS7Type(const std::string &type_);
    int getTypeSize(S7Type type_);
    void setupConnConfig();
    ReadConfigItem createAreaReadConfigItem(const nlohmann::json_abi_v3_12_0::json &block_);
    std::vector<ReadConfigItem> createSingleReadConfigItem(const nlohmann::json_abi_v3_12_0::json &block_);

public:
    S7Adapter(const std::string &configPath_);
    ~S7Adapter();
};