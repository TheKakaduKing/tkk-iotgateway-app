#pragma once
#include "types.hpp"
#include "IDataSourceAdapater.hpp"

/**
 * @brief Adapter for S7comm
 *
 */
class S7Adapter : public IDataSourceAdapter
{
private:
    u32 ip{0};
    u32 rack{0};
    u32 slot{2};

    void connect();
    void disconnect();
    void getStatus();
    std::vector<DataPoint> read();
    void write();

public:
    S7Adapter() {}
};