#pragma once
#include <memory>
#include "types.hpp"
#include "IDataSourceAdapater.hpp"
#include "snap7micro/s7_micro_client.h"

/**
 * @brief Adapter for S7comm
 *
 */
class S7Adapter : public IDataSourceAdapter
{
private:
    const char ip[16];
    const u32 rack;
    const u32 slot;
    std::unique_ptr<TSnap7MicroClient> client;
    bool status;

    void connect() const override;
    void disconnect() const;
    bool getConnectedState() const;
    std::vector<DataPoint> read() const;
    void write() const;

public:
    S7Adapter(const char ip_, u32 rack_, u32 slot_);
    ~S7Adapter();
};