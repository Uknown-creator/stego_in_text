#include "ws_server.hpp"

#include <ixwebsocket/IXConnectionState.h>
#include <ixwebsocket/IXWebSocket.h>

#include <iostream>
#include <memory>
#include <stdexcept>

namespace stego {

WsServer::WsServer(const std::string& host, std::uint16_t port)
    : host_(host), port_(port), server_(port, host) {}

void WsServer::run() {
    server_.setOnClientMessageCallback([this](std::shared_ptr<ix::ConnectionState>,
                                              ix::WebSocket& socket,
                                              const ix::WebSocketMessagePtr& msg) {
        if (msg->type != ix::WebSocketMessageType::Message) {
            return;
        }

        std::string response;
        try {
            response = broker_.handle(msg->str);
        } catch (const std::exception& e) {
            response = nlohmann::json{{"error", e.what()}}.dump();
        } catch (...) {
            response = nlohmann::json{{"error", "unknown server error"}}.dump();
        }
        socket.send(response);
    });

    const auto result = server_.listen();
    if (!result.first) {
        throw std::runtime_error("cannot listen on " + host_ + ":" + std::to_string(port_) + ": " +
                                 result.second);
    }

    std::cout << "Server listens on ws://" << host_ << ":" << port_ << "\n";
    server_.start();
    server_.wait();
}

std::uint16_t parse_port(const std::string& value) {
    try {
        std::size_t pos = 0;
        const int port = std::stoi(value, &pos);
        if (pos != value.size() || port < 1 || port > 65535) {
            throw std::runtime_error("bad port");
        }
        return static_cast<std::uint16_t>(port);
    } catch (const std::exception&) {
        throw std::runtime_error("bad port: " + value);
    }
}

}  // namespace stego
