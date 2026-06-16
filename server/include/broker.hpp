#ifndef STEGO_SERVER_BROKER_HPP
#define STEGO_SERVER_BROKER_HPP

#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

/// @file broker.hpp
/// @brief Логика сервера: ключи клиентов и очереди сообщений.

namespace stego {

/// @brief Сообщение, которое хранится до запроса получателя.
struct QueuedMessage {
    std::string from;   ///< Идентификатор отправителя.
    std::string words;  ///< Текст сообщения в виде слов.
};

/// @brief Хранилище ключей и сообщений для JSON-протокола клиента.
class MessageBroker {
  public:
    /// @brief Обрабатывает один JSON-запрос клиента.
    /// @param request_text Строка с JSON-объектом запроса.
    /// @return JSON-ответ в виде строки.
    std::string handle(const std::string& request_text);

    /// @brief Регистрирует публичный ключ клиента.
    /// @param request JSON с полями type, id и pubkey.
    /// @return Объект {"status":"ok"}.
    /// @throws std::runtime_error если запрос некорректный.
    nlohmann::json handle_register(const nlohmann::json& request);

    /// @brief Возвращает публичный ключ по id.
    /// @param request JSON с полями type и id.
    /// @return Объект {"pubkey":"..."}.
    /// @throws std::runtime_error если ключ не найден или запрос некорректный.
    nlohmann::json handle_get_pubkey(const nlohmann::json& request);

    /// @brief Сохраняет сообщение в очереди получателя.
    /// @param request JSON с полями type, from, to и words.
    /// @return Объект {"status":"ok"}.
    /// @throws std::runtime_error если запрос некорректный.
    nlohmann::json handle_send(const nlohmann::json& request);

    /// @brief Забирает и очищает очередь сообщений клиента.
    /// @param request JSON с полями type и id.
    /// @return Объект {"messages":[...]}.
    /// @throws std::runtime_error если запрос некорректный.
    nlohmann::json handle_fetch(const nlohmann::json& request);

    /// @brief Возвращает число зарегистрированных клиентов.
    /// @return Размер таблицы публичных ключей.
    std::size_t registered_count() const;

    /// @brief Возвращает число сообщений, ожидающих получателя.
    /// @return Суммарный размер всех очередей.
    std::size_t pending_count() const;

  private:
    static std::string require_string(const nlohmann::json& request, const std::string& field);
    static void check_id(const std::string& id, const std::string& field);
    static void check_pubkey(const std::string& pubkey);
    static bool is_hex(const std::string& value);
    static nlohmann::json error_response(const std::string& message);

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::string> public_keys_;
    std::unordered_map<std::string, std::vector<QueuedMessage>> queues_;
};

}  // namespace stego

#endif  // STEGO_SERVER_BROKER_HPP
