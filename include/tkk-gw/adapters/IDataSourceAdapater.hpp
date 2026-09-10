#pragma once
#include "types.hpp"
#include <string>
#include <variant>
#include <vector>

using S7type = std::variant<

    std::string,
    bool,
    i8,
    u8,
    i16,
    u16,
    i32,
    u32,
    f32,
    f64,
    >;

struct DataPoint
{
    i32 id;
    i32 Quality;
    S7type data;
    std::string timestamp;
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

    virtual ~IDataSourceAdapter() {}
};