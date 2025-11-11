#include "room.hpp"

#include "loop.hpp"

#include <rtc/description.hpp>
#include <rtc/rtc.hpp>

namespace sfu {

Room::Room(const std::shared_ptr<Loop>& loop)
    : Loop_(loop)
{ }

void Room::Broadcast(uint64_t fromId, rtc::message_variant&& message) {
    for (auto& [id, dc] : Clients_) {
        if (id == fromId || dc->isClosed()) {
            continue;
        }

        try {
            dc->send(message);
        } catch (const std::exception& e) {
            // Retries?
            std::cerr << "Failed to send to client " << id << ": " << e.what() << std::endl;
        }
    }
}

void Room::AddClient(uint64_t clientId, std::shared_ptr<rtc::DataChannel> dataChannel) {
    if (Clients_.count(clientId)) {
        Clients_[clientId]->close();
    }
    Clients_[clientId] = dataChannel;

    dataChannel->onMessage([this, clientId](rtc::message_variant data) {
        Loop_->EnqueueTask([this, clientId, message = std::move(data)]() mutable {
            Broadcast(clientId, std::move(message));
        });
    });
}

void Room::RemoveClient(uint64_t id) {
    if (!Clients_.count(id)) {
        return;
    }

    Clients_.at(id)->close();
    Clients_.erase(id);
}

} // namespace sfu
