#pragma once
#include <memory>
#include "types.hpp"
#include "IDataSourceAdapater.hpp"
#include "snap7micro/s7_micro_client.h"
#include "nlohmann/json.hpp"

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
    nlohmann::json configData;

    void connect() const override;
    void disconnect() const override;
    bool getConnectedState() const override;
    std::vector<DataPoint> readData() const override;
    void writeData() const override;

    void openConfigFile(const std::string &path_);
    void parseConfigFile(std::ifstream &file_);
    void interpretConfigFile();

public:
    S7Adapter(const char ip_, u32 rack_, u32 slot_);
    ~S7Adapter();
};