#ifndef STEGO_CLIENT_NET_CLIENT_HPP
#define STEGO_CLIENT_NET_CLIENT_HPP

#include <ixwebsocket/IXWebSocket.h>

#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

/// @file net_client.hpp
/// @brief Клиент WebSocket поверх IXWebSocket с синхронным API запрос-ответ.

namespace stego {

/// @brief Одно сообщение, полученное при выборке с сервера.
struct StoredMessage {
    std::string from;   ///< Идентификатор отправителя.
    std::string words;  ///< Стеганографически закодированный шифртекст.
};

/// @brief Синхронная обёртка над асинхронным WebSocket-клиентом.
///
/// Реализует прикладной протокол поверх JSON: регистрация публичного ключа,
/// запрос ключа собеседника, отправка и выборка сообщений из очереди сервера.
class WsClient {
   public:
    /// @brief Создаёт клиента с заданным таймаутом ожидания ответа.
    /// @param timeout Максимальное время ожидания ответа сервера.
    explicit WsClient(std::chrono::milliseconds timeout = std::chrono::seconds(10));

    /// @brief Останавливает соединение до разрушения примитивов синхронизации.
    ~WsClient();

    /// @brief Создаёт соединение с сервером и дожидается его открытия.
    /// @param url Адрес сервера, например "ws://localhost:8080".
    /// @throws NetworkError если соединение не удалось установить за таймаут.
    void connect(const std::string& url);

    /// @brief Регистрирует на сервере публичный ключ под данным идентификатором.
    /// @param id Идентификатор текущего клиента.
    /// @param public_key_hex Публичный ключ DH в hex-виде.
    /// @throws NetworkError при ошибке передачи или ответе с ошибкой.
    void register_key(const std::string& id, const std::string& public_key_hex);

    /// @brief Запрашивает публичный ключ другого клиента по идентификатору.
    /// @param peer_id Идентификатор собеседника.
    /// @return Публичный ключ собеседника в hex-виде.
    /// @throws NetworkError если ключ не найден или произошла ошибка обмена.
    std::string request_public_key(const std::string& peer_id);

    /// @brief Отправляет закодированное сообщение получателю через сервер.
    /// @param from Идентификатор отправителя.
    /// @param to Идентификатор получателя.
    /// @param words Стеганографически закодированный шифртекст.
    /// @throws NetworkError при ошибке передачи.
    void send_message(const std::string& from, const std::string& to, const std::string& words);

    /// @brief Запрашивает все накопленные для клиента сообщения.
    /// @param id Идентификатор текущего клиента.
    /// @return Список сообщений из очереди сервера (возможно пустой).
    /// @throws NetworkError при ошибке обмена.
    std::vector<StoredMessage> fetch(const std::string& id);

   private:
    /// @brief Отправляет JSON-запрос и блокирующе ожидает JSON-ответ.
    /// @param payload Сериализованный JSON-запрос.
    /// @return Текст ответа сервера.
    /// @throws NetworkError при таймауте или закрытии соединения.
    std::string send_and_wait(const std::string& payload);

    std::chrono::milliseconds timeout_;  ///< Таймаут ожидания ответа.
    std::mutex mutex_;                   ///< Защищает поля состояния ниже.
    std::condition_variable cv_;         ///< Пробуждение при событии сети.
    std::deque<std::string> inbox_;      ///< Очередь полученных сообщений.
    bool open_ = false;                  ///< Признак установленного соединения.
    bool closed_ = false;                ///< Признак закрытия/ошибки соединения.

    // Объявлен последним: при разрушении останавливается первым, поэтому его
    // фоновый поток не обращается к уже уничтоженным mutex_/cv_ выше.
    ix::WebSocket socket_;  ///< Низкоуровневый WebSocket.
};

}  // namespace stego

#endif  // STEGO_CLIENT_NET_CLIENT_HPP
