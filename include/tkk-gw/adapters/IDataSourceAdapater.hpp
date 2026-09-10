#pragma once
#include <string>
#include <variant>
#include <vector>

using S7type = std::variant<
    bool,
    int8_t,
    u_int8_t,
    int16_t,
    u_int16_t,
    int32_t,
    u_int32_t,
    int64_t,
    u_int64_t,
    float,
    double,
    std::string,
    >;

struct DataPoint
{
    std::string timestamp;
    int id;
    S7type data;
    int Quality;
};

/**
 * @brief Interface for communication adapters
 * @warning All methods are non thread safe
 */
struct IDataSourceAdapter
{
public:
    virtual void connect() = 0;
    virtual void disconnect() = 0;
    virtual void getStatus() = 0;
    virtual std::vector<DataPoint> read() = 0;
    virtual void write() = 0;
};