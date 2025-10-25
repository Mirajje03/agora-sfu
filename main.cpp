#include "rtc/description.hpp"
#include <rtc/rtc.hpp>
#include <nlohmann/json.hpp>

#include <iostream>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>
#include <string>

using json = nlohmann::json;

struct Client {
    std::string id;
    std::shared_ptr<rtc::WebSocket> ws; // signaling connection
    std::shared_ptr<rtc::PeerConnection> pc;
    std::shared_ptr<rtc::DataChannel> dc; // set when remote creates it
};

int main() {
    rtc::InitLogger(rtc::LogLevel::Info);
    std::cout << "Starting minimal libdatachannel WebRTC hub server..." << std::endl;

    // Keep track of connected clients
    std::unordered_map<std::string, std::shared_ptr<Client>> clients;
    std::mutex clientsMutex;
    std::atomic_uint64_t nextId{1};

    // Helper: broadcast a message received on one client's DataChannel to all others
    auto broadcastToOthers = [&](const std::string& fromId, const rtc::message_variant& data) {
        std::lock_guard<std::mutex> lock(clientsMutex);
        for (auto& [id, c] : clients) {
            if (id == fromId) continue;
            if (!c->dc || c->dc->isClosed()) continue;
            c->dc->send(data);
        }
    };

    // WebSocket server for signaling
    rtc::WebSocketServer::Configuration wsCfg;
    wsCfg.port = 8000;     // ws://127.0.0.1:8000

    auto wsServer = std::make_shared<rtc::WebSocketServer>(wsCfg);

    // Handle incoming WS (signaling) clients
    wsServer->onClient([&](std::shared_ptr<rtc::WebSocket> ws) {
        // Assign an ID when the WS opens
        ws->onOpen([&, ws]() {
            std::string id = std::to_string(nextId++);
            auto client = std::make_shared<Client>();
            client->id = id;
            client->ws = ws;

            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                clients[id] = client;
            }

            std::cout << "WS client connected, id=" << id << std::endl;

            // Let the client know their assigned ID (optional)
            json hello = {{"type", "hello"}, {"id", id}};
            ws->send(hello.dump());
        });

        ws->onClosed([&, ws]() {
            // Clean up client on WS close
            std::shared_ptr<Client> clientToClose;
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                for (auto it = clients.begin(); it != clients.end(); ++it) {
                    if (it->second->ws == ws) {
                        clientToClose = it->second;
                        clients.erase(it);
                        break;
                    }
                }
            }
            if (clientToClose) {
                std::cout << "WS client disconnected, id=" << clientToClose->id << std::endl;
                if (clientToClose->dc) clientToClose->dc->close();
                if (clientToClose->pc) clientToClose->pc->close();
            }
        });

        // Receive signaling messages: offer/candidate
        ws->onMessage([&, ws](rtc::message_variant msg) {
            auto pstr = std::get_if<std::string>(&msg);
            if (!pstr) return;

            json j;
            try {
                j = json::parse(*pstr);
            } catch (...) {
                std::cerr << "Invalid JSON signaling message" << std::endl;
                return;
            }

            // Find client by ws
            std::shared_ptr<Client> client;
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                for (auto& [id, c] : clients) {
                    if (c->ws == ws) {
                        client = c;
                        break;
                    }
                }
            }
            if (!client) {
                std::cerr << "Client not found for signaling message" << std::endl;
                return;
            }

            auto typeIt = j.find("type");
            if (typeIt == j.end() || !typeIt->is_string()) {
                std::cerr << "Signaling message missing type" << std::endl;
                return;
            }

            const std::string type = *typeIt;

            if (type == "offer") {
                // Create PeerConnection for this client (if not already created)
                if (!client->pc) {
                    rtc::Configuration config;
                    // For local tests, no STUN/TURN is required. Host candidates will do.
                    // If needed later:
                    // config.iceServers.emplace_back("stun:stun.l.google.com:19302");

                    client->pc = std::make_shared<rtc::PeerConnection>(config);

                    // Send local description (answer) back over signaling
                    client->pc->onLocalDescription([&, ws, weakClient = std::weak_ptr<Client>(client)](rtc::Description desc) {
                        if (auto c = weakClient.lock()) {
                            json answer = {
                                {"type", desc.typeString()},
                                {"sdp", desc}
                            };
                            ws->send(answer.dump());
                        }
                    });

                    // Send trickle ICE candidates back over signaling
                    client->pc->onLocalCandidate([&, ws, weakClient = std::weak_ptr<Client>(client)](rtc::Candidate cand) {
                        if (!cand.isResolved()) {
                            json jcand = {
                                {"type", "candidate"},
                                {"candidate", cand.candidate()},
                            };
                            if (auto mid = cand.mid(); !mid.empty()) {
                                jcand["sdpMid"] = mid;
                            }
                            ws->send(jcand.dump());
                        } else {
                            // End-of-candidates (optional to signal)
                            json done = {{"type", "endOfCandidates"}};
                            ws->send(done.dump());
                        }
                    });

                    // Get remote-created DataChannel
                    client->pc->onDataChannel([&, weakClient = std::weak_ptr<Client>(client)](std::shared_ptr<rtc::DataChannel> dc) {
                        if (weakClient.expired()) {
                            return;
                        }

                        auto c = weakClient.lock();
                        c->dc = dc;
                        std::cout << "Client " << c->id << " data channel: label=" << dc->label() << std::endl;

                        dc->onOpen([weakClient]() {
                            if (auto c = weakClient.lock()) {
                                std::cout << "DataChannel open for client " << c->id << std::endl;
                            }
                        });

                        dc->onClosed([weakClient]() {
                            if (auto c = weakClient.lock()) {
                                std::cout << "DataChannel closed for client " << c->id << std::endl;
                            }
                        });

                        dc->onMessage([&, weakClient](rtc::message_variant data) {
                            if (auto c = weakClient.lock()) {
                            // Forward message to all other clients' data channels
                                broadcastToOthers(c->id, data);
                            }
                        });
                    });

                    // Optional: connection state logs
                    client->pc->onStateChange([weakClient = std::weak_ptr<Client>(client)](rtc::PeerConnection::State state) {
                        if (auto c = weakClient.lock()) {
                            std::cout << "PC state for client " << c->id << ": " << static_cast<int>(state) << std::endl;
                        }
                    });
                }

                auto sdpIt = j.find("sdp");
                if (sdpIt == j.end() || !sdpIt->is_string()) {
                    std::cerr << "Offer missing sdp" << std::endl;
                    return;
                }

                std::string sdp = *sdpIt;
                client->pc->setRemoteDescription(rtc::Description(sdp, "offer"));
                client->pc->setLocalDescription(); // triggers onLocalDescription
            }
            else if (type == "candidate") {
                // Handle remote ICE candidate
                auto candIt = j.find("candidate");
                if (candIt == j.end() || !candIt->is_string()) return;

                std::string candidate = *candIt;
                std::string sdpMid = j.value("sdpMid", "");

                if (client->pc) {
                    client->pc->addRemoteCandidate(rtc::Candidate(candidate, sdpMid));
                }
            }
            else if (type == "ping") {
                // Optional keep-alive
                ws->send(json({{"type","pong"}}).dump());
            }
            else {
                // Ignore other message types for now
            }
    });
  });

  std::cout << "Signaling server listening on ws://127.0.0.1:8000" << std::endl;
  std::cout << "This server expects JSON signaling from clients:" << std::endl;
  std::cout << "  - Send {\"type\":\"offer\",\"sdp\":\"...\"}" << std::endl;
  std::cout << "  - Send {\"type\":\"candidate\",\"candidate\":\"...\",\"sdpMid\":\"data\",\"sdpMLineIndex\":0}" << std::endl;
  std::cout << "Server replies with \"answer\" and \"candidate\" messages." << std::endl;
  std::cout << "Any DataChannel message received from one client is forwarded to all others." << std::endl;
  std::cout << "Press Enter to quit." << std::endl;

  // Keep the process alive
  std::string dummy;
  std::getline(std::cin, dummy);

  // Clean up
  {
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (auto& [_, c] : clients) {
      if (c->dc) c->dc->close();
      if (c->pc) c->pc->close();
      if (c->ws) c->ws->close();
    }
    clients.clear();
  }

  std::cout << "Server exited." << std::endl;
  return 0;
}
