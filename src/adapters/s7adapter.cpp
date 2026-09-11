#include <fstream>
#include "tkk-gw/adapters/s7adapter.hpp"

S7Adapter::S7Adapter(const char ip_, u32 rack_, u32 slot_) : ip{ip_},
                                                             rack{rack_},
                                                             slot{slot_},
                                                             client{std::make_unique<TSnap7MicroClient>()},
                                                             status{false}
{
}

S7Adapter::~S7Adapter()
{
    this->client->Disconnect();
}

/**
 * @brief *pens the adapter config file
 *
 * @param path_
 */
void S7Adapter::openConfigFile(const std::string &path_)
{
    std::ifstream file(path_); // std::ifstream is RAII -> closes itself when leaving scope
    if (!file.is_open())
    {
        return;
    }
    this->parseConfigFile(file);
}

/**
 * @brief Parses config file into nlohmann::json object
 *
 * @param file_
 */
void S7Adapter::parseConfigFile(std::ifstream &file_)
{
    this->configData = nlohmann::json::parse(file_);
}

/**
 * @brief Interprets the config file
 *
 */
void S7Adapter::interpretConfigFile() {

};

void S7Adapter::connect() const
{
    int result;
    result = this->client->ConnectTo(this->ip, this->rack, this->slot);
}

void S7Adapter::disconnect() const
{
    int result;
    result = this->client->Disconnect();
}

bool S7Adapter::getConnectedState() const
{
    return client->Connected;
}
