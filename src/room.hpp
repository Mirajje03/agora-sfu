#pragma once

#include "fwd.hpp"
#include "participant.hpp"

#include <rtc/description.hpp>

#include <memory>
#include <unordered_map>

namespace sfu {

class Loop;

using ClientId = uint64_t;

class Room {
public:
    Room() = default;

    void AddParticipant(ClientId clientId, const std::shared_ptr<Participant>& participant);
    void RemoveParticipant(ClientId clientId);

    bool HasParticipant(ClientId clientId) {
        return Participants_.count(clientId);
    }

    void HandleTrackForParticipant(ClientId clientId, const std::shared_ptr<rtc::Track>& track);

private:
    std::unordered_map<ClientId, std::shared_ptr<Participant>> Participants_;
};

} // namespace sfu
