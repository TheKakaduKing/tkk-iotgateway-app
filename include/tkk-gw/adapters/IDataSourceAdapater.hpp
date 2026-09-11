#pragma once
#include "types.hpp"
#include <string>
#include <variant>
#include <vector>
#include <chrono>

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
    std::chrono::system_clock::time_point timestamp;
};

/**
 * @brief Interface for communication adapters
 * @warning All methods are non thread safe
 */
struct IDataSourceAdapter
{
public:
    virtual void connect() const = 0;
    virtual void disconnect() const = 0;
    virtual bool getConnectedState() const;
    virtual std::vector<DataPoint> readData() = 0;
    virtual void writeData() = 0;

    virtual ~IDataSourceAdapter() {}
};