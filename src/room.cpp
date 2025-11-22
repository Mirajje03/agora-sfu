#include "room.hpp"

#include "loop.hpp"

#include <rtc/description.hpp>
#include <rtc/rtc.hpp>

namespace sfu {

void Room::AddParticipant(ClientId newClientId, const std::shared_ptr<Participant>& participant) {
    for (auto& [id, other] : Participants_) {
        auto audioTrack = other->GetAudioTrack();

        if (!audioTrack) {
            continue;
        }

        std::cout << "Adding existing track from participant " << id << " to participant " << newClientId << std::endl;
        
        auto remoteTrack = participant->GetConnection()->addTrack(audioTrack->description());
        other->AddRemoteTrack(newClientId, remoteTrack);
    }

    Participants_[newClientId] = participant;
}

void Room::HandleTrackForParticipant(ClientId clientId, const std::shared_ptr<rtc::Track>& track) {
    auto& participant = Participants_.at(clientId);

    participant->SetAudioTrack(track);
    for (auto& [id, other] : Participants_) {
        if (id == clientId) {
            continue;
        }

        std::cout << "Adding track from participant " << clientId << " to participant " << id << std::endl;

        auto remoteTrack = other->GetConnection()->addTrack(track->description());
        participant->AddRemoteTrack(id, remoteTrack);
    }
}

void Room::RemoveParticipant(ClientId clientId) {
    if (!Participants_.count(clientId)) {
        return;
    }

    std::cout << "Removing participant " << clientId << " from the room" << std::endl;

    for (auto& [id, participant] : Participants_) {
        if (id == clientId) {
            continue;
        }

        std::cout << "Removing track from " << id << " to " << clientId << std::endl;

        participant->RemoveRemoteTrack(clientId);
    }

    Participants_.at(clientId)->GetAudioTrack()->close();
    Participants_.erase(clientId);
}

} // namespace sfu
