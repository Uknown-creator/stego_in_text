#ifndef STEGO_SERVER_WS_SERVER_HPP
#define STEGO_SERVER_WS_SERVER_HPP

#include <ixwebsocket/IXWebSocketServer.h>

#include <cstdint>
#include <string>

#include "broker.hpp"

/// @file ws_server.hpp
/// @brief WebSocket-сервер для MessageBroker.

namespace stego {

/// @brief Запускает WebSocket-сервер и передаёт сообщения в брокер.
class WsServer {
  public:
    /// @brief Создаёт сервер на указанном адресе и порту.
    /// @param host Адрес для прослушивания.
    /// @param port TCP-порт.
    WsServer(const std::string& host, std::uint16_t port);

    /// @brief Запускает прослушивание и блокирует поток до остановки.
    /// @throws std::runtime_error если сервер не смог слушать порт.
    void run();

  private:
    std::string host_;
    std::uint16_t port_;
    MessageBroker broker_;
    ix::WebSocketServer server_;
};

/// @brief Разбирает номер порта из строки.
/// @param value Строковое значение порта.
/// @return Порт в диапазоне 1..65535.
/// @throws std::runtime_error если порт некорректный.
std::uint16_t parse_port(const std::string& value);

}  // namespace stego

#endif  // STEGO_SERVER_WS_SERVER_HPP
