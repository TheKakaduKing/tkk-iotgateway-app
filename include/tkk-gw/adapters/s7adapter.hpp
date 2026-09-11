#pragma once
#include <memory>
#include "types.hpp"
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

enum class S7Type
{
    BOOL,
    BYTE,
    WORD,
    DWORD,
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
    bool readArea{false};
    u32 dbNumber{0};
    u32 offset{0};
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
    using ReadConfig = std::vector<std::pair<ReadType, std::vector<TagItem>>>;

private:
    S7Connection connectionConfig{};
    std::unique_ptr<TSnap7MicroClient> client;
    bool status{false};
    ReadConfig snap7Config;
    nlohmann::json configDataJson;

    void connect() const override;
    void disconnect() const override;
    bool getConnectedState() const override;
    std::vector<DataPoint> readData() const override;
    void writeData() const override;

    void openConfigFile(const std::string &path_);
    void parseConfigFile(std::ifstream &file_);
    ReadConfig configureAdapter();
    TagItem createTagItem(bool readArea_, const nlohmann::json_abi_v3_12_0::json &tag_);
    Target parseTarget(const std::string &target_);
    S7Type parseS7Type(const std::string &type_);

public:
    S7Adapter();
    ~S7Adapter();
};