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
