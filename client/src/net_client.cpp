#include "net_client.hpp"

#include <nlohmann/json.hpp>

#include "errors.hpp"

namespace stego {

using nlohmann::json;

WsClient::WsClient(std::chrono::milliseconds timeout) : timeout_(timeout) {
    socket_.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        switch (msg->type) {
            case ix::WebSocketMessageType::Open:
                open_ = true;
                break;
            case ix::WebSocketMessageType::Message:
                inbox_.push_back(msg->str);
                break;
            case ix::WebSocketMessageType::Close:
            case ix::WebSocketMessageType::Error:
                closed_ = true;
                break;
            default:
                break;
        }
        cv_.notify_all();
    });
}

WsClient::~WsClient() { socket_.stop(); }

void WsClient::connect(const std::string& url) {
    socket_.setUrl(url);
    socket_.start();
    std::unique_lock<std::mutex> lock(mutex_);
    if (!cv_.wait_for(lock, timeout_, [this] { return open_ || closed_; }) || closed_) {
        throw NetworkError("Не удалось подключиться к серверу: " + url);
    }
}

std::string WsClient::send_and_wait(const std::string& payload) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        inbox_.clear();
    }
    if (!socket_.send(payload).success) {
        throw NetworkError("Не удалось отправить запрос на сервер");
    }
    std::unique_lock<std::mutex> lock(mutex_);
    if (!cv_.wait_for(lock, timeout_, [this] { return !inbox_.empty() || closed_; })) {
        throw NetworkError("Истёк таймаут ожидания ответа сервера");
    }
    if (inbox_.empty()) {
        throw NetworkError("Соединение закрыто до получения ответа");
    }
    std::string response = inbox_.front();
    inbox_.pop_front();
    return response;
}

void WsClient::register_key(const std::string& id, const std::string& public_key_hex) {
    const json request = {{"type", "register"}, {"id", id}, {"pubkey", public_key_hex}};
    const json response = json::parse(send_and_wait(request.dump()));
    if (response.value("status", "") != "ok") {
        throw NetworkError("Сервер отклонил регистрацию: " + response.value("error", "?"));
    }
}

std::string WsClient::request_public_key(const std::string& peer_id) {
    const json request = {{"type", "get_pubkey"}, {"id", peer_id}};
    const json response = json::parse(send_and_wait(request.dump()));
    if (!response.contains("pubkey")) {
        throw NetworkError("Публичный ключ не найден: " + response.value("error", peer_id));
    }
    return response.at("pubkey").get<std::string>();
}

void WsClient::send_message(const std::string& from, const std::string& to,
                            const std::string& words) {
    const json request = {{"type", "send"}, {"from", from}, {"to", to}, {"words", words}};
    const json response = json::parse(send_and_wait(request.dump()));
    if (response.value("status", "") != "ok") {
        throw NetworkError("Сервер не принял сообщение: " + response.value("error", "?"));
    }
}

std::vector<StoredMessage> WsClient::fetch(const std::string& id) {
    const json request = {{"type", "fetch"}, {"id", id}};
    const json response = json::parse(send_and_wait(request.dump()));
    if (!response.contains("messages")) {
        throw NetworkError("Некорректный ответ сервера на выборку сообщений");
    }
    std::vector<StoredMessage> result;
    for (const auto& item : response.at("messages")) {
        result.push_back({item.value("from", ""), item.value("words", "")});
    }
    return result;
}

}  // namespace stego
