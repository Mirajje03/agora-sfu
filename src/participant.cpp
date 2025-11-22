#include "participant.hpp"

namespace sfu {

Participant::Participant(const std::shared_ptr<rtc::PeerConnection>& peerConnection)
    : PeerConnection_(peerConnection)
{ }

void Participant::SetAudioTrack(const std::shared_ptr<rtc::Track>& track) {
    Track_ = track;

    Track_->onMessage([this](rtc::binary message) {
        std::lock_guard<std::mutex> lock(TracksMutex_);
        
        for (auto& [id, target] : OutgoingTracks_) {
            if (target->isOpen()) {
                target->send(message);
            }
        }
    }, nullptr);
}

} // namespace sfu
