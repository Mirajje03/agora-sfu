#pragma once

#include "fwd.hpp"

#include <rtc/description.hpp>

#include <memory>
#include <unordered_map>

namespace sfu {

class Loop;

using ClientId = uint64_t;

class Room {
public:
    Room(const std::shared_ptr<Loop>& loop);
    void AddClient(uint64_t clientId, std::shared_ptr<rtc::DataChannel> dataChannel);
    void RemoveClient(uint64_t clientId);

private:
    void Broadcast(uint64_t fromId, rtc::message_variant&& message);

private:
    std::shared_ptr<Loop> Loop_;

    std::unordered_map<uint64_t, std::shared_ptr<rtc::DataChannel>> Clients_;
};

} // namespace sfu
