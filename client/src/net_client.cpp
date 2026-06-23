#include "net_client.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <thread>

#include "errors.hpp"

namespace stego {

using nlohmann::json;

WsClient::WsClient(std::chrono::milliseconds timeout) : timeout_(timeout) {
    socket_.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (msg->type == ix::WebSocketMessageType::Open) {
            open_ = true;
        } else if (msg->type == ix::WebSocketMessageType::Message) {
            response_ = msg->str;
            has_response_ = true;
        } else if (msg->type == ix::WebSocketMessageType::Close ||
                   msg->type == ix::WebSocketMessageType::Error) {
            closed_ = true;
        }
    });
}

WsClient::~WsClient() { socket_.stop(); }

void WsClient::connect(const std::string& url) {
    socket_.setUrl(url);
    socket_.start();

    std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + timeout_;
    while (std::chrono::steady_clock::now() < deadline) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (open_) {
                return;
            }
            if (closed_) {
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    throw NetworkError("Не удалось подключиться к серверу: " + url);
}

std::string WsClient::send_and_wait(const std::string& request) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        has_response_ = false;
    }
    if (!socket_.send(request).success) {
        throw NetworkError("Не удалось отправить запрос на сервер");
    }

    std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + timeout_;
    while (std::chrono::steady_clock::now() < deadline) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (has_response_) {
                return response_;
            }
            if (closed_) {
                throw NetworkError("Соединение закрыто до получения ответа");
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    throw NetworkError("Истёк таймаут ожидания ответа сервера");
}

void WsClient::register_key(const std::string& id, const std::string& public_key_hex) {
    json request = {{"type", "register"}, {"id", id}, {"pubkey", public_key_hex}};
    json response = json::parse(send_and_wait(request.dump()));
    if (response.value("status", "") != "ok") {
        throw NetworkError("Сервер отклонил регистрацию: " + response.value("error", "?"));
    }
}

std::string WsClient::request_public_key(const std::string& peer_id) {
    json request = {{"type", "get_pubkey"}, {"id", peer_id}};
    json response = json::parse(send_and_wait(request.dump()));
    if (!response.contains("pubkey")) {
        throw NetworkError("Публичный ключ не найден: " + response.value("error", peer_id));
    }
    return response.at("pubkey").get<std::string>();
}

void WsClient::send_message(const std::string& from, const std::string& to,
                            const std::string& words) {
    json request = {{"type", "send"}, {"from", from}, {"to", to}, {"words", words}};
    json response = json::parse(send_and_wait(request.dump()));
    if (response.value("status", "") != "ok") {
        throw NetworkError("Сервер не принял сообщение: " + response.value("error", "?"));
    }
}

std::vector<StoredMessage> WsClient::fetch(const std::string& id) {
    json request = {{"type", "fetch"}, {"id", id}};
    json response = json::parse(send_and_wait(request.dump()));
    if (!response.contains("messages")) {
        throw NetworkError("Некорректный ответ сервера на выборку сообщений");
    }
    std::vector<StoredMessage> result;
    for (const json& item : response.at("messages")) {
        StoredMessage message;
        message.from = item.value("from", "");
        message.words = item.value("words", "");
        result.push_back(message);
    }
    return result;
}

}  // namespace stego
